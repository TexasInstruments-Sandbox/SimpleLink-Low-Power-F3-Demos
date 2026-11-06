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

#include <stdbool.h>
#include <stdint.h>
#include <ti/drivers/ADCBuf.h>
#include <ti/drivers/timer/LGPTimerLPF3.h>

/* RTOS */
#include <ti/drivers/dpl/SemaphoreP.h>
#include <ti/drivers/dpl/TaskP.h>
#include <ti/drivers/dpl/MessageQueueP.h>

#define ONESECVOICE_BUFFSIZE    16000
#define FRAME_BUFFSIZE          512

#define TASK_STACK_SIZE_BYTES   2048
#define TASK_STACK_SIZE_WORDS   (TASK_STACK_SIZE_BYTES >> 2)
#define TASK_PRIORITY           1
#define QUEUE_NO_ELEMENTS       4
#define OVERSAMPLING_FACTOR     1
#define QUEUE_BUFFER_SIZE       FRAME_BUFFSIZE*2
#define QUEUE_TIMEOUT           1000000

/* Sensor State */
typedef enum
{
    /*! Sensor task waits until user requests data. */
    SENSOR_STATE_WAIT,
    /*! Configure the Timer and ADC drivers. */
    SENSOR_STATE_START,
    /*! Get one buffer of ADC samples. */
    SENSOR_STATE_GETDATA,
    /*! Stop Sampling ADC */
    SENSOR_STATE_STOP,
} Sensor_State;

/* Sensor Context */
typedef struct
{
    /*! Driver Handles */
    ADCBuf_Handle adcHandle;

    /*! Semaphore and Queue */
    SemaphoreP_Handle adcSem;
    SemaphoreP_Handle triggerSem;
    MessageQueueP_Handle queue;

    /*! Sensor state and collect data state */
    Sensor_State state;
    bool collectData;

    /*! Sampling parameters */
    uint32_t samplingFreq;
    uint8_t decimationFactor;

} Sensor_Context;

/*! 
 *  PIR Model Inference result structure.
 *
 *  The __attribute__((packed)) is used so that
 *  the expected frame form by the DAP is respected.
 *
*/
typedef struct __attribute__((packed))
{
    uint8_t inferred_class;
    float left_probability;
    float right_probability;
    float up_probability;
    float down_probability;
    
} Sensor_InferenceResult;

/*!
 *  @brief  Function to initialize the Sensor Task.
 *
 */

void Sensor_init(void);

/*!
 *  @brief  Function to start the data acquisition state machine
 *          if DAP is ready to start streaming.
 *
 *  @param[in]  buffer  A pointer to the buffer with the data
 *                      ready to be used by the application.
 *
 */

uint8_t Sensor_getData(uint16_t *buffer, uint8_t reset);

/*!
 *  @brief  Swap Endianess value to support the endianess format expected by
 *          EdgeAI Studio
 *
 *  @param[in]  value   Scalar float value to be modified.
 */

float Sensor_swapEndianness(float value);