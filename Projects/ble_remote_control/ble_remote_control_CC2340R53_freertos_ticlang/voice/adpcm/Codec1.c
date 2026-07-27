/******************************************************************************

@file  Codec1.c

@brief This file contains the ADPCM codec functions.

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

 Copyright (c) 2022-2025, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************

 Copyright (c) 2022-2025, Texas Instruments Incorporated
 All rights reserved.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 ******************************************************************************
 *****************************************************************************/

/***********************************************************************************
* INCLUDES
*/
#include <stdint.h>
#include "Codec1.h"

/*********************************************************************
 * GLOBAL VARIABLES
 *
 * ADPCM State Variables:
 * - Codec1_pvEnc: Predicted value for encoder (tracks audio waveform)
 * - Codec1_pvDec: Predicted value for decoder (reconstructs audio)
 * - Codec1_siEnc: Step index for encoder (0-88, selects quantization step size)
 * - Codec1_siDec: Step index for decoder (0-88, mirrors encoder step index)
 *
 * These maintain the adaptive state between encode/decode calls.
 *********************************************************************/

static int16_t Codec1_pvEnc;  // Encoder predicted value (-32768 to 32767)
static int16_t Codec1_pvDec;  // Decoder predicted value (-32768 to 32767)
static int8_t Codec1_siEnc;   // Encoder step index (0-88)
static int8_t Codec1_siDec;   // Decoder step index (0-88)

/*********************************************************************
 * ADPCM STEP SIZE LOOKUP TABLE
 *
 * Table of 89 quantization step sizes used by the ADPCM algorithm.
 * The step size adapts based on the signal characteristics:
 * - Index 0 (step=7): Fine quantization for quiet signals
 * - Index 88 (step=32767): Coarse quantization for loud signals
 *
 * The step size determines how each 4-bit ADPCM nibble is mapped to a
 * 16-bit PCM difference value. Larger steps = more compression but less precision.
 *********************************************************************/
static const uint16_t Codec1_stepsizeLut[89] =
{
    7,    8,    9,   10,   11,   12,   13,   14, 16,   17,   19,   21,   23,   25,   28,   31,
    34,   37,   41,   45,   50,   55,   60,   66, 73,   80,   88,   97,  107,  118,  130,  143,
    157,  173,  190,  209,  230,  253,  279,  307, 337,  371,  408,  449,  494,  544,  598,  658,
    724,  796,  876,  963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,10442,11487,12635,13899,
    15289,16818,18500,20350,22385,24623,27086,29794, 32767
};

/*********************************************************************
 * ADPCM INDEX ADJUSTMENT LOOKUP TABLE
 *
 * Maps the 4-bit ADPCM code (0-15) to a step index adjustment (-1 to +8).
 * This controls how quickly the algorithm adapts to signal changes:
 *
 * Small codes (0-3, 8-11): Decrease step index by 1 (adapt to quieter signal)
 * Large codes (4-7, 12-15): Increase step index by 2,4,6,8 (adapt to louder signal)
 *
 * Bit 3 (0x8) represents the sign bit, so patterns repeat:
 * - Nibbles 0-7: Positive differences
 * - Nibbles 8-15: Negative differences (same magnitude adjustments)
 *
 * This adaptive behavior is what makes ADPCM "Adaptive" - it continuously
 * adjusts quantization to match signal dynamics.
 *********************************************************************/
static const int8_t Codec1_IndexLut[16] =
{
    -1, -1, -1, -1, 2, 4, 6, 8,  // Adjustments for nibbles 0-7 (positive)
    -1, -1, -1, -1, 2, 4, 6, 8   // Adjustments for nibbles 8-15 (negative)
};

