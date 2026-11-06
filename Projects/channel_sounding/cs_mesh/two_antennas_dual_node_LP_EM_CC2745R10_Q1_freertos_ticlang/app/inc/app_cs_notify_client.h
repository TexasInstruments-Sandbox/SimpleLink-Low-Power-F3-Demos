/******************************************************************************

@file  app_cs_notify_client.h

@brief This file contains the CS Notify Client definitions and prototypes.

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

 Copyright (c) 2025-2026, Texas Instruments Incorporated
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

#ifndef APP_CS_NOTIFY_CLIENT_H
#define APP_CS_NOTIFY_CLIENT_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include <stdint.h>
#include "ti/ble/app_util/framework/bleapputil_api.h"

/*********************************************************************
 * TYPEDEFS
 */

// Callback type for discovery complete notification
typedef void (*CSNotifyClient_DiscoveryCB_t)(uint16_t connHandle, uint8_t success);

/*********************************************************************
 * FUNCTIONS
 */

/*********************************************************************
 * @fn      CSNotifyClient_registerDiscoveryCB
 *
 * @brief   Register a callback to be called when CS Notify discovery completes.
 *
 * @param   callback - callback function to register
 *
 * @return  None
 */
void CSNotifyClient_registerDiscoveryCB(CSNotifyClient_DiscoveryCB_t callback);

/*********************************************************************
 * @fn      CSNotifyClient_start
 *
 * @brief   Initialize the CS Notify Client and register event handlers.
 *
 * @return  SUCCESS or error code
 */
bStatus_t CSNotifyClient_start(void);

/*********************************************************************
 * @fn      CSNotifyClient_discoverService
 *
 * @brief   Start discovery of the CS Notify service on the key node.
 *          Should be called after connection is established.
 *
 * @param   connHandle - connection handle
 *
 * @return  SUCCESS or error code
 */
bStatus_t CSNotifyClient_discoverService(uint16_t connHandle);

/*********************************************************************
 * @fn      CSNotifyClient_writeCommand
 *
 * @brief   Write to the key node's CS Notify characteristic to trigger restart.
 *
 * @param   connHandle - connection handle
 *
 * @return  SUCCESS or error code
 */
bStatus_t CSNotifyClient_writeCommand(uint16_t connHandle);

#ifdef __cplusplus
}
#endif

#endif /* APP_CS_NOTIFY_CLIENT_H */
