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
#include <stdbool.h>

#include <semaphore.h>
#include <pthread.h>

/* Driver Header files */
#include <ti/log/Log.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/ADCBuf.h>
#include <ti/drivers/apps/Button.h>
#include "ti/drivers/timer/LGPTimerLPF3.h"
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/PWM.h>
#include <ti/drivers/NVS.h>
#include DeviceFamily_constructPath(inc/hw_systim.h)

/* Driver configuration */
#include "ti_drivers_config.h"

// ADC Definitions
#define ADCSAMPLESIZE       100         // Length of ADCBuf
#define ADCBUF_SAMPLING_FREQ 1000       // Sample ADCBuf 1 kHz
#define OCP_THRESHOLD       1500000      // Threshold for OCP condition, detected by VSENSE

// PWM Definitions
#define PWM_DUTY_INC        1           // Duty Cycle increase per cycle (in us)
#define PWM_PERIOD          100         // Duty Cycle period (in us -> 1000us = 1ms)
#define PWM_START_POINT     0           // Duty Cycle to ramp up from (zero is min)
#define PWM_END_POINT       100         // Duty Cycle to ramp up to (1000 is max)

// Event mask
#define ACTION_LOWER        0x01        // Counter-clockwise action
#define ACTION_RAISE        0x02        // Clockwise action
#define ACTION_STOP         0x04        // Stop the motor
#define ACTION_ADC          0x08        // ADC OCP check

// Other configurations
#define THREADSTACKSIZE     1024
#define NVS_SIG             0x5AA5      // Signature of NVS
#define EDGE_DELTA          20          // Delta before updating edge counter
#define USE_HALL                        // Determines whether Hall effect logic is used

// Timer configurations
#define STALL_TIMEOUT       1000000     // 1s timeout for no movement detected by hall effect sensor
#define MOVEMENT_TIME       3000000     // 3s timeout for total movement with no hall effect sensor

// ADC Variables
ADCBuf_Handle adcBufHandle;                     // ADCBuf Handle
ADCBuf_Params adcBufParams;                     // ADCBuf Params
ADCBuf_Conversion adcBufConversionContinuous;   // ADCBuf Conversion set to Continuous

uint16_t sampleBufferOne[ADCSAMPLESIZE];     // Buffer 1 for ADCBUF ping pong
uint16_t sampleBufferTwo[ADCSAMPLESIZE];     // Buffer 2 for ADCBUF ping pong
uint32_t microVoltBuffer[ADCSAMPLESIZE];     // Buffer of voltages in uV

int32_t buffersCompletedCounter = 0;         // Count of buffers completed
uint8_t ocpFaultCounter = 0;                 // Count of OCP faults
volatile uint32_t adcBufAvg;                 // Average of microVoltBuffer for smoothing of data

// PWM Variables
uint8_t pwmDutyValue = 0;                  // Duty cycle of variable, init to 0
PWM_Handle pwmHandleForward;                // Handle for forward drive (pwmHandleForward)
PWM_Handle pwmHandleReverse;                // Handle for reverse drive (pwmHandleReverse)
PWM_Params pwmParams;                       // Params for PWM

// Hall Variables
uint32_t bottomPoint = 49000;               // bottom most location of hall sensor.
uint32_t topPoint = 51000;                  // top most location of hall sensor.
uint32_t risingEdgeCounter = 50000;         // counter of hall sensor cycles - initialzed to middle
uint32_t targetEdgeCounter = 50000;         // target counter for hall sensor
uint32_t lastSavedEdge = 50000;             // last hall sensor counter value saved to NV
uint32_t liftPercent = 0;                   // percent of shade lift updated in Zigbee
uint32_t NVSbuffer[4];                      // buffer for shade counters save in NVS
bool shadeLowered = false;                  // true if shade is in lowered position - default to false
bool shadeMoving = false;                   // true if shade is moving - default to false

// ClockP Variables
ClockP_Params clkParams;                        // Params for ClockP
ClockP_Handle clkHandle;                        // Handle for ClockP
ClockP_Struct clkStruct;                        // Struct for ClockP
volatile bool oneMillisecondElapsed = false; // Boolean to determine whether the timer has elapsed

// Timeout ClockP Variables
ClockP_Handle timeoutClkHandle;           // Handle for timeout ClockP
ClockP_Struct timeoutClkStruct;           // Struct for timeout ClockP
volatile bool timeoutOccurred = false;    // Boolean to determine whether timeout has occurred

