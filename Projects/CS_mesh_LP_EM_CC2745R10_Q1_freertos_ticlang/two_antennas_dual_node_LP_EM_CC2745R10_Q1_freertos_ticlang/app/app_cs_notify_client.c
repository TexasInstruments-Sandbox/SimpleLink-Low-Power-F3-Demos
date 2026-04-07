/******************************************************************************

@file  app_cs_notify_client.c

@brief This file contains the CS Notify client functionality for the car node.
       It discovers and writes to the CS Notify characteristic on the key node
       to signal that channel sounding has completed successfully.

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

//*****************************************************************************
//! Includes
//*****************************************************************************
#include <string.h>
#include <stdio.h>
#include "ti/ble/stack_util/bcomdef.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/framework/bleapputil_timers.h"
#include "ti/ble/host/gatt/gatt.h"
#include "ti/ble/host/gatt/gatt_uuid.h"
#include "app_cs_notify_client.h"
#include "app_cs_notify_service.h"

#include <ti/drivers/UART2.h>

#include DeviceFamily_constructPath(driverlib/hapi.h)
#include DeviceFamily_constructPath(inc/hw_pmctl.h)
#include DeviceFamily_constructPath(inc/hw_types.h)

//*****************************************************************************
//! Defines
//*****************************************************************************

// Index positions in read by type response
#define CS_NOTIFY_CHAR_VALUE_HANDLE_INDEX  3
#define CS_NOTIFY_CHAR_UUID_HANDLE_INDEX   5

// 16-bit high and low index in the 128-bit custom UUID
#define CS_NOTIFY_HIGH_UUID_INDEX          0x0C
#define CS_NOTIFY_LOW_UUID_INDEX           0x0D

//*****************************************************************************
//! Local Variables
//*****************************************************************************

// Handle of the CS Notify characteristic on the key node
static uint16_t gCsNotifyCharHandle = 0;

// Service start and end handles
static uint16_t gCsNotifyStartHandle = 0;
static uint16_t gCsNotifyEndHandle = 0;

// Flag to track if discovery is complete
static uint8_t gCsNotifyDiscoveryComplete = FALSE;

// Flag to track if we're actively discovering
static uint8_t gCsNotifyDiscoveryInProgress = FALSE;

// Connection handle for retry
static uint16_t gCsNotifyConnHandle = 0;

// External UART handle
extern UART2_Handle uart;

// Discovery complete callback
static CSNotifyClient_DiscoveryCB_t gDiscoveryCallback = NULL;

// Flag to track if we're waiting for a write response to our characteristic
static uint8_t gCsNotifyWritePending = FALSE;

// Connection handle for the pending write (to verify response)
static uint16_t gCsNotifyWriteConnHandle = 0;

//*****************************************************************************
//! Local Function Prototypes
//*****************************************************************************
static void CSNotifyClient_GATTEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
static bStatus_t CSNotifyClient_startDiscoveryInternal(uint16_t connHandle);

//*****************************************************************************
//! Event Handler
//*****************************************************************************

// GATT event handler for CS Notify Client
static BLEAppUtil_EventHandler_t csNotifyClientGATTHandler =
{
    .handlerType    = BLEAPPUTIL_GATT_TYPE,
    .pEventHandler  = CSNotifyClient_GATTEventHandler,
    .eventMask      = BLEAPPUTIL_ATT_FIND_BY_TYPE_VALUE_RSP |
                      BLEAPPUTIL_ATT_READ_BY_TYPE_RSP |
                      BLEAPPUTIL_ATT_WRITE_RSP |
                      BLEAPPUTIL_ATT_ERROR_RSP
};

//*****************************************************************************
//! Public Functions
//*****************************************************************************

/*********************************************************************
 * @fn      CSNotifyClient_start
 *
 * @brief   Initialize the CS Notify Client and register event handlers.
 *
 * @return  SUCCESS or error code
 */
bStatus_t CSNotifyClient_start(void)
{
    bStatus_t status = SUCCESS;

    // Register the GATT event handler
    status = BLEAppUtil_registerEventHandler(&csNotifyClientGATTHandler);

    return status;
}

