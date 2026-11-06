/*
 * Copyright (c) 2025-2026, Texas Instruments Incorporated
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
 *  ======== smart_detection_pir.c ========
 */

#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* EdgeAI configuration */
#include "ti_drivers_config.h"

/* TVM model */
#include <ai_artifacts/tvmgen_default.h>

/* Sensor Application */
#include <sensor/sensor_app.h>

/* Feature Extraction */
#include <feature_extraction/feature_extract_app.h>

/*BLE Stack Inclusion*/
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ti/ble/profiles/simple_gatt/simple_gatt_profile.h>

/* MACROS */
#define NUM_CLASSES                 3

/* GLOBALS */
extern void InferenceNotify();

/* Model TVM expected input tensors */
static float pir_input[1][1][NUM_WINDOWS][NUM_TOTAL_FEATURES];
static struct tvmgen_default_inputs nn_inputs   = {.onnx__Add_0 = (void *)&pir_input[0][0][0][0]};

/* Model TVM expected output tensors */
static float pir_output[1][NUM_CLASSES];
static struct tvmgen_default_outputs nn_outputs = {.output = &pir_output[0][0]};

/* Application buffers */
uint8_t sensor_buffer[QUEUE_BUFFER_SIZE];
static float feature_buffer[NUM_WINDOWS * NUM_TOTAL_FEATURES];

/* Feature Extraction parameters */
static FeatureExtract_Params featureExtractParams;

/* Sensor Inference Result Struct */
Sensor_InferenceResult pirNetInferenceResult;

/* Softmax output buffer */
static float softmax_probs[NUM_CLASSES];

/*!
 *  @brief  Apply softmax to convert logits to probabilities
 *
 *  @param[in]  logits  Input array of raw logits from the model
 *  @param[out] probs   Output array of softmax probabilities (0.0 to 1.0)
 *  @param[in]  size    Number of classes
 */
static void softmax(const float *logits, float *probs, uint8_t size)
{
    float max_logit = logits[0];
    float sum = 0.0f;

    /* Find max logit for numerical stability */
    for (uint8_t i = 1; i < size; i++)
    {
        if (logits[i] > max_logit)
        {
            max_logit = logits[i];
        }
    }

    /* Compute exp(logit - max) and sum */
    for (uint8_t i = 0; i < size; i++)
    {
        probs[i] = expf(logits[i] - max_logit);
        sum += probs[i];
    }

    /* Normalize to get probabilities */
    for (uint8_t i = 0; i < size; i++)
    {
        probs[i] /= sum;
    }
}

/* ======== mainThread ======== */
void *mainThread(void *arg0)
{
    /* Initialize the Sensor task */
    Sensor_init();

    while (1)
    {
        
        uint8_t start = 0; 
        
        SimpleGattProfile_getParameter(SIMPLEGATTPROFILE_CHAR1, &start); // Retrieve current value of GATT char from simple GATT Profile and store this value in the start variable
        
                if (Sensor_getData(sensor_buffer, start)) // If the buffer is full with sensor data, and start = 1, begin feature extraction
                {
                    /* Open Feature Extraction */
                    FeatureExtract_config(&featureExtractParams);

                    /* Run Feature Extraction */
                    FeatureExtract_run(&featureExtractParams, sensor_buffer, feature_buffer);

                    /* Copy concatenated features into the model's input tensor*/
                    memcpy(pir_input, feature_buffer, sizeof(feature_buffer));

                    /* Run Inference */
                    tvmgen_default_run(&nn_inputs, &nn_outputs);

                    /* Calculate the Label Class based on the inference result
                    */
                    pirNetInferenceResult.inferred_class = FeatureExtract_findClass(pir_output[0], NUM_CLASSES);
                    
                    /* Apply softmax to convert logits to probabilities */
                    softmax(pir_output[0], softmax_probs, NUM_CLASSES);

                    /* Get probability of each class (* 100 makes the range from 0%-100%) */
                    pirNetInferenceResult.human_probability = (uint8_t)(softmax_probs[0] * 100);
                    pirNetInferenceResult.background_probability = (uint8_t)(softmax_probs[1] * 100);
                    pirNetInferenceResult.dog_probability = (uint8_t)(softmax_probs[2] * 100);

                    /* Transmit inference results using BLE */
                    BLEAppUtil_invokeFunctionNoData(InferenceNotify); 
                }
    }
}