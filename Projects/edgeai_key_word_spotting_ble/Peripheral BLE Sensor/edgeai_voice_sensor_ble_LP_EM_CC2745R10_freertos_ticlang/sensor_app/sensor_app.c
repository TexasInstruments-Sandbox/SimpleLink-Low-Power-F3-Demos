/*
* Copyright (c) 2026, Texas Instruments Incorporated
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
#include <ti/drivers/ADCBuf.h>

/* Sensor Application */
#include <sensor_app/sensor_app.h>

static Sensor_Context gSensorCtx;
static uint8_t buf_count;

/* Application Buffers */
uint16_t queueBuf[MessageQueueP_BUFFER_SIZE(FRAME_BUFFSIZE, QUEUE_NO_ELEMENTS)];
static uint16_t sampleBufferOne[FRAME_BUFFSIZE];
static uint16_t sampleBufferTwo[FRAME_BUFFSIZE];
static uint16_t fullBuffer[FRAME_BUFFSIZE];
static uint16_t tempBuf[FRAME_BUFFSIZE];

SemaphoreP_Struct adcsemaphore_struct;
SemaphoreP_Struct trigsemaphore_struct;
MessageQueueP_Struct queueObj;

/* Task objects */
static TaskP_Struct Sensor_Task;
static uint32_t Sensor_TaskStack[TASK_STACK_SIZE_WORDS];

/* Function forward declarations */
static void SensorThread(void *arg);

/* ADCBuff Callback */
static void adcBufCallback(ADCBuf_Handle handle, ADCBuf_Conversion *conversion, void *completedADCBuffer, uint32_t completedChannel, int_fast16_t status)
{
    memcpy(fullBuffer, completedADCBuffer, sizeof(fullBuffer));
    SemaphoreP_post(gSensorCtx.adcSem);
    GPIO_write(CONFIG_GPIO_LED_GREEN, 1);
    GPIO_write(CONFIG_GPIO_LED_GREEN, 0);
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

    ADCBuf_Params adcBufParams;
    ADCBuf_Conversion continuousConversion;

    /* Configure ADCBuff */
    ADCBuf_init();
    //Configure & Open ADC Buffer driver
    adcBufParams.callbackFxn = adcBufCallback;
    adcBufParams.recurrenceMode = ADCBuf_RECURRENCE_MODE_CONTINUOUS;
    adcBufParams.returnMode = ADCBuf_RETURN_MODE_CALLBACK;
    adcBufParams.samplingFrequency = samplingfreq;

    //Configure the conversion struct
    continuousConversion.arg = NULL;
    continuousConversion.adcChannel = CONFIG_ADCBUF_0;
    continuousConversion.sampleBuffer = sampleBufferOne;
    continuousConversion.sampleBufferTwo = sampleBufferTwo;
    continuousConversion.samplesRequestedCount = FRAME_BUFFSIZE;

    if (ctx->adcHandle == NULL)
    {
        ctx->adcHandle = ADCBuf_open(CONFIG_ADCBUF_0, &adcBufParams);
        ADCBuf_convert(ctx->adcHandle, &continuousConversion, 1);
    }
}

/* ADC Control Stop */
static void Sensor_ADC_stop(Sensor_Context *ctx)
{
    ADCBuf_convertCancel(ctx->adcHandle);
    ADCBuf_close(ctx->adcHandle);
}

/* Application Public API to transfer sampled data */
uint8_t Sensor_getData(uint16_t *buffer, uint8_t start)
{

    Sensor_Context *ctx = &gSensorCtx;

    if (start==0)
    {
        ctx->collectData      = false;
        ctx->samplingFreq     = 16000; // to get 16KHz
        ctx->decimationFactor = OVERSAMPLING_FACTOR;
    }
    else if (start==1)
    {
        ctx->samplingFreq     = 16000; // to get 16KHz
        ctx->decimationFactor = OVERSAMPLING_FACTOR;
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
    if (!ctx->collectData)
    {
        ctx->state = SENSOR_STATE_STOP;
        return;
    }

    /* Wait until Timer has triggered a callback */
    SemaphoreP_pend(ctx->adcSem, SemaphoreP_WAIT_FOREVER);

    if (ctx->collectData)
    {
        MessageQueueP_post(ctx->queue, fullBuffer, MessageQueueP_WAIT_FOREVER);

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