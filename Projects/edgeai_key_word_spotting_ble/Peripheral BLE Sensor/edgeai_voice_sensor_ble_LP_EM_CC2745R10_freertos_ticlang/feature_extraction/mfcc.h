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

#ifndef MFCC_H_
#define MFCC_H_

#include <stdint.h>


#define SR              16000
#define N_FFT           1024
#define WIN_LENGTH      1024
#define HOP_LENGTH      512
#define N_MELS          40
#define N_MFCC          13
#define FFT_BINS        (N_FFT/2 + 1)

/*!
 *  @brief    Specifies the FE status codes.
 *
 *  This enum defines the return states of the FE functions.
 *
 */
typedef enum
{
    /*!< Process ended successfully. */
    FE_OK            = 0x00,
    /*!< The pointer to one of the function arguments has a NULL value. */
    FE_ERR_NULLPTR   = 0x01,
    /*!< One of the Feature Extraction configuration parameters is incorrect or not supported. */
    FE_ERR_BAD_PARAM = 0x02,
    /*!< DSP operation has failed. */
    FE_ERR_DSP       = 0x03,
} fe_status_t;

/*!
 * @brief                       Compute MFCC for one full frame
 *
 * @param[in]  data_stream      pointer to one audio frame of size WIN_LENGTH.
 * @param[out] mfcc             pointer to a mfcc output buffer of size N_MFCC.
 *
 * @retval  FE_OK               Process ended successfully.
 * @retval  FE_ERR_NULLPTR      The pointer to one of the function arguments has a NULL value.
 * @retval  FE_ERR_BAD_PARAM    One of the Feature Extraction configuration parameters is incorrect or not supported.
 * @retval  FE_ERR_DSP          DSP operation has failed.
 *
 */

void mfcc_compute(const float *input, float *mfcc);

/*!
 * @brief                       Shift main buffer by 512 bytes, append
 *                              the new samples and compute the mfcc.
 *
 * @param[in]  input_frame      pointer to one audio frame of size HOP_LENGTH.
 * @param[out] mfcc_out         pointer to a mfcc output buffer of size N_MFCC.
 *
 * @retval  FE_OK               Process ended successfully.
 * @retval  FE_ERR_NULLPTR      The pointer to one of the function arguments has a NULL value.
 * @retval  FE_ERR_BAD_PARAM    One of the Feature Extraction configuration parameters is incorrect or not supported.
 * @retval  FE_ERR_DSP          DSP operation has failed.
 */
void mfcc_processFrame(const int16_t *input_frame, float *mfcc_out);

#endif
