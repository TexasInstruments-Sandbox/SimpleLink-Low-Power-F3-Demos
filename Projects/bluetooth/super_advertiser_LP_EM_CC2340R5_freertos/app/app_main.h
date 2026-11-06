/******************************************************************************

@file  app_main.h

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

#ifndef APP_MAIN_H_
#define APP_MAIN_H_

//*****************************************************************************
//! Includes
//*****************************************************************************
#include <ti/ble/app_util/framework/bleapputil_api.h>

//*****************************************************************************
//! Defines
//*****************************************************************************

//*****************************************************************************
//! Typedefs
//*****************************************************************************
typedef enum
{
    APP_MENU_GENERAL_STATUS_LINE,
    APP_MENU_DEVICE_ADDRESS,
    APP_MENU_DEVICE_RP_ADDRESS,
    APP_MENU_ADV_EVENT,
    APP_MENU_SCAN_EVENT,
    APP_MENU_NUM_CONNS,
    APP_MENU_CONN_EVENT,
    APP_MENU_PAIRING_EVENT,
    APP_MENU_PROFILE_STATUS_LINE,
    APP_MENU_PROFILE_STATUS_LINE1,
    APP_MENU_PROFILE_STATUS_LINE2,
    APP_MENU_PROFILE_STATUS_LINE3,
    APP_MENU_PROFILE_STATUS_LINE4
} AppMenu_rows;

PACKED_ALIGNED_TYPEDEF_STRUCT
{
  /// Type of TargetA address in the directed advertising PDU
  uint8_t  addressType;
  /// TargetA address
  BLEAppUtil_BDaddr  address;
} App_scanResults;

// Connected device information
PACKED_ALIGNED_TYPEDEF_STRUCT
{
  uint16_t  connHandle;             // Connection Handle
  BLEAppUtil_BDaddr peerAddress;    // The address of the peer device
  uint16_t   notifyCbCnt;           // Notify callback counter
} App_connInfo;

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      Broadcaster_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the specific events handlers of the broadcaster
 *          application module
 *
 * @return  SUCCESS, errorInfo
 */
bStatus_t Broadcaster_start(void);

/*********************************************************************
 * @fn      Menu_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize the
 *          menu
 *
 * @return  SUCCESS, errorInfo
 */
bStatus_t Menu_start(void);

#endif /* APP_MAIN_H_ */