// Button Variables
Button_Params buttonParams;               // Params for Buttons
Button_Handle buttonHandle[2];            // Handles for Buttons

NVS_Handle nvsHandle;                     // Handle for application NVS
NVS_Attrs regionAttrs;                    // Attributes for application NVS
NVS_Params nvsParams;                     // Params for application NVS

uint8_t appTaskEvents;                    // Application Task Events
sem_t sem;                                // Semaphore for event handling
uint32_t idleCount;                       // Idle count to measure CPU load
volatile bool longpressed = false;        // Whether buttons long pressed or not

extern void updateWindowPosition(uint8_t lift);

/*
 * Function used to update NVS hall countervalues
 */
#ifdef USE_HALL
void updateNVS(void)
{
  NVSbuffer[0] = NVS_SIG;
  NVSbuffer[1] = bottomPoint;
  NVSbuffer[2] = topPoint;
  NVSbuffer[3] = risingEdgeCounter;

  NVS_write(nvsHandle, 0, (void *)NVSbuffer, sizeof(NVSbuffer), NVS_WRITE_ERASE | NVS_WRITE_POST_VERIFY);
}
#endif

/*
 * ClockP callback fxn for 1 ms timer
 */
void oneMillisecondCallback(void *arg)
{
  oneMillisecondElapsed = true;
}

/*
 * ClockP timer for 1 ms
 */
void startOneMilliSecondTimer()
{
  oneMillisecondElapsed = false;
  ClockP_start(clkHandle);
}

/*
 * ClockP callback fxn for timeout timer
 */
void timeoutCallback(void *arg)
{
  appTaskEvents |= ACTION_STOP;
  sem_post(&sem);
  timeoutOccurred = true;
  Log_printf(LogModule_Zigbee_App, Log_ERROR, "Timeout occurred during shade operation");
}

/*
 * PWM_setup function - sets up and starts device PWM for forward and reverse drive
 */
void PWM_setup(void)
{
  // PWM for Forward Drive (pin DIO5)
  pwmHandleForward = NULL;
  PWM_Params_init(&pwmParams);
  pwmParams.dutyUnits   = PWM_DUTY_US;
  pwmParams.dutyValue   = 0;
  pwmParams.periodUnits = PWM_PERIOD_US;
  pwmParams.periodValue = PWM_PERIOD;

  // PWM for Reverse Drive (pin DIO1)
  pwmHandleReverse = NULL;

}

/*
 * This function is called whenever an ADC buffer is full.
 * The content of the buffer is then converted into human-readable format and
 * sent to the main thread.
 */
void adcBufCallback(ADCBuf_Handle handle,
                    ADCBuf_Conversion *conversion,
                    void *completedADCBuffer,
                    uint32_t completedChannel,
                    int_fast16_t status)
{

  /* Adjust raw ADC values and convert them to microvolts */
  ADCBuf_adjustRawValues(handle, completedADCBuffer, ADCSAMPLESIZE, completedChannel);
  ADCBuf_convertAdjustedToMicroVolts(handle, completedChannel, completedADCBuffer, microVoltBuffer, ADCSAMPLESIZE);

  buffersCompletedCounter++;
  appTaskEvents |= ACTION_ADC;
  sem_post(&sem);
}

void ADCBuf_setup(void)
{
  /* Set up an ADCBuf peripheral in ADCBuf_RECURRENCE_MODE_ONE_SHOT */
  ADCBuf_Params_init(&adcBufParams);
  adcBufParams.callbackFxn       = adcBufCallback;
  adcBufParams.recurrenceMode    = ADCBuf_RECURRENCE_MODE_CONTINUOUS;
  adcBufParams.returnMode        = ADCBuf_RETURN_MODE_CALLBACK;
  adcBufParams.samplingFrequency = ADCBUF_SAMPLING_FREQ;

  Log_printf(LogModule_Zigbee_App,Log_INFO,"ADC Params Initialized");

  /* Configure the conversion struct */
  adcBufConversionContinuous.arg                   = NULL;
  adcBufConversionContinuous.adcChannel            = CONFIG_ADCBUF_CHANNEL_0;
  adcBufConversionContinuous.sampleBuffer          = sampleBufferOne;
  adcBufConversionContinuous.sampleBufferTwo       = sampleBufferTwo;
  adcBufConversionContinuous.samplesRequestedCount = ADCSAMPLESIZE;

  Log_printf(LogModule_Zigbee_App,Log_INFO,"Continuous Conversion Initialized");
}

