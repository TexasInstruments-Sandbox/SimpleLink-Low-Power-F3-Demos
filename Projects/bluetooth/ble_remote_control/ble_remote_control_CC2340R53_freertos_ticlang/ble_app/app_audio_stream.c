/******************************************************************************

@file  app_audio_stream.c

@brief BLE audio stream GATT profile management for Google Voice over BLE v1.1

This file implements the application-level interface to the Audio Stream Profile
(ASP), which provides the GATT characteristics for voice streaming per Google's
Voice over BLE specification v1.1.

GATT Service: Android TV Voice Service (ATVV)
- Service UUID: 0x64A70001-F691-4B93-A6F4-0968F5B648F8

GATT Characteristics:
1. ATVV_CHAR_CTL (Control, UUID: 0x64A70002-...)
   - Properties: Write, Notify
   - Purpose: Host sends voice control commands (MIC_OPEN, MIC_CLOSE, GET_CAPS, etc.)
   - Remote sends capability responses (CAPS_RESP, MIC_OPEN_ERROR, etc.)

2. ATVV_CHAR_DATA (Audio Data, UUID: 0x64A70003-...)
   - Properties: Notify
   - Purpose: Remote sends compressed audio frames to host
   - CCC (Client Characteristic Configuration) enables/disables notifications

Key Responsibilities:
1. GATT Profile Initialization
   - Register Audio Stream Profile (ASP) with BLE stack
   - Register callbacks for CCC updates and incoming control commands

2. CCC (Client Characteristic Configuration) Management
   - Detect when host enables audio data notifications (CCC = 0x0001)
   - Detect when host disables audio data notifications (CCC = 0x0000)
   - Call Application_onBLEStreamEnabled() when notifications enabled
   - Call Application_onBLEStreamDisabled() when notifications disabled

3. Voice Control Command Processing
   - Receive commands from host on ATVV_CHAR_CTL (Write)
   - Commands: MIC_OPEN, MIC_CLOSE, MIC_EXTEND, GET_CAPS
   - Delegate command processing to voice_interaction.c state machine

Voice Control Commands (per Google Voice over BLE v1.1):
- MIC_OPEN (0x04): Start audio streaming
  * Byte 0: 0x04 (command)
  * Byte 1: Codec (0x01=ADPCM 8kHz, 0x02=ADPCM 16kHz, 0x08=mSBC)
  * Byte 2-3: Reserved

- MIC_CLOSE (0x05): Stop audio streaming
  * Byte 0: 0x05 (command)

- MIC_EXTEND (0x0B): Extend audio session timeout
  * Byte 0: 0x0B (command)
  * For PTT/HTT modes only

- GET_CAPS (0x0A): Request remote capabilities
  * Byte 0: 0x0A (command)
  * Remote responds with CAPS_RESP (0x0B) on ATVV_CHAR_CTL

Audio Data Flow:
1. Host enables notifications on ATVV_CHAR_DATA (writes 0x0001 to CCC)
   → AS_onCccUpdateCB() called with pValue=1
   → Application_onBLEStreamEnabled() sets bleStreamIsEnabled flag
2. Host sends MIC_OPEN on ATVV_CHAR_CTL
   → AS_incomingDataCB() called
   → Voice interaction state machine starts audio capture
3. Audio frames transmitted via ASP_sendData() (GATT notifications)
   → Called by Audio_sendBufferOverBLE() in app_audio.c
4. Host sends MIC_CLOSE or timeout occurs
   → Audio capture stops
   → Host disables notifications (writes 0x0000 to CCC)
   → AS_onCccUpdateCB() called with pValue=0
   → Application_onBLEStreamDisabled() clears bleStreamIsEnabled flag

Integration:
- audio_stream_profile.c: Low-level GATT characteristic registration
- app_audio.c: Audio buffer management and BLE transmission
- voice_interaction.c: Voice state machine for PTT/HTT/On-request
- voice_processing.c: Audio capture and codec compression

Google ATV Spec Compliance: Voice over BLE v1.1

Group: WCS, BTS
Target Device: cc23xx

Copyright (c) 2022-2026, Texas Instruments Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

*  Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

*  Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

*  Neither the name of Texas Instruments Incorporated nor the names of
its contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************

Copyright (c) 2022-2026, Texas Instruments Incorporated
All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

******************************************************************************
*****************************************************************************/

