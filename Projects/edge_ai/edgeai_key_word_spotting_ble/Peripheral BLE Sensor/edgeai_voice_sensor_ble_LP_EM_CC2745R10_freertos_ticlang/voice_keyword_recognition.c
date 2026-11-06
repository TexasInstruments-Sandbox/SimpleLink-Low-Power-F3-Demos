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

/*
 *  ======== smart_detection_pir.c ========
 */

#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

/* Drivers configuration */
#include "ti_drivers_config.h"

/* TVM model */
#include <ai_artifacts/tvmgen_default.h>

/* Sensor Application */
#include <sensor_app/sensor_app.h>

/* Feature Extraction */
#include <feature_extraction/feature_extract_app.h>

/* BLE configuration */
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ti/ble/profiles/simple_gatt/simple_gatt_profile.h>

#define N_MAX_FRAMES        60
#define N_MFCC_INPUT        13
#define NUM_CLASSES         4

/* Model TVM expected input tensors */
static float voice_input[1][N_MAX_FRAMES][N_MFCC_INPUT];
static struct tvmgen_default_inputs nn_inputs   = {(void *)&voice_input[0][0][0]};

/* Model TVM expected output tensors for preduction results */
static float voice_output_class[1][NUM_CLASSES];
static struct tvmgen_default_outputs nn_outputs = {&voice_output_class[0]};

/* Application buffers */
uint16_t sensor_buffer[FRAME_BUFFSIZE];
volatile int16_t temp_buffer[FRAME_BUFFSIZE];
static float feature_buffer[N_MAX_FRAMES][N_MFCC_INPUT];

/* Feature Extraction parameters */
static FeatureExtract_Params featureExtractParams;
static uint16_t partial_count;

/* Sensor Inference Result Structure */
Sensor_InferenceResult voiceNetInferenceResult;

extern void InferenceNotify();

/* ======== mainThread ======== */
void *mainThread(void *arg0)
{
    /* Initialize the Sensor task */
    Sensor_init();

    while (1)
    {
        uint8_t start=0;

        /* Open Feature Extraction */
        FeatureExtract_config(&featureExtractParams);

        /* Process 1 second data from the sensor */
        while (partial_count<(ONESECVOICE_BUFFSIZE/FRAME_BUFFSIZE))
        {
            SimpleGattProfile_getParameter(SIMPLEGATTPROFILE_CHAR1, &start);
            /* Fetch a partial buffer from the sensor */
            if (Sensor_getData(temp_buffer, start))
            {
                /* Copy frame buffer */
                memcpy(sensor_buffer, temp_buffer, sizeof(temp_buffer));

                GPIO_write(CONFIG_GPIO_LED_RED, 1);
                /* Run Feature Extraction per Audio Frame */
                FeatureExtract_run(&featureExtractParams, sensor_buffer, sizeof(sensor_buffer), &feature_buffer[partial_count]);
                GPIO_write(CONFIG_GPIO_LED_RED, 0);

                partial_count++;
            }
        }

        /* Reset full buffer pointer count */
        partial_count=0;

        /* Copy concatenated features into the model's input tensor*/
        memcpy(voice_input, feature_buffer, sizeof(feature_buffer));

        GPIO_write(CONFIG_GPIO_LED_RED, 1);
        /* Run Inference */
        tvmgen_default_run(&nn_inputs, &nn_outputs);
        GPIO_write(CONFIG_GPIO_LED_RED, 0);

        // TARGET_LABELS = ["left", "right", "up", "down"]
        // 3.5 ms to process each frame
        // SW Inference: 11.5 ms
        // CDE Enabled Inference: ms
        /* Calculate the Label Class based on the inference result */
        voiceNetInferenceResult.inferred_class      = FeatureExtract_findClass(&voice_output_class[0], NUM_CLASSES);
        /* Get probability of each class */
        voiceNetInferenceResult.left_probability    = voice_output_class[0][0];
        voiceNetInferenceResult.right_probability   = voice_output_class[0][1];
        voiceNetInferenceResult.up_probability      = voice_output_class[0][2];
        voiceNetInferenceResult.down_probability    = voice_output_class[0][3];

        /* Transmit inference results using BLE */
        BLEAppUtil_invokeFunctionNoData(InferenceNotify);

    }
}