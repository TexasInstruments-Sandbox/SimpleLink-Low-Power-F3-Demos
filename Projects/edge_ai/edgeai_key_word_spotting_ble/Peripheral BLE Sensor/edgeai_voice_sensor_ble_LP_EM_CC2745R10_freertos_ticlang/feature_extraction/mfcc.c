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

#include <math.h>
#include <string.h>
#include "hann.h"
#include "mel_fb.h"
#include "dct_mat.h"
#include "mfcc.h"

#include <feature_extraction/cmsisdsp/Include/dsp/transform_functions.h>
#include <feature_extraction/cmsisdsp/Include/dsp/complex_math_functions.h>
#include <feature_extraction/cmsisdsp/Include/dsp/statistics_functions.h>

/* Internal Buffers */
static float temp[WIN_LENGTH];
static float frame_buffer[WIN_LENGTH];
static float fft_cmplx_out[N_FFT];
static float power[FFT_BINS];
static float mel_energy[N_MELS];
static float mel_db[N_MELS];

/* Same implementation of power_to_db as Librosa library */
static inline void power_to_db(const float *mel, float *mel_db)
{
    const float amin = 1e-10f;
    const float top_db = 80.0f;

    float maxv = -1e30f;

    for (int i = 0; i < N_MELS; i++) {
        float v = mel[i];
        if (v < amin) v = amin;
        float logv = 10.0f * log10f(v);
        mel_db[i] = logv;
        if (logv > maxv)
            maxv = logv;
    }

    float cutoff = maxv - top_db;
    for (int i = 0; i < N_MELS; i++) {
        if (mel_db[i] < cutoff)
            mel_db[i] = cutoff;
    }
}

/*!
 * @fn               mfcc_compute
 *
 * @brief            Compute MFCC for one full frame
 *
 */

void mfcc_compute(const float *input, float *mfcc)
{
    uint16_t count=0;

    /* Compute windowing with hann window */
    for (int i = 0; i < WIN_LENGTH; i++)
        temp[i] = input[i] * hann_window[i];

    /* Compute FFT */
    arm_rfft_fast_instance_f32 fft;
    arm_rfft_fast_init_f32(&fft, N_FFT);
    arm_rfft_fast_f32(&fft, temp, fft_cmplx_out, 0);

    /* Compute Power spectrum */
    arm_cmplx_mag_f32(fft_cmplx_out, power, FFT_BINS);

    /* Compute Mel Energy banks based on precomputed mel filterbank */
    for (int m = 0; m < N_MELS; m++) 
    {
        float sum = 0.f;
        for (int k = 0; k < FFT_BINS; k++)
            sum += mel_fb[m][k] * power[k];
        mel_energy[m] = sum;
    }

    /* dB conversion */
    power_to_db(mel_energy, mel_db);

    /* Compute DCT based on precomputed DCT bank */
    for (int c = 0; c < N_MFCC; c++) {
        float sum = 0.f;
        for (int m = 0; m < N_MELS; m++)
            sum += dct_mat[c][m] * mel_db[m];
        mfcc[c] = sum;
    }
}

/*!
 * @fn               mfcc_processFrame
 *
 * @brief            Shift main buffer by 512 bytes, append
 *                   the new samples and compute the mfcc.
 */

void mfcc_processFrame(const int16_t *input_frame, float *mfcc_out)
{
    /* shift HOP_LENGTH bytes left */
    memmove(frame_buffer, frame_buffer + HOP_LENGTH,
            (WIN_LENGTH - HOP_LENGTH) * sizeof(float));

    /* append new samples that are first normalized (ADC 12-bit) */
    for (int i = 0; i < HOP_LENGTH; i++)
        frame_buffer[WIN_LENGTH - HOP_LENGTH + i] = (float)input_frame[i] / 2048.0f;
        
    /* computed mfcc of one frame */
    mfcc_compute(frame_buffer, mfcc_out);
}