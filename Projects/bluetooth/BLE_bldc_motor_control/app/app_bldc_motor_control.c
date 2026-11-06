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
 *  ======== app_bldc_motor_control.c ========
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
#include <ti/drivers/ADCBuf.h>
#include <ti/drivers/adcbuf/ADCBufLPF3.h>
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
#include <ti/devices/cc23x0r5/inc/hw_systim.h>

#include <ti/bleapp/profiles/data_stream/data_stream_profile.h>

/* Driver configuration */
#include "ti/drivers/timer/LGPTimerLPF3.h"
#include "ti_drivers_config.h"

/* Display Macros */
#include <app_bldc_motor_control.h>

#include <simple_gatt_profile.h>

extern bStatus_t SimpleGattProfile_setParameter( uint8 param, uint8 len, void *value );

ADCLPF3_HWAttrs const *hwAttrs;
uint8_t pwmDuty = DUTY_MIN;         // Current PWM percentage
uint8_t pwmGoal = DUTY_MIN;         // Target PWM percentage
uint8_t pwmOrig = 0;                // PWM value during 1-level OCP
uint8_t phaseCount = 0;             // Phase state in commutation table
uint8_t hallState = 0;              // Combined hall value
uint8_t i = 0;
uint8_t motorDirection = 1;         // Forward or reverse
uint8_t motorStop = 1;              // Start or stop
uint8_t faultPin = 0;               // Pin fault
uint8_t ocpFault = 0;               // OCP fault
uint8_t vsenFault = 0;              // ADC fault
uint32_t iocfgRegAddr[COMMUTATIONS];
uint8_t reversePhaseTable[COMMUTATIONS] = {1, 5, 0, 3, 2, 4}; // Reverse translation table
uint8_t forwardPhaseTable[COMMUTATIONS] = {5, 1, 0, 3, 4, 2}; // Forward translation table
uint16_t adcRawValues[ADC_COUNT-2]; // ADC values not in buffer conversion
uint16_t rpmCount;                  // RPM counter
uint16_t rpmValue;                  // RPM total
uint32_t cpuLoad;                   // CPU Load
uint32_t idleCount;                 // CPU idle counter
uint32_t adcCount;                  // ADC counter
uint32_t adcTotal;                  // ADC total
uint16_t sampleBufferOne[ADCSAMPLESIZE];
uint16_t sampleBufferTwo[ADCSAMPLESIZE];
uint16_t outputBuffer[ADCSAMPLESIZE];// ADC buffer conversion result
ADC_Handle adcHandle[ADC_COUNT];
UART2_Handle uart;
ClockP_Handle rpmClockHandle;
ClockP_Handle adcClockHandle;
ClockP_Handle accelClockHandle;
ClockP_Handle delayClockHandle;
ClockP_Handle spinClockHandle;
ADCBuf_Handle adcBuf;
ADCBuf_Params adcBufParams;
ADCBuf_Conversion continuousConversion;
ADCBufLPF3_Object *object;
PWM_Handle pwm0 = NULL;
PWM_Params pwmParams;

ClockP_Handle testClockHandle;

void reverseDirection(void);
void forwardDirection(void);

// Function to reset PWM pins
void resetMotor(void)
{
    GPIO_resetConfig(CONFIG_GPIO_LGPTIMER_1_UH);
    GPIO_resetConfig(CONFIG_GPIO_LGPTIMER_1_UL);
    GPIO_resetConfig(CONFIG_GPIO_LGPTIMER_1_VH);
    GPIO_resetConfig(CONFIG_GPIO_LGPTIMER_1_VL);
    GPIO_resetConfig(CONFIG_GPIO_LGPTIMER_1_WH);
    GPIO_resetConfig(CONFIG_GPIO_LGPTIMER_1_WL);
}

// BLE input
void BLECallback(void)
{
    numBytesRead = 0xFF;
    appTaskEvents |= ACTION_UART;
    sem_post(&sem);
}

// When a hall sensor changes levels, figure out the new PWM drive state
void gpioHall(uint_least8_t index)
{
    if(pwm0 != NULL)
    {
        appTaskEvents |= ACTION_HALL;
        sem_post(&sem);
    }
}

#ifdef TEST_NO_HALL
void testTimeoutHandler (uintptr_t arg)
{
    appTaskEvents |= ACTION_HALL;
    sem_post(&sem);
}
#endif

