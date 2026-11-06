/******************************************************************************

@file  app_att_read.h

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

#ifndef APP_ATTUtil_READ_H_
#define APP_ATTUtil_READ_H_

//*****************************************************************************
//! Includes
//*****************************************************************************

#include <ti/ble/app_util/framework/bleapputil_api.h>

//*****************************************************************************
//! Typedefs
//*****************************************************************************

typedef struct
{
    uint16_t          len;                          //!< Length of the value
    uint8_t*          pValue;                       //!< Value
} GATTUtil_value;

typedef struct
{
    uint16_t          len;                          //!< Length of the description
    uint8_t*          pDesc;                        //!< Description
} GATTUtil_desc;

//*****************************************************************************
//! Prototypes
//*****************************************************************************

bStatus_t ATTUtil_handleReadRsp(attReadRsp_t readResponse, uint8_t rspStatus);

//! Memory functions

bStatus_t GATTUtil_freeValue(GATTUtil_value value);

bStatus_t GATTUtil_freeDesc(GATTUtil_desc description);


#endif /* APP_ATTUtil_READ_H_ */