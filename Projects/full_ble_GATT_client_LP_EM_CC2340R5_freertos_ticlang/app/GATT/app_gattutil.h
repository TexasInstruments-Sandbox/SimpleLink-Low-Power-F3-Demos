/******************************************************************************

@file  app_gatt.h

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

#ifndef APP_GATTUtil_H_
#define APP_GATTUtil_H_

//*****************************************************************************
//! Includes
//*****************************************************************************

#include <ti/ble/app_util/framework/bleapputil_api.h>
#include "app_attutil.h"
#include "ATT/app_attutil_service.h"
#include "ATT/app_attutil_char.h"
#include "ATT/app_attutil_read.h"

//*****************************************************************************
//! Typedefs
//*****************************************************************************

typedef enum 
{
    CHARS_DISCOVERY,                       //!< GATTUtil_DiscAllChars
    PRIMARY_SERVICE_DISCOVERY,             //!< GATTUtil_DiscAllPrimaryServices
    CHAR_DESCRIPTOR,                       //!< GATTUtil_ReadCharDesc
    CHAR_VALUE                             //!< GATTUtil_ReadCharValue
} GATTUtil_Queue_Req_Type;

typedef union
{
    void (*servicesCallback)(GATTUtil_service* GATTServices, 
                             uint16_t GATTServicesLength, 
                             void* cbContext);
    void (*charsCallback)(GATTUtil_char* GATTChars, 
                          uint16_t GATTCharsLength,
                          void* cbContext);
    void (*descCallback)(GATTUtil_desc GATTDescription,
                         void* cbContext);
    void (*valueCallback)(GATTUtil_value GATTValue,
                          void* cbContext);
} GATTUtil_callback;

typedef struct
{
    GATTUtil_Queue_Req_Type       reqType;            //!< Type of the request
    uint16_t                      connectionHandle;
    uint16_t                      handle;             //!< Handle of the characteristic of the request
    GATTUtil_callback             callback; 
    void*                         callbackContext;
} GATTUtil_Queue_Elem;

//*****************************************************************************
//! Prototypes
//*****************************************************************************

//! GATT Functions

bStatus_t GATTUtil_GetPrimaryServices(
    uint16_t connectionHandle,
    void (*callback)(
        GATTUtil_service* GATTServices, 
        uint16_t GATTServicesLength,
        void* cbContext),
    void* cbContext);

bStatus_t GATTUtil_GetCharsOfService(
    uint16_t connectionHandle,
    GATTUtil_service GATTService,
    void (*callback)(
        GATTUtil_char* GATTChars, 
        uint16_t GATTCharsLength,
        void* cbContext),
    void* cbContext
);

bStatus_t GATTUtil_GetDescOfAttribute(
    uint16_t connectionHandle, 
    uint16_t attHandle, 
    void (*callback)(
        GATTUtil_desc GATTDescription,
        void* cbContext),
    void* cbContext);

bStatus_t GATTUtil_GetValueOfAttribute(
    uint16_t connectionHandle, 
    uint16_t attHandle, 
    void (*callback)(
        GATTUtil_value GATTValue,
        void* cbContext),
    void* cbContext);

//! Queue Functions

bStatus_t GATTUtil_addElementToQueue(GATTUtil_Queue_Elem queueElem);

bStatus_t GATTUtil_popElementFromQueue(GATTUtil_Queue_Elem* queueElem);

bStatus_t GATTUtil_resetAndFreeQueue();

uint8_t GATTUtil_QueueIsFull();

uint8_t GATTUtil_QueueIsEmpty();


#endif /* APP_GATTUtil_H_ */