/*********************************************************************
 * @fn      Codec1_encodeSingle
 *
 * @brief   Encode a single 16-bit PCM audio sample to 4-bit ADPCM
 *
 *          ADPCM Encoding Algorithm:
 *          1. Calculate difference: current sample - predicted value
 *          2. Encode sign bit (bit 3): 1 if negative, 0 if positive
 *          3. Quantize absolute difference to 3 bits (bits 2-0) using step size
 *          4. Update predicted value using quantized difference
 *          5. Adapt step index based on encoded nibble magnitude
 *
 * @param   audSample - 16-bit signed PCM audio sample (-32768 to 32767)
 *
 * @return  4-bit ADPCM code (0-15):
 *          Bit 3: Sign (1=negative, 0=positive)
 *          Bits 2-0: Quantized magnitude
 *
 * @note    This function maintains state in Codec1_pvEnc and Codec1_siEnc.
 *          Must call Codec1_initEncode() before first use.
 *********************************************************************/
uint8_t Codec1_encodeSingle(int16_t audSample)
{
    // Step 1: Calculate prediction error (difference to be encoded)
    int16_t difference = audSample - Codec1_pvEnc;

    // Step 2: Initialize output nibble (will build it bit by bit)
    uint8_t tic1_nibble_4bits = 0;

    // Step 3: Get current quantization step size from lookup table
    int16_t step = Codec1_stepsizeLut[Codec1_siEnc];

    // Step 4: Initialize cumulative difference (used to update predicted value)
    // Start with step/8 as initial bias to reduce quantization error
    int16_t cum_diff = step>>3;

    // Step 5: Encode sign bit (bit 3)
    if(difference<0)
    {
        tic1_nibble_4bits = 8;     // Set bit 3 for negative
        difference = -difference;   // Work with absolute value
    }

    // Step 6: Quantize magnitude to 3 bits using successive approximation
    // This is a form of binary search to find the best 3-bit representation
    //
    // Bit 2: Is difference >= step? (represents step/1 = 100% of step)
    if(difference>=step)
    {
        tic1_nibble_4bits |= 4;    // Set bit 2
        difference -= step;         // Remove this portion from remaining difference
        cum_diff += step;           // Track quantized value for prediction update
    }

    // Bit 1: Is remaining difference >= step/2? (represents 50% of original step)
    step >>= 1;  // Divide step by 2
    if(difference>=step)
    {
        tic1_nibble_4bits |= 2;    // Set bit 1
        difference -= step;
        cum_diff += step;
    }

    // Bit 0: Is remaining difference >= step/4? (represents 25% of original step)
    step >>= 1;  // Divide step by 2 again (now step/4 of original)
    if(difference>=step)
    {
        tic1_nibble_4bits |= 1;    // Set bit 0
        cum_diff += step;
        // Note: No need to subtract here since we're done quantizing
    }

    // Step 7: Update predicted value using quantized difference
    // This prediction will be used as the baseline for the next sample
    if(tic1_nibble_4bits&8)  // Negative difference?
    {
        // Subtract with saturation to prevent underflow
        if (Codec1_pvEnc < (-32768+cum_diff))
            (Codec1_pvEnc) = -32768;
        else
            Codec1_pvEnc -= cum_diff;
    }
    else  // Positive difference
    {
        // Add with saturation to prevent overflow
        if (Codec1_pvEnc > (0x7fff-cum_diff))
            (Codec1_pvEnc) = 0x7fff;
        else
            Codec1_pvEnc += cum_diff;
    }

    // Step 8: Adapt step index based on encoded nibble magnitude
    // Large codes (+2 to +8) increase step for louder signals
    // Small codes (-1) decrease step for quieter signals
    Codec1_siEnc += Codec1_IndexLut[tic1_nibble_4bits];

    // Step 9: Clamp step index to valid range [0, 88]
    if(Codec1_siEnc<0)
        Codec1_siEnc = 0;
    else if(Codec1_siEnc>88)
        Codec1_siEnc = 88;

    // Return the 4-bit ADPCM code
    return tic1_nibble_4bits;
}


