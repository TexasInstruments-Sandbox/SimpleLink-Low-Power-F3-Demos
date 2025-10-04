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
 *  ======== stepper_motor.c ========
 */
/* For usleep() */
#include <unistd.h>
#include <stddef.h>

#include <semaphore.h>
#include <pthread.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/adcbuf/ADCBufLPF3.h>
#include "ti/drivers/timer/LGPTimerLPF3.h"
#include DeviceFamily_constructPath(inc/hw_systim.h)

/* Driver configuration */
#include "ti_drivers_config.h"

#include <ti/drivers/dpl/ClockP.h>

#define ACTION_CCLOCK   0x01        // Counter-clockwise action
#define ACTION_CLOCK    0x02        // Clockwise action
#define ACTION_START    0x04        // Start the motor
#define ACTION_STOP     0x08        // Stop the motor

//#define FULL_STEP
//#define HALF_STEP_SLOW
#define HALF_STEP_FAST

#if defined(FULL_STEP) && (defined(HALF_STEP_SLOW) || defined(HALF_STEP_FAST))
    #error "Only one step mode can be defined"
#elif !defined(FULL_STEP) && !defined(HALF_STEP_SLOW) && !defined(HALF_STEP_FAST)
    #error "One step mode must be defined"
#endif

#define STEP_FREQUENCY  400     // Hz
#ifdef FULL_STEP
    #define STEP_ITERATIONS 4       // 4 iterations for full-step
#else
    #define STEP_ITERATIONS 8       // 8 iterations for half-step
#endif
#define NUMBER_STEPS    400    // Steps per action
#define ADC_SAMPLE_SIZE 100      // Buffer size 
#define ADC_PER_STEP    50      // ADC measurements per step

#define WINDOW_HIGH     1500    // Value for ADC to trigger over-current protection

#define THREADSTACKSIZE     1024

ClockP_Handle motorClockHandle;
ClockP_Handle idleClockHandle;
ADCBuf_Handle adcBuf;
ADC_Handle adcHandle;
ADCLPF3_HWAttrs const *hwAttrs;
LGPTimerLPF3_Handle lgptHandle;
LGPTimerLPF3_Params lgptParams;

uint32_t cpuLoad;               // CPU load value
uint32_t idleCount;             // Idle count to measure CPU load
uint16_t stepCount = 0;         // Number of steps in this activity
uint8_t action = 0;             // Whether motor is moving or not
uint8_t iter = 0;               // Iterations in the commutation table
uint8_t direction = 0;          // Direction of the motor
uint8_t appTaskEvents;
uint16_t sampleBufferOne[ADC_SAMPLE_SIZE];
uint16_t sampleBufferTwo[ADC_SAMPLE_SIZE];
uint32_t microVoltBuffer[ADC_SAMPLE_SIZE];  // Output buffer of ADC converted to uV
uint32_t outputBuffer[ADC_SAMPLE_SIZE];     // Raw output buffer of ADC
sem_t sem;

// Periodic timer for measuring Idle time
void idleTimeoutHandler (uintptr_t arg)
{
    cpuLoad = 100-(idleCount/10000);
    idleCount = 0;
}

// Function to reset motor pins
void resetMotor(void)
{
    GPIO_resetConfig(CONFIG_GPIO_AOUT1);
    GPIO_resetConfig(CONFIG_GPIO_AOUT2);
    GPIO_resetConfig(CONFIG_GPIO_BOUT1);
    GPIO_resetConfig(CONFIG_GPIO_BOUT2);
}

