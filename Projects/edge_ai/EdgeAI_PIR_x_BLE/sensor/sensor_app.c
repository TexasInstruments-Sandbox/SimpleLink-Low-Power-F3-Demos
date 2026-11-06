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

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Driver configuration */
#include "ti_drivers_config.h"

/* Sensor Application */
#include <sensor/sensor_app.h>

static Sensor_Context gSensorCtx;

/* Application Buffers */
uint8_t queueBuf[MessageQueueP_BUFFER_SIZE(QUEUE_BUFFER_SIZE * OVERSAMPLING_FACTOR, QUEUE_NO_ELEMENTS)];
static uint8_t inputBuf[QUEUE_BUFFER_SIZE * OVERSAMPLING_FACTOR];
static uint8_t tempBuf[QUEUE_BUFFER_SIZE];
static uint8_t procBuf[QUEUE_BUFFER_SIZE];

SemaphoreP_Struct adcsemaphore_struct;
SemaphoreP_Struct trigsemaphore_struct;
MessageQueueP_Struct queueObj;

/* Task objects */
static TaskP_Struct Sensor_Task;
static uint32_t Sensor_TaskStack[TASK_STACK_SIZE_WORDS];

/* Function forward declarations */
static void SensorThread(void *arg);

/* Timer Callback */
static void timerCallback(LGPTimerLPF3_Handle handle, LGPTimerLPF3_IntMask mask)
{
    SemaphoreP_post(gSensorCtx.adcSem);
}

float Sensor_swapEndianness(float value)
{
    /* Use a union to access the same bytes as both floats and uint32_t */
    union
    {
        float f;
        uint32_t i;
    } u;

    u.f = value;

    /* Swap byte order of the 32-bit integer */
    u.i = ((u.i & 0x000000FFU) << 24) | ((u.i & 0x0000FF00U) << 8) | ((u.i & 0x00FF0000U) >> 8) | ((u.i & 0xFF000000U) >> 24);

    return u.f;
}

/* Sensor Task Initialization */
void Sensor_init(void)
{

    Sensor_Context *ctx   = &gSensorCtx;
    ctx->state            = SENSOR_STATE_WAIT;
    ctx->collectData      = false;
    ctx->decimationFactor = OVERSAMPLING_FACTOR;

    /* Create ADC Buffer Msg Queue */
    ctx->queue = MessageQueueP_construct(&queueObj,
                                         (QUEUE_BUFFER_SIZE * OVERSAMPLING_FACTOR),
                                         QUEUE_NO_ELEMENTS,
                                         queueBuf);

    /* Create ADC Timer Semaphore */
    ctx->adcSem = SemaphoreP_constructBinary(&adcsemaphore_struct, 0);

    /* Create State Machine trigger semaphore */
    ctx->triggerSem = SemaphoreP_constructBinary(&trigsemaphore_struct, 0);

    /* Create ADC Task */
    TaskP_Params taskParams;
    TaskP_Params_init(&taskParams);
    taskParams.name      = "Sensor Task";
    taskParams.priority  = TASK_PRIORITY;
    taskParams.stackSize = TASK_STACK_SIZE_BYTES;
    taskParams.stack     = Sensor_TaskStack;
    TaskP_construct(&Sensor_Task, SensorThread, &taskParams);
}

/* ADC Control Start */
static void Sensor_ADC_start(Sensor_Context *ctx, uint32_t samplingfreq)
{

    ADC_Params adc_params;
    LGPTimerLPF3_Params lgpt_params;

    /* Configure ADC and Timer */
    ADC_init();
    ADC_Params_init(&adc_params);
    LGPTimerLPF3_Params_init(&lgpt_params);

    if (ctx->adcHandle == NULL)
    {
        ctx->adcHandle = ADC_open(CONFIG_ADC_0, &adc_params);
    }

    lgpt_params.hwiCallbackFxn = timerCallback;
    /* Set a fix clock prescaler for the ADC */
    lgpt_params.prescalerDiv   = 64;
    uint32_t timerClock    = (48000000) / lgpt_params.prescalerDiv;
    uint32_t counterTarget = (timerClock / (samplingfreq * ctx->decimationFactor)) - 1;

    /* Configure Timer with counter and start running */
    ctx->timerHandle = LGPTimerLPF3_open(CONFIG_LGPTIMER_0, &lgpt_params);
    LGPTimerLPF3_setInitialCounterTarget(ctx->timerHandle, counterTarget, true);
    LGPTimerLPF3_enableInterrupt(ctx->timerHandle, LGPTimerLPF3_INT_TGT);
    LGPTimerLPF3_start(ctx->timerHandle, LGPTimerLPF3_CTL_MODE_UP_PER);
}

