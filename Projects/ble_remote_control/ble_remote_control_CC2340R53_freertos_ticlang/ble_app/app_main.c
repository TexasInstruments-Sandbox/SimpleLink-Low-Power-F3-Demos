/******************************************************************************

@file  app_main.c

@brief This file contains the application main functionality

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
#include "ti_ble_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <ble_app/app_main.h>
#include <ble_app/Profiles/audio_stream/audio_stream_profile.h>
#include <shutdown/power_shutdown.h>
#include "infrared/IR.h"
#include "ble_hid/hiddev.h"
#include "fmr/fmr.h"
#include "backlight/backlight.h"
#include "nvs_flash/dev_info_flash.h"
#include "nvs_flash/dev_info_flash_test.h"

uint8_t previous_connection_address = 0;

// Parameters that should be given as input to the BLEAppUtil_init function
BLEAppUtil_GeneralParams_t appMainParams =
{
    .taskPriority = 1,
    .taskStackSize = 2048,
    .profileRole = (BLEAppUtil_Profile_Roles_e)(HOST_CONFIG),
    .addressMode = DEFAULT_ADDRESS_MODE,
    .deviceNameAtt = attDeviceName,
    .pDeviceRandomAddress = pRandomAddress,
};

BLEAppUtil_PeriCentParams_t appMainPeriCentParams =
{
 .connParamUpdateDecision = DEFAULT_PARAM_UPDATE_REQ_DECISION,
 .gapBondParams = &gapBondParams
};

/*********************************************************************
 * @fn      criticalErrorHandler
 *
 * @brief   Application task entry point
 *
 * @return  none
 */
void criticalErrorHandler(int32 errorCode , void* pInfo)
{

}

/*********************************************************************
 * @fn      App_StackInitDone
 *
 * @brief   This function will be called when the BLE stack init is
 *          done.
 *          It should call the applications modules start functions.
 *
 * @return  none
 */

uint8_t featSetLegacyChannel[8];

void App_StackInitDoneHandler(gapDeviceInitDoneEvent_t *deviceInitDoneData)
{
    featSetLegacyChannel[0] = 0xFD;
    featSetLegacyChannel[1] = 0x06;
    featSetLegacyChannel[2] = 0x00;
    featSetLegacyChannel[3] = 0x08;
    featSetLegacyChannel[4] = 0x00;
    featSetLegacyChannel[5] = 0x00;
    featSetLegacyChannel[6] = 0x00;
    featSetLegacyChannel[7] = 0x00;
    HCI_EXT_SetLocalSupportedFeaturesCmd(featSetLegacyChannel);

    /* Set TX power from NVS config. Valid range: 0–8 dBm. Default: 4 dBm. */
    int8_t txPowerdBm = 4;
    if (g_rcConfig.deviceConfig.isValid &&
        g_rcConfig.deviceConfig.txPower <= 8)
    {
        txPowerdBm = (int8_t)g_rcConfig.deviceConfig.txPower;
    }
    // Disable During Tests
    //HCI_EXT_SetTxPowerDbmCmd(txPowerdBm, 0);

    /* BLE ADV switch from NVS: 0x00 = Disable advertising, 0xFF = Enable (default).
     * Only controls whether the device starts BLE advertising. All other
     * BLE modules still initialize their event handlers and services. */
    if (!(g_rcConfig.bleConfig.isValid &&
          g_rcConfig.bleConfig.bleSwitch == 0x00))
    {
        Peripheral_start();
    }

    Connection_start();

    Pairing_start();

    OAD_start();

    DevInfo_start();

    AudioStream_start();

    Audio_start();

    HidDev_start();

    IRNec_start();

    FMR_start();

    Backlight_start();
}

/*********************************************************************
 * @fn      appMain
 *
 * @brief   Application main function
 *
 * @return  none
 */

void appMain(void)
{
    PowerLPF3_ResetReason resetReason = PowerLPF3_getResetReason();
    /* If we are waking up from shutdown, we do something extra. */
    if (resetReason == PowerLPF3_RESET_SHUTDOWN_IO)
    {
        // Application code must always disable the IO latches when coming out of shutdown
        PowerLPF3_releaseLatches();
    }

      /* DEVELOPMENT ONLY: Program test NVS data.                                     
       * Step 1: Uncomment the two lines below.                                    
       * Step 2: Flash and run ONCE (data is written to NVS).                         
       * Step 3: Comment them out again and re-flash.                                 
       * Step 4: On next boot, DevInfoFlash_loadAll() reads the test data. */                                  
    erase_nvs_test_data();             
    //program_full_test_data(); 

    /* Load all NVS config into RAM once before the BLE stack starts.
     * Must be here (not in App_StackInitDoneHandler) because address mode
     * must be set before GAP_DeviceInit() is called inside BLEAppUtil_init(). */
    DevInfoFlash_loadAll();

    /* Apply RPA switch from NVS.
     * 0x00 = Enable  → ADDRMODE_RP_WITH_PUBLIC_ID
     * 0xFF = Disable → ADDRMODE_PUBLIC
     * invalid        → leave DEFAULT_ADDRESS_MODE from SysConfig untouched */
    if (g_rcConfig.bleConfig.isValid)
    {
        if (g_rcConfig.bleConfig.rpaSwitch == 0x00)
        {
            appMainParams.addressMode = ADDRMODE_RP_WITH_PUBLIC_ID;
        }
        else if (g_rcConfig.bleConfig.rpaSwitch == 0xFF)
        {
            appMainParams.addressMode = ADDRMODE_PUBLIC;
        }
    }

    // Call the BLEAppUtil module init function
    BLEAppUtil_init(&criticalErrorHandler, &App_StackInitDoneHandler, &appMainParams, &appMainPeriCentParams);
}