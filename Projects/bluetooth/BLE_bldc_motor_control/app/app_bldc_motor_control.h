/*
 * Copyright (c) 2015-2024, Texas Instruments Incorporated
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
 *  ======== pwmled1.c ========
 */



/* For usleep() */
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* POSIX Header files */
#include <semaphore.h>
#include <pthread.h>

/* Driver Header files */
#include <ti/drivers/PWM.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/ADC.h>
#include <ti/drivers/adc/ADCLPF3.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/pwm/PWMTimerLPF3.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/dpl/HwiP.h>
#include <ti/bleapp/menu_module/menu_module.h>

#include <ti/devices/cc23x0r5/driverlib/adc.h>
#include <ti/devices/cc23x0r5/inc/hw_ints.h>
#include <ti/devices/cc23x0r5/inc/hw_evtsvt.h>
#include <ti/devices/cc23x0r5/inc/hw_gpio.h>
#include <ti/devices/cc23x0r5/inc/hw_memmap.h>

/* Driver configuration */
#include "ti/drivers/timer/LGPTimerLPF3.h"
#include "ti_drivers_config.h"

/* Stack size in bytes */
#define THREADSTACKSIZE     1024
#define PWM_PERIOD          40000   // 20 ms period BLDC standard, 40000 because Timer up/down mode
#define DUTY_MAX            90      // Need a powerful enough supply
#define DUTY_MIN            10      // Motor won't start spinning at much lower than 10
#define DUTY_INC            5       // UART step size
#define ADC_COUNT           5       // VSENPDD/A/B/C and ISEN and trigger
#define COMMUTATIONS        6
#define MAX_STRING          20      // For UART
#define VSEN_THRESHOLD      2000
#define SPIN_TIMEOUT        1000000 // If the motor isn't moving at least once per 100 ms, stop
#define TEST_TIMEOUT        4000    // 4 ms to manually switch phases
#define RPM_INTERVAL        500000  // 500 ms to evaluate RPM and idle time
#define ADC_INTERVAL        100000  // 100 ms between measuring ADC pins
#define ACCEL_INTERVAL      10000   // 10 ms delay before increment/decrement PWM
#define CHANGE_DELAY        1000000 // 1 second before changing motor direction
#define WINDOW_LOW          4094    // Low value of window comparator, of 4096
#define WINDOW_HIGH         4095    // High value of window comparator, of 4096
#define ADCSAMPLESIZE       128

// Bit mask events
#define ACTION_ADC      0x01
#define ACTION_HALL     0x02
#define ACTION_STOP     0x04
#define ACTION_CONTINUE 0x08
#define ACTION_UART     0x10
#define ACTION_PWM      0x20

#define CHAR1_INPUT     0x01
#define CHAR2_INPUT     0x02
#define CHAR3_INPUT     0x03
#define CHAR4_INPUT     0x04

#define IOC_BASE_PIN_REG 0x00000100
#define IOC_ADDR(index)  (IOC_BASE + IOC_BASE_PIN_REG + (sizeof(uint32_t) * index))

#define USE_UART

void BLECallback(void);

extern uint16_t rpmValue;
extern uint32_t cpuLoad;
extern uint8_t motorStop;
extern uint8_t motorDirection;
extern uint8_t pwmDuty;
char uartBuffer[MAX_STRING];

static uint8_t appTaskEvents;

uint8_t charEvents;

static volatile size_t numBytesRead;

static sem_t sem;

uint8_t charEvents;

/* Data to be transmitted over BLE*/
uint8_t char1;
uint8_t char2;
uint8_t char3;
uint8_t char4;

UART2_Handle uart;
UART2_Params uartParams;
char input; // UART Input