void ADCBuf_start(void)
{
  adcBufHandle = ADCBuf_open(CONFIG_ADCBUF_0, &adcBufParams);
  if (adcBufHandle == NULL)
  {
      /* ADCBuf failed to open. */
      while (1) {
        GPIO_toggle(CONFIG_GPIO_GLED);
        sleep(1);
      }
  }

  /* Start converting. */
  if (ADCBuf_convert(adcBufHandle, &adcBufConversionContinuous, 1) != ADCBuf_STATUS_SUCCESS)
  {
      /* Did not start conversion process correctly. */
      while (1) {
          GPIO_toggle(CONFIG_GPIO_RLED);
          sleep(1);
      }
  }
}

/*********************************************************************
 * @fn      accelerate
 *
 * @brief   Called when evaluateNewCharacteristicValue is called and the right value is input
 *
 * @param   pwmX    the pwm handle (pwmHandleForward or pwmHandleReverse) to use, which corresponds to forward or reverse drive
 * @param   start   the Duty value to accelerate from
 * @param   end     the Duty value to accelerate to
 *
 * @return  None.
 */
void accelerate(PWM_Handle pwmX, uint16_t start, uint16_t end)
{
  PWM_start(pwmX);
  pwmDutyValue = start;
  while(pwmDutyValue <= end) {
    if (oneMillisecondElapsed) {
      PWM_setDuty(pwmX, pwmDutyValue);
      if (pwmDutyValue >= PWM_PERIOD) { break; }
      pwmDutyValue += PWM_DUTY_INC;
      startOneMilliSecondTimer();
    }
  }
}

/*********************************************************************
 * @fn      decelerate
 *
 * @brief   Called when evaluateNewCharacteristicValue is called and the right value is input
 *
 * @param   pwmX    the pwm handle (pwmHandleForward or pwmHandleReverse) to use, which corresponds to forward or reverse drive
 * @param   start   the pwmDutyValue value to decelerate from
 * @param   end     the pwmDutyValue value to decelerate to
 *
 * @return  None.
 */
void decelerate(PWM_Handle pwmX, uint16_t start, uint16_t end)
{
  pwmDutyValue = start;
  while(pwmDutyValue >= end) {
    if (oneMillisecondElapsed) {
      PWM_setDuty(pwmX, pwmDutyValue);
      if (pwmDutyValue <= 0) { break; }
      pwmDutyValue -= PWM_DUTY_INC;
      startOneMilliSecondTimer();
    }
  }
  PWM_stop(pwmX);
}

/*
 * This function is called there is a rising edge on hall effect GPIO
 * It either increments or decrements based on direction of motor
 * drive to keep track of motor position
 *
 * Uses interrupts
 */
#ifdef USE_HALL
void gpioCallbackFxnHallSensor(uint_least8_t index)
{
  if(shadeLowered) 
  {
    risingEdgeCounter--;
    if(!longpressed && risingEdgeCounter <= targetEdgeCounter)
    {
      appTaskEvents |= ACTION_STOP;
      sem_post(&sem);
    }
  } 
  else {
    risingEdgeCounter++;
    if(!longpressed && risingEdgeCounter >= targetEdgeCounter)
    {
      appTaskEvents |= ACTION_STOP;
      sem_post(&sem);
    }
  }
  Log_printf(LogModule_Zigbee_App, Log_DEBUG, "Edge Count: %i", risingEdgeCounter);
  if((lastSavedEdge >= risingEdgeCounter + EDGE_DELTA) || (lastSavedEdge <= risingEdgeCounter - EDGE_DELTA))
  {
    updateNVS();
    lastSavedEdge = risingEdgeCounter;
    if(shadeMoving) ClockP_start(timeoutClkHandle); 
  }
}
#endif

/*
 * This function is called by the Zigbee state machine
 * or through the Button TI Driver
 *
 * It determines the motor direction accordingly
 */
void motorCommand(uint8_t percentLift)
{
  targetEdgeCounter = ((topPoint - bottomPoint)*(100-percentLift)/100) + bottomPoint;
#ifdef USE_HALL
  if(risingEdgeCounter < (targetEdgeCounter-EDGE_DELTA)) appTaskEvents |= ACTION_RAISE;
  else if(risingEdgeCounter > (targetEdgeCounter+EDGE_DELTA)) appTaskEvents |= ACTION_LOWER;
#else
  if(!percentLift) appTaskEvents |= ACTION_RAISE;
  else appTaskEvents |= ACTION_LOWER;
  liftPercent = percentLift;
#endif

  timeoutOccurred = false;
  ClockP_start(timeoutClkHandle);   

  sem_post(&sem);
}