/*********************************************************************
 * @fn      CSNotifyClient_registerDiscoveryCB
 *
 * @brief   Register a callback to be called when CS Notify discovery completes.
 *
 * @param   callback - callback function to register
 *
 * @return  None
 */
void CSNotifyClient_registerDiscoveryCB(CSNotifyClient_DiscoveryCB_t callback)
{
    gDiscoveryCallback = callback;
}

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
/*********************************************************************
 * @fn      CSNotifyClient_startDiscoveryInternal
 *
 * @brief   Internal function to start the discovery. Called directly
 *          or via retry timer.
 */
static bStatus_t CSNotifyClient_startDiscoveryInternal(uint16_t connHandle)
{
    bStatus_t status = SUCCESS;

    UART2_write(uart, "Discovering services of key node\r\n", 34, NULL);

    // Discover CS Notify service by UUID
    uint8_t csNotifyUUID[ATT_UUID_SIZE] = { TI_BASE_UUID_128(CS_NOTIFY_SERVICE_UUID_SHORT) };
    status = GATT_DiscPrimaryServiceByUUID(connHandle, csNotifyUUID,
                                           ATT_UUID_SIZE, BLEAppUtil_getSelfEntity());

    if (status == SUCCESS)
    {
        gCsNotifyDiscoveryInProgress = TRUE;
    }
    else
    {
        char uartBuffer[34] = {"\0"};
        sprintf(uartBuffer, "Failed to discover services: %d\r\n", status);
        UART2_write(uart, uartBuffer, sizeof(uartBuffer) - 1, NULL);
        
        gCsNotifyDiscoveryInProgress = FALSE;
    }

    return status;
}

bStatus_t CSNotifyClient_discoverService(uint16_t connHandle)
{
    if(gCsNotifyDiscoveryInProgress == TRUE)
    {
        return FAILURE;
    }
    
    // Reset discovery state
    gCsNotifyCharHandle = 0;
    gCsNotifyStartHandle = 0;
    gCsNotifyEndHandle = 0;
    gCsNotifyDiscoveryComplete = FALSE;
    gCsNotifyDiscoveryInProgress = FALSE;
    gCsNotifyConnHandle = connHandle;

    return CSNotifyClient_startDiscoveryInternal(connHandle);
}

/*********************************************************************
 * @fn      CSNotifyClient_writeCommand
 *
 * @brief   Write to the key node's CS Notify characteristic to trigger restart.
 *
 * @param   connHandle - connection handle
 *
 * @return  SUCCESS or error code
 */
bStatus_t CSNotifyClient_writeCommand(uint16_t connHandle)
{
    bStatus_t status = SUCCESS;

    if (gCsNotifyCharHandle == 0)
    {
        return FAILURE;
    }

    // Prepare write request (with response)
    attWriteReq_t req;
    req.pValue = GATT_bm_alloc(connHandle, ATT_WRITE_REQ, 1, NULL);

    if (req.pValue != NULL)
    {
        req.handle = gCsNotifyCharHandle;
        req.len = 1;
        req.pValue[0] = CS_NOTIFY_CMD_RESTART;
        req.sig = FALSE;
        req.cmd = FALSE;  // Use write request (with response)

        status = GATT_WriteCharValue(connHandle, &req, BLEAppUtil_getSelfEntity());

        if (status != SUCCESS)
        {
            GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
        }
        else
        {
            gCsNotifyWritePending = TRUE;
            gCsNotifyWriteConnHandle = connHandle;
        }
    }
    else
    {
        status = bleMemAllocError;
    }

    return status;
}

//*****************************************************************************
//! Local Functions
//*****************************************************************************

/*********************************************************************
 * @fn      CSNotifyClient_GATTEventHandler
 *
 * @brief   Handle GATT events for CS Notify Client discovery and writes.
 *
 * @param   event - message event
 * @param   pMsgData - pointer to message data
 *
 * @return  none
 */