// Take the next step in the commutation table
void nextStep()
{
    if(direction) 
    {
        if(iter == STEP_ITERATIONS-1) iter = 0;
        else iter++;
    }
    else 
    {
        if(iter == 0) iter = STEP_ITERATIONS-1;
        else iter--;
    }
   switch(iter)
   {
#if defined(FULL_STEP)
        case 0: 
            GPIO_write(CONFIG_GPIO_AOUT1, 1);
            GPIO_write(CONFIG_GPIO_AOUT2, 0);
            GPIO_write(CONFIG_GPIO_BOUT1, 0);
            GPIO_write(CONFIG_GPIO_BOUT2, 1);
            break;
        case 1:
            GPIO_write(CONFIG_GPIO_AOUT1, 1);
            GPIO_write(CONFIG_GPIO_AOUT2, 0);
            GPIO_write(CONFIG_GPIO_BOUT1, 1);
            GPIO_write(CONFIG_GPIO_BOUT2, 0);
            break;
        case 2:
            GPIO_write(CONFIG_GPIO_AOUT1, 0);
            GPIO_write(CONFIG_GPIO_AOUT2, 1);
            GPIO_write(CONFIG_GPIO_BOUT1, 1);
            GPIO_write(CONFIG_GPIO_BOUT2, 0);
            break;
        case 3:
            GPIO_write(CONFIG_GPIO_AOUT1, 0);
            GPIO_write(CONFIG_GPIO_AOUT2, 1);
            GPIO_write(CONFIG_GPIO_BOUT1, 0);
            GPIO_write(CONFIG_GPIO_BOUT2, 1);
            break;
        default:
            break;
#elif defined(HALF_STEP_FAST)
        case 0: 
            GPIO_write(CONFIG_GPIO_AOUT1, 0);
            GPIO_write(CONFIG_GPIO_AOUT2, 0);
            GPIO_write(CONFIG_GPIO_BOUT1, 0);
            GPIO_write(CONFIG_GPIO_BOUT2, 1);
            break;
        case 1:
            GPIO_write(CONFIG_GPIO_AOUT1, 1);
            GPIO_write(CONFIG_GPIO_AOUT2, 0);
            GPIO_write(CONFIG_GPIO_BOUT1, 0);
            GPIO_write(CONFIG_GPIO_BOUT2, 1);
            break;
        case 2:
            GPIO_write(CONFIG_GPIO_AOUT1, 1);
            GPIO_write(CONFIG_GPIO_AOUT2, 0);
            GPIO_write(CONFIG_GPIO_BOUT1, 0);
            GPIO_write(CONFIG_GPIO_BOUT2, 0);
            break;
        case 3:
            GPIO_write(CONFIG_GPIO_AOUT1, 1);
            GPIO_write(CONFIG_GPIO_AOUT2, 0);
            GPIO_write(CONFIG_GPIO_BOUT1, 1);
            GPIO_write(CONFIG_GPIO_BOUT2, 0);
            break;
        case 4: 
            GPIO_write(CONFIG_GPIO_AOUT1, 0);
            GPIO_write(CONFIG_GPIO_AOUT2, 0);
            GPIO_write(CONFIG_GPIO_BOUT1, 1);
            GPIO_write(CONFIG_GPIO_BOUT2, 0);
            break;
        case 5:
            GPIO_write(CONFIG_GPIO_AOUT1, 0);
            GPIO_write(CONFIG_GPIO_AOUT2, 1);
            GPIO_write(CONFIG_GPIO_BOUT1, 1);
            GPIO_write(CONFIG_GPIO_BOUT2, 0);
            break;
        case 6:
            GPIO_write(CONFIG_GPIO_AOUT1, 0);
            GPIO_write(CONFIG_GPIO_AOUT2, 1);
            GPIO_write(CONFIG_GPIO_BOUT1, 0);
            GPIO_write(CONFIG_GPIO_BOUT2, 0);
            break;
        case 7:
            GPIO_write(CONFIG_GPIO_AOUT1, 0);
            GPIO_write(CONFIG_GPIO_AOUT2, 1);
            GPIO_write(CONFIG_GPIO_BOUT1, 0);
            GPIO_write(CONFIG_GPIO_BOUT2, 1);
            break;
        default:
            break;
#elif defined(HALF_STEP_SLOW)
        case 0: 
            GPIO_write(CONFIG_GPIO_AOUT1, 1);
            GPIO_write(CONFIG_GPIO_AOUT2, 1);
            GPIO_write(CONFIG_GPIO_BOUT1, 0);
            GPIO_write(CONFIG_GPIO_BOUT2, 1);
            break;
        case 1:
            GPIO_write(CONFIG_GPIO_AOUT1, 1);
            GPIO_write(CONFIG_GPIO_AOUT2, 0);
            GPIO_write(CONFIG_GPIO_BOUT1, 0);
            GPIO_write(CONFIG_GPIO_BOUT2, 1);
            break;
        case 2:
            GPIO_write(CONFIG_GPIO_AOUT1, 1);
            GPIO_write(CONFIG_GPIO_AOUT2, 0);
            GPIO_write(CONFIG_GPIO_BOUT1, 1);
            GPIO_write(CONFIG_GPIO_BOUT2, 1);
            break;
        case 3:
            GPIO_write(CONFIG_GPIO_AOUT1, 1);
            GPIO_write(CONFIG_GPIO_AOUT2, 0);
            GPIO_write(CONFIG_GPIO_BOUT1, 1);
            GPIO_write(CONFIG_GPIO_BOUT2, 0);
            break;
        case 4: 
            GPIO_write(CONFIG_GPIO_AOUT1, 1);
            GPIO_write(CONFIG_GPIO_AOUT2, 1);
            GPIO_write(CONFIG_GPIO_BOUT1, 1);
            GPIO_write(CONFIG_GPIO_BOUT2, 0);
            break;
        case 5:
            GPIO_write(CONFIG_GPIO_AOUT1, 0);
            GPIO_write(CONFIG_GPIO_AOUT2, 1);
            GPIO_write(CONFIG_GPIO_BOUT1, 1);
            GPIO_write(CONFIG_GPIO_BOUT2, 0);
            break;
        case 6:
            GPIO_write(CONFIG_GPIO_AOUT1, 0);
            GPIO_write(CONFIG_GPIO_AOUT2, 1);
            GPIO_write(CONFIG_GPIO_BOUT1, 1);
            GPIO_write(CONFIG_GPIO_BOUT2, 1);
            break;
        case 7:
            GPIO_write(CONFIG_GPIO_AOUT1, 0);
            GPIO_write(CONFIG_GPIO_AOUT2, 1);
            GPIO_write(CONFIG_GPIO_BOUT1, 0);
            GPIO_write(CONFIG_GPIO_BOUT2, 1);
            break;
        default:
            break;
#endif
    }
}

