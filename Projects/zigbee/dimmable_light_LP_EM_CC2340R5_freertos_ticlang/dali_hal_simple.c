/*
 * Copyright (c) 2025, Texas Instruments Incorporated
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

#include "dali_hal_simple.h"
#include "ti_drivers_config.h"
#include <ti/drivers/GPIO.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/dpl/HwiP.h>
#include <FreeRTOS.h>
#include <task.h>

/* Timer clock frequencies (MHz) */
uint32_t DALI_HAL_TIMER_RX_CLK_MHZ = 48; /* 48MHz system clock */
uint32_t DALI_HAL_TIMER_TX_CLK_MHZ = 48;

/* Clock handles and objects */
static ClockP_Handle clockHandles[DALI_CLOCK_COUNT];
static ClockP_Struct clockStructs[DALI_CLOCK_COUNT];
static ClockP_Params clockParams[DALI_CLOCK_COUNT];

/* Callback function pointers */
static void (*txCallback)(void) = NULL;
static void (*rxTimeoutCallback)(void) = NULL;
static void (*frCallback)(void) = NULL;
static void (*settlingCallback)(void) = NULL;
static void (*rxEdgeCallback)(void) = NULL;

/* Clock callback functions */
static void txClockCallback(uintptr_t arg);
static void rxTimeoutClockCallback(uintptr_t arg);
static void frClockCallback(uintptr_t arg);
static void settlingClockCallback(uintptr_t arg);

/* GPIO interrupt callback */
static void rxGpioCallback(uint_least8_t index);

/* =============================================================================
 * Initialization
 * =============================================================================
 */

void DALI_HAL_init(void)
{
    /* Initialize TX Clock - 416.67us period for DALI half-bit timing */
    ClockP_Params_init(&clockParams[DALI_CLOCK_TX]);
    clockParams[DALI_CLOCK_TX].period = 416; /* 416us in microseconds */
    clockParams[DALI_CLOCK_TX].startFlag = false;
    clockParams[DALI_CLOCK_TX].arg = (uintptr_t)NULL;
    clockHandles[DALI_CLOCK_TX] = ClockP_construct(&clockStructs[DALI_CLOCK_TX],
                                                     txClockCallback,
                                                     0,
                                                     &clockParams[DALI_CLOCK_TX]);

    /* Initialize RX Timeout Clock - 2ms for receive timeout */
    ClockP_Params_init(&clockParams[DALI_CLOCK_RX_TIMEOUT]);
    clockParams[DALI_CLOCK_RX_TIMEOUT].period = 0; /* One-shot */
    clockParams[DALI_CLOCK_RX_TIMEOUT].startFlag = false;
    clockParams[DALI_CLOCK_RX_TIMEOUT].arg = (uintptr_t)NULL;
    clockHandles[DALI_CLOCK_RX_TIMEOUT] = ClockP_construct(&clockStructs[DALI_CLOCK_RX_TIMEOUT],
                                                             rxTimeoutClockCallback,
                                                             2000, /* 2ms timeout */
                                                             &clockParams[DALI_CLOCK_RX_TIMEOUT]);

    /* Initialize FR Clock - 10ms free running timer */
    ClockP_Params_init(&clockParams[DALI_CLOCK_FR]);
    clockParams[DALI_CLOCK_FR].period = 10000; /* 10ms */
    clockParams[DALI_CLOCK_FR].startFlag = false;
    clockParams[DALI_CLOCK_FR].arg = (uintptr_t)NULL;
    clockHandles[DALI_CLOCK_FR] = ClockP_construct(&clockStructs[DALI_CLOCK_FR],
                                                     frClockCallback,
                                                     10000,
                                                     &clockParams[DALI_CLOCK_FR]);

    /* Initialize Settling Timer - One-shot, variable period */
    ClockP_Params_init(&clockParams[DALI_CLOCK_SETTLING]);
    clockParams[DALI_CLOCK_SETTLING].period = 0; /* One-shot */
    clockParams[DALI_CLOCK_SETTLING].startFlag = false;
    clockParams[DALI_CLOCK_SETTLING].arg = (uintptr_t)NULL;
    clockHandles[DALI_CLOCK_SETTLING] = ClockP_construct(&clockStructs[DALI_CLOCK_SETTLING],
                                                           settlingClockCallback,
                                                           100000, /* Default 100ms */
                                                           &clockParams[DALI_CLOCK_SETTLING]);

    /* Configure GPIO pins */
    GPIO_setConfig(CONFIG_GPIO_DALI_TX, GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH);
    GPIO_setConfig(CONFIG_GPIO_DALI_RX, GPIO_CFG_INPUT | GPIO_CFG_IN_INT_BOTH_EDGES);
    GPIO_setCallback(CONFIG_GPIO_DALI_RX, rxGpioCallback);
}