#include <string.h>
#include <time.h>
#include <ti/drivers/GPIO.h>
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <ble_app/app_main.h>
#include <ble_app/Profiles/audio_stream/audio_stream_profile.h>
#include <voice/google_voice_spec.h>
#include <voice/voice_interaction.h>

//*****************************************************************************
//! Defines
//*****************************************************************************
#define AS_CCC_UPDATE_NOTIFICATION_ENABLED  1

//*****************************************************************************
//! Globals
//*****************************************************************************
extern void Application_onSystemInitialized(void);
extern void Application_onBLEStreamEnabled(void);
extern void Application_onBLEStreamDisabled(void);
extern void Application_DoAudioEvent();
extern uint16_t connHandle;  // Global connection handle from app_connection.c
//*****************************************************************************
//!LOCAL FUNCTIONS
//*****************************************************************************

static void AS_onCccUpdateCB( uint16 connHandle, uint16 pValue );
static void AS_incomingDataCB( uint16 connHandle, char *pValue, uint16 len );
static void AS_handleGetCaps( uint16 connHandle );

//*****************************************************************************
//!APPLICATION CALLBACK
//*****************************************************************************
// Data Stream application callback function for incoming data
static ASP_cb_t as_profileCB =
{
  AS_onCccUpdateCB,
  AS_incomingDataCB
};

//*****************************************************************************
//! Functions
//*****************************************************************************
/*********************************************************************
 * @fn      AS_onCccUpdateCB
 *
 * @brief   Handle CCC updates for ATVV_CHAR_DATA notifications
 *
 * @param   connHandle - BLE connection handle
 * @param   pValue - CCC value (1=enabled, 0=disabled)
 *
 * @return  None
 *
 * @note    Called by Audio Stream Profile when host writes to CCC descriptor.
 *
 *          CCC (Client Characteristic Configuration):
 *          - Descriptor UUID: 0x2902
 *          - Written by host to enable/disable GATT notifications
 *          - Value 0x0001: Notifications enabled
 *          - Value 0x0000: Notifications disabled
 *
 *          Voice Streaming Lifecycle:
 *          1. Host enables notifications on ATVV_CHAR_DATA (writes 0x0001)
 *             → This callback called with pValue=1
 *             → Application_onBLEStreamEnabled() sets bleStreamIsEnabled flag
 *             → Remote ready to stream audio
 *
 *          2. Host sends MIC_OPEN on ATVV_CHAR_CTL
 *             → Audio capture starts, data sent via ASP_sendData()
 *
 *          3. Host disables notifications (writes 0x0000)
 *             → This callback called with pValue=0
 *             → Application_onBLEStreamDisabled() clears bleStreamIsEnabled flag
 *             → Remote stops audio streaming
 *
 *          Per Google Voice over BLE v1.1:
 *          - Host must enable notifications before sending MIC_OPEN
 *          - Remote should not start audio capture if notifications disabled
 *          - Host should disable notifications after MIC_CLOSE to save power
 *
 *          Integration:
 *          - Registered in as_profileCB callback structure
 *          - Called by audio_stream_profile.c when CCC write occurs
 *          - Updates bleStreamIsEnabled flag checked by audio subsystem
 *********************************************************************/
static void AS_onCccUpdateCB( uint16 connHandle, uint16 pValue )
{
  if ( pValue == AS_CCC_UPDATE_NOTIFICATION_ENABLED)  // 0x0001
  {
    MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                      "AudioStream status: CCC Update - connectionHandle: "
                      MENU_MODULE_COLOR_YELLOW "%d " MENU_MODULE_COLOR_RESET
                      "Notifications enabled", connHandle);
    Application_onBLEStreamEnabled();  // Set bleStreamIsEnabled = 1
  }
  else  // 0x0000
  {
    MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                      "AudioStream status: CCC Update - connectionHandle: "
                      MENU_MODULE_COLOR_YELLOW "%d " MENU_MODULE_COLOR_RESET
                      "Notifications disabled", connHandle);
    Application_onBLEStreamDisabled();  // Clear bleStreamIsEnabled = 0
  }
}

