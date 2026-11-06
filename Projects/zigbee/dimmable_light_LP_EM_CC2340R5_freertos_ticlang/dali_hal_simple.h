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

#ifndef DALI_HAL_SIMPLE_H_
#define DALI_HAL_SIMPLE_H_

/*
 * Simplified DALI HAL using ClockP and GPIO
 *
 * This approach uses:
 * - ClockP for all timing (RX, TX, FR, Settling)
 * - GPIO for DALI TX and RX pins (bit-banging)
 * - No hardware timers required
 */

#include <stdint.h>
#include <stdbool.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/dpl/HwiP.h>

/* Clock frequencies for timing calculations */
extern uint32_t DALI_HAL_TIMER_RX_CLK_MHZ;
extern uint32_t DALI_HAL_TIMER_TX_CLK_MHZ;

/* Clock handles for each timer function */
typedef enum {
    DALI_CLOCK_TX,          /* TX bit timing - 416.67us */
    DALI_CLOCK_RX_TIMEOUT,  /* RX timeout - 2ms */
    DALI_CLOCK_FR,          /* Free running - 10ms */
    DALI_CLOCK_SETTLING,    /* Settling time - variable */
    DALI_CLOCK_COUNT
} DALI_Clock_ID;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize DALI HAL with ClockP and GPIO
 */
void DALI_HAL_init(void);

/* =============================================================================
 * Clock/Timer Functions
 * =============================================================================
 */

/**
 * @brief Start a clock
 * @param clockID Clock identifier
 */
void DALI_HAL_Clock_start(DALI_Clock_ID clockID);

/**
 * @brief Stop a clock
 * @param clockID Clock identifier
 */
void DALI_HAL_Clock_stop(DALI_Clock_ID clockID);

/**
 * @brief Set clock period in microseconds
 * @param clockID Clock identifier
 * @param periodUs Period in microseconds
 */
void DALI_HAL_Clock_setPeriod(DALI_Clock_ID clockID, uint32_t periodUs);

/**
 * @brief Get current tick count (for timing measurements)
 * @return Current tick count in microseconds
 */
uint32_t DALI_HAL_getTicks(void);

/**
 * @brief Calculate time difference between two tick values
 * @param startTick Start tick value
 * @param endTick End tick value
 * @return Time difference in microseconds
 */
uint32_t DALI_HAL_getTickDiff(uint32_t startTick, uint32_t endTick);

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

/**
 * @brief Write to TX pin
 * @param value Pin value (0 or 1)
 */
void DALI_HAL_GPIO_writeTx(uint8_t value);

/**
 * @brief Read RX pin
 * @return Pin value (0 or 1)
 */
uint8_t DALI_HAL_GPIO_readRx(void);

/**
 * @brief Set TX pin to output mode
 */
void DALI_HAL_GPIO_setTxOutput(void);

/**
 * @brief Set RX pin to input mode with edge interrupt
 * @param callback Function to call on RX edge detection
 */
void DALI_HAL_GPIO_setRxInput(void (*callback)(void));

/**
 * @brief Enable RX edge interrupt
 */
void DALI_HAL_GPIO_enableRxInterrupt(void);

/**
 * @brief Disable RX edge interrupt
 */
void DALI_HAL_GPIO_disableRxInterrupt(void);

/* =============================================================================
 * Interrupt Control
 * =============================================================================
 */

/**
 * @brief Enter critical section (disable interrupts)
 * @return Key for restoring interrupt state
 */
uintptr_t DALI_HAL_enterCritical(void);

/**
 * @brief Exit critical section (restore interrupts)
 * @param key Key from enterCritical
 */
void DALI_HAL_exitCritical(uintptr_t key);

/* =============================================================================
 * Clock Callback Registration
 * =============================================================================
 */

/**
 * @brief Register callback for TX clock
 * @param callback Function to call on TX clock tick
 */
void DALI_HAL_registerTxCallback(void (*callback)(void));

/**
 * @brief Register callback for RX timeout clock
 * @param callback Function to call on RX timeout
 */
void DALI_HAL_registerRxTimeoutCallback(void (*callback)(void));

/**
 * @brief Register callback for FR clock (10ms tick)
 * @param callback Function to call on FR tick
 */
void DALI_HAL_registerFrCallback(void (*callback)(void));

/**
 * @brief Register callback for settling timer
 * @param callback Function to call when settling time expires
 */
void DALI_HAL_registerSettlingCallback(void (*callback)(void));

#endif /* DALI_HAL_SIMPLE_H_ */
