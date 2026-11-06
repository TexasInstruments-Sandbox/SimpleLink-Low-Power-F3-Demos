/*
 * Copyright (c) 2024, Texas Instruments Incorporated
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
 */

/*
 * DALI Test Controller - CC2745R10_Q1 Port
 *
 * Ported from MSPM0G3507 to CC2745R10_Q1 with FreeRTOS
 */

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>

/* Driver configuration */
#include "ti_drivers_config.h"

/* DALI Library */
#include "dali_hal_simple.h"
#include <ti/dali/dali_103/dali_gpio_comm.h>
#include <ti/dali/dali_103/dali_cd_comm.h>

/* Global variables required by DALI library */
volatile dali_controlDeviceVar gControlDeviceVar1;
volatile TickCounter gTickCounter;
volatile bool gReceive = false;
volatile DALI_GPIO dali_gpio;

/* User variables for test transmission - userVars defined in dali_103_variables.h */
volatile userVars gUserVar;
volatile bool gSendConfigEvent;

/*
 * Timer callback implementations
 * These are called from the HAL layer
 */

/* Free Running Timer Callback (10ms Interval) */
static void frTimerCallback(void)
{
    if(gTickCounter.resetCounter == 1)
    {
        gControlDeviceVar1.resetState = false;
    }

    if(gTickCounter.quiescentMode == 1)
    {
        gControlDeviceVar1.quiescentMode = DISABLED;
    }

    if(gTickCounter.initialisationCounter == 1)
    {
        gControlDeviceVar1.initialisationState = DISABLED;
    }

    if(gTickCounter.quiescentMode > 0)
        gTickCounter.quiescentMode--;

    if(gTickCounter.resetCounter > 0)
        gTickCounter.resetCounter--;

    if(gTickCounter.initialisationCounter > 0)
        gTickCounter.initialisationCounter--;

    if(gTickCounter.quiescentMode == 0  &&
    gTickCounter.initialisationCounter == 0 &&
    gTickCounter.resetCounter == 0 )
    {
        DALI_HAL_Clock_stop(DALI_CLOCK_FR);
    }
}

/* Backward compatibility - keep old function name */
void TIMER_FR_INST_IRQHandler(void)
{
    frTimerCallback();
}

/*
 *  ======== daliThread ========
 *  Main DALI test controller thread
 */
void *daliThread(void *arg0)
{
    /* Initialize DALI HAL - sets up ClockP and GPIO */
    DALI_HAL_init();

    /* Initialize DALI GPIO Communication - registers TX/RX callbacks */
    DALI_GPIO_init();

    /* Register FR timer callback */
    DALI_HAL_registerFrCallback(frTimerCallback);

    /* Start the Free Running Clock for 10ms ticks */
    DALI_HAL_Clock_start(DALI_CLOCK_FR);

    /* DALI Control Device Initialization */
    DALI_ControlDevice_init();

    /* Initialize gUserVar with Reset Command */
    /* Broadcast Address Scheme */
    gUserVar.addrByte = 0xFF;
    gUserVar.opcodeByte = 0x10;
    gUserVar.isSendTwiceCmd = true;
    DALI_ControlDevice_testTransmit();

    /* Query Control Gear */
    gUserVar.addrByte = 0xFF;
    gUserVar.opcodeByte = 0x91;
    gUserVar.isSendTwiceCmd = false;
    DALI_ControlDevice_testTransmit();

    /* Set DTR0 */
    gUserVar.addrByte = 0xA3;
    gUserVar.opcodeByte = 0x03;
    gUserVar.isSendTwiceCmd = false;
    DALI_ControlDevice_testTransmit();

    /* Read DTR0 */
    gUserVar.addrByte = 0xFF;
    gUserVar.opcodeByte = 0x98;
    gUserVar.isSendTwiceCmd = false;
    DALI_ControlDevice_testTransmit();

    /* Set Address */
    gUserVar.addrByte = 0xFF;
    gUserVar.opcodeByte = 0x80;
    gUserVar.isSendTwiceCmd = true;
    DALI_ControlDevice_testTransmit();

    /* Query Status */
    gUserVar.addrByte = 0x03;
    gUserVar.opcodeByte = 0x90;
    gUserVar.isSendTwiceCmd = false;
    DALI_ControlDevice_testTransmit();

    gUserVar.addrByte = 0x02;
    gUserVar.opcodeByte = 128;
    gUserVar.isSendTwiceCmd = false;
    gUserVar.toSend = true;

    /* Main processing loop */
    while(1)
    {
        if(gUserVar.toSend)
        {
            DALI_ControlDevice_testTransmit();
            gUserVar.toSend = false;
        }
        /* Delay to allow other tasks */
        usleep(250000); /* 250ms */
    }
}

/*
 *  ======== mainThread ========
 *  Main thread that starts DALI thread
 */
void *mainThread(void *arg0)
{
    /* Call driver init functions */
    GPIO_init();

    /* Start DALI controller thread */
    daliThread(arg0);

    return NULL;
}