/*********************************************************************
 * @fn      AS_handleGetCaps
 *
 * @brief   Handle GET_CAPS command and send CAPS_RESP
 *
 * @param   connHandle - BLE connection handle
 *
 * @return  None
 *
 * @note    Builds and sends capability response per Google Voice over BLE v1.1.
 *
 *          CAPS_RESP Format (0x0B):
 *          - Byte 0: 0x0B (CAPS_RESP command)
 *          - Bytes 1-2: Spec version 0x01 0x01 (v1.1)
 *          - Byte 3: Codecs supported (bitmask)
 *            * bit 0: ADPCM 8kHz
 *            * bit 1: ADPCM 16kHz
 *            * Result: 0x03 (both supported)
 *          - Byte 4: Assistant interaction models (bitmask)
 *            * bit 0: On-request
 *            * bit 1: PTT
 *            * bit 2: HTT
 *            * Result: 0x07 (all three supported)
 *          - Bytes 5-6: ADPCM frame size (big-endian)
 *            * 0x00 0xA0 = 160 bytes (recommended for 16kHz + DLE)
 *          - Byte 7: Extra configuration flags
 *            * bit 0: DLE support (0x01)
 *          - Byte 8: Reserved (0x00)
 *
 *          Total: 9 bytes
 *********************************************************************/
static void AS_handleGetCaps( uint16 connHandle )
{
    // Build CAPS_RESP payload
    uint8_t capsResp[9] = {
        GVOICE_CMD_CAPS_RESP,                  // 0x0B - CAPS_RESP command
        GVOICE_SPEC_VERSION_MAJOR,             // 0x01 - Spec version major
        GVOICE_SPEC_VERSION_MINOR,             // 0x01 - Spec version minor
        GVOICE_CODEC_ADPCM_16KHZ,              // 0x02 - 16kHz ADPCM supported
        0x07,                                  // 0x07 - On-request + PTT + HTT models
        (GVOICE_FRAME_SIZE_RECOMMENDED >> 8),  // 0x00 - Frame size MSB (160 bytes)
        (GVOICE_FRAME_SIZE_RECOMMENDED & 0xFF),// 0xA0 - Frame size LSB
        GVOICE_EXTRA_CFG_DLE,                  // 0x01 - DLE supported
        0x00                                   // 0x00 - Reserved
    };

    // Send CAPS_RESP on ATVV_CHAR_CTL (control characteristic)
    bStatus_t status = ASP_sendData( capsResp, sizeof(capsResp) );

    if (status == SUCCESS)
    {
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                          "Voice: GET_CAPS received, CAPS_RESP sent (spec v1.1, ADPCM 8k+16k, HTT+PTT+On-req)");
    }
    else
    {
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                          "Voice: CAPS_RESP send failed, status=0x%02X", status);
    }
}

/*********************************************************************
 * @fn      AS_incomingDataCB
 *
 * @brief   Handle incoming voice control commands on ATVV_CHAR_CTL
 *
 * @param   connHandle - BLE connection handle
 * @param   pValue - Pointer to received data
 * @param   len - Length of received data
 *
 * @return  None
 *
 * @note    Called by Audio Stream Profile when host writes to ATVV_CHAR_CTL.
 *
 *          Voice Control Commands (per Google Voice over BLE v1.1):
 *
 *          MIC_OPEN (0x04): Start audio streaming
 *          - Byte 0: 0x04 (command)
 *          - Byte 1: Codec (0x01=ADPCM 8kHz, 0x02=ADPCM 16kHz, 0x08=mSBC)
 *          - Byte 2-3: Reserved
 *
 *          MIC_CLOSE (0x05): Stop audio streaming
 *          - Byte 0: 0x05 (command)
 *
 *          GET_CAPS (0x0A): Request remote capabilities
 *          - Byte 0: 0x0A (command)
 *          - Remote responds with CAPS_RESP via ASP_sendData()
 *
 *          MIC_EXTEND (0x0B): Extend audio session timeout
 *          - Byte 0: 0x0B (command)
 *          - For PTT/HTT modes only
 *
 *          SYNC (0x0D): Synchronize audio session
 *          - Byte 0: 0x0D (command)
 *
 *          Expected Implementation:
 *          - Parse pValue[0] to determine command type
 *          - Delegate to voice_interaction.c state machine:
 *            * MIC_OPEN → VoiceInteraction_handleMicOpen()
 *            * MIC_CLOSE → VoiceInteraction_handleMicClose()
 *            * GET_CAPS → VoiceInteraction_handleGetCaps()
 *            * MIC_EXTEND → VoiceInteraction_handleMicExtend()
 *
 *          Current Implementation:
 *          - Legacy echo functionality (case conversion and echo back)
 *          - Should be updated to handle actual voice control commands
 *          - For now, this serves as placeholder for GATT Write testing
 *
 *          Error Handling:
 *          - len == 0: Invalid length, send error response
 *          - Unknown command: Ignore or send MIC_OPEN_ERROR response
 *
 *          Integration:
 *          - Registered in as_profileCB callback structure
 *          - Called by audio_stream_profile.c when Write occurs on ATVV_CHAR_CTL
 *          - Should delegate to voice_interaction.c for command processing
 *
 *          TODO: Replace legacy echo logic with voice command parsing
 *********************************************************************/
