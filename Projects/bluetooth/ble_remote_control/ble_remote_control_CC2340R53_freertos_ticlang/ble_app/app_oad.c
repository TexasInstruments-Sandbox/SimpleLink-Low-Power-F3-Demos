/******************************************************************************

@file  app_dev_info.c

@brief This file contains the device info application functionality

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
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <ble_app/app_main.h>
#include <ble_app/Profiles/oad/oad_profile.h>
#include "ti/ble/app_util/sw_update/sw_update.h"
#include <led/led.h>
#include <ble_app/Profiles/batmon/battservice.h>
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"

//*****************************************************************************
//! Defines
//*****************************************************************************

/* Google ATV PRD v1.5 Section 2.3: Minimal Battery level to support firmware upgrade */
#define OAD_MIN_BATTERY_LEVEL_PERCENT   30

/* Fast connection parameters for OAD to meet <30 second requirement */
/* Connection interval: 7.5ms-15ms (6*1.25ms to 12*1.25ms) */
#define OAD_CONN_INTERVAL_MIN           6    // 7.5ms
#define OAD_CONN_INTERVAL_MAX           6   // 15ms
#define OAD_CONN_LATENCY                0    // No latency for maximum throughput
#define OAD_CONN_TIMEOUT                600  // 6 seconds

//*****************************************************************************
//! Globals
//*****************************************************************************

//*****************************************************************************
//! Functions
//*****************************************************************************

OADProfile_AppCommand_e App_OADCallback(OADProfile_App_Msg_e msg)
{
    OADProfile_AppCommand_e cmd = OAD_PROFILE_PROCEED;
    switch(msg)
    {
        case OAD_PROFILE_MSG_REVOKE_IMG_HDR:
        {
            MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0, "App_OADCallback: Revoke image header");
            cmd = OAD_PROFILE_PROCEED;
            break;
        }
        case OAD_PROFILE_MSG_NEW_IMG_IDENDIFY:
        {
            MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0, "App_OADCallback: New image identify");
            cmd = OAD_PROFILE_PROCEED;
            break;
        }
        case OAD_PROFILE_MSG_START_DOWNLOAD:
        {
            /* Google ATV PRD v1.5 Section 2.3: Check minimal battery level (30%) before OTA */
            uint8_t batteryLevel = 0;
            bStatus_t status = Batt_GetParameter(BATT_PARAM_LEVEL, &batteryLevel);

            if (status != SUCCESS)
            {
                MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0, "App_OADCallback: Battery read failed");
                /* Assume low battery if read fails - safety first */
                LED_bleStatus(KEY_FAIL);  // 4x blink RED for error
                cmd = OAD_PROFILE_CANCEL;
                break;
            }

            if (batteryLevel < OAD_MIN_BATTERY_LEVEL_PERCENT)
            {
                MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0, "App_OADCallback: Battery too low (%d%% < 30%%)", batteryLevel);
                /* Battery too low - reject OTA update */
                LED_bleStatus(KEY_FAIL);  // 4x blink RED for error
                cmd = OAD_PROFILE_CANCEL;
                break;
            }

            /* Battery level sufficient - proceed with OTA */
            MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0, "App_OADCallback: Download new image (Battery: %d%%)", batteryLevel);

            /* Request fast connection parameters (7.5-15ms) for OAD to complete in <30 seconds */
            /* Get connection handle - typically index 0 for single-connection remote */
            uint16_t connHandle = Connection_getConnhandle(0);
            if (connHandle != MAX_NUM_BLE_CONNS)
            {
                /* Send L2CAP connection parameter update request */
                gapUpdateLinkParamReq_t params = {
                    .connectionHandle = connHandle,
                    .intervalMin = OAD_CONN_INTERVAL_MIN,
                    .intervalMax = OAD_CONN_INTERVAL_MAX,
                    .connLatency = OAD_CONN_LATENCY,
                    .connTimeout = OAD_CONN_TIMEOUT
                };

                status = GAP_UpdateLinkParamReq(&params);
                if (status == SUCCESS)
                {
                    MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0, "App_OADCallback: Requested fast conn params (7.5-15ms)");
                }
                else
                {
                    MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0, "App_OADCallback: Param request failed, continuing with current params");
                }
            }

            /* Google ATV Spec Section 2.7: OTA in progress - Static RED LED */
            LED_bleStatus(KEY_BLE_OAD_START);

            cmd = OAD_PROFILE_PROCEED;
            break;
        }
        case OAD_PROFILE_MSG_FINISH_DOWNLOAD:
        {
            MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0, "App_OADCallback: Download complete");

            /* Google ATV Spec Section 2.7: OTA complete - Turn off RED, show GREEN success */
            LED_bleStatus(KEY_BLE_OAD_FINISH);
            LED_bleStatus(KEY_SUCCESS);  // 2x blink GREEN for successful OTA

            cmd = OAD_PROFILE_PROCEED;
            break;
        }
        case OAD_PROFILE_MSG_RESET_REQ:
        {
            MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0, "App_OADCallback: Reset device");
            cmd = OAD_PROFILE_PROCEED;
            break;
        }
    }
    return (cmd);
}

/*********************************************************************
 * @fn      OAD_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the OAD profile and service.
 *
 * @return  SUCCESS or stack call status
 */
bStatus_t OAD_start(void)
{
  bStatus_t status = SUCCESS;

  OADProfile_start(&App_OADCallback);

  //APP_HDR_ADDR is the place in the flash memory of APP header, and it is imported from the predefined symbols
  //SwUpdate_GetSWVersion function extract image version struct from given address
  struct image_version * img_ver = (struct image_version *)SwUpdate_GetSWVersion(APP_HDR_ADDR);
  MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0, "App version: %d.%d.%d.%d",
                    img_ver->iv_major,
                    img_ver->iv_minor,
                    img_ver->iv_revision,
                    img_ver->iv_build_num);
  return ( status );
}