// ADC callback for over-current protection or reading ADC buffer
void adcBufCallback(ADCBuf_Handle handle,
                    ADCBuf_Conversion *conversion,
                    void *completedADCBuffer,
                    uint32_t completedChannel,
                    int_fast16_t status)
{
    if (status == ADC_INT_HIGHIFG)  // OCP event
    {
        ADCClearInterrupt(ADC_INT_HIGHIFG);
        appTaskEvents |= ACTION_STOP;
        sem_post(&sem);
    }
    else                            // ADC buffer filled
    {
        /* Adjust raw ADC values and convert them to microvolts */
        ADCBuf_adjustRawValues(handle, completedADCBuffer, ADC_SAMPLE_SIZE, completedChannel);
        ADCBuf_convertAdjustedToMicroVolts(handle, completedChannel, completedADCBuffer, microVoltBuffer, ADC_SAMPLE_SIZE);
    }
}

// Periodic timer for stepping the motor
void motorUpdateHandler (uintptr_t arg)
{
    stepCount++;
    if(stepCount < NUMBER_STEPS * (STEP_ITERATIONS/4))  // More steps to go
    {
        nextStep();
    }
    else                                                // All steps finished
    {
        appTaskEvents |= ACTION_STOP;
        sem_post(&sem);
    }
}

// Fault pin has triggered, stop the motor
void faultCallback(uint_least8_t index)
{
    appTaskEvents |= ACTION_STOP;
    sem_post(&sem);
}

// Button 1 pressed, perform counter-clockwise action
void gpioButtonFxn0(uint_least8_t index)
{
    appTaskEvents |= ACTION_CCLOCK;
    sem_post(&sem);
}