// ADC callback
void adcBufCallback(ADCBuf_Handle handle,
                    ADCBuf_Conversion *conversion,
                    void *completedADCBuffer,
                    uint32_t completedChannel,
                    int_fast16_t status)
{
    // 2-level ADC OCP, stop the motor
    if (status == ADC_INT_HIGHIFG)
    {
        ocpFault = 1;
        appTaskEvents |= ACTION_STOP;
        sem_post(&sem);
    }
    // 1-level ADC OCP, try to reduce PWM duty cycle
    else if (status == ADC_INT_INIFG)       
    {
        if(!pwmOrig) pwmOrig = pwmGoal;
        if (pwmGoal > DUTY_MIN) pwmGoal--;
        if(!ClockP_isActive(accelClockHandle)) ClockP_start(accelClockHandle);
        appTaskEvents |= ACTION_PWM;
        sem_post(&sem);
        ADCEnableInterrupt(ADC_INT_LOWIFG);
    }
    // No ADC OCP, return to normal functionality
    else if (status == ADC_INT_LOWIFG)
    {
        if(pwmOrig)
        {
            pwmGoal = pwmOrig;
            pwmOrig = 0;
            if(!ClockP_isActive(accelClockHandle)) ClockP_start(accelClockHandle);
            ADCDisableInterrupt(ADC_INT_LOWIFG);
        }

    }
    // ADC buffer has filled
    else
    {
        memcpy(outputBuffer, completedADCBuffer, ADCSAMPLESIZE*2);
        adcCount++;
        if(outputBuffer[ADCSAMPLESIZE-1] > VSEN_THRESHOLD) 
        {
            vsenFault = 1;
            if(vsenFault) appTaskEvents |= ACTION_STOP;
            sem_post(&sem);
        }
    }

    ADCClearInterrupt(ADC_INT_HIGHIFG | ADC_INT_INIFG | ADC_INT_LOWIFG);

}

#ifdef USE_UART
// Incoming UART byte
void uartCallback(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    numBytesRead = count;
    appTaskEvents |= ACTION_UART;
    sem_post(&sem);
}
#endif

// Timer for motor acceleration or deceleration
void accelTimeoutHandler (uintptr_t arg)
{
    appTaskEvents |= ACTION_PWM;
    sem_post(&sem);
}

// Timer for spin detection
void spinTimeoutHandler (uintptr_t arg)
{
    if(!motorStop) 
    {
        appTaskEvents |= ACTION_STOP;
        sem_post(&sem);
    }
}

// Periodic timer for taking ADC samples
void adcTimeoutHandler (uintptr_t arg)
{
    appTaskEvents |= ACTION_ADC;
    sem_post(&sem);
}

// Periodic timer for measuring RPM and Idle time
void rpmTimeoutHandler (uintptr_t arg)
{
    rpmValue = rpmCount*(60000000/RPM_INTERVAL);
    cpuLoad = 100-(idleCount*(1000000/RPM_INTERVAL)/10000);    // Logic has to change if RPM_INTERVAL > 1M
    rpmCount = 0;
    idleCount = 0;

    adcTotal = adcCount*(1000000/RPM_INTERVAL)*ADCSAMPLESIZE;
    adcCount = 0;
}

// Timer to expire before changing motor direction
void delayTimeoutHandler (uintptr_t arg)
{
    appTaskEvents |= ACTION_CONTINUE;
    sem_post(&sem);
}

// This is for sleep and fault pins to stop the motor if active
void gpioInterrupt(uint_least8_t index)
{
    faultPin = GPIO_read(CONFIG_GPIO_nFAULT_49C);
    if (!faultPin) 
    {
        appTaskEvents |= ACTION_STOP;
    }
    sem_post(&sem);
}

/*
 *  ======== mainThread ========
 *  Task periodically increments the PWM duty for the on board LED.
 */