/*********************************************************************
 * @fn      Codec1_decodeSingle
 *
 * @brief   Decode a single 4-bit ADPCM code to 16-bit PCM audio sample
 *
 *          ADPCM Decoding Algorithm (mirror of encoding):
 *          1. Get current quantization step size
 *          2. Reconstruct difference from 3 magnitude bits
 *          3. Apply sign bit to determine direction
 *          4. Update predicted value (adds/subtracts reconstructed difference)
 *          5. Adapt step index for next sample
 *
 *          The decoder must maintain the EXACT same state as the encoder
 *          (same step index, same predicted value) to correctly reconstruct audio.
 *
 * @param   nibble_4bits - 4-bit ADPCM code (0-15):
 *                         Bit 3: Sign (1=negative, 0=positive)
 *                         Bits 2-0: Quantized magnitude
 *
 * @return  16-bit signed PCM audio sample (-32768 to 32767)
 *
 * @note    This function maintains state in Codec1_pvDec and Codec1_siDec.
 *          Must call Codec1_initDecode() before first use.
 *********************************************************************/
int16_t Codec1_decodeSingle(uint8_t nibble_4bits)
{
    // Step 1: Get current quantization step size (must match encoder)
    int16_t step = Codec1_stepsizeLut[Codec1_siDec];

    // Step 2: Initialize cumulative difference with step/8 bias
    // (matches encoder's initial bias for reduced quantization error)
    int16_t cum_diff  = step>>3;

    // Step 3: Adapt step index FIRST (before using current step)
    // This ensures decoder stays in sync with encoder's adaptation
    Codec1_siDec += Codec1_IndexLut[nibble_4bits];

    // Step 4: Clamp step index to valid range [0, 88]
    if(Codec1_siDec<0)
        Codec1_siDec = 0;
    else if(Codec1_siDec>88)
        Codec1_siDec = 88;

    // Step 5: Reconstruct difference from 3 magnitude bits
    // Bit 2: Add step (100% of step)
    if(nibble_4bits&4)
        cum_diff += step;

    // Bit 1: Add step/2 (50% of step)
    if(nibble_4bits&2)
        cum_diff += step>>1;

    // Bit 0: Add step/4 (25% of step)
    if(nibble_4bits&1)
        cum_diff += step>>2;

    // Step 6: Apply sign and update predicted value
    if(nibble_4bits&8)  // Bit 3 set = negative difference
    {
        // Subtract with saturation to prevent underflow
        if (Codec1_pvDec < (-32767+cum_diff))
            (Codec1_pvDec) = -32767;
        else
            Codec1_pvDec -= cum_diff;
    }
    else  // Bit 3 clear = positive difference
    {
        // Add with saturation to prevent overflow
        if (Codec1_pvDec > (0x7fff-cum_diff))
            (Codec1_pvDec) = 0x7fff;
        else
            Codec1_pvDec += cum_diff;
    }
    return Codec1_pvDec;
}


/*
*  ======== Codec1_encodeBuff ========
*/
uint8_t Codec1_encodeBuff(uint8_t* dst, int16_t* src, int16_t srcSize, int8_t *si, int16_t *pv)
{
    Codec1_pvEnc = *pv;
    Codec1_siEnc = *si;
    int16_t* end = (src + srcSize);
    int8_t len = 0;

    while(src < end)
    {
        // encode a pcm value from input buffer
        uint8_t nibble = Codec1_encodeSingle( *src++ );

        nibble |= (Codec1_encodeSingle( *src++ ) << 4);

        *dst++ = nibble;

        len++;
    }

    *pv = Codec1_pvEnc;
    *si = Codec1_siEnc;
    return len;
}


/*
*  ======== Codec1_decodeBuff ========
*/
void Codec1_decodeBuff(int16_t* dst, uint8_t* src, unsigned srcSize,  int8_t *si, int16_t *pv)
{
    Codec1_pvDec = *pv;
    Codec1_siDec = *si;

    // calculate pointers to iterate output buffer
    int16_t* out = dst;
    int16_t* end = out+(srcSize>>1);

    while(out<end)
    {
        // get byte from src
        uint8_t nibble = *src;
        *out++ = Codec1_decodeSingle((nibble&0xF));  // decode value and store it
        nibble >>= 4;  // use high nibble of byte
        nibble &= 0xF;  // use high nibble of byte
        *out++ = Codec1_decodeSingle(nibble);  // decode value and store it
        ++src;        // move on a byte for next sample
    }

    *pv = Codec1_pvDec;
    *si = Codec1_siDec;
}