static void AS_incomingDataCB( uint16 connHandle, char *pValue, uint16 len )
{

  /* Validate input */
  if ( len == 0 || pValue == NULL )
  {
    MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                      "Voice: Invalid command (len=0)");
    return;
  }

  /* Parse command byte (first byte) */
  uint8_t command = (uint8_t)pValue[0];

  MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                    "Voice: Command 0x%02X received (len=%d)", command, len);

  /* Process voice command */
  switch (command)
  {
    case GVOICE_CMD_GET_CAPS:  // 0x0A - Get Capabilities
    {
      AS_handleGetCaps(connHandle);
      break;
    }

    case GVOICE_CMD_MIC_OPEN:  // 0x04 - Open Microphone (On-request model)
    {
      if (len >= 2)
      {
        uint8_t codec = (uint8_t)pValue[1];
        bool result = VoiceInteraction_handleMicOpen(connHandle, (uint8_t*)pValue, len);
        if (!result)
        {
          MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                            "Voice: MIC_OPEN rejected (busy or timeout)");
        }
      }
      else
      {
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                          "Voice: MIC_OPEN invalid length");
      }
      break;
    }

    case GVOICE_CMD_MIC_CLOSE:  // 0x05 - Close Microphone
    {
      bool result = VoiceInteraction_handleMicClose(connHandle, (uint8_t*)pValue, len);
      if (!result)
      {
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                          "Voice: MIC_CLOSE rejected (not in On-request mode)");
      }
      break;
    }

    case GVOICE_CMD_MIC_EXTEND:  // 0x0B - Extend Audio Session
    {
      bool result = VoiceInteraction_handleMicExtend(connHandle, (uint8_t*)pValue, len);
      if (!result)
      {
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                          "Voice: MIC_EXTEND rejected (not in PTT/HTT mode)");
      }
      break;
    }

    default:
    {
      MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                        "Voice: Unknown command 0x%02X", command);
      break;
    }
  }
}

/*********************************************************************
 * @fn      AudioStream_start
 *
 * @brief   Initialize and register Audio Stream Profile for voice over BLE
 *
 * @param   None
 *
 * @return  SUCCESS on success, error code on failure
 *
 * @note    Called by app_main.c after BLE stack initialization.
 *
 *          Initialization:
 *          - Call ASP_start() to register Audio Stream Profile with BLE stack
 *          - Pass as_profileCB callback structure containing:
 *            * AS_onCccUpdateCB: Handles CCC updates (notifications enable/disable)
 *            * AS_incomingDataCB: Handles voice control commands (MIC_OPEN, etc.)
 *
 *          GATT Service Registration:
 *          ASP_start() registers Android TV Voice Service (ATVV) with:
 *          - Service UUID: 0x64A70001-F691-4B93-A6F4-0968F5B648F8
 *          - ATVV_CHAR_CTL: Control characteristic (Write, Notify)
 *            * UUID: 0x64A70002-F691-4B93-A6F4-0968F5B648F8
 *            * Host writes voice control commands
 *            * Remote sends capability responses and status
 *          - ATVV_CHAR_DATA: Audio data characteristic (Notify)
 *            * UUID: 0x64A70003-F691-4B93-A6F4-0968F5B648F8
 *            * Remote sends compressed audio frames
 *            * CCC descriptor for enabling/disabling notifications
 *
 *          Error Handling:
 *          - If ASP_start() fails, returns error code
 *          - Caller should check return value and halt if initialization fails
 *
 *          Integration:
 *          - Called once during system boot by app_main.c
 *          - Must be called after BLE stack initialization
 *          - Must be called before Audio_start() to ensure profile registered
 *
 *          Google ATV Spec Compliance: Voice over BLE v1.1
 *********************************************************************/
bStatus_t AudioStream_start( void )
{
  bStatus_t status = SUCCESS;

  /* Register Audio Stream Profile with BLE stack and callbacks */
  status = ASP_start( &as_profileCB );
  if( status != SUCCESS )
  {
    /* Profile registration failed - return error to caller */
    return status;
  }

  return ( SUCCESS );
}
