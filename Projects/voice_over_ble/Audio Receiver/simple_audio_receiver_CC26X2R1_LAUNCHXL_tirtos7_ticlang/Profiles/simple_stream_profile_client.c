/**********************************************************************************************
 * Filename:       simple_stream_profile_client.c
 *
 * Description:    This file contains the implementation of the service.
 *
 * Copyright (c) 2018-2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *************************************************************************************************/


/*********************************************************************
 * INCLUDES
 */
#include <string.h>

#include <icall.h>
#include <simple_stream_profile_client.h>
#include "icall_ble_api.h"

/*********************************************************************
 * DEFINES
 */

// Service not discovered
#define SSC_SERVICE_NOT_DISCOVERED    (0xFF)

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
* GLOBAL VARIABLES
*/

// SimpleStreamClient Service UUID
CONST uint8_t SimpleStreamServerUUID[ATT_UUID_SIZE] =
{
 TI_BASE_UUID_128(SIMPLESTREAMSERVER_SERV_UUID)
};

// DataIn UUID
CONST uint8_t SimpleStreamServer_DataInUUID[ATT_UUID_SIZE] =
{

  TI_BASE_UUID_128(SIMPLESTREAMSERVER_DATAIN_UUID)
};
// DataOut UUID
CONST uint8_t SimpleStreamServer_DataOutUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(SIMPLESTREAMSERVER_DATAOUT_UUID)
};

/*********************************************************************
 * LOCAL VARIABLES
 */

// Data queue to hold the outgoing data
static List_List streamOutQueue[MAX_NUM_BLE_CONNS];

static uint16_t heapHeadroom = 0;

/*********************************************************************
* Service Discovery Table
*/
simpleStreamService_t streamServiceHandle[MAX_NUM_BLE_CONNS];

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t SimpleStreamClient_transmitNode( SimpleStreamNode_t *node );
static bStatus_t SimpleStreamClient_queueData( SimpleStreamNode_t *node );
static void      SimpleStreamClient_clearQueue(uint8_t connIndex);
/*********************************************************************
 * PROFILE CALLBACKS
 */

/*********************************************************************
* PUBLIC FUNCTIONS
*/

/*********************************************************************
 * EXTERN FUNCTIONS
 */
extern uint8_t SimpleSerialSocketClient_getConnIndex(uint16_t connHandle);

/*********************************************************************
 * @fn      SimpleStreamClient_queueData
 *
 * @brief   Adds a new SimpleStreamNode_t node to the data queue
 *
 * @param   node  - data node to add to the queue
 *
 * @return  SUCCESS, FAILURE or INVALIDPARAMETER
 */
static bStatus_t SimpleStreamClient_queueData( SimpleStreamNode_t *node )
{
    bStatus_t ret = SUCCESS;
    uint8_t connIndex = SimpleSerialSocketClient_getConnIndex(node->connHandle);

    if (node != NULL)
    {
        List_put(&streamOutQueue[connIndex], (List_Elem *) node);
    }
    else
    {
        ret = INVALIDPARAMETER;
    }

    return ret;
}

/*********************************************************************
 * @fn          SimpleStreamClient_transmitNode
 *
 * @brief       Transmits as much as possible of a SimpleStreamNode_t node
 *              over BLE notifications.
 *
 * @param       node - The node to send
 *
 * @return      SUCCESS, FAILURE, INVALIDPARAMETER, MSG_BUFFER_NOT_AVAIL,
 *              bleNotCOnnected, bleMemAllocError, blePending, bleInvaludMtuSize or
 *              bleTimeout
 */
static bStatus_t SimpleStreamClient_transmitNode( SimpleStreamNode_t* node)
{
    bStatus_t ret = SUCCESS;
    linkDBInfo_t connInfo;
    attWriteReq_t req;

    // Find out what the maximum MTU size is
    ret = linkDB_GetInfo(node->connHandle, &connInfo);

    // Queue up as many notification slots as possible
    if ( (ret == SUCCESS) && (node != NULL) ) {

        // Determine allocation size
        uint16_t allocLen = (node->len - node->offset);
        if ( allocLen > (connInfo.MTU - ATT_WRITE_REQ_HDR_SIZE) )
        {
            allocLen = connInfo.MTU - ATT_WRITE_REQ_HDR_SIZE;
        }

        req.pValue = (uint8 *)GATT_bm_alloc( node->connHandle, ATT_WRITE_CMD,
                                              allocLen, &req.len );

        // If allocation was successful, copy out data out of the buffer and send it
        if (NULL != req.pValue) {

            uint8_t connIndex = SimpleSerialSocketClient_getConnIndex(node->connHandle);

            req.handle = streamServiceHandle[connIndex].chars[0].handle;
            memcpy(req.pValue, (void *) ((uint8_t *) node->payload + node->offset), req.len);
            req.cmd = TRUE;
            req.sig = FALSE;

            ret = GATT_WriteNoRsp( node->connHandle, &req );

            // If unable to send the message, free allocated buffers and return
            if ( ret != SUCCESS )
            {
                GATT_bm_free( (gattMsg_t *)&req, ATT_WRITE_CMD );
            }
            else
            {
                // Increment node data offset
                node->offset += req.len;
            }
        }
        else
        {
            // Unable to allocate space for a notification, return failure
            ret = bleMemAllocError;
        }
    }

    return ret;
}

