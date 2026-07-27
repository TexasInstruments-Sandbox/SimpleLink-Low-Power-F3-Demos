/******************************************************************************

@file  app_audio.c

@brief Audio application layer for Google Voice over BLE v1.1

This file coordinates audio streaming between the voice processing layer
and BLE GATT audio stream service, implementing the application-level
logic for voice interaction.

Key Responsibilities:
1. Audio System Initialization
   - Initialize audio capture (AMIC or DMIC)
   - Initialize voice interaction state machine
   - Start periodic timeout check timer (1-second period)

2. BLE Stream Lifecycle Management
   - Enable/disable audio streaming based on BLE connection state
   - Track stream enable flag (bleStreamIsEnabled)

3. Audio Buffer Management
   - Aggregate compressed audio frames into BLE packets
   - ADPCM: 5 frames per packet (80 bytes total, fits in MTU=251)
   - mSBC: 3 frames per packet (171 bytes total, fits in MTU=251)
   - Buffer rotation with encBufIndex tracking

4. GATT Event Handling
   - ATT_MTU_UPDATED_EVENT: Adjust packet sizing if MTU changes
   - ATT_FLOW_CTRL_VIOLATED_EVENT: Handle BLE flow control violations

5. Privacy Timeout Enforcement
   - Periodic 1-second timer checks for 30-second max recording timeout
   - Delegates timeout checking to VoiceInteraction_checkTimeout()

Audio Data Flow:
1. Voice processing (voice_processing.c) captures and compresses audio
   → Invokes Application_DoAudioEvent() via callback
2. Application_DoAudioEvent() calls Audio_getCompressedBuffer()
   → Receives 16 bytes (ADPCM) or 57 bytes (mSBC) per frame
3. Frames accumulated in audio_encBuf[][] until packet full
   → ADPCM: 5 frames = 80 bytes
   → mSBC: 3 frames = 171 bytes
4. Audio_sendBufferOverBLE() transmits packet via GATT notifications
   → Handled by audio_stream service (app_audio_stream.c)

BLE Packet Sizing Strategy:
- ATT MTU: 251 bytes (negotiated via GATT_EventHandler)
- Connection interval: 7.5ms (preferred for audio streaming)
- ADPCM frame: 16 bytes → 5 frames = 80 bytes (fits comfortably)
- mSBC frame: 57 bytes → 3 frames = 171 bytes (fits comfortably)
- Headroom: 251 - 171 = 80 bytes for BLE headers and overhead

Larger NUM_OF_ENC_BUFFERS_PER_BLE_PACKET values:
- Increase RAM consumption (more buffer space)
- Increase latency (longer aggregation time)
- Reduce likelihood of dropped data during RF transmission
- Trade-off: latency vs reliability

Integration:
- voice_processing.c: Audio capture and codec compression
- voice_interaction.c: Voice state machine (PTT/HTT/On-request)
- app_audio_stream.c: BLE GATT audio stream service
- app_main.c: Calls Audio_start() during system initialization

Google ATV Spec Compliance: Voice over BLE v1.1

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

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
#include <stdarg.h>
#include <FreeRTOS.h>
#include <task.h>
#include "ti/ble/app_util/common/util.h"
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti_drivers_config.h>
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "app_main.h"
#include "voice/voice_processing.h"
#include "voice/voice_interaction.h"

// The selection of the number of encoded buffers per Bluetooth packet
// should be selected considering the payload size, the lengh of
// each encoded buffer, the connection interval, etc.
// The sizes suggested here are for ATT MTU=251 and CI=7.5 ms
// Larger values increase RAM consumption and delay.
// Smaller values make the system more likely to drop data during RF transmission.
#ifdef USE_ADPCM_CODEC
#define NUM_OF_ENC_BUFFERS_PER_BLE_PACKET    5 // max is 15
#else
#define NUM_OF_ENC_BUFFERS_PER_BLE_PACKET    3 // max is 4
#endif

uint8_t audio_encBuf[NUM_OF_ENC_BUFFERS_PER_BLE_PACKET][AUDIO_ENC_FRAME_SIZE];

uint8_t bleStreamIsEnabled = 0;
uint8_t noDataInstances = 0;
uint8_t dataInstances = 0;

// Voice interaction timeout check timer (1 second periodic)
#define VOICE_TIMEOUT_CHECK_PERIOD_MS  1000
static ClockP_Struct voiceTimeoutCheckClkStruct;
static ClockP_Handle voiceTimeoutCheckClkHandle;

void Application_onSystemInitialized(void);
void Application_onBLEStreamEnabled(void);
void Application_onBLEStreamDisabled(void);
extern void Audio_sendBufferOverBLE();
extern uint16_t connHandle;  // Global connection handle from app_connection.c

static void GATT_EventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
static void voiceTimeoutCheckCallback(uintptr_t arg);

// Events handlers struct, contains the handlers and event masks
// of the application audio module
BLEAppUtil_EventHandler_t audioGATTHandler =
{
    .handlerType    = BLEAPPUTIL_GATT_TYPE,
    .pEventHandler  = GATT_EventHandler,
    .eventMask      = BLEAPPUTIL_ATT_FLOW_CTRL_VIOLATED_EVENT |
                      BLEAPPUTIL_ATT_MTU_UPDATED_EVENT
};

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      GATT_EventHandler
 *
 * @brief   Handle GATT events for audio streaming
 *
 * @param   event - GATT event bitmask
 * @param   pMsgData - Pointer to GATT message data
 *
 * @return  None
 *
 * @note    Registered via BLEAppUtil_registerEventHandler() in Audio_start().
 *          Handles two critical events for audio streaming:
 *
 *          1. ATT_FLOW_CTRL_VIOLATED_EVENT:
 *             - Triggered when BLE stack cannot keep up with notification rate
 *             - Indicates audio data is being sent faster than BLE can transmit
 *             - Current implementation: No action (placeholder for future handling)
 *             - Potential actions: Reduce sample rate, increase connection interval,
 *               or implement dynamic bandwidth adjustment
 *
 *          2. ATT_MTU_UPDATED_EVENT:
 *             - Triggered when ATT MTU is negotiated/changed
 *             - MTU (Maximum Transmission Unit) determines max packet size
 *             - Standard: 23 bytes, Extended: 251 bytes (preferred for audio)
 *             - Current implementation: No action (packet size fixed at compile time)
 *             - Potential actions: Adjust NUM_OF_ENC_BUFFERS_PER_BLE_PACKET dynamically
 *               based on negotiated MTU
 *
 *          Event Mask: Registered in audioGATTHandler struct with:
 *          - BLEAPPUTIL_ATT_FLOW_CTRL_VIOLATED_EVENT
 *          - BLEAPPUTIL_ATT_MTU_UPDATED_EVENT
 *
 *          Integration: Called by BLE stack when registered events occur
 *********************************************************************/