/*
 * This button callback services several different events
 * and determines the correct action accordingly
 *
 */
void handleButtonCallback(Button_Handle handle, Button_EventMask events)
{
    static uint8_t buttonIndex = 0;
    // Figure out which button caused the callback
    for (int i = 0; i < CONFIG_TI_DRIVERS_BUTTON_COUNT; i++)
    {
        if (buttonHandle[i] == handle) buttonIndex = i;
    }

    if (events & Button_EV_CLICKED)
    {
      if(!buttonIndex) motorCommand(0);
      else motorCommand(100);
    }
    if (events & Button_EV_DOUBLECLICKED)
    {
#ifdef USE_HALL
      if(!buttonIndex) topPoint = risingEdgeCounter;
      else bottomPoint = risingEdgeCounter;
      updateNVS();
#endif
    }

    if (events & Button_EV_LONGPRESSED)
    {
      longpressed = true;
      if(!buttonIndex) appTaskEvents |= ACTION_RAISE;
      else appTaskEvents |= ACTION_LOWER;
      
      sem_post(&sem);
    }
    if (events & Button_EV_RELEASED)
    {
      if(longpressed)
      {
        longpressed = false;
        appTaskEvents |= ACTION_STOP;
        sem_post(&sem);
      }
    }
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

    /* Call ClockP init functions */
    ClockP_Params_init(&clkParams);
    clkParams.period = 0;
    clkParams.startFlag = true;
    ClockP_construct(&clkStruct, (ClockP_Fxn) oneMillisecondCallback, 1000, &clkParams);
    clkHandle = ClockP_handle(&clkStruct);

#ifdef USE_HALL
    ClockP_construct(&timeoutClkStruct, (ClockP_Fxn) timeoutCallback, STALL_TIMEOUT, &clkParams);
#else
    ClockP_construct(&timeoutClkStruct, (ClockP_Fxn) timeoutCallback, MOVEMENT_TIME, &clkParams);
#endif
    timeoutClkHandle = ClockP_handle(&timeoutClkStruct);

    /* Call PWM init functions */
    PWM_init();
    PWM_setup();
    Log_printf(LogModule_Zigbee_App,Log_INFO,"PWM Initialized");

    /* Call GPIO init functions */
    GPIO_init();
    __enable_irq();
    Log_printf(LogModule_Zigbee_App,Log_INFO,"GPIO Initialized");

    /* Call Hall sensor init functions */
#ifdef USE_HALL
    GPIO_setConfig(CONFIG_GPIO_HALL_SENSOR, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_RISING);
    GPIO_setCallback(CONFIG_GPIO_HALL_SENSOR, gpioCallbackFxnHallSensor);
    GPIO_enableInt(CONFIG_GPIO_HALL_SENSOR);
    Log_printf(LogModule_Zigbee_App,Log_INFO,"Hall Sensor Initialized");
#endif

    /* Call Button init functions */
    Button_Params_init(&buttonParams);
    buttonParams.longPressDuration = 500;     //500 ms
    buttonParams.doublePressDetectiontimeout = 1000;     //1000 ms
    buttonParams.buttonEventMask = Button_EV_CLICKED | Button_EV_DOUBLECLICKED | Button_EV_LONGPRESSED | Button_EV_RELEASED;
    buttonParams.buttonCallback   = handleButtonCallback;
    buttonHandle[0] = Button_open(CONFIG_BUTTON_1, &buttonParams);
    buttonHandle[1] = Button_open(CONFIG_BUTTON_2, &buttonParams);

#ifdef USE_HALL
    /* Call NVS init functions */
    NVS_init();
    NVS_Params_init(&nvsParams);
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_APP, &nvsParams);

    NVS_read(nvsHandle, 0, (void *)NVSbuffer, sizeof(NVSbuffer));

    if (NVSbuffer[0] != NVS_SIG)
    {
      updateNVS();
    }
    else 
    {
      bottomPoint = NVSbuffer[1];
      topPoint = NVSbuffer[2];
      risingEdgeCounter = NVSbuffer[3];
    }
