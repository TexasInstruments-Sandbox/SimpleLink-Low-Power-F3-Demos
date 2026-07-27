/******************************************************************************

@file  logdebug.c

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


*****************************************************************************/
#include <string.h>
#include "ti_drivers_config.h"
#include <log/logdebug.h>

/* Drivers configuration */
#include "ti_drivers_config.h"
#include <ti/drivers/GPIO.h>

UART2_Handle uart;

void uartwriteCallbackFxn(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
   
}

void Debug_UART_Config(void){

    UART2_Params uartParams;
    UART2_Params_init(&uartParams);
    uartParams.writeMode     = UART2_Mode_CALLBACK;
    uartParams.writeCallback = uartwriteCallbackFxn;
    uartParams.readMode      = UART2_Mode_BLOCKING;
    uartParams.baudRate      = 1500000;

    uart = UART2_open(CONFIG_UART2_0, &uartParams);
    if (uart == NULL){
        while(1);
    }
}