/* ADC Control Stop */
static void Sensor_ADC_stop(Sensor_Context *ctx)
{

    LGPTimerLPF3_stop(ctx->timerHandle);
    LGPTimerLPF3_close(ctx->timerHandle);
}

/* Sensor Data Processing */
static void Sensor_Processing_downsample(uint8_t *input, uint8_t *output, uint8_t factor)
{
    
    int j = 0;
    for (int i = 0; i <= (QUEUE_BUFFER_SIZE * OVERSAMPLING_FACTOR) - factor; i += factor, j++)
    {
        uint32_t sum = 0;
        for (int k = 0; k < factor; k++)
        {
            sum += input[i + k];
        }
        output[j] = (uint8_t)(sum / factor);
    }
}


/* Application Public API to transfer sampled data */
uint8_t Sensor_getData(uint8_t *buffer, uint8_t start)
{

    Sensor_Context *ctx = &gSensorCtx;

    if (buffer == NULL)
    {
        return 0;
    }
    ctx->samplingFreq     = 34; // to get 33.3 Hz

    if (start==0)
    {
        ctx->collectData      = false;

    }
    else if (start==1)
    {
        SemaphoreP_post(ctx->triggerSem);
    }

    /* Wait until ADC buffer data is in the Queue */
    if (MessageQueueP_pend(ctx->queue, (void *)tempBuf, QUEUE_TIMEOUT) == MessageQueueP_OK)
    {
        memcpy(buffer, tempBuf, sizeof(tempBuf));
        return 1;
    }

    return 0;
}

/* Sensor State Handlers */
static void Sensor_handleWait(Sensor_Context *ctx)
{

    /* Wait until user requests data */
    SemaphoreP_pend(ctx->triggerSem, SemaphoreP_WAIT_FOREVER);
    ctx->state = SENSOR_STATE_START;
}

static void Sensor_handleStart(Sensor_Context *ctx)
{

    /* Configure the Timer and ADC drivers */
    Sensor_ADC_start(ctx, ctx->samplingFreq);
    ctx->collectData = true;
    ctx->state       = SENSOR_STATE_GETDATA;
}

static void Sensor_handleGetData(Sensor_Context *ctx)
{

    /* Get one buffer of ADC samples */
    for (int i = 0; i < (QUEUE_BUFFER_SIZE * OVERSAMPLING_FACTOR); i++)
    {
        if (!ctx->collectData)
        {
            ctx->state = SENSOR_STATE_STOP;
            return;
        }
        /* Wait until Timer has triggered a callback */
        SemaphoreP_pend(ctx->adcSem, SemaphoreP_WAIT_FOREVER);
        uint16_t sample = 0;
        /* Get one ADC sample */
        ADC_convert(ctx->adcHandle, &sample);
        /* Fill ADC buffer */
        inputBuf[i] = (uint8_t)sample;
    }

    if (ctx->collectData)
    {
        /* Downsample the ADC buffer */
        //Sensor_Processing_downsample(inputBuf, procBuf, ctx->decimationFactor);
        /* Send the ADC buffer data with a Queue */
        MessageQueueP_post(ctx->queue, inputBuf, MessageQueueP_WAIT_FOREVER);

    }
}

static void Sensor_handleStop(Sensor_Context *ctx)
{
    /* Stop Sampling ADC */
    ctx->collectData = false;
    Sensor_ADC_stop(ctx);
    ctx->state = SENSOR_STATE_WAIT;
}

/* ===== Sensor Task ===== */
static void SensorThread(void *arg)
{

    Sensor_Context *ctx = &gSensorCtx;

    while (1)
    {
        switch (ctx->state)
        {
            case SENSOR_STATE_WAIT:

                Sensor_handleWait(ctx);

                break;

            case SENSOR_STATE_START:

                Sensor_handleStart(ctx);

                break;

            case SENSOR_STATE_GETDATA:

                Sensor_handleGetData(ctx);

                break;

            case SENSOR_STATE_STOP:

                Sensor_handleStop(ctx);

                break;
        }
    }
}