static void GATT_EventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
  gattMsgEvent_t *gattMsg = ( gattMsgEvent_t * )pMsgData;
  switch ( gattMsg->method )
  {
    case ATT_FLOW_CTRL_VIOLATED_EVENT:
      {
        /* Flow control violation: BLE stack cannot keep up with audio data rate
         * Placeholder for future handling (e.g., reduce sample rate, drop frames)
         */
      }
      break;

    case ATT_MTU_UPDATED_EVENT:
      {
        /* ATT MTU updated: Adjust packet sizing based on negotiated MTU
         * Placeholder for future handling (e.g., adjust NUM_OF_ENC_BUFFERS_PER_BLE_PACKET)
         */
      }
      break;

    default:
      break;
  }
}

/*********************************************************************
 * @fn      voiceTimeoutCheckCallback
 *
 * @brief   Periodic callback to check voice interaction timeouts
 *          (30-second max recording timeout)
 *
 * @param   arg - unused
 *
 * @return  None
 */
static void voiceTimeoutCheckCallback(uintptr_t arg)
{
    // Check for 30-second max recording timeout
    VoiceInteraction_checkTimeout(connHandle);
}

/*********************************************************************
 * @fn      Application_onSystemInitialized
 *
 * @brief   Initialize audio subsystem and voice interaction
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called once during system initialization by Audio_start().
 *
 *          Initialization Sequence:
 *          1. Audio_init():
 *             - Initialize ADCBuf (AMIC) or SPI (DMIC) driver
 *             - Initialize codec (ADPCM or mSBC)
 *
 *          2. VoiceInteraction_init():
 *             - Initialize voice state machine (IDLE/ON_REQUEST/PTT/HTT)
 *             - Clear stream ID counter
 *             - Initialize interaction timestamps
 *
 *          3. Start Voice Timeout Check Timer:
 *             - Period: 1000ms (1 second)
 *             - Mode: Periodic (continues indefinitely)
 *             - Callback: voiceTimeoutCheckCallback()
 *             - Purpose: Enforce 30-second max recording timeout per Google spec
 *
 *          Privacy Timeout (per Google Voice over BLE v1.1):
 *          - Max recording duration: 30 seconds
 *          - Inactivity timeout: 1 minute (no user interaction)
 *          - Periodic check every 1 second detects timeout condition
 *          - VoiceInteraction_checkTimeout() sends AUDIO_STOP if timeout exceeded
 *
 *          Integration: Called by Audio_start() after BLE stack initialization
 *********************************************************************/
void Application_onSystemInitialized(void)
{
    Audio_init();               // Initialize audio capture hardware
    VoiceInteraction_init();    // Initialize voice state machine

    /* Start periodic timer to check voice interaction timeouts (1 second period) */
    ClockP_Params clockParams;
    ClockP_Params_init(&clockParams);
    clockParams.period = VOICE_TIMEOUT_CHECK_PERIOD_MS * 1000;  // Convert ms to µs (1000ms = 1000000µs)
    clockParams.startFlag = true;  // Start timer immediately
    clockParams.arg = 0;           // No argument needed for callback

    voiceTimeoutCheckClkHandle = ClockP_construct(&voiceTimeoutCheckClkStruct,
                                                   voiceTimeoutCheckCallback,
                                                   VOICE_TIMEOUT_CHECK_PERIOD_MS * 1000,  // Initial timeout in µs
                                                   &clockParams);
}

/*********************************************************************
 * @fn      Application_onBLEStreamEnabled
 *
 * @brief   Enable BLE audio streaming
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called when GATT client enables notifications on ATVV_CHAR_DATA
 *          characteristic (audio data notifications).
 *
 *          Sets bleStreamIsEnabled flag which is checked by:
 *          - Audio_enable() (DMIC only): Prevents starting SPI if BLE not ready
 *          - Audio_disable() (DMIC only): Prevents closing SPI if BLE was never enabled
 *
 *          Integration: Called by AudioStream GATT CCC callback when host
 *          enables notifications (writes 0x0001 to CCC descriptor).
 *********************************************************************/
void Application_onBLEStreamEnabled(void)
{
    bleStreamIsEnabled = 1;
}

/*********************************************************************
 * @fn      Application_onBLEStreamDisabled
 *
 * @brief   Disable BLE audio streaming
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called when GATT client disables notifications on ATVV_CHAR_DATA
 *          characteristic (audio data notifications).
 *
 *          Clears bleStreamIsEnabled flag which prevents audio subsystem
 *          from starting or continuing audio capture.
 *
 *          Integration: Called by AudioStream GATT CCC callback when host
 *          disables notifications (writes 0x0000 to CCC descriptor).
 *********************************************************************/
void Application_onBLEStreamDisabled(void)
{
    bleStreamIsEnabled = 0;
}

/*********************************************************************
 * @fn      Application_DoAudioEvent
 *
 * @brief   Process audio data and aggregate into BLE packets
 *
 * @param   None
 *
 * @return  None
 *
 * @note    This is the main audio processing function called every 2ms.
 *
 *          Call Flow:
 *          1. adcBufCallback() (AMIC) or spiCallbackFxn() (DMIC) completes
 *          2. Callback invokes BLEAppUtil_invokeFunctionNoData(Application_DoAudioEvent)
 *          3. This function runs in BLEAppUtil context (safe for BLE operations)
 *
 *          Processing Steps:
 *          1. Call Audio_getCompressedBuffer() to get next compressed frame
 *             - ADPCM: 16 bytes per frame (32 samples @ 4:1 compression)
 *             - mSBC: 57 bytes per frame (120 samples @ ~4.2:1 compression)
 *
 *          2. Store frame in audio_encBuf[encBufIndex][]
 *             - Buffer is 2D array: [NUM_OF_ENC_BUFFERS_PER_BLE_PACKET][AUDIO_ENC_FRAME_SIZE]
 *
 *          3. Increment encBufIndex and wait for more frames
 *
 *          4. When buffer full (encBufIndex == NUM_OF_ENC_BUFFERS_PER_BLE_PACKET):
 *             - ADPCM: 5 frames = 80 bytes
 *             - mSBC: 3 frames = 171 bytes
 *             - Call Audio_sendBufferOverBLE() to transmit via GATT notification
 *             - Reset encBufIndex to 0
 *
 *          Frame Aggregation Strategy:
 *          - ADPCM frame rate: 500 Hz (2ms period, 16 bytes)
 *            * 5 frames → 10ms aggregation time → 100 packets/second
 *            * Packet size: 80 bytes (fits comfortably in ATT MTU=251)
 *
 *          - mSBC frame rate: ~111 Hz (9ms period, 57 bytes)
 *            * 3 frames → 27ms aggregation time → 37 packets/second
 *            * Packet size: 171 bytes (fits comfortably in ATT MTU=251)
 *
 *          Latency:
 *          - ADPCM: 2ms (capture) + 10ms (aggregation) = 12ms total
 *          - mSBC: 9ms (capture) + 27ms (aggregation) = 36ms total
 *
 *          Error Handling:
 *          - AUDIO_FAIL_NO_DATA: No compressed data available (increments noDataInstances)
 *          - dataLength == 0: Frame not complete yet (still accumulating samples)
 *          - dataLength > 0: Valid frame received (increments dataInstances)
 *
 *          Integration:
 *          - Called by voice_processing.c via BLEAppUtil context switch
 *          - Transmits data via Audio_sendBufferOverBLE() in app_audio_stream.c
 *********************************************************************/
void Application_DoAudioEvent()
{

    static int encBufIndex = 0;
    uint16_t dataLength = Audio_getCompressedBuffer(audio_encBuf[encBufIndex]);

    if(AUDIO_FAIL_NO_DATA == dataLength)
    {
        /* No data available from audio capture (should not happen in normal operation) */
        noDataInstances++;
    }
    else
    {
        if(0 != dataLength)
        {
            /* Valid compressed frame received, advance to next buffer slot */
            encBufIndex++;
            dataInstances++;
        }
        /* If dataLength == 0, frame not complete yet (accumulating samples) */
    }

    /* When enough frames accumulated, send the aggregated packet over BLE */
    if(encBufIndex == NUM_OF_ENC_BUFFERS_PER_BLE_PACKET)
    {
        Audio_sendBufferOverBLE(audio_encBuf, sizeof(audio_encBuf), 1);
        encBufIndex = 0;  // Reset buffer index for next packet
    }

}

/*********************************************************************
 * @fn      Audio_start
 *
 * @brief   Initialize audio application module and register GATT handlers
 *
 * @param   None
 *
 * @return  SUCCESS on success, error code on failure
 *
 * @note    Called by app_main.c after BLE stack initialization.
 *
 *          Initialization Sequence:
 *          1. Register GATT event handler (audioGATTHandler)
 *             - Handles ATT_MTU_UPDATED_EVENT
 *             - Handles ATT_FLOW_CTRL_VIOLATED_EVENT
 *
 *          2. Call Application_onSystemInitialized()
 *             - Initialize audio capture hardware (AMIC or DMIC)
 *             - Initialize voice interaction state machine
 *             - Start periodic timeout check timer (1-second period)
 *
 *          Error Handling:
 *          - If BLEAppUtil_registerEventHandler() fails, returns error code
 *          - Caller should check return value and halt if initialization fails
 *
 *          Integration:
 *          - Called once during system boot by app_main.c
 *          - Must be called after BLE stack initialization
 *          - Must be called before any audio streaming can occur
 *********************************************************************/
bStatus_t Audio_start( void )
{
  bStatus_t status = SUCCESS;
  /* Register GATT event handlers for audio streaming */
  status = BLEAppUtil_registerEventHandler( &audioGATTHandler );
  Application_onSystemInitialized();

  // Return status value
  return( status );
}