// Button 2 pressed, perform clockwise action
void gpioButtonFxn1(uint_least8_t index)
{
    appTaskEvents |= ACTION_CLOCK;
    sem_post(&sem);
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    int32_t semStatus;
    semStatus = sem_init(&sem, 0, 0);
    if (semStatus != 0)
    {
        /* Error creating semaphore */
        while (1) {}
    }

    /* Configure the GPIOs */
    GPIO_init();

    GPIO_write(CONFIG_GPIO_nSLEEP, 0);

    GPIO_setConfig(CONFIG_GPIO_BTN_LEFT, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setConfig(CONFIG_GPIO_BTN_RIGHT, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setConfig(CONFIG_GPIO_nFAULT, GPIO_CFG_IN_NOPULL | GPIO_CFG_IN_INT_FALLING);
    /* Install Button callback */
    GPIO_setCallback(CONFIG_GPIO_BTN_LEFT, gpioButtonFxn0);
    GPIO_setCallback(CONFIG_GPIO_BTN_RIGHT, gpioButtonFxn1);
    GPIO_setCallback(CONFIG_GPIO_nFAULT, faultCallback);
    /* Enable interrupts */
    GPIO_enableInt(CONFIG_GPIO_BTN_LEFT);
    GPIO_enableInt(CONFIG_GPIO_BTN_RIGHT);
    GPIO_disableInt(CONFIG_GPIO_nFAULT);

    // Initialize timers
    ClockP_Params clockParams;
    ClockP_Params_init(&clockParams);
    clockParams.arg       = 0xAA;
    clockParams.startFlag = false;
    clockParams.period    = (1000000 / (STEP_FREQUENCY*(STEP_ITERATIONS/4))) / ClockP_getSystemTickPeriod();
    motorClockHandle        = ClockP_create(motorUpdateHandler, 0, &clockParams);
    clockParams.startFlag = true;                   
    clockParams.period    = 1000000 / ClockP_getSystemTickPeriod();
    idleClockHandle       = ClockP_create(idleTimeoutHandler, 0, &clockParams);

    resetMotor();

    LGPTimerLPF3_Params_init(&lgptParams);

    // LGPT3 triggers ADC sampling
    EVTSVTConfigureEvent(EVTSVT_SUB_ADCTRG, EVTSVT_PUB_LGPT3_ADC);

    // ADC initialization tasks
    ADC_init();
    ADC_Params adcParams;
    ADC_Params_init(&adcParams);
    adcParams.isProtected = true;
    adcHandle = ADC_open(CONFIG_ADC_VSENB, &adcParams);

    ADCBuf_Params adcBufParams;
    ADCBuf_Conversion continuousConversion;

    ADCBuf_init();
    /* Set up an ADCBuf peripheral in ADCBuf_RECURRENCE_MODE_CONTINUOUS */
    ADCBuf_Params_init(&adcBufParams);
    adcBufParams.callbackFxn       = adcBufCallback;
    adcBufParams.recurrenceMode    = ADCBuf_RECURRENCE_MODE_CONTINUOUS;
    adcBufParams.returnMode        = ADCBuf_RETURN_MODE_CALLBACK;

    /* Configure the conversion struct */
    continuousConversion.arg                   = NULL;
    continuousConversion.adcChannel            = CONFIG_ADC_CHANNEL_0;
    continuousConversion.sampleBuffer          = sampleBufferOne;
    continuousConversion.sampleBufferTwo       = sampleBufferTwo;
    continuousConversion.samplesRequestedCount = ADC_SAMPLE_SIZE;
    adcBuf = ADCBuf_open(CONFIG_ADCBUF_VSENA, &adcBufParams);

    hwAttrs = adcHandle->hwAttrs;
    ADCSetInput(hwAttrs->refSource, hwAttrs->internalChannel, 1);
    ADCSetInput(hwAttrs->refSource, hwAttrs->internalChannel, 3);
    hwAttrs = adcBuf->hwAttrs;
    ADCSetInput(hwAttrs->refSource, hwAttrs->internalChannel, 2);

    // Configure ADC window comparator
    HWREG(ADC_BASE + ADC_O_MEMCTL0) |= ADC_MEMCTL0_WINCOMP;
    HWREG(ADC_BASE + ADC_O_MEMCTL1) |= ADC_MEMCTL1_WINCOMP;
    HWREG(ADC_BASE + ADC_O_WCHIGH) |= WINDOW_HIGH;

    /* Loop forever */
    while (1)
    {
        sem_wait(&sem);
        if(appTaskEvents & ACTION_CCLOCK)     // Counter-Clockwise
        {
            if(!action)
            {
                action |= 0x01;
                direction = 0;
                appTaskEvents |= ACTION_START;
            }
            appTaskEvents &= ~ACTION_CCLOCK;
        }
        if(appTaskEvents & ACTION_CLOCK)     // Clockwise
        {
            if(!action)
            {
                action |= 0x02;
                direction = 1;
                appTaskEvents |= ACTION_START;
            }
            appTaskEvents &= ~ACTION_CLOCK;
        }
        if(appTaskEvents & ACTION_START)    // Start
        {
            stepCount = 0;
            GPIO_write(CONFIG_GPIO_nSLEEP, 1);
            usleep(200);
            GPIO_clearInt(CONFIG_GPIO_nFAULT);
            GPIO_enableInt(CONFIG_GPIO_nFAULT);
            ClockP_start(motorClockHandle);
            ADCEnableInterrupt(ADC_INT_HIGHIFG);
            ADCBuf_convert(adcBuf, &continuousConversion, 1);
            lgptHandle = LGPTimerLPF3_open(CONFIG_LGPTIMER_0, &lgptParams);
            LGPTimerLPF3_setInitialCounterTarget(lgptHandle, 48000000/(STEP_FREQUENCY*ADC_PER_STEP)-1, true);
            LGPTimerLPF3_start(lgptHandle, LGPTimerLPF3_CTL_MODE_UP_PER);
            HWREG(LGPT3_BASE + LGPT_O_ADCTRG) |= LGPT_ADCTRG_SRC_TGT;
            appTaskEvents &= ~ACTION_START;
        }
        if(appTaskEvents & ACTION_STOP)     // Stop
        {
            if(action)
            {
                action = 0;
                GPIO_disableInt(CONFIG_GPIO_nFAULT);
                ClockP_stop(motorClockHandle);
                LGPTimerLPF3_stop(lgptHandle);
                LGPTimerLPF3_close(lgptHandle);
                ADCBuf_convertCancel(adcBuf);
                ADCDisableInterrupt(ADC_INT_HIGHIFG);
                resetMotor();
                GPIO_write(CONFIG_GPIO_nSLEEP, 0);
            }
            appTaskEvents &= ~ACTION_STOP;
        }
    }
}

void StepperMain(void)
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
void PowerCC23X0_customPolicy(void)
{
    uint32_t sysTimerCurrTime;
    sysTimerCurrTime = HWREG(SYSTIM_BASE + SYSTIM_O_TIME1U);
    PowerCC23X0_standbyPolicy();
    idleCount += (HWREG(SYSTIM_BASE + SYSTIM_O_TIME1U) - sysTimerCurrTime);
}