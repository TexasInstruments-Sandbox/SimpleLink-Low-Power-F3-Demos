/******************************************************************************

@file  data_stream_profile.c

@brief This file contains the Audio Stream profile sample GATT profile
for use with the BLE sample application.

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

//*****************************************************************************
//! Includes
//*****************************************************************************
#include <string.h>
#include "ti/ble/stack_util/icall/app/icall.h"
/* This Header file contains all BLE API and icall structure definition */
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"

#include <ble_app/Profiles/audio_stream/audio_stream_profile.h>
#include <ble_app/Profiles/audio_stream/audio_stream_server.h>
#include <voice/google_voice_spec.h>
#include <voice/voice_interaction.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/dpl/ClockP.h>

#define RC_IDLE_TIMEOUT         3000  // Initial delay: 3 seconds before first param update request
#define RC_RETRY_INTERVAL       5000  // Retry interval: 5 seconds if update rejected/ignored
#define RC_MAX_RETRIES          3     // Maximum retry attempts before giving up
#define CLOCK_UNITS_MS          1000  // convert to ms

// Connection parameter update state
static ClockP_Struct rc_audio_parmupdate_clkStruct;
static ClockP_Handle rc_audio_parmupdate_clkHandle;
static ClockP_Params clockpParams;
static uint16_t paramUpdate_connHandle = 0xFFFF;  // Connection handle for parameter update
static uint8_t paramUpdate_retryCount = 0;        // Retry counter

// CAPS_RESP buffer - Google Voice over BLE spec v1.1
// Format: CMD(1) + Version(2) + Codecs(1) + Model(1) + FrameSize(2) + ExtraCfg(1) + Reserved(1) + FwData(var)
uint8_t buf_ctl_response[10] = {
    GVOICE_CMD_CAPS_RESP,        // 0x0B: CAPS_RESP command
    0x01, 0x01,                  // Spec version 1.1 (0x0101, big-endian)
    GVOICE_CODEC_ADPCM_16KHZ,     // 0x02: Codecs - ADPCM 16kHz
    GVOICE_MODEL_HTT,            // 0x03: Interaction model - HTT/PTT/On-request
    0x00, 0xA0,                  // ADPCM frame size: 160 bytes (0x00A0, big-endian)
    GVOICE_EXTRA_CFG_DLE,        // 0x01: Extra config - DLE supported
    0x00,                        // Reserved
    0x00                         // Optional FW version data (0 bytes)
};

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
static ASP_cb_t *asp_appCB = NULL;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void ASP_onCccUpdateCB( char *pValue );
static void ASP_incomingDataCB( char *pValue );

/*********************************************************************
 * PROFILE CALLBACKS
 */
// Audio Stream profile callback function for incoming data
static AS_cb_t asp_serverCB =
{
  (void *)ASP_onCccUpdateCB,
  (void *)ASP_incomingDataCB
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      ASP_start
 *
 * @brief   This function adds the Audio Stream Server service,
 *          registers the service and profile callback function and allocates
 *          buffer for incoming data.
 *
 * @return  SUCCESS or stack call status
 */
bStatus_t ASP_start( ASP_cb_t *appCallbacks )
{
  uint8 status = SUCCESS;

  // Add audio stream service
  status = AS_addService();
  if ( status != SUCCESS )
  {
    // Return status value
    return ( status );
  }

  // Register to service callback function
  status = AS_registerProfileCBs( &asp_serverCB );
  if ( status != SUCCESS )
  {
    // Return status value
    return ( status );
  }

  // Registers the application callback function
  if ( appCallbacks )
  {
    asp_appCB = appCallbacks;
  }
  else
  {
    return ( INVALIDPARAMETER );
  }

  // Return status value
  return ( status );
}

/*********************************************************************
 * @fn      ASP_SendData
 *
 * @brief   Send data over the GATT notification
 *
 * @param   pValue - pointer to data to write
 * @param   len - length of data to write
 *
 * @return  SUCCESS or stack call status
 */
bStatus_t ASP_sendData( uint8 *pValue, uint16 len )
{
  uint8 status = SUCCESS;

  status = AS_setParameter( AUDIOPROFILE_AUDIO, pValue, len );

  // Return status value
  return ( status );
}

/*********************************************************************
 * @fn      ASP_onCccUpdateCB
 *
 * @brief   Callback from Audio_Stream_Server indicating CCC has been update
 *
 * @param   connHandle - connection message was received on
 * @param   pValue - pointer to data
 *
 * @return  none
 */
static void ASP_onCccUpdateCB( char *pValue  )
{
  AS_cccUpdate_t *cccUpdate = ( AS_cccUpdate_t *)pValue;

  if ( asp_appCB && asp_appCB->pfnOnCccUpdateCb )
  {
    asp_appCB->pfnOnCccUpdateCb( cccUpdate->connHandle, cccUpdate->value );
  }
}

/*********************************************************************
 * @fn      ASP_IncomingDataCB
 *
 * @brief   Callback from Audio_Stream_Server indicating incoming data
 *
 * @param   pValue - pointer to the incoming data
 *
 * @return  none
 */

/*********************************************************************
 * @fn      rc_audio_parmupdate_clkHandleCB
 *
 * @brief   Timer callback to request BLE connection parameter update for audio streaming
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called 3 seconds after connection establishment (first attempt) or
 *          5 seconds after rejected update (retry attempts).
 *
 *          Optimal Parameters for Google Voice over BLE v1.1:
 *          - Connection Interval: 11.25ms (9 * 1.25ms) - Balanced for audio + HID
 *            * Google recommends 7.5-11.25ms for audio streaming
 *            * 11.25ms allows ~88 packets/second (sufficient for ADPCM 500Hz frame rate)
 *          - Slave Latency: 0 - Low latency required for real-time audio
 *          - Supervision Timeout: 6 seconds (600 * 10ms) - Standard timeout
 *
 *          Retry Logic:
 *          - Retries up to 3 times if parameter update rejected or ignored
 *          - 5-second interval between retries
 *          - Timer stops after max retries or successful update
 *
 *          Integration:
 *          - Started by config_audio_parmupdate() on connection or GET_CAPS
 *          - Stopped by successful LINK_PARAM_UPDATE_EVENT or max retries
 *********************************************************************/
static void rc_audio_parmupdate_clkHandleCB(void)
{
    bStatus_t status;

    // Validate connection handle
    if (paramUpdate_connHandle == 0xFFFF) {
        // Invalid connection handle - stop timer
        if (rc_audio_parmupdate_clkHandle != NULL) {
            ClockP_stop(rc_audio_parmupdate_clkHandle);
        }
        return;
    }

    // Check retry limit
    if (paramUpdate_retryCount >= RC_MAX_RETRIES) {
        // Max retries reached - stop timer and give up
        if (rc_audio_parmupdate_clkHandle != NULL) {
            ClockP_stop(rc_audio_parmupdate_clkHandle);
        }
        paramUpdate_retryCount = 0;  // Reset for next connection
        return;
    }

    // Prepare L2CAP parameter update request
    // Note: For BLE peripheral role, L2CAP_ConnParamUpdateReq sends
    // LL_CONNECTION_PARAM_REQ to the central device
    l2capParamUpdateReq_t updateReq;
    updateReq.intervalMin = 6;              // 11.25ms (9 * 1.25ms) - Optimal for audio + HID
    updateReq.intervalMax = 6;              // 11.25ms (keep min=max for consistent timing)
    updateReq.peripheralLatency = 0;        // 0 latency for real-time audio streaming
    updateReq.timeoutMultiplier = 600;      // 6 seconds (600 * 10ms) supervision timeout

    // Send parameter update request to BLE stack
    status = L2CAP_ConnParamUpdateReq(paramUpdate_connHandle, &updateReq, BLEAppUtil_getSelfEntity());

    if (status == SUCCESS) {
        // Request sent successfully - increment retry counter
        paramUpdate_retryCount++;

        // If not max retries yet, keep timer running for potential retry
        // (Timer will be stopped in LINK_PARAM_UPDATE_EVENT handler if successful)
    } else {
        // Request failed to send - stop timer
        if (rc_audio_parmupdate_clkHandle != NULL) {
            ClockP_stop(rc_audio_parmupdate_clkHandle);
        }
        paramUpdate_retryCount = 0;
    }
}

/*********************************************************************
 * @fn      config_audio_parmupdate
 *
 * @brief   Configure and start connection parameter update timer
 *
 * @param   connHandle - BLE connection handle to update parameters for
 *
 * @return  None
 *
 * @note    Starts a periodic timer that requests optimal connection parameters for
 *          audio streaming. The timer fires after 3 seconds initially, then every
 *          5 seconds for up to 3 retry attempts if the central rejects the request.
 *
 *          Call this function:
 *          - On connection establishment (from app_connection.c)
 *          - On GET_CAPS command (when host queries capabilities)
 *
 *          The timer automatically stops when:
 *          - Parameter update succeeds (detected in LINK_PARAM_UPDATE_EVENT)
 *          - Maximum retries reached (3 attempts)
 *          - Connection is terminated
 *********************************************************************/
void config_audio_parmupdate(uint16_t connHandle)
{
    // Store connection handle for use in callback
    paramUpdate_connHandle = connHandle;
    paramUpdate_retryCount = 0;  // Reset retry counter

    // Stop existing timer if already running
    if (rc_audio_parmupdate_clkHandle != NULL) {
        ClockP_stop(rc_audio_parmupdate_clkHandle);
    }

    // Initialize timer parameters
    ClockP_Params_init(&clockpParams);
    uint32_t clockTicks = RC_IDLE_TIMEOUT * (CLOCK_UNITS_MS);  // 3 seconds initial delay
    clockpParams.period = RC_RETRY_INTERVAL * (CLOCK_UNITS_MS);  // 5 seconds for retries
    clockpParams.startFlag = true;  // Start immediately
    clockpParams.arg = (uintptr_t)rc_audio_parmupdate_clkHandleCB;

    // Construct and start periodic timer
    rc_audio_parmupdate_clkHandle = ClockP_construct(&rc_audio_parmupdate_clkStruct,
                                                       (void *)BLEAppUtil_invokeFunctionNoData,
                                                       clockTicks,
                                                       &clockpParams);
}

/*********************************************************************
 * @fn      stop_audio_parmupdate
 *
 * @brief   Stop connection parameter update timer
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Stops the periodic timer and resets retry state.
 *          Call this function when:
 *          - Connection parameter update succeeds (LINK_PARAM_UPDATE_EVENT)
 *          - Connection is terminated (LINK_TERMINATED_EVENT)
 *********************************************************************/
void stop_audio_parmupdate(void)
{
    // Stop the timer if it's running
    if (rc_audio_parmupdate_clkHandle != NULL) {
        ClockP_stop(rc_audio_parmupdate_clkHandle);
    }

    // Reset state
    paramUpdate_connHandle = 0xFFFF;
    paramUpdate_retryCount = 0;
}

static void ASP_incomingDataCB( char *pValue )
{
  AS_dataIn_t *dataIn = ( AS_dataIn_t * )pValue;

  if ( asp_appCB && asp_appCB->pfnIncomingDataCB )
  {
    asp_appCB->pfnIncomingDataCB( dataIn->connHandle, dataIn->pValue, dataIn->len );
  }

  // Handle Google Voice over BLE commands
  if (dataIn->len < 1) {
      return;  // Invalid: no command byte
  }

  uint8_t command = dataIn->pValue[0];

  switch (command) {
      case GVOICE_CMD_GET_CAPS:  // 0x0A - Get capabilities
      {
          // GET_CAPS payload: CMD(1) + version(2) + codecs(1) + models(1) = 5 bytes minimum
          if (dataIn->len >= 5) {
              uint16_t host_version = (dataIn->pValue[1] << 8) | dataIn->pValue[2];  // big-endian
              uint8_t host_codecs = dataIn->pValue[3];
              uint8_t host_models = dataIn->pValue[4];

              // Log host capabilities (optional, for debugging)
              // MenuModule_printf("Voice: GET_CAPS from host v%d.%d, codecs=0x%02X, models=0x%02X",
              //                   (host_version >> 8), (host_version & 0xFF), host_codecs, host_models);
          }

          // Configure audio connection parameters with retry logic
          // Pass connection handle to ensure correct connection is updated
          config_audio_parmupdate(dataIn->connHandle);

          // Send CAPS_RESP on ATVV_CHAR_CTL (char_type=2)
          Audio_sendBufferOverBLE(buf_ctl_response, sizeof(buf_ctl_response), 2);
          break;
      }

      case GVOICE_CMD_MIC_OPEN:  // 0x0C - Open microphone
      {
          uint8_t *payload = (dataIn->len > 1) ? &dataIn->pValue[1] : NULL;
          uint16_t payloadLen = (dataIn->len > 1) ? dataIn->len - 1 : 0;
          VoiceInteraction_handleMicOpen(dataIn->connHandle, payload, payloadLen);
          break;
      }

      case GVOICE_CMD_MIC_CLOSE:  // 0x0D - Close microphone
      {
          uint8_t *payload = (dataIn->len > 1) ? &dataIn->pValue[1] : NULL;
          uint16_t payloadLen = (dataIn->len > 1) ? dataIn->len - 1 : 0;
          VoiceInteraction_handleMicClose(dataIn->connHandle, payload, payloadLen);
          break;
      }

      case GVOICE_CMD_MIC_EXTEND:  // 0x0E - Extend audio session
      {
          uint8_t *payload = (dataIn->len > 1) ? &dataIn->pValue[1] : NULL;
          uint16_t payloadLen = (dataIn->len > 1) ? dataIn->len - 1 : 0;
          VoiceInteraction_handleMicExtend(dataIn->connHandle, payload, payloadLen);
          break;
      }

      default:
          // Unknown command - ignore silently (host may send vendor-specific commands)
          break;
  }
}

/*********************************************************************
*********************************************************************/
