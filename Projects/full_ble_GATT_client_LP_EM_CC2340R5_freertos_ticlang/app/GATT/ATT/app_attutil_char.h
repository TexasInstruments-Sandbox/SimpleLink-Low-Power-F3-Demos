/******************************************************************************

@file  app_att_char.h

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

#ifndef APP_ATTUtil_CHAR_H_
#define APP_ATTUtil_CHAR_H_

//*****************************************************************************
//! Includes
//*****************************************************************************

#include <ti/ble/app_util/framework/bleapputil_api.h>
#include "../app_attutil.h"

//*****************************************************************************
//! Typedefs
//*****************************************************************************

typedef struct
{
    uint16_t                   handle;            //!< Handle of the attribute
    uint16_t                   valueHandle;       //!< Handle of the value of the characteristic value
    uint8_t                    permissions;       //!< Permission byte of the characteristic
    GATTUtil_UUID              UUID;              //!< UUID of the characteristic
} GATTUtil_char;

//*****************************************************************************
//! Prototypes
//*****************************************************************************

//! ATT functions

bStatus_t ATTUtil_handleReadByTypeRsp(attReadByTypeRsp_t typeResponse, uint8_t rspStatus);

bStatus_t ATTUtil_completeHandleReadByTypeRsp();

//! Permissions functions

uint8_t GATTUtil_isCharReadable(GATTUtil_char characteristic);

uint8_t GATTUtil_isCharWritable(GATTUtil_char characteristic);

uint8_t GATTUtil_isCharNotif(GATTUtil_char characteristic);

//! Memory functions

bStatus_t GATTUtil_freeChars();


#endif /* APP_ATTUtil_CHAR_H_ */