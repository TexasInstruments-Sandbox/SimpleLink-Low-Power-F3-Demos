/******************************************************************************

@file  app_audio.c

@brief Audio module integration for basic_ble project.
       Handles audio encoding and BLE streaming callbacks.

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

 Copyright (c) 2022-2025, Texas Instruments Incorporated
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
*****************************************************************************/

#include <string.h>
#include <stdarg.h>
#include <FreeRTOS.h>
#include <task.h>
#include "ti/ble/app_util/common/util.h"
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "app_main.h"
#include "voice/voice_processing.h"

// The selection of the number of encoded buffers per Bluetooth packet
// should be selected considering the payload size, the length of
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

// Tracks whether the audio hardware (SPI/ADC) is currently open and running.
// Guards against double-enable or disable-before-enable, both of which
// corrupt the driver state and require a full reset to recover.
static uint8_t audioIsRunning = 0;

void Application_onSystemInitialized(void);
void Application_onBLEStreamEnabled(void);
void Application_onBLEStreamDisabled(void);
extern void Audio_sendBufferOverBLE(uint8_t *buffer, uint16_t size, uint8_t char_type);

/*********************************************************************
 * @fn      Audio_startStreaming / Audio_stopStreaming
 *
 * @brief   Safe wrappers around Audio_enable / Audio_disable.
 *          Idempotent: calling start while already running, or stop
 *          while already stopped, is a no-op instead of a crash.
 */
void Audio_startStreaming(void)
{
    if (!audioIsRunning)
    {
        audioIsRunning = 1;
        Audio_enable();
    }
}

void Audio_stopStreaming(void)
{
    if (audioIsRunning)
    {
        audioIsRunning = 0;
        Audio_disable();
    }
}

static void GATT_EventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);

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
 * @brief   The purpose of this function is to handle GATT events
 *          that rise from the GATT and were registered in
 *          @ref BLEAppUtil_RegisterGAPEvent
 *
 * @param   event - message event.
 * @param   pMsgData - pointer to message data.
 *
 * @return  none
 */
static void GATT_EventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    gattMsgEvent_t *gattMsg = ( gattMsgEvent_t * )pMsgData;
    switch ( gattMsg->method )
    {
        case ATT_FLOW_CTRL_VIOLATED_EVENT:
        {
        }
        break;

        case ATT_MTU_UPDATED_EVENT:
        {
        }
        break;

        default:
        break;
    }
}

void Application_onSystemInitialized(void)
{
    Audio_init();
}

void Application_onBLEStreamEnabled(void)
{
    bleStreamIsEnabled = 1;
}

void Application_onBLEStreamDisabled(void)
{
    bleStreamIsEnabled = 0;
    Audio_stopStreaming();
}

void Application_DoAudioEvent(void)
{
    static int encBufIndex = 0;
    uint16_t dataLength = Audio_getCompressedBuffer(audio_encBuf[encBufIndex]);

    if (AUDIO_FAIL_NO_DATA == dataLength)
    {
        noDataInstances++;
    }
    else
    {
        if (0 != dataLength)
        {
            encBufIndex++;
            dataInstances++;
        }
    }
    // When we have enough encoded data, send the packet
    if (encBufIndex == NUM_OF_ENC_BUFFERS_PER_BLE_PACKET)
    {
        Audio_sendBufferOverBLE((uint8_t *)audio_encBuf, sizeof(audio_encBuf), 1);
        encBufIndex = 0;
    }
}

/*********************************************************************
 * @fn      Audio_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the specific events handlers of the audio
 *          application module
 *
 * @return  SUCCESS, errorInfo
 */
bStatus_t Audio_start(void)
{
    bStatus_t status = SUCCESS;
    // Register the handlers
    status = BLEAppUtil_registerEventHandler(&audioGATTHandler);
    Application_onSystemInitialized();

    // Return status value
    return(status);
}