/*********************************************************************
 * @fn      SimpleStreamClient_init
 *
 * @brief   Initialize the streamServiceHandle table
 *
 * @return  SUCCESS
 */
bStatus_t SimpleStreamClient_init( void )
{
    bStatus_t ret = SUCCESS;

    int connIndex;
    for(connIndex = 0; connIndex < MAX_NUM_BLE_CONNS; connIndex++)
    {

        // Set Simple Stream Server service UUID
        streamServiceHandle[connIndex].uuid.len = ATT_UUID_SIZE;
        streamServiceHandle[connIndex].uuid.uuid = SimpleStreamServerUUID;
        streamServiceHandle[connIndex].startHandle = GATT_INVALID_HANDLE;
        streamServiceHandle[connIndex].endHandle = GATT_INVALID_HANDLE;
        streamServiceHandle[connIndex].numChars = 2;

        // Set DataIn UUID
        streamServiceHandle[connIndex].chars[0].uuid.len = ATT_UUID_SIZE;
        streamServiceHandle[connIndex].chars[0].uuid.uuid = SimpleStreamServer_DataInUUID;
        streamServiceHandle[connIndex].chars[0].handle = GATT_INVALID_HANDLE;
        streamServiceHandle[connIndex].chars[0].cccdHandle = GATT_INVALID_HANDLE;

        // Set DataOut UUID
        streamServiceHandle[connIndex].chars[1].uuid.len = ATT_UUID_SIZE;
        streamServiceHandle[connIndex].chars[1].uuid.uuid = SimpleStreamServer_DataOutUUID;
        streamServiceHandle[connIndex].chars[1].handle = GATT_INVALID_HANDLE;
        streamServiceHandle[connIndex].chars[1].cccdHandle = GATT_INVALID_HANDLE;

        // Set-up the queue
        SimpleStreamClient_clearQueue(connIndex);
    }

    return ret;
}

/*********************************************************************
 * @fn      SimpleStreamClient_clearQueue
 *
 * @brief   Clears and free the allocated outgoing stream queue
 *
 * @param   None
 *
 * @return  None
 */
void SimpleStreamClient_clearQueue(uint8_t connIndex)
{
    // Pop and free the whole queue
    while(!List_empty(&streamOutQueue[connIndex]))
    {
        SimpleStreamNode_t *node = (SimpleStreamNode_t *) List_get(&streamOutQueue[connIndex]);
        ICall_free(node);
    }
}

/*********************************************************************
 * @fn      SimpleStreamClient_processStream
 *
 * @brief   Sends out as much as possible from the outgoing stream
 *          queue using BLE notifications
 *
 * @param   connHandle  - connection message was received on
 * @param   *pValue     - pointer to data buffer
 * @param   len         - size of the data buffer
 *
 * @return  SUCCESS, FAILURE, INVALIDPARAMETER, MSG_BUFFER_NOT_AVAIL,
 *          bleNotCOnnected, bleMemAllocError, blePending, bleInvaludMtuSize or
 *          bleTimeout
 */
bStatus_t SimpleStreamClient_processStream(uint16_t connHandle)
{
    bStatus_t ret = SUCCESS;
    uint8_t connIndex = SimpleSerialSocketClient_getConnIndex(connHandle);

    // Send data starting from the list head
    SimpleStreamNode_t *node = (SimpleStreamNode_t *) List_get(&streamOutQueue[connIndex]);

    while ((ret == SUCCESS) && (node != NULL))
    {
        ret = SimpleStreamClient_transmitNode(node);

        // Check that we really did send all data before freeing the node
        if ((node->len - node->offset) == 0)
        {
            ICall_free(node);
            // Move to next queue entry
            node = (SimpleStreamNode_t *) List_get(&streamOutQueue[connIndex]);
        }

        if (ret != SUCCESS)
        {
            // We could not send all the data contained in the node, add it back to the queue
            List_putHead(&streamOutQueue[connIndex], (List_Elem *) node);
        }
    }

    return ret;
}

/*********************************************************************
 * @fn      SimpleStreamClient_disconnectStream
 *
 * @brief   Disconnect the steam.
 *          Clear and free up the existing outgoing stream queue.
 *
 * @param   none
 *
 * @return  none
 */