void *mainThread(void *arg0)
{
    /* Create semaphore */
    int32_t semStatus;
    semStatus = sem_init(&sem, 0, 0);

    if (semStatus != 0)
    {
        /* Error creating semaphore */
        while (1) {}
    }

    ADC_init();
    ADC_Params adcParams;
    ADC_Params_init(&adcParams);
    adcParams.isProtected = true;
    for (i = 0; i < ADC_COUNT; i++)
    {
        adcHandle[i] = ADC_open(i, &adcParams);
    }

    ADCBuf_init();
    /* Set up an ADCBuf peripheral in ADCBuf_RECURRENCE_MODE_CONTINUOUS */
    ADCBuf_Params_init(&adcBufParams);
    adcBufParams.callbackFxn       = adcBufCallback;
    adcBufParams.recurrenceMode    = ADCBuf_RECURRENCE_MODE_CONTINUOUS;
    adcBufParams.returnMode        = ADCBuf_RETURN_MODE_CALLBACK;
    adcBuf                         = ADCBuf_open(CONFIG_ADC_ISEN, &adcBufParams);

    /* Configure the conversion struct */
    continuousConversion.arg                   = NULL;
    continuousConversion.adcChannel            = CONFIG_ADC_CHANNEL_0;
    continuousConversion.sampleBuffer          = sampleBufferOne;
    continuousConversion.sampleBufferTwo       = sampleBufferTwo;
    continuousConversion.samplesRequestedCount = ADCSAMPLESIZE;

    if (adcBuf == NULL)
    {
        /* ADCBuf failed to open. */
        while (1) {}
    }

    object = adcBuf->object;
    
    // Add a window comparator on incoming ADC values for OCP
    HWREG(ADC_BASE + ADC_O_MEMCTL2) |= ADC_MEMCTL2_WINCOMP;
    HWREG(ADC_BASE + ADC_O_WCLOW) |= WINDOW_LOW;
    HWREG(ADC_BASE + ADC_O_WCHIGH) |= WINDOW_HIGH;

    // Unique ADC input control for custom ADC driver
    hwAttrs = adcHandle[CONFIG_ADC_VSEN]->hwAttrs;
    ADCSetInput(hwAttrs->refSource, hwAttrs->internalChannel, 1);
    ADCSetInput(hwAttrs->refSource, hwAttrs->internalChannel, 3);
    hwAttrs = adcBuf->hwAttrs;
    ADCSetInput(hwAttrs->refSource, hwAttrs->internalChannel, 2);

    GPIO_init();
    // install Button callback and enable interrupts
    GPIO_setConfig(CONFIG_GPIO_HALLA, GPIO_CFG_IN_NOPULL | GPIO_CFG_INT_BOTH_EDGES_INTERNAL);
    GPIO_setConfig(CONFIG_GPIO_HALLB, GPIO_CFG_IN_NOPULL | GPIO_CFG_INT_BOTH_EDGES_INTERNAL);
    GPIO_setConfig(CONFIG_GPIO_HALLC, GPIO_CFG_IN_NOPULL | GPIO_CFG_INT_BOTH_EDGES_INTERNAL);

#ifndef TEST_NO_HALL
    // set GPIO callbacks
    GPIO_setCallback(CONFIG_GPIO_HALLA, gpioHall);
    GPIO_setCallback(CONFIG_GPIO_HALLB, gpioHall);
    GPIO_setCallback(CONFIG_GPIO_HALLC, gpioHall);

    // enable GPIO interrupts
    GPIO_enableInt(CONFIG_GPIO_HALLA);
    GPIO_enableInt(CONFIG_GPIO_HALLB);
    GPIO_enableInt(CONFIG_GPIO_HALLC);
#endif

    // Again for Fault pin
    GPIO_setConfig(CONFIG_GPIO_nFAULT_49C, GPIO_CFG_IN_NOPULL | GPIO_CFG_IN_INT_FALLING);
    GPIO_setCallback(CONFIG_GPIO_nFAULT_49C, gpioInterrupt);
    GPIO_enableInt(CONFIG_GPIO_nFAULT_49C);

    /* Call PWM init functions. */
    PWM_init();
    PWM_Params_init(&pwmParams);
    pwmParams.idleLevel = PWM_IDLE_LOW; 
    pwmParams.dutyUnits    = PWM_DUTY_FRACTION;
    pwmParams.dutyValue    = 0;
    pwmParams.periodUnits  = PWM_PERIOD_HZ;
    pwmParams.periodValue  = PWM_PERIOD;

    // LGPT1 triggers the ADC sampling
    HWREG(EVTSVT_BASE + EVTSVT_O_ADCTRGSEL) = EVTSVT_ADCTRGSEL_PUBID_LGPT1_ADC;

    // Take back the PWM pins since we will modify through register control
    resetMotor();
    iocfgRegAddr[0] = IOC_ADDR(CONFIG_GPIO_LGPTIMER_1_UH);
    iocfgRegAddr[1] = IOC_ADDR(CONFIG_GPIO_LGPTIMER_1_UL);
    iocfgRegAddr[2] = IOC_ADDR(CONFIG_GPIO_LGPTIMER_1_VH);
    iocfgRegAddr[3] = IOC_ADDR(CONFIG_GPIO_LGPTIMER_1_VL);
    iocfgRegAddr[4] = IOC_ADDR(CONFIG_GPIO_LGPTIMER_1_WH);
    iocfgRegAddr[5] = IOC_ADDR(CONFIG_GPIO_LGPTIMER_1_WL);

    // Initializing various clocks
    ClockP_Params clockParams;
    ClockP_Params_init(&clockParams);
    clockParams.period    = ADC_INTERVAL / ClockP_getSystemTickPeriod();
    clockParams.startFlag = true;                   // ADC and RPM/idle readings start with the device
    clockParams.arg       = 0xAA;
    adcClockHandle        = ClockP_create(adcTimeoutHandler, 0, &clockParams);
    clockParams.period    = RPM_INTERVAL / ClockP_getSystemTickPeriod();
    rpmClockHandle        = ClockP_create(rpmTimeoutHandler, 0, &clockParams);
    clockParams.startFlag = false;                  // Accel/decel and motor change delay are situational
    clockParams.period    = ACCEL_INTERVAL / ClockP_getSystemTickPeriod();
    accelClockHandle      = ClockP_create(accelTimeoutHandler, 0, &clockParams);
    clockParams.period    = 0;
    delayClockHandle      = ClockP_create(delayTimeoutHandler, CHANGE_DELAY / ClockP_getSystemTickPeriod(), &clockParams);
    spinClockHandle       = ClockP_create(spinTimeoutHandler, SPIN_TIMEOUT / ClockP_getSystemTickPeriod(), &clockParams);
#ifdef TEST_NO_HALL
    clockParams.period    = TEST_TIMEOUT / ClockP_getSystemTickPeriod();
    testClockHandle       = ClockP_create(testTimeoutHandler, TEST_TIMEOUT / ClockP_getSystemTickPeriod(), &clockParams);
#endif

#ifdef USE_UART
    // UART should be removed when not needed for improved power consumption
    UART2_Params uartParams;
    UART2_Params_init(&uartParams);
    uartParams.baudRate = 921600;
    uartParams.readMode = UART2_Mode_CALLBACK;
    uartParams.readCallback = uartCallback;
    uartParams.writeMode = UART2_Mode_NONBLOCKING;
    uart = UART2_open(CONFIG_UART2_0, &uartParams);
    UART2_read(uart, &input, 1, NULL);

    // Init the terminal UI
    strncpy(uartBuffer, "BLDC Motor Driver\r\n", MAX_STRING);
    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
    strncpy(uartBuffer, "s: Start/Stop\r\n", MAX_STRING);
    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
    strncpy(uartBuffer, "u: PWM Up 5%\r\n", MAX_STRING);
    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
    strncpy(uartBuffer, "d: PWM Down 5%\r\n", MAX_STRING);
    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
    strncpy(uartBuffer, "f: Forward\r\n", MAX_STRING);
    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
    strncpy(uartBuffer, "r: Reverse\r\n", MAX_STRING);
    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
    strncpy(uartBuffer, "l: Read RPM & CPU\r\n", MAX_STRING);
    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
#endif

    /* Loop forever incrementing the PWM duty */
    while (1)
    {
        if(appTaskEvents & ACTION_UART)     // Incoming byte
        {
            if (numBytesRead == 0xFF)       // Indicates BLE
            {
                memset(uartBuffer, 0, sizeof(uartBuffer));

                // Direction Control
                if(charEvents == CHAR3_INPUT){
                    if(char3 == 0 && motorDirection)          // reverse command
                    {
                        motorDirection = 0;
                        if(!motorStop)
                        {
                            motorStop |= 2;
                            appTaskEvents |= ACTION_STOP;
                            ClockP_start(delayClockHandle);
                        }
#ifdef USE_UART
                        strncpy(uartBuffer, "Reverse\r\n", MAX_STRING);
                        UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
#endif
                    }
                    else if(char3 == 1 && !motorDirection)    // forward command
                    {
                        motorDirection = 1;
                        if(!motorStop)
                        {
                            motorStop |= 2;
                            appTaskEvents |= ACTION_STOP;
                            ClockP_start(delayClockHandle);
                        }
#ifdef USE_UART
                        strncpy(uartBuffer, "Forward\r\n", MAX_STRING);
                        UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
#endif
                    }
                }

                // Start/Stop Control
                else if(charEvents == CHAR1_INPUT)
                {
                    if(char1 == 1 && motorStop )                       // Start
                    {
                        ocpFault = 0;
                        appTaskEvents |= ACTION_CONTINUE;
                    }
                    else if(char1 == 0 && !motorStop)                  // Stop
                    {
                        motorStop |= 2;
                        appTaskEvents |= ACTION_STOP;
                    }       
                }

#ifndef TEST_NO_HALL
                // Set PWM Duty Cycle in percentage
                else if(charEvents == CHAR4_INPUT)
                {
                    if(char4 >= DUTY_MIN && char4 <= DUTY_MAX)
                    {
                        pwmGoal = char4;
                        if(!ClockP_isActive(accelClockHandle)) ClockP_start(accelClockHandle);
#ifdef USE_UART
                        sprintf(uartBuffer, "PWM is %d%%\r\n", pwmGoal);
                        UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
#endif
                    }
                }
#endif
            }
#ifdef USE_UART
            else if (numBytesRead > 0)      // Indicates UART
            {
                memset(uartBuffer, 0, sizeof(uartBuffer));
                if(input == 'r' && motorDirection)          // reverse command
                {
                    motorDirection = 0;
                    if(!motorStop)
                    {
                        motorStop |= 2;
                        appTaskEvents |= ACTION_STOP;
                        ClockP_start(delayClockHandle);
                    }
                    strncpy(uartBuffer, "Reverse\r\n", MAX_STRING);
                    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
                }
                else if(input == 'f' && !motorDirection)    // forward command
                {
                    motorDirection = 1;
                    if(!motorStop)
                    {
                        motorStop |= 2;
                        appTaskEvents |= ACTION_STOP;
                        ClockP_start(delayClockHandle);
                    }
                    strncpy(uartBuffer, "Forward\r\n", MAX_STRING);
                    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
                }
                else if(input == 's')                       // start/stop command
                {
                    if (motorStop) 
                    {
                        appTaskEvents |= ACTION_CONTINUE;
                        ocpFault = 0;
                    }
                    else 
                    {
                        motorStop |= 2;
                        appTaskEvents |= ACTION_STOP;
                    }
                }
#ifndef TEST_NO_HALL
                else if(input == 'u' && (pwmGoal + DUTY_INC) <= DUTY_MAX) // increment PWM up command
                {
                    pwmGoal += DUTY_INC;
                    if(!ClockP_isActive(accelClockHandle)) ClockP_start(accelClockHandle);
                    sprintf(uartBuffer, "PWM Up to %d%%\r\n", pwmGoal);
                    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
                    
                }
                else if (input == 'd' && (pwmGoal - DUTY_INC) >= DUTY_MIN) // decrement PWM up command
                {
                    pwmGoal -= DUTY_INC;
                    if(!ClockP_isActive(accelClockHandle)) ClockP_start(accelClockHandle);
                    sprintf(uartBuffer, "PWM Dn to %d%%\r\n", pwmGoal);
                    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
                }
#endif
                else if (input == 'l')                       // for reporting RPM & CPU readings
                {
                    sprintf(uartBuffer, "RPM   %5d  \r\n", rpmValue);
                    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
                    sprintf(uartBuffer, "CPU   %5d %%\r\n", cpuLoad);
                    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
                }
            UART2_read(uart, &input, 1, NULL);
            }
#endif
            appTaskEvents &= ~ACTION_UART;
        }

        if(appTaskEvents & ACTION_HALL) // Hall sensor changed level
        {
            hallState = HWREGB(GPIO_BASE + GPIO_O_DIN3_0 + CONFIG_GPIO_HALLA) +\
            HWREGB(GPIO_BASE + GPIO_O_DIN3_0 + CONFIG_GPIO_HALLB)*2 +\
            HWREGB(GPIO_BASE + GPIO_O_DIN3_0 + CONFIG_GPIO_HALLC)*4;
#ifdef TEST_NO_HALL
            phaseCount++;
            if(phaseCount == 6) phaseCount = 0;
            if(!motorDirection) reverseDirection();
            else if(motorDirection) forwardDirection();
            sprintf(uartBuffer, "Phase %d Hall %d  \r\n", phaseCount, hallState);
            UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
#else
            if(!motorDirection) // Next step in the reverse table
            {
                phaseCount = reversePhaseTable[hallState - 1];
                reverseDirection();
            }
            else if(motorDirection) // Next step in the forward table
            {
                phaseCount = forwardPhaseTable[hallState - 1];
                forwardDirection();
            }
#endif
            if(phaseCount == 5) 
            {
                rpmCount++; // Increment RPM counter
                ClockP_start(spinClockHandle);
            }
            appTaskEvents &= ~ACTION_HALL;
        }

        if(appTaskEvents & ACTION_ADC)  // Periodic ADC timer expired
        {
            if(object->conversionStatus)
            {
                ADCBuf_convertCancel(adcBuf);
                ADCDisableInterrupt(ADC_INT_HIGHIFG | ADC_INT_INIFG | ADC_INT_LOWIFG);
            }
            // Take ADC measurements of those not in the ADC buffer
            ADC_convert(adcHandle[0], adcRawValues);
            ADC_convertChain(adcHandle, adcRawValues, ADC_COUNT-2);

            // Continue ADC buffer measurements if motor is running
            if(!motorStop && !object->conversionStatus)
            {
                ADCClearInterrupt(0xFFFFFFFF);
                ADCEnableInterrupt(ADC_INT_HIGHIFG | ADC_INT_INIFG);
                /* Start converting. */
                if (ADCBuf_convert(adcBuf, &continuousConversion, 1) != ADCBuf_STATUS_SUCCESS)
                {
                    /* Did not start conversion process correctly. */
                    while (1) {}
                }
            }
            appTaskEvents &= ~ACTION_ADC;
        }

        if(appTaskEvents & ACTION_STOP)     // Stop the motor
        {
            if(pwm0 != NULL)
            {
                ClockP_stop(accelClockHandle);
                ClockP_stop(spinClockHandle);
#ifdef TEST_NO_HALL
                ClockP_stop(testClockHandle);
#endif
                resetMotor();
                PWM_stop(pwm0);
                PWM_close(pwm0);

                pwm0 = NULL;
                faultPin = GPIO_read(CONFIG_GPIO_nFAULT_49C);
#ifdef USE_UART
                if(motorStop & 0x2)
                {
                    strncpy(uartBuffer, "Stop: User\r\n", MAX_STRING);
                    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
                }
                else if (!faultPin)
                {
                    strncpy(uartBuffer, "Stop: fault\r\n", MAX_STRING);
                    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
                }
                if(ocpFault)
                {
                    strncpy(uartBuffer, "Stop: ocp\r\n", MAX_STRING);
                    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
                }
                if(vsenFault)
                {
                    strncpy(uartBuffer, "Stop: voltage\r\n", MAX_STRING);
                    UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
                }
#endif
                ocpFault = 0;
                vsenFault = 0;
                motorStop |= 1;                
                Power_releaseConstraint(PowerLPF3_DISALLOW_STANDBY);
                rpmCount = 0;
            }
            appTaskEvents &= ~ACTION_STOP;
        }

        if(appTaskEvents & ACTION_CONTINUE) // Start the motor
        {
            if(pwm0 == NULL)
            {
                pwm0= PWM_open(CONFIG_PWM_6, &pwmParams);
                resetMotor();
                usleep(100);
                pwmDuty = DUTY_MIN;
                if (pwmGoal < DUTY_MIN) pwmGoal = DUTY_MIN;
                PWM_start(pwm0);
                PWM_setDuty(pwm0, (uint32_t) (((uint64_t) PWM_DUTY_FRACTION_MAX * (100-pwmDuty)) / 100));
                // Determine the starting motor state and drive PWM accordingly
                phaseCount = HWREGB(GPIO_BASE + GPIO_O_DIN3_0 + CONFIG_GPIO_HALLA) +\
                            HWREGB(GPIO_BASE + GPIO_O_DIN3_0 + CONFIG_GPIO_HALLB)*2 +\
                            HWREGB(GPIO_BASE + GPIO_O_DIN3_0 + CONFIG_GPIO_HALLC)*4;
                if(motorDirection) 
                {
                    phaseCount = forwardPhaseTable[phaseCount - 1];
                    forwardDirection();
                }
                else 
                {
                    phaseCount = reversePhaseTable[phaseCount - 1];
                    reverseDirection();
                }
                if(!ClockP_isActive(accelClockHandle)) ClockP_start(accelClockHandle);
                motorStop = 0;
#ifdef USE_UART
                strncpy(uartBuffer, "Start\r\n", MAX_STRING);
                UART2_write(uart, uartBuffer, sizeof(uartBuffer), NULL);
#endif
                /* Set constraints to guarantee operation */
                Power_setConstraint(PowerLPF3_DISALLOW_STANDBY);
                ClockP_start(spinClockHandle);
#ifdef TEST_NO_HALL
                ClockP_start(testClockHandle);
#endif
            }
            appTaskEvents &= ~ACTION_CONTINUE;
        }

        if(appTaskEvents & ACTION_PWM)  // PWM change timer expired
        {
            if(pwm0 != NULL)
            {
                // An "acceleration"/deceleration timer is used to increment or decrement the 
                // duty cycle by one percent each timeout until the goal is achieved.
                if(pwmDuty < pwmGoal && pwmGoal <= DUTY_MAX) pwmDuty++;
                else if(pwmDuty > pwmGoal && pwmGoal >= DUTY_MIN) pwmDuty--;
                if (pwmDuty == pwmGoal) ClockP_stop(accelClockHandle);
                PWM_setDuty(pwm0, (uint32_t) (((uint64_t) PWM_DUTY_FRACTION_MAX * (100-pwmDuty)) / 100));
            }
            appTaskEvents &= ~ACTION_PWM;
        }

        sem_wait(&sem);
    }
}

