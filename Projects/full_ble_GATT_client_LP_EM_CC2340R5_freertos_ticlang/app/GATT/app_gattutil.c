/******************************************************************************

@file  app_gatt.c

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


//*****************************************************************************
//! Includes
//*****************************************************************************

#include <app_main.h>
#include "ti_ble_config.h"

#include "app_gattutil.h"

//*****************************************************************************
//! Defines
//*****************************************************************************

#define INITIAL_QUEUE_SIZE 5

//*****************************************************************************
//! Prototypes
//*****************************************************************************

static bStatus_t GATTUtil_expandQueueSize();
static bStatus_t GATTUtil_initializeQueue();

extern void* memcpy (void* destination, const void* source, size_t num);

//*****************************************************************************
//! Globals
//*****************************************************************************

// FIFO Queue
static GATTUtil_Queue_Elem* GATTUtil_Queue = NULL;
static uint16_t maxGATTQueueSize;

static int16_t queueFront = -1;
static int16_t queueBack = -1;

//*****************************************************************************
//! Functions
//*****************************************************************************

//! GATT Functions

/*********************************************************************
 * @fn      GATTUtil_GetPrimaryServices
 *
 * @brief   This function add a element to the GATT queue with the
 *          type PRIMARY_SERVICE_DISCOVERY and calls the SDK function
 *          GATTUtil_DiscAllPrimaryServices to get the services of a GATT table.
 * 
 * @param   connectionHandle - the handle for the connection
 * @param   callback - a callback that will be called once the ATT 
 *                     requests for the primary services are completed
 * @param   cbContext - pointer that will be passed to the callback,
 *                      used to preserve context between functions
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_GetPrimaryServices(
    uint16_t connectionHandle,
    void (*callback)(
        GATTUtil_service* GATTServices, 
        uint16_t GATTServicesLength,
        void* cbContext),
    void* cbContext)
{
    // Add the characteristic description request to the queue
    GATTUtil_Queue_Elem queueElem =
    {
        .reqType = PRIMARY_SERVICE_DISCOVERY,
        .connectionHandle = connectionHandle,
        .callback.servicesCallback = callback,
        .callbackContext = cbContext
    };
    GATTUtil_addElementToQueue(queueElem);

    // Execute the ATT request
    GATT_DiscAllPrimaryServices(connectionHandle, BLEAppUtil_getSelfEntity());

    return SUCCESS;
}

/*********************************************************************
 * @fn      GATTUtil_GetCharsOfService
 *
 * @brief   This function add a element to the GATT queue with the
 *          type CHARS_DISCOVERY and calls the SDK function
 *          GATTUtil_DiscAllChars to get the characteristics of a service.
 * 
 * @param   connectionHandle - the handle for the connection
 * @param   GATTService - the service to be discovered
 * @param   callback - a callback that will be called once the ATT 
 *                     requests for the primary services are completed
 * @param   cbContext - pointer that will be passed to the callback,
 *                      used to preserve context between functions
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_GetCharsOfService(
    uint16_t connectionHandle,
    GATTUtil_service GATTService,
    void (*callback)(
        GATTUtil_char* GATTChars, 
        uint16_t GATTCharsLength,
        void* cbContext),
    void* cbContext)
{
    // Add the characteristic description request to the queue
    GATTUtil_Queue_Elem queueElem =
    {
        .reqType = CHARS_DISCOVERY,
        .connectionHandle = connectionHandle,
        .callback.charsCallback = callback,
        .callbackContext = cbContext
    };
    GATTUtil_addElementToQueue(queueElem);

    // Execute the ATT request
    GATT_DiscAllChars(
        connectionHandle, 
        GATTService.startHandle,
        GATTService.endHandle,
        BLEAppUtil_getSelfEntity());

    return SUCCESS;
}

/*********************************************************************
 * @fn      GATTUtil_GetDescOfAttribute
 *
 * @brief   This function add a element to the GATT queue with the
 *          type CHAR_DESCRIPTOR and calls the SDK function
 *          GATTUtil_ReadCharDesc to get the description of a characteristic.
 *          The GATTUtil_desc object can be freed using the GATTUtil_freeDesc
 *          function.
 *          Effectively, this function is the same as 
 *          GATTUtil_GetValueOfAttribute, but shows how to differentiate two
 *          SDK functions with the same GATTUtil_MSG_EVENT method.
 * 
 * @param   connectionHandle - the handle for the connection
 * @param   attHandle - the handle of the characteristic to read
 * @param   callback - a callback that will be called once the ATT 
 *                     requests for the primary services are completed
 * @param   cbContext - pointer that will be passed to the callback,
 *                      used to preserve context between functions
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_GetDescOfAttribute(
    uint16_t connectionHandle, 
    uint16_t attHandle, 
    void (*callback)(
        GATTUtil_desc GATTDescription,
        void* cbContext),
    void* cbContext)
{
    // Add the characteristic description request to the queue
    GATTUtil_Queue_Elem queueElem =
    {
        .reqType = CHAR_DESCRIPTOR,
        .connectionHandle = connectionHandle,
        .handle = attHandle,
        .callback.descCallback = callback,
        .callbackContext = cbContext
    };
    GATTUtil_addElementToQueue(queueElem);

    // Execute the ATT request
    attReadReq_t pReq =
    {
        .handle = attHandle
    };
    GATT_ReadCharDesc(connectionHandle, &pReq, BLEAppUtil_getSelfEntity());

    return SUCCESS;
}

/*********************************************************************
 * @fn      GATTUtil_GetValueOfAttribute
 *
 * @brief   This function add a element to the GATT queue with the
 *          type CHAR_VALUE and calls the SDK function
 *          GATTUtil_ReadCharValue to get the description of a characteristic.
 *          The GATTUtil_value object can be freed using the GATTUtil_freeValue
 *          function.
 * 
 * @param   connectionHandle - the handle for the connection
 * @param   attHandle - the handle of the characteristic to read
 * @param   callback - a callback that will be called once the ATT 
 *                     requests for the primary services are completed
 * @param   cbContext - pointer that will be passed to the callback,
 *                      used to preserve context between functions
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_GetValueOfAttribute(
    uint16_t connectionHandle, 
    uint16_t attHandle, 
    void (*callback)(
        GATTUtil_value GATTValue,
        void* cbContext),
    void* cbContext)
{
    // Add the characteristic description request to the queue
    GATTUtil_Queue_Elem queueElem =
    {
        .reqType = CHAR_VALUE,
        .connectionHandle = connectionHandle,
        .handle = attHandle,
        .callback.valueCallback = callback,
        .callbackContext = cbContext
    };
    GATTUtil_addElementToQueue(queueElem);

    // Execute the ATT request
    attReadReq_t pReq =
    {
        .handle = attHandle
    };
    GATT_ReadCharValue(connectionHandle, &pReq, BLEAppUtil_getSelfEntity());

    return SUCCESS;
}

//! Queue Functions

/*********************************************************************
 * @fn      GATTUtil_addElementToQueue
 *
 * @brief   This function add a element to the GATT queue. If the queue
 *          is full, expands the queue.
 * 
 * @param   queueElem - element to be added to the queue
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_addElementToQueue(GATTUtil_Queue_Elem queueElem)
{
    if(GATTUtil_Queue == NULL)
    {
        GATTUtil_initializeQueue();
    }

    if(GATTUtil_QueueIsFull())
    {
        bStatus_t status = GATTUtil_expandQueueSize();
        if(status == FAILURE)
        {
            return FAILURE;
        }
    }

    // If the queue is empty, set the front to the first
    // position
    if (queueFront == -1)
    {
        queueFront = 0;
    }

    // Add the data to the queue and move the rear pointer
    queueBack = (queueBack + 1) % maxGATTQueueSize;
    GATTUtil_Queue[queueBack] = queueElem;

    return SUCCESS;
}

/*********************************************************************
 * @fn      GATTUtil_popElementFromQueue
 *
 * @brief   This function pops a element of the GATT queue and stores
 *          it in the queueElem parameter.
 * 
 * @param   queueElem - pointer to an element that will store the 
 *                      popped element
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_popElementFromQueue(GATTUtil_Queue_Elem* queueElem)
{
    if (GATTUtil_QueueIsEmpty())
    {
        return FAILURE;
    }

    // Get the data from the front of the queue
    GATTUtil_Queue_Elem poppedElement = GATTUtil_Queue[queueFront];

    // If the front and rear pointers are at the same
    // position, reset them
    if (queueFront == queueBack)
    {
        queueFront = -1;
        queueBack  = -1;
    }
    else
    {
        // Otherwise, move the front pointer to the next
        // position
        queueFront = (queueFront + 1) % maxGATTQueueSize;
    }

    // Return the popped data
    *queueElem = poppedElement;

    return SUCCESS;
}

/*********************************************************************
 * @fn      GATTUtil_resetAndFreeQueue
 *
 * @brief   This function resets and frees the queue.
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_resetAndFreeQueue()
{
    ICall_free(GATTUtil_Queue);
    queueFront = -1;
    queueBack  = -1;

    return SUCCESS;
}

/*********************************************************************
 * @fn      GATTUtil_expandQueueSize
 *
 * @brief   This function doubles the queue size by allocating new 
 *          memory and copying the old queue into the new one.
 *
 * @return  SUCCESS, FAILURE
 */
static bStatus_t GATTUtil_expandQueueSize()
{
    // Double the size of the queue every time it needs to be expanded
    GATTUtil_Queue_Elem* temporary_GATTUtil_Queue = ICall_malloc(sizeof(GATTUtil_Queue_Elem) * maxGATTQueueSize * 2);

    // Copy old queue into new one
    memcpy(temporary_GATTUtil_Queue, GATTUtil_Queue, sizeof(GATTUtil_Queue_Elem) * maxGATTQueueSize);

    // Free old queue
    ICall_free(GATTUtil_Queue);

    // Update values
    GATTUtil_Queue = temporary_GATTUtil_Queue;
    maxGATTQueueSize = maxGATTQueueSize * 2;

    return SUCCESS;
}

/*********************************************************************
 * @fn      GATTUtil_QueueIsFull
 *
 * @brief   This function checks if the queue is full.
 *
 * @return  TRUE, FALSE
 */
uint8_t GATTUtil_QueueIsFull()
{
    return (queueBack + 1) % maxGATTQueueSize == queueFront;
}

/*********************************************************************
 * @fn      GATTUtil_QueueIsEmpty
 *
 * @brief   This function checks if the queue is empty.
 *
 * @return  TRUE, FALSE
 */
uint8_t GATTUtil_QueueIsEmpty()
{
    return queueFront == -1;
}

/*********************************************************************
 * @fn      GATTUtil_initializeQueue
 *
 * @brief   This function allocates the initial queue size and sets
 *          the appropriate values for the queue values 
 *          (queueFront, queueBack and maxGATTQueueSize).
 *
 * @return  SUCCESS, FAILURE
 */
static bStatus_t GATTUtil_initializeQueue()
{
    GATTUtil_Queue = ICall_malloc(sizeof(GATTUtil_Queue_Elem) * INITIAL_QUEUE_SIZE);
    maxGATTQueueSize = INITIAL_QUEUE_SIZE;

    queueFront = -1;
    queueBack  = -1;

    return SUCCESS;
}