void SimpleStreamClient_disconnectStream(uint16_t connHandle)
{
    uint8_t connIndex = SimpleSerialSocketClient_getConnIndex(connHandle);

    // Clear the outgoing stream queue
    SimpleStreamClient_clearQueue(connIndex);
}

/*********************************************************************
 * @fn      SimpleStreamClient_setHeadroomLimit
 *
 * @brief   Sets the limit on how much heap that needs to be available
 *          following a memory allocation.
 *
 * @param   minHeapHeadRoom - Smallest amount of free heap following
 *          an memory allocation.
 *
 * @return  none
 */
void SimpleStreamClient_setHeadroomLimit(uint16_t minHeapHeadroom)
{
    // Store the minimal heap headroom limit
    heapHeadroom = minHeapHeadroom;
}

/*********************************************************************
 * @fn      SimpleStreamClient_allocateWithHeadroom
 *
 * @brief   Checks if there will be enough free heap left following
 *          a memory allocation. If there is enough heap, it will allocate
 *          the memory.
 *
 * @param   allocSize - number of bytes to be allocated
 *
 * @return  none
 */
void* SimpleStreamClient_allocateWithHeadroom(uint16_t allocSize)
{
    void *allocatedBuffer = NULL;
    ICall_heapStats_t  stats;
    ICall_CSState key;

    // Perform this inside a critical section
    key = ICall_enterCriticalSection();

    // Get the current free heap
    ICall_getHeapStats(&stats);

    if (((int16_t) allocSize) < ((int32_t)(stats.totalFreeSize - heapHeadroom)))
    {
        allocatedBuffer = ICall_malloc(allocSize);
    }

    // Leave the critical section
    ICall_leaveCriticalSection(key);

    return allocatedBuffer;
}

/*********************************************************************
 * @fn      SimpleStreamClient_sendData
 *
 * @brief   Put the data into the outgoing stream queue and sends as
 *          much as possible using BLE notifications.
 *
 * @param   connHandle  - connection message was received on
 * @param   *pValue     - pointer to data buffer
 * @param   len         - size of the data buffer
 *
 * @return  SUCCESS, FAILURE, INVALIDPARAMETER, MSG_BUFFER_NOT_AVAIL,
 *          bleNotCOnnected, bleMemAllocError, blePending, bleInvaludMtuSize or
 *          bleTimeout
 */
bStatus_t SimpleStreamClient_sendData(uint16_t connHandle, void *data, uint16_t len)
{
    bStatus_t ret = bleMemAllocError;
    SimpleStreamNode_t* newNode;

    uint8_t connIndex = SimpleSerialSocketClient_getConnIndex(connHandle);

    // Reject if service is not yet discovered
    if (NULL != streamServiceHandle[connIndex].chars[0].handle)
    {
        // Store the data into the queue
        newNode = (SimpleStreamNode_t*) SimpleStreamClient_allocateWithHeadroom(sizeof(SimpleStreamNode_t) + len);
        if (newNode != NULL)
        {
            newNode->connHandle = connHandle;
            newNode->offset     = 0;
            newNode->len        = len;
            memcpy(newNode->payload, data, len);

            // Add the data to the stream queue
            ret = SimpleStreamClient_queueData(newNode);

            if (ret == SUCCESS)
            {
                ret = SimpleStreamClient_processStream(connHandle);
            }
            else if (ret == FAILURE)
            {
                ICall_free(newNode);
            }
        }
    }
    else
    {
        ret = SSC_SERVICE_NOT_DISCOVERED;
    }

    return ret;
}

/*********************************************************************
 * @fn      SimpleStreamClient_enableNotifications
 *
 * @brief   Enable notifications for given connection and CCCD handle
 *
 * @return  FAILURE or GATT_WriteNoRsp return value
 */
bStatus_t SimpleStreamClient_enableNotifications(uint16_t connHandle)
{
    uint8_t connIndex = SimpleSerialSocketClient_getConnIndex(connHandle);

    // Process message.
    attWriteReq_t req;
    bStatus_t retVal = FAILURE;
    uint8 configData[2] = {0x01,0x00};
    req.pValue = GATT_bm_alloc(connHandle, ATT_WRITE_REQ, 2, NULL);

    // Enable notify for outgoing data
    if ( (req.pValue != NULL) && streamServiceHandle[connIndex].chars[1].cccdHandle)
    {
        req.handle = streamServiceHandle[connIndex].chars[1].cccdHandle;
        req.len = 2;
        memcpy(req.pValue, configData, 2);
        req.cmd = TRUE;
        req.sig = FALSE;
        retVal = GATT_WriteNoRsp(connHandle, &req);
    }

    return retVal;
}