/* =============================================================================
 * Clock Functions
 * =============================================================================
 */

void DALI_HAL_Clock_start(DALI_Clock_ID clockID)
{
    if (clockHandles[clockID] != NULL) {
        ClockP_start(clockHandles[clockID]);
    }
}

void DALI_HAL_Clock_stop(DALI_Clock_ID clockID)
{
    if (clockHandles[clockID] != NULL) {
        ClockP_stop(clockHandles[clockID]);
    }
}

void DALI_HAL_Clock_setPeriod(DALI_Clock_ID clockID, uint32_t periodUs)
{
    if (clockHandles[clockID] != NULL) {
        /* Stop clock, update period, restart if needed */
        bool wasRunning = ClockP_isActive(clockHandles[clockID]);
        ClockP_stop(clockHandles[clockID]);

        /* Set timeout in microseconds directly */
        ClockP_setTimeout(clockHandles[clockID], periodUs);

        if (wasRunning) {
            ClockP_start(clockHandles[clockID]);
        }
    }
}

uint32_t DALI_HAL_getTicks(void)
{
    /* Return current time in microseconds using ClockP tick count
     */
    return ClockP_getSystemTicks(); /* Convert ticks to microseconds */
}

uint32_t DALI_HAL_getTickDiff(uint32_t startTick, uint32_t endTick)
{
    /* Handle rollover */
    if (endTick >= startTick) {
        return endTick - startTick;
    } else {
        return (0xFFFFFFFF - startTick) + endTick + 1;
    }
}

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

void DALI_HAL_GPIO_writeTx(uint8_t value)
{
    GPIO_write(CONFIG_GPIO_DALI_TX, value);
}

uint8_t DALI_HAL_GPIO_readRx(void)
{
    return GPIO_read(CONFIG_GPIO_DALI_RX);
}

void DALI_HAL_GPIO_setTxOutput(void)
{
    /* Already configured in init */
}

void DALI_HAL_GPIO_setRxInput(void (*callback)(void))
{
    rxEdgeCallback = callback;
}

void DALI_HAL_GPIO_enableRxInterrupt(void)
{
    GPIO_enableInt(CONFIG_GPIO_DALI_RX);
}

void DALI_HAL_GPIO_disableRxInterrupt(void)
{
    GPIO_disableInt(CONFIG_GPIO_DALI_RX);
}

/* =============================================================================
 * Interrupt Control
 * =============================================================================
 */

uintptr_t DALI_HAL_enterCritical(void)
{
    return HwiP_disable();
}

void DALI_HAL_exitCritical(uintptr_t key)
{
    HwiP_restore(key);
}

/* =============================================================================
 * Callback Registration
 * =============================================================================
 */

void DALI_HAL_registerTxCallback(void (*callback)(void))
{
    txCallback = callback;
}

void DALI_HAL_registerRxTimeoutCallback(void (*callback)(void))
{
    rxTimeoutCallback = callback;
}

void DALI_HAL_registerFrCallback(void (*callback)(void))
{
    frCallback = callback;
}

void DALI_HAL_registerSettlingCallback(void (*callback)(void))
{
    settlingCallback = callback;
}

/* =============================================================================
 * Internal Callback Functions
 * =============================================================================
 */

static void txClockCallback(uintptr_t arg)
{
    if (txCallback != NULL) {
        txCallback();
    }
}

static void rxTimeoutClockCallback(uintptr_t arg)
{
    if (rxTimeoutCallback != NULL) {
        rxTimeoutCallback();
    }
}

static void frClockCallback(uintptr_t arg)
{
    if (frCallback != NULL) {
        frCallback();
    }
}

static void settlingClockCallback(uintptr_t arg)
{
    if (settlingCallback != NULL) {
        settlingCallback();
    }
}

static void rxGpioCallback(uint_least8_t index)
{
    if (rxEdgeCallback != NULL) {
        rxEdgeCallback();
    }
}