#endif

    /* Call ADC init functions */
    ADCBuf_init();
    ADCBuf_setup();
    Log_printf(LogModule_Zigbee_App,Log_INFO,"ADC Initialized");

    /* Loop forever */
    while (1)
    {
        sem_wait(&sem);
        if(appTaskEvents & ACTION_ADC)
        {
            /* Average buffer values */
            adcBufAvg = 0;
            for(uint8_t i = 0; i < ADCSAMPLESIZE; i++) {
              adcBufAvg += microVoltBuffer[i];
            }
            adcBufAvg /= ADCSAMPLESIZE;
            if(adcBufAvg >= OCP_THRESHOLD) 
            {
              /* Stop motor immediately if average is above threshold */
              ocpFaultCounter++;
              appTaskEvents |= ACTION_STOP;
            }
            appTaskEvents &= ~ACTION_ADC;
        }
        if(appTaskEvents & ACTION_RAISE)     // Raise
        {
            if(!shadeMoving)
            {
                shadeMoving = 1;
                shadeLowered = 0;
                Log_printf(LogModule_Zigbee_App, Log_INFO,"Window Opening");
                GPIO_write(CONFIG_GPIO_HALL_PWR, 1);                 // powers Hall-effect sensor
                ADCBuf_start();                                                   // opens ADCBuf
                pwmHandleForward = PWM_open(CONFIG_PWM_0, &pwmParams);     // opens PWM
                accelerate(pwmHandleForward,PWM_START_POINT, PWM_END_POINT);          // starts PWM and accelerates to desired PWM value
            }
            appTaskEvents &= ~ACTION_RAISE;
        }
        if(appTaskEvents & ACTION_LOWER)     // Lower
        {
            if(!shadeMoving)
            {
                shadeMoving = 1;
                shadeLowered = 1;
                Log_printf(LogModule_Zigbee_App, Log_INFO,"Window Closing");
                GPIO_write(CONFIG_GPIO_HALL_PWR, 1);                 // powers Hall-effect sensor
                ADCBuf_start();                                                   // opens ADCBuf
                pwmHandleReverse = PWM_open(CONFIG_PWM_1, &pwmParams);     // opens PWM
                accelerate(pwmHandleReverse, PWM_START_POINT, PWM_END_POINT);         // starts PWM and accelerates to desired PWM value
            }
            appTaskEvents &= ~ACTION_LOWER;
        }
        if(appTaskEvents & ACTION_STOP)     // Stop
        {
            if(shadeMoving)
            {
                shadeMoving = 0;
                if(ClockP_isActive(timeoutClkHandle)) ClockP_stop(timeoutClkHandle);                                // stop timeout timer early if end point is reached
                if(!shadeLowered)
                {
                    decelerate(pwmHandleForward, PWM_END_POINT, PWM_START_POINT);         // decelerates to a PWM duty value of 0 and stops PWM
                    PWM_close(pwmHandleForward);                                      // closes PWM
                    if (timeoutOccurred) {                                            // update counter if timeout reached
                    Log_printf(LogModule_Zigbee_App, Log_ERROR, "Raise shade operation terminated due to timeout");
                    }
                }
                else
                {
                    decelerate(pwmHandleReverse, PWM_END_POINT, PWM_START_POINT);         // decelerates to a PWM duty value of 0 and stops PWM
                    PWM_close(pwmHandleReverse);                                      // closes PWM
                    if (timeoutOccurred) {                                            // update counter if timeout reached
                    Log_printf(LogModule_Zigbee_App, Log_ERROR, "Lower shade operation terminated due to timeout");
                    }
                }
                ADCBuf_convertCancel(adcBufHandle);                               // cancels ADCBuf conversion
                ADCBuf_close(adcBufHandle);                                       // closes ADCBuf
                GPIO_write(CONFIG_GPIO_HALL_PWR, 0);                        // unpowers Hall-effect sensor
#ifdef USE_HALL
                if(risingEdgeCounter > topPoint) liftPercent = 0;
                else if(risingEdgeCounter < bottomPoint) liftPercent = 100;
                else liftPercent = 100-((risingEdgeCounter - bottomPoint)*100/(topPoint - bottomPoint));
#endif
                updateWindowPosition((uint8_t)liftPercent);                       // Sends position update to Zigbee Window Covering ZCL
            }
            appTaskEvents &= ~ACTION_STOP;
        }
    }
}

void BDCMain(void)
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