static void CSNotifyClient_GATTEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    gattMsgEvent_t *gattMsg = (gattMsgEvent_t *)pMsgData;

    // Handle write responses only if we're waiting for our characteristic write
    // and the response is on the same connection we wrote to
    if (event == BLEAPPUTIL_ATT_WRITE_RSP && gCsNotifyWritePending &&
        gattMsg->connHandle == gCsNotifyWriteConnHandle)
    {
        gCsNotifyWritePending = FALSE;
        gCsNotifyWriteConnHandle = 0;
        UART2_write(uart, "Key Node write response recieved, restarting!\r\n", 48, NULL);
        // Key node has been notified, now restart to move to mesh image

        HapiResetDevice();
        return;
    }

    if (event == BLEAPPUTIL_ATT_ERROR_RSP && gCsNotifyWritePending &&
        gattMsg->connHandle == gCsNotifyWriteConnHandle)
    {
        // Error response for our write
        gCsNotifyWritePending = FALSE;
        gCsNotifyWriteConnHandle = 0;
        HapiResetDevice();
        return;
    }

    // Only process discovery events if we're actively discovering
    if (!gCsNotifyDiscoveryInProgress)
    {
        return;
    }

    switch (event)
    {
        case BLEAPPUTIL_ATT_FIND_BY_TYPE_VALUE_RSP:
        {
            if (gattMsg->hdr.status == SUCCESS)
            {
                // Found the CS Notify service, save handles
                gCsNotifyStartHandle = ATT_ATTR_HANDLE(gattMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
                gCsNotifyEndHandle = ATT_GRP_END_HANDLE(gattMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
            }
            else if (gattMsg->hdr.status == bleProcedureComplete)
            {
                if (gCsNotifyStartHandle != 0 && gCsNotifyEndHandle != 0)
                {
                    // Discover characteristics within the service
                    GATT_DiscAllChars(gattMsg->connHandle, gCsNotifyStartHandle,
                                      gCsNotifyEndHandle, BLEAppUtil_getSelfEntity());
                }
                else
                {
                    gCsNotifyDiscoveryComplete = TRUE;
                    gCsNotifyDiscoveryInProgress = FALSE;
                }
            }
            break;
        }

        case BLEAPPUTIL_ATT_READ_BY_TYPE_RSP:
        {
            attReadByTypeRsp_t att = gattMsg->msg.readByTypeRsp;

            if ((gattMsg->hdr.status == SUCCESS) && (att.numPairs > 0) && (att.len == 21))
            {
                // 128-bit UUID response: len=21 (2+1+2+16)
                for (uint8_t i = 0; i < att.numPairs; i++)
                {
                    uint8_t index = i * att.len;

                    // Get ATT handle (characteristic value handle)
                    uint16_t currAttHandle = BUILD_UINT16(
                        att.pDataList[index + CS_NOTIFY_CHAR_VALUE_HANDLE_INDEX],
                        att.pDataList[index + CS_NOTIFY_CHAR_VALUE_HANDLE_INDEX + 1]);

                    // Extract the 16-bit portion from TI base UUID
                    uint8_t customUUID[ATT_UUID_SIZE];
                    memcpy(customUUID, &att.pDataList[index + CS_NOTIFY_CHAR_UUID_HANDLE_INDEX], ATT_UUID_SIZE);

                    uint16_t charUUID = BUILD_UINT16(customUUID[CS_NOTIFY_HIGH_UUID_INDEX],
                                                     customUUID[CS_NOTIFY_LOW_UUID_INDEX]);

                    // Check if this is the CS Notify characteristic UUID
                    if (charUUID == CS_NOTIFY_CHAR_UUID_SHORT)
                    {
                        gCsNotifyCharHandle = currAttHandle;
                    }
                }
            }
            else if (gattMsg->hdr.status == bleProcedureComplete)
            {
                gCsNotifyDiscoveryComplete = TRUE;
                gCsNotifyDiscoveryInProgress = FALSE;
                
                if (gCsNotifyCharHandle != 0)
                {
                    // Notify callback of successful discovery
                    if (gDiscoveryCallback != NULL)
                    {
                        gDiscoveryCallback(gattMsg->connHandle, TRUE);
                    }
                }
                else
                {
                    // Notify callback of failed discovery
                    if (gDiscoveryCallback != NULL)
                    {
                        gDiscoveryCallback(gattMsg->connHandle, FALSE);
                    }
                }
            }
            break;
        }

        case BLEAPPUTIL_ATT_ERROR_RSP:
        {
            UART2_write(uart, "CS Notify GATT error!\r\n", 24, NULL);
            break;
        }

        default:
            break;
    }
}
