/******************************************************************************

@file  app_pairing.c

@brief This file contains the application pairing role functionality

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
#include <shutdown/power_shutdown.h>
#include <led/led.h>

//*****************************************************************************
//! Prototypes
//*****************************************************************************

void Pairing_passcodeHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
void Pairing_pairStateHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);

// TRUE if connection is secure
extern uint8_t hidDevConnSecure;
// TRUE if pairing in progress
extern uint8_t hidDevPairingStarted;

uint8_t pairing_done;

//*****************************************************************************
//! Globals
//*****************************************************************************

BLEAppUtil_EventHandler_t pairingPasscodeHandler =
{
    .handlerType    = BLEAPPUTIL_PASSCODE_TYPE,
    .pEventHandler  = Pairing_passcodeHandler
};

BLEAppUtil_EventHandler_t PairingPairStateHandler =
{
    .handlerType    = BLEAPPUTIL_PAIR_STATE_TYPE,
    .pEventHandler  = Pairing_pairStateHandler,
    .eventMask      = BLEAPPUTIL_PAIRING_STATE_STARTED |
                      BLEAPPUTIL_PAIRING_STATE_COMPLETE |
                      BLEAPPUTIL_PAIRING_STATE_ENCRYPTED |
                      BLEAPPUTIL_PAIRING_STATE_BOND_SAVED |
                      BLEAPPUTIL_BOND_LOST_EVENT,
};
//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      Pairing_passcodeHandler
 *
 * @brief   The purpose of this function is to handle passcode data
 *          that rise from the GAPBondMgr and were registered
 *          in @ref BLEAppUtil_RegisterGAPEvent
 *
 * @param   pMsgData - pointer to message data.
 *
 * @return  none
 */
void Pairing_passcodeHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    BLEAppUtil_PasscodeData_t *pData = (BLEAppUtil_PasscodeData_t *)pMsgData;

    // Send passcode response
    GAPBondMgr_PasscodeRsp(pData->connHandle, SUCCESS, B_APP_DEFAULT_PASSCODE);
}

/*********************************************************************
 * @fn      Pairing_pairStateHandler
 *
 * @brief   The purpose of this function is to handle pairing state
 *          events that rise from the GAPBondMgr and were registered
 *          in @ref BLEAppUtil_RegisterGAPEvent
 *
 * @param   event - message event.
 * @param   pMsgData - pointer to message data.
 *
 * @return  none
 */

extern uint8_t factory_reset;

void Pairing_pairStateHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    switch(event)
    {
        case BLEAPPUTIL_PAIRING_STATE_STARTED:
        {
            LED_bleStatus(KEY_BLE_PAIRING);
            hidDevPairingStarted = TRUE;
            break;
        }
        case BLEAPPUTIL_PAIRING_STATE_COMPLETE:
        {
            hidDevPairingStarted = FALSE;
            // The pairing is completed, so update the entry in connection list
            // to the ID address instead of the RP address
            linkDBInfo_t linkInfo;
            // Get the list of connected devices
            App_connInfo* connList = Connection_getConnList();
            if (linkDB_GetInfo(((BLEAppUtil_PairStateData_t *)pMsgData)->connHandle, &linkInfo) == SUCCESS)
            {
              // If the peer was using private address, update with ID address
              if ((linkInfo.addrType == ADDRTYPE_PUBLIC_ID ||
                   linkInfo.addrType == ADDRTYPE_RANDOM_ID) &&
                   !osal_isbufset(linkInfo.addrPriv, 0, B_ADDR_LEN))
              {

                // Get the index of connection list by connHandle
                uint8_t connIdx = Connection_getConnIndex(((BLEAppUtil_PairStateData_t *)pMsgData)->connHandle);

                // Verify that there is a match of connection handle
                if (connIdx != LL_INACTIVE_CONNECTIONS)
                {
                  // Update the connection list with the ID address
                  memcpy(connList[connIdx].peerAddress, linkInfo.addr, B_ADDR_LEN);
                }
              }
            }
            /* Google ATV Spec Section 2.7: Pairing successful - 2x blink GREEN */
            LED_bleStatus(KEY_SUCCESS);
            break;
        }

        case BLEAPPUTIL_PAIRING_STATE_ENCRYPTED:
        {
            hidDevConnSecure = TRUE;
            break;
        }

        case BLEAPPUTIL_PAIRING_STATE_BOND_SAVED:
        {            
            /* Pairing and Bonding Successful*/
            hidDevConnSecure = TRUE;
            pairing_done = 1;
            break;
        }

        case BLEAPPUTIL_BOND_LOST_EVENT:
        {
            break;
        }

        default:
        {
            break;
        }
    }

}

/*********************************************************************
 * @fn      Pairing_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the specific events handlers of the pairing
 *          application module
 *
 * @return  SUCCESS, errorInfo
 */
bStatus_t Pairing_start()
{
    bStatus_t status = SUCCESS;

    // Register the handlers
    status = BLEAppUtil_registerEventHandler(&pairingPasscodeHandler);
    if(status != SUCCESS)
    {
        return(status);
    }

    status = BLEAppUtil_registerEventHandler(&PairingPairStateHandler);
    if(status != SUCCESS)
    {
        // Return status value
        return(status);
    }

    // Return status value
    return(status);
}