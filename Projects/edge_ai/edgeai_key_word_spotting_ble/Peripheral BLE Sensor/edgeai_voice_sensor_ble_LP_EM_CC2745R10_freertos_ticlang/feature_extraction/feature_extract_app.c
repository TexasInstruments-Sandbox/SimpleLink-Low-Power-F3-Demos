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
#include "feature_extract_app.h"

/*!
 *  Function to initialize FeatureExtract_Params based on a Model type
 */

fe_status_t FeatureExtract_config(FeatureExtract_Params *params)
{

    /* Initialize FeatureExtract_PIR_Params with values coming from DAP. */
    params->model_type = KEYWORD_Model;

    return FE_OK;
}

/*!
 *  Function to execute a sequence of Feature Extraction functions for
 *  a certain model type.
 */

fe_status_t FeatureExtract_run(FeatureExtract_Params *params, uint16_t *input_stream, uint32_t input_size, float *output_stream)
{

    fe_status_t status = FE_ERR_BAD_PARAM;

    if (params->model_type == KEYWORD_Model)
    {
       mfcc_processFrame(input_stream, output_stream);
    }

    return status;
}


uint8_t FeatureExtract_findClass(float *input, uint8_t size)
{
    
    float maxValue = input[0];
    uint8_t maxIndex = 0;

    if ((input[0] == input[1]) && (input[2] == input[3]) && (input[1] == input[2]))
    {
        return size;
    }

    for (int i = 1; i < size; ++i)
    {
        if (input[i] > maxValue)
        {
            maxValue = input[i];
            maxIndex = i;
        }
    }

    return maxIndex;
}