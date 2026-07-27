/*
 * Copyright (c) 2015-2020, Texas Instruments Incorporated
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

/* Standard library includes */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/* CC26xx specific Drivers */
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/timer/GPTimerCC26XX.h>
#include <ti/drivers/gpio/GPIOCC26XX.h>

/* DPL includes required to create locks */
#include <ti/drivers/dpl/HwiP.h>
#include <ti/drivers/dpl/SwiP.h>

#include <AudioHAL/MCLKCC26XX.h>

#include "ti_drivers_config.h"

#include <minimal_board.h>

/* GPTimer handle */
GPTimerCC26XX_Handle timerHandle = NULL;

/* Number of constraints to keep track of when no clients need MCLK */
uint8_t openConstraint = 0;
uint8_t runConstraint = 0;

bool MCLKCC26XX_open(void)
{
    uintptr_t hwiKey, swiKey;
    /* Check if the MCLK interface is already open */
    if(openConstraint  > 0)
    {
        openConstraint++;
        return(true);
    }

    /* Generate MCLK Signal using GPTimer */
    GPTimerCC26XX_Params params;
    GPTimerCC26XX_Params_init(&params);
    params.width          = GPT_CONFIG_32BIT;
    params.mode           = GPT_MODE_PERIODIC_UP;
    params.debugStallMode = GPTimerCC26XX_DEBUG_STALL_OFF;
    timerHandle = GPTimerCC26XX_open(GPT_IDX, &params);

    /* Ensure that GPTImer was opened successfully */
    if(timerHandle == NULL)
    {
        return false;
    }

    /* Configure GPTimer to set clear pin initially and toggle on timeout */
    HWREG(timerHandle->hwAttrs->baseAddr + GPT_O_TAMR) |= GPT_TBMR_TCACT_CLRTOG_ON_TO;

    /* Set pin MUX based on GPTimer index */
    IOCPortConfigureSet(MCLK_PIN_ID, GPTimerCC26XX_getPinMux(timerHandle),
                        IOC_IOMODE_NORMAL);

    /* All went well, so set open to true and return success */
    swiKey = SwiP_disable();
    hwiKey = HwiP_disable();
    openConstraint++;
    HwiP_restore(hwiKey);
    SwiP_restore(swiKey);
    return (true);
}

void MCLKCC26XX_start(void)
{
    uintptr_t hwiKey, swiKey;
    /* Depend on XOSC_HF for precise clocks */
    Power_setDependency(PowerCC26XX_XOSC_HF);

    /* Set timer divider as fraction on CPU frequency */
    ClockP_FreqHz freqHz;
    ClockP_getCpuFreq(&freqHz);
    GPTimerCC26XX_Value loadVal = ((uint32_t)(freqHz.lo) / (2*MCLK_FREQ)) - 1;
    GPTimerCC26XX_setLoadValue(timerHandle, loadVal);

    /* Start the Timer */
    GPTimerCC26XX_start(timerHandle);

    /* Keep track of the number of clients that need clock running */
    swiKey = SwiP_disable();
    hwiKey = HwiP_disable();
    runConstraint++;
    HwiP_restore(hwiKey);
    SwiP_restore(swiKey);
}

void MCLKCC26XX_stop(void)
{
    /* Lock around reading and writing of constraints */
    uintptr_t hwiKey, swiKey;
    swiKey = SwiP_disable();
    hwiKey = HwiP_disable();
    runConstraint--;
    HwiP_restore(hwiKey);
    SwiP_restore(swiKey);
    if(runConstraint == 0)
    {
        /* Stop the timer from running */
        GPTimerCC26XX_stop(timerHandle);

        /* XOSC_HF is no longer needed as clock is not running */
        Power_releaseDependency(PowerCC26XX_XOSC_HF);
    }
}

void MCLKCC26XX_close(void)
{
    /* Lock around reading and writing of constraints */
    uintptr_t hwiKey, swiKey;
    swiKey = SwiP_disable();
    hwiKey = HwiP_disable();
    openConstraint--;
    HwiP_restore(hwiKey);
    SwiP_restore(swiKey);
    if(openConstraint == 0)
    {
        /* Close the timer instance */
        GPTimerCC26XX_close(timerHandle);
        timerHandle = NULL;
    }
}
