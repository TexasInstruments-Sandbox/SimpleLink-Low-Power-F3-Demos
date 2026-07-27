/******************************************************************************

@file  app_connection.c

@brief This example file demonstrates how to activate the central role with
the help of BLEAppUtil APIs.

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

//*****************************************************************************
//! Includes
//*****************************************************************************
#include <string.h>
#include <stdarg.h>

#include "ti_ble_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <app_main.h>

/* Drivers configuration */
#include "ti_drivers_config.h"
#include <ti/drivers/GPIO.h>
#include <ti/drivers/dpl/ClockP.h>

#define RC_IDLE_CONN_TIMEOUT         3000  // 3 seconds
#define CLOCK_UNITS_MS               1000  // convert to ms

/* Increase GATT Max MTU size to 251 so that the raw Sensor data can be send through BLE */
void Exchange_gattMTU(){

    bStatus_t status;
    attExchangeMTUReq_t req;
    req.clientRxMTU = 251-L2CAP_HDR_SIZE;
    status = GATT_ExchangeMTU(0,&req,BLEAppUtil_getSelfEntity());
    if (status == SUCCESS){
       GPIO_write(CONFIG_GPIO_LED_GREEN, 1);
    }
}

/* Enable Notifications */
void Enable_notification(){

    bStatus_t status;
    attWriteReq_t req;

    uint8 configData[2] = {0x01,0x00};
    req.pValue = GATT_bm_alloc(0, ATT_WRITE_REQ, 2, NULL);

    /* Enable notify for outgoing data */
    if (req.pValue != NULL)
    {
        /* Harcoded value for the Notification handle */
        req.handle = 0x29;
        req.len = 2;
        memcpy(req.pValue, configData, 2);
        req.cmd = TRUE;
        req.sig = FALSE;
        status = GATT_WriteNoRsp(0, &req);
        if ( status != SUCCESS )
        {
            GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
        }
    }
}

/* Start sensor process - Transmit inference results */
void GATT_write(uint8_t start)
{
    bStatus_t status;
    attWriteReq_t req;

    /* Harcoded value for the Sensor Control Inference Results Characteristic handle */
    req.handle = 0x25;
    req.pValue = GATT_bm_alloc(0, ATT_WRITE_REQ, 1, NULL);
    req.len = 1;
    req.pValue[0] = start;
    req.sig = 0;
    req.cmd = 0;

    status = GATT_WriteCharValue(0, &req, BLEAppUtil_getSelfEntity());
    if ( status != SUCCESS )
    {
        GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
    }

    if ( status != SUCCESS )
    {
        GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
    }
}

ClockP_Handle rc_timeout_clkHandle_conn;

void MTUExchangeTimer_start(void)
{
    ClockP_Params clockpParams;
    static ClockP_Struct rc_timeout_clkStructConn;
    ClockP_Params_init(&clockpParams);
    uint32_t clockTicks = RC_IDLE_CONN_TIMEOUT * (CLOCK_UNITS_MS); //set timeout
    clockpParams.period = 0; //config for one-shot timer
    clockpParams.startFlag = true; //start now
    clockpParams.arg = (uintptr_t)Exchange_gattMTU;
    rc_timeout_clkHandle_conn = ClockP_construct(&rc_timeout_clkStructConn, (void *)BLEAppUtil_invokeFunctionNoData, clockTicks, &clockpParams); //Initialize clock instance.
}
