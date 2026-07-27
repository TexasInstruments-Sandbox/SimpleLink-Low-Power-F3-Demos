/******************************************************************************

@file  app_audio_stream.c

@brief Audio Stream profile initialization for basic_ble project.
       Registers the audio stream GATT service and handles CCC updates
       to enable/disable streaming.

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
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <app_main.h>
#include <app/Profiles/audio_stream/audio_stream_profile.h>

//*****************************************************************************
//! Defines
//*****************************************************************************
#define AS_CCC_UPDATE_NOTIFICATION_ENABLED  1

//*****************************************************************************
//! Globals
//*****************************************************************************
extern void Application_onBLEStreamEnabled(void);
extern void Application_onBLEStreamDisabled(void);
extern void Application_DoAudioEvent(void);

//*****************************************************************************
//! LOCAL FUNCTIONS
//*****************************************************************************
static void AS_onCccUpdateCB(uint16 connHandle, uint16 pValue);
static void AS_incomingDataCB(uint16 connHandle, char *pValue, uint16 len);

//*****************************************************************************
//! APPLICATION CALLBACK
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
 * @brief   Callback from Audio_Stream_Profile indicating CCC update.
 *          Enables or disables the audio stream based on client notification
 *          subscription state.
 *
 * @param   connHandle - connection the CCC update was received on
 * @param   pValue     - new CCC value (1 = notifications enabled, 0 = disabled)
 *
 * @return  none
 */
static void AS_onCccUpdateCB(uint16 connHandle, uint16 pValue)
{
    if (pValue == AS_CCC_UPDATE_NOTIFICATION_ENABLED)
    {
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                          "AudioStream status: CCC Update - connectionHandle: "
                          MENU_MODULE_COLOR_YELLOW "%d " MENU_MODULE_COLOR_RESET
                          "Notifications enabled", connHandle);
        Application_onBLEStreamEnabled();
    }
    else
    {
        MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                          "AudioStream status: CCC Update - connectionHandle: "
                          MENU_MODULE_COLOR_YELLOW "%d " MENU_MODULE_COLOR_RESET
                          "Notifications disabled", connHandle);
        Application_onBLEStreamDisabled();
    }
}

/*********************************************************************
 * @fn      AS_incomingDataCB
 *
 * @brief   Callback from Audio_Stream_Profile indicating incoming data
 *          from the receiver (e.g. control commands).
 *
 * @param   connHandle - connection handle
 * @param   pValue     - pointer to received data
 * @param   len        - length of received data
 *
 * @return  none
 */
static void AS_incomingDataCB(uint16 connHandle, char *pValue, uint16 len)
{
    // Handle incoming control data from the receiver if needed
    (void)connHandle;
    (void)pValue;
    (void)len;
}

/*********************************************************************
 * @fn      AudioStream_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the Audio Stream profile.
 *
 * @return  SUCCESS or stack call status
 */
bStatus_t AudioStream_start(void)
{
    bStatus_t status = SUCCESS;

    status = ASP_start(&as_profileCB);
    if (status != SUCCESS)
    {
        return status;
    }

    return(SUCCESS);
}