// Forward states
void forwardDirection(void)
{
    switch(phaseCount)
    {
        case 0:
            // Reset WH and WL
            HWREG(iocfgRegAddr[4]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;
            HWREG(iocfgRegAddr[5]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM VH and VL
            HWREG(iocfgRegAddr[2]) = GPIO_MUX_PORTCFG_PFUNC4;
            HWREG(iocfgRegAddr[3]) = GPIO_MUX_PORTCFG_PFUNC2;

            //Set UL high
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_UL) = 0x1;
            HWREG(iocfgRegAddr[1]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            break;

        case 1:
            //Reset UL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_UL) = 0;
            HWREG(iocfgRegAddr[1]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //Set high WL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_WL) = 0x1;
            HWREG(iocfgRegAddr[5]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM VH and VL
            HWREG(iocfgRegAddr[2]) = GPIO_MUX_PORTCFG_PFUNC4;
            HWREG(iocfgRegAddr[3]) = GPIO_MUX_PORTCFG_PFUNC2;

            break;

        case 2:
            //Reset VH and VL
            HWREG(iocfgRegAddr[2]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;
            HWREG(iocfgRegAddr[3]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM UH and UL
            HWREG(iocfgRegAddr[0]) = GPIO_MUX_PORTCFG_PFUNC2;
            HWREG(iocfgRegAddr[1]) = GPIO_MUX_PORTCFG_PFUNC3;

            //Set high WL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_WL) = 0x1;
            HWREG(iocfgRegAddr[5]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            break;  
        
        case 3:
            //Reset WL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_WL) = 0;
            HWREG(iocfgRegAddr[5]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //Set high VL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_VL) = 0x1;
            HWREG(iocfgRegAddr[3]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM UH and UL
            HWREG(iocfgRegAddr[0]) = GPIO_MUX_PORTCFG_PFUNC2;
            HWREG(iocfgRegAddr[1]) = GPIO_MUX_PORTCFG_PFUNC3;

            break;  

        case 4:
            //Reset UH and UL
            HWREG(iocfgRegAddr[0]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;
            HWREG(iocfgRegAddr[1]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM WH and WL
            HWREG(iocfgRegAddr[4]) = GPIO_MUX_PORTCFG_PFUNC3;
            HWREG(iocfgRegAddr[5]) = GPIO_MUX_PORTCFG_PFUNC2;

            //Set high VL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_VL) = 0x1;
            HWREG(iocfgRegAddr[3]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            break; 

        case 5:
            //Reset WL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_VL) = 0;
            HWREG(iocfgRegAddr[3]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM WH and WL
            HWREG(iocfgRegAddr[4]) = GPIO_MUX_PORTCFG_PFUNC3;
            HWREG(iocfgRegAddr[5]) = GPIO_MUX_PORTCFG_PFUNC2;

            //Set UL high
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_UL) = 0x1;
            HWREG(iocfgRegAddr[1]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            break; 
    }
}

// Reverse states
void reverseDirection(void)
{
    switch(phaseCount)
    {
        case 0:
            //Reset UH and UL
            HWREG(iocfgRegAddr[0]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;
            HWREG(iocfgRegAddr[1]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM VH and VL
            HWREG(iocfgRegAddr[2]) = GPIO_MUX_PORTCFG_PFUNC4;
            HWREG(iocfgRegAddr[3]) = GPIO_MUX_PORTCFG_PFUNC2;

            //Set high WL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_WL) = 0x1;
            HWREG(iocfgRegAddr[5]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            break;

        case 1:
            //Reset WL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_WL) = 0;
            HWREG(iocfgRegAddr[5]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //Set high UL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_UL) = 0x1;
            HWREG(iocfgRegAddr[1]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM VH and VL
            HWREG(iocfgRegAddr[2]) = GPIO_MUX_PORTCFG_PFUNC4;
            HWREG(iocfgRegAddr[3]) = GPIO_MUX_PORTCFG_PFUNC2;

            break; 

        case 2:
            //Reset VH and VL
            HWREG(iocfgRegAddr[2]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;
            HWREG(iocfgRegAddr[3]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM WH and WL
            HWREG(iocfgRegAddr[4]) = GPIO_MUX_PORTCFG_PFUNC3;
            HWREG(iocfgRegAddr[5]) = GPIO_MUX_PORTCFG_PFUNC2;

            //Set high UL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_UL) = 0x1;
            HWREG(iocfgRegAddr[1]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            break; 

        case 3:
            //Reset UL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_UL) = 0;
            HWREG(iocfgRegAddr[1]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //Set high VL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_VL) = 0x1;
            HWREG(iocfgRegAddr[3]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM WH and WL
            HWREG(iocfgRegAddr[4]) = GPIO_MUX_PORTCFG_PFUNC3;
            HWREG(iocfgRegAddr[5]) = GPIO_MUX_PORTCFG_PFUNC2;

            break; 
        
        case 4:
            //Reset WH and WL
            HWREG(iocfgRegAddr[4]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;
            HWREG(iocfgRegAddr[5]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM UH and UL
            HWREG(iocfgRegAddr[0]) = GPIO_MUX_PORTCFG_PFUNC2;
            HWREG(iocfgRegAddr[1]) = GPIO_MUX_PORTCFG_PFUNC3;

            //Set high VL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_VL) = 0x1;
            HWREG(iocfgRegAddr[3]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            break;  

        case 5:
            //Reset VL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_VL) = 0;
            HWREG(iocfgRegAddr[3]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //Set high WL
            HWREGB(GPIO_BASE + GPIO_O_DOUT3_0 + CONFIG_GPIO_LGPTIMER_1_WL) = 0x1;
            HWREG(iocfgRegAddr[5]) = GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED;

            //PWM UH and UL
            HWREG(iocfgRegAddr[0]) = GPIO_MUX_PORTCFG_PFUNC2;
            HWREG(iocfgRegAddr[1]) = GPIO_MUX_PORTCFG_PFUNC3;

            break;
           
    }
}

void BLDCMain(void)
{
  pthread_t thread;
  pthread_attr_t attrs;
  struct sched_param priParam;
  int retc;

  /* Initialize the attributes structure with default values */
  pthread_attr_init(&attrs);

  /* Set priority, detach state, and stack size attributes */
  priParam.sched_priority = 5; // Lower the priority of this task
  retc = pthread_attr_setschedparam(&attrs, &priParam);
  retc |= pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
  retc |= pthread_attr_setstacksize(&attrs, THREADSTACKSIZE);
  if (retc != 0)
  {
    /* failed to set attributes */
    while (1) {}
  }

  retc = pthread_create(&thread, &attrs, mainThread, NULL);
  if (retc != 0)
  {
    /* pthread_create() failed */
    while (1) {}
  }
}

// Here is the custom power policy to monitor CPU usage
void customPolicyFxn(void)
{
    uint32_t sysTimerCurrTime;
    sysTimerCurrTime = HWREG(SYSTIM_BASE + SYSTIM_O_TIME1U);
    PowerCC23X0_standbyPolicy();
    idleCount += (HWREG(SYSTIM_BASE + SYSTIM_O_TIME1U) - sysTimerCurrTime);
}