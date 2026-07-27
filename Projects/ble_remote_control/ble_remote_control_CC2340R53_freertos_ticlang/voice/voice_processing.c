/******************************************************************************

@file  voice_processing.c

@brief Audio capture and codec processing for Google Voice over BLE v1.1

This file implements audio capture using either Analog Microphone (AMIC) or
Digital Microphone (DMIC) with codec compression for BLE audio streaming.

Audio Capture Architectures:

1. AMIC (Analog Microphone):
   - ADC-based sampling at 16kHz (or 128kHz with 8x oversampling)
   - ADCBuf driver with continuous DMA and ping-pong buffers
   - Optional 8x oversampling for +9dB SNR improvement (1.5 extra bits resolution)
   - CIC filter (4th order) for decimation when oversampling enabled
   - Direct 16-bit PCM output

2. DMIC (Digital Microphone):
   - PDM (Pulse Density Modulation) interface via SPI at 1.024 MHz (or 3 MHz)
   - 6-buffer circular queue with callback-driven processing
   - pdm2pcm16k decimation with biquad filtering for droop compensation
   - Converts 1-bit PDM to 16-bit PCM at 16kHz

Codec Support (per Google Voice over BLE v1.1):
- ADPCM (Adaptive Differential PCM):
  * 8kHz or 16kHz sample rate
  * 4:1 compression ratio (16-bit PCM → 4-bit ADPCM)
  * Frame size: 32 samples (16 bytes compressed) or 160 samples (80 bytes)
  * Low latency, suitable for constrained BLE bandwidth

- mSBC (Modified Sub-Band Coding):
  * Wideband speech codec (higher quality than ADPCM)
  * 120 samples per frame → 57 bytes compressed
  * Better for high-bandwidth BLE connections

Signal Processing Pipeline (AMIC with Oversampling):
1. ADC samples at 128kHz (16kHz * 8) with DMA
2. Callback triggers on buffer completion (32 samples = 0.25ms @ 128kHz)
3. CIC filter decimates 8:1 → 32 samples @ 16kHz
4. Optional DC notch filter removes DC bias
5. ADPCM/mSBC encoder compresses PCM data
6. Compressed data sent over BLE via Application_DoAudioEvent()

Signal Processing Pipeline (DMIC):
1. SPI captures PDM data (1-bit @ 1.024 MHz) with DMA
2. Callback triggers on buffer completion (256 bytes = 2ms @ 1.024 MHz)
3. pdm2pcm16k decimates PDM → 32 samples @ 16kHz (with biquad filtering)
4. PCM buffer accumulates until frame size reached (144 samples for mSBC)
5. ADPCM/mSBC encoder compresses PCM data
6. Compressed data sent over BLE via Application_DoAudioEvent()

Buffer Management:
- AMIC: Ping-pong buffer (sampleBufferOne/sampleBufferTwo) for continuous ADC
- DMIC: 6-buffer circular queue for continuous SPI transfers
- Both architectures use Application_DoAudioEvent() to notify app layer

Integration:
- Application layer (app_audio.c): Manages audio streaming lifecycle
- Voice interaction (voice_interaction.c): State machine for PTT/HTT/On-request
- BLE audio stream (app_audio_stream.c): GATT notifications to host
- Codec libraries: Codec1 (ADPCM) and Codec2 (mSBC)

Google ATV Spec Compliance: Voice over BLE v1.1

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

#ifdef USE_AMIC

/**** Include Libraries ****/

#include <string.h>

/* Include BLE Libraries */
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ti/drivers/utils/List.h>

/* Include Application Drivers */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/ADCBuf.h>
#include "voice/filter/digital_filter.h"
#include "voice_processing.h"
#include "ti_drivers_config.h"

/**** Define Constants ****/

#ifdef USE_ADPCM_CODEC
    #include <voice/adpcm/Codec1.h>
    #ifndef OVERSAMPLING
        #define ADCSAMPLESIZE           (32)
    #endif
    #ifdef OVERSAMPLING
        #define OVERSAMPLING_FACTOR      8
        #define ADCSAMPLESIZE           (32*OVERSAMPLING_FACTOR)
    #endif
#else
#ifdef USE_MSBC_CODEC
    #include <voice/sbc/Codec2.h>
    #define ADCSAMPLESIZE           (120)
#endif
#endif

#ifdef USE_MSBC_CODEC
int16_t pcmData[144];
uint8_t pcmDataIndex = 0;
#endif

/**** Define Globals ****/

ADCBuf_Handle adcBuf;
ADCBuf_Params adcBufParams;
ADCBuf_Conversion continuousConversion;
extern uint8_t bleStreamIsEnabled;

uint16_t sampleBufferOne[ADCSAMPLESIZE];
uint16_t sampleBufferTwo[ADCSAMPLESIZE];
int16_t fullBuffer[ADCSAMPLESIZE];

#ifdef USE_ANALOG_FILTER
int16_t finalbuffer[32];
int16_t filterdecimatedBuffer[32];
#endif

#ifdef OVERSAMPLING
#ifdef USE_ANALOG_FILTER
#define ORDER 4
int32_t integrator[ORDER] = {0};
int32_t comb[ORDER] = {0};
int16_t decimated_dcnotch_buffer[32];

#ifdef USE_ADPCM_CODEC
static uint8_t sequenceNumber = 0;
int8_t            si = 0;
int16_t           pv = 0;
#endif

/**** Define Local Functions ****/

extern void Application_DoAudioEvent();

/*********************************************************************
 * @fn      adcBufCallback
 *
 * @brief   ADC completion callback - triggers audio processing
 *
 * @param   handle - ADCBuf driver handle
 * @param   conversion - Pointer to conversion configuration
 * @param   completedADCBuffer - Pointer to completed buffer (ping or pong)
 * @param   completedChannel - ADC channel that completed
 * @param   status - Conversion status
 *
 * @return  None
 *
 * @note    Called by ADCBuf driver when a buffer completes sampling.
 *          Timing (without oversampling):
 *          - 32 samples @ 16kHz = 2ms per callback
 *          - Callback rate: 500 Hz
 *
 *          Timing (with 8x oversampling):
 *          - 256 samples @ 128kHz = 2ms per callback
 *          - After decimation: 32 samples @ 16kHz
 *          - Callback rate: 500 Hz (same as non-oversampled)
 *
 *          Ping-pong buffering: While one buffer is processed, ADC fills the other.
 *          Copies completed buffer to fullBuffer for codec processing.
 *          Invokes Application_DoAudioEvent() to trigger BLE transmission.
 *********************************************************************/
void adcBufCallback(ADCBuf_Handle handle, ADCBuf_Conversion *conversion, void *completedADCBuffer, uint32_t completedChannel, int_fast16_t status)
{
    /* Scale 12-bit ADC output to full 16-bit range */
    uint16_t* adcSamples = (uint16_t*)completedADCBuffer;

    for (int i = 0; i < ADCSAMPLESIZE; i++)
    {
        // Shift 12-bit ADC left by 4 to fill 16-bit range, then center around 0
        // Assuming ADC bias point is 2048 (mid-scale for 12-bit single-supply ADC)
        fullBuffer[i] = (int16_t)((adcSamples[i] << 4) - 32768);
    }

    BLEAppUtil_invokeFunctionNoData(Application_DoAudioEvent);
}

/*********************************************************************
 * @fn      Audio_init
 *
 * @brief   Initialize audio subsystem for AMIC capture
 *
 * @param   None
 *
 * @return  0 on success
 *
 * @note    Called once during system initialization before voice interaction starts.
 *          Initializes:
 *          - GPIO driver (for MIC power control)
 *          - ADCBuf driver (for continuous ADC sampling with DMA)
 *          - mSBC codec (if USE_MSBC_CODEC defined)
 *
 *          ADPCM codec initialization is deferred until Audio_enable()
 *          because ADPCM is stateless (no persistent state between sessions).
 *
 *          Integration: Called by Application_onSystemInitialized() in app_audio.c
 *********************************************************************/
uint8_t Audio_init(void)
{
    GPIO_init();
    ADCBuf_init();
    #ifdef USE_ADPCM_CODEC
    // Do nothing - ADPCM is stateless, no initialization needed
    #else
    #ifdef USE_MSBC_CODEC
    Codec2_init();
    #endif
    #endif

    return 0;
}

/*********************************************************************
 * @fn      Audio_enable
 *
 * @brief   Start audio capture with ADC+DMA and power on microphone
 *
 * @param   None
 *
 * @return  0 on success, 1 on ADC conversion start failure
 *
 * @note    Called when voice interaction starts (PTT/HTT/On-request).
 *
 *          Configuration:
 *          - Sampling frequency: 16kHz (or 128kHz with 8x oversampling)
 *          - Recurrence mode: Continuous (ping-pong buffers)
 *          - Return mode: Callback (adcBufCallback invoked on completion)
 *          - Buffer size: 32 samples (or 256 with oversampling)
 *
 *          Oversampling Benefits:
 *          - Factor k=8: Theoretical SNR improvement of 9dB (10*log10(8) = 9.03dB)
 *          - Effective resolution: 12-bit ADC + 1.5 extra bits = ~13.5 ENOB
 *          - Sampling frequency: 16kHz * 8 = 128kHz
 *          - Must use CIC filter to decimate back to 16kHz
 *
 *          Ping-pong Buffer Operation:
 *          - sampleBufferOne: First buffer (ADC writes while CPU processes Two)
 *          - sampleBufferTwo: Second buffer (ADC writes while CPU processes One)
 *          - Callback rate: 500Hz (32 samples @ 16kHz = 2ms per buffer)
 *          - Zero-copy operation: Callback receives pointer to completed buffer
 *
 *          Microphone Power:
 *          - CONFIG_GPIO_MIC set HIGH to enable analog microphone
 *          - Microphone requires ~100µs settling time after power-on
 *
 *          Integration: Called by Voice_startStream() in voice_interaction.c
 *********************************************************************/
uint8_t Audio_enable(void)
{
    uint8_t retVal = 0;
    #ifdef USE_ADPCM_CODEC
    // ADPCM is stateless, no re-initialization needed
    #else
    #ifdef USE_MSBC_CODEC
    Codec2_init();  // Re-initialize mSBC codec state
    #endif
    #endif

    GPIO_write(CONFIG_GPIO_MIC,1);  // Power on analog microphone
    GPIO_write(CONFIG_GPIO_MIC_DTM,1);

    /* Configure ADCBuf driver for continuous sampling with callback */
    adcBufParams.callbackFxn = adcBufCallback;
    adcBufParams.recurrenceMode = ADCBuf_RECURRENCE_MODE_CONTINUOUS;
    adcBufParams.returnMode = ADCBuf_RETURN_MODE_CALLBACK;
    #ifndef OVERSAMPLING
    adcBufParams.samplingFrequency = 16000;  // Standard 16kHz sampling
    #endif
    #ifdef OVERSAMPLING
    // Oversampling with Factor (k) = 8
    // - Theoretical SNR improvement: 9dB
    // - Effective extra resolution: 1.5 bits
    // - Oversampling frequency = 16kHz * 8 = 128kHz
    adcBufParams.samplingFrequency = 16000 * OVERSAMPLING_FACTOR;
    #endif

    adcBuf = ADCBuf_open(CONFIG_ADCBUF_0, &adcBufParams);

    /* Configure continuous conversion with ping-pong buffers */
    continuousConversion.arg = NULL;
    continuousConversion.adcChannel = CONFIG_ADCBUF_0;
    continuousConversion.sampleBuffer = sampleBufferOne;          // Ping buffer
    continuousConversion.sampleBufferTwo = sampleBufferTwo;       // Pong buffer
    continuousConversion.samplesRequestedCount = ADCSAMPLESIZE;   // 32 or 256 samples

    if (ADCBuf_convert(adcBuf, &continuousConversion, 1) != 0) {
            retVal = 1;  // ADC conversion start failed
    }

    return retVal;
}

/*********************************************************************
 * @fn      Audio_disable
 *
 * @brief   Stop audio capture and power down microphone
 *
 * @param   None
 *
 * @return  0 on success, 1 on ADC cancel failure
 *
 * @note    Called when voice interaction ends (button release, timeout, or error).
 *
 *          Shutdown Sequence:
 *          1. Cancel ongoing ADC conversion (stops DMA transfers)
 *          2. Close ADCBuf driver (releases resources)
 *          3. Power down microphone (CONFIG_GPIO_MIC set LOW)
 *
 *          Power Savings:
 *          - Analog microphone: ~1-2mA when powered
 *          - ADC+DMA: ~500µA when active
 *          - Total savings: ~1.5-2.5mA when disabled
 *
 *          Safety:
 *          - ADCBuf_convertCancel() is safe to call even if conversion not active
 *          - Microphone power-down does not require settling time
 *
 *          Integration: Called by Voice_stopStream() in voice_interaction.c
 *********************************************************************/
uint8_t Audio_disable(void)
{
    uint8_t retVal = 0;
    if (ADCBuf_convertCancel(adcBuf) != 0)
    {
        retVal = 1;  // ADC cancel failed (should never happen)
    }
    ADCBuf_close(adcBuf);           // Release ADCBuf driver resources
    GPIO_write(CONFIG_GPIO_MIC,0);  // Power down analog microphone
    GPIO_write(CONFIG_GPIO_MIC_DTM,0);

    return retVal;
}

/*********************************************************************
 * @fn      Audio_Decimate
 *
 * @brief   Decimate oversampled signal using CIC filter
 *
 * @param   input_buffer - Input buffer with oversampled data (128kHz)
 * @param   output_buffer - Output buffer for decimated data (16kHz)
 *
 * @return  Number of decimated samples produced (typically 32)
 *
 * @note    CIC (Cascaded Integrator-Comb) Filter Decimation:
 *
 *          Purpose: Reduce 128kHz oversampled data → 16kHz target sample rate
 *          Decimation factor: 8 (OVERSAMPLING_FACTOR)
 *          Filter order: 4 (ORDER)
 *
 *          CIC Filter Theory:
 *          - Integrator stage: Accumulates samples (low-pass filtering)
 *          - Decimation: Discard 7 out of 8 samples (keep every 8th)
 *          - Comb stage: Differentiator to remove DC and shape frequency response
 *          - Scale factor: 8^4 = 4096 (prevents overflow and normalizes output)
 *
 *          Advantages:
 *          - No floating-point math (uses only integer operations)
 *          - Low computational cost (no multiplications)
 *          - Good anti-aliasing filter characteristics
 *          - Minimal RAM usage (only 8 state variables: 4 integrators + 4 combs)
 *
 *          Input/Output:
 *          - Input: 256 samples @ 128kHz (2ms of audio)
 *          - Output: 32 samples @ 16kHz (2ms of audio)
 *          - Latency: <50µs processing time
 *
 *          Integration: Called by Audio_getCompressedBuffer() when OVERSAMPLING defined
 *********************************************************************/
uint8_t Audio_Decimate(int16_t* input_buffer, int16_t* output_buffer){

    int32_t decimated_counter = 0;

    #ifdef USE_CIC_FILTER
    uint8_t count = 0;
    // Scale Factor = OVERSAMPLING_FACTOR ^ ORDER = 8^4 = 4096
    // This prevents overflow in integrator stages and normalizes output
    int64_t scale_factor = 4096;

    for (int i = 0; i<ADCSAMPLESIZE; i++) {

       int32_t input_sample = (int32_t)input_buffer[i];

        /* Integrator Stage: Cascade 4 integrators (low-pass filtering) */
        integrator[0] += input_sample;
        for (int j = 1; j < ORDER; j++) {
             integrator[j] += integrator[j - 1];
        }

        /* Decimation by OVERSAMPLING_FACTOR (8)
         * Keep every 8th sample, discard 7 out of 8 */
        count++;
        if (count < OVERSAMPLING_FACTOR) {
           continue;  // Skip to next iteration (discard this sample)
        }

        count = 0;

        /* Comb Stage: Cascade 4 differentiators (shape frequency response) */
        int64_t temp = integrator[ORDER - 1];
        for (int j = 0; j < ORDER; j++) {
            int64_t comb_input = comb[j];
            comb[j] = temp;
            temp -= comb_input;  // Differentiate
        }

        /* Store output sample with scaling to prevent overflow */
        output_buffer[decimated_counter++] = (int16_t)(temp/scale_factor);
     }
    #endif

    return decimated_counter;
}
#endif

#endif

/*********************************************************************
 * @fn      Audio_getCompressedBuffer
 *
 * @brief   Compress audio signal using ADPCM or mSBC codec (AMIC)
 *
 * @param   encBuf - Output buffer for compressed audio data
 *
 * @return  Length of compressed data in bytes, or AUDIO_FAIL_NO_DATA
 *
 * @note    This function processes the audio captured in fullBuffer by adcBufCallback
 *          and compresses it using the selected codec.
 *
 *          Processing Pipeline (with oversampling + analog filter):
 *          1. fullBuffer: 256 samples @ 128kHz (from ADC)
 *          2. Audio_Decimate(): 256 samples → 32 samples @ 16kHz (CIC filter)
 *          3. decimated_dcnotch_buffer: 32 samples @ 16kHz
 *          4. Codec1_encodeBuff() or Codec2_encodeBuff(): Compress to encBuf
 *
 *          Processing Pipeline (without oversampling):
 *          1. fullBuffer: 32 samples @ 16kHz (from ADC)
 *          2. Codec1_encodeBuff() or Codec2_encodeBuff(): Compress to encBuf
 *
 *          ADPCM Codec (4:1 compression):
 *          - Input: 32 samples (16-bit PCM) = 64 bytes
 *          - Output: 16 bytes (4-bit per sample)
 *          - State: si (step index), pv (previous value) maintained across calls
 *          - Latency: ~20µs encoding time
 *
 *          mSBC Codec (higher quality):
 *          - Input: 120 samples (16-bit PCM) = 240 bytes
 *          - Output: 57 bytes
 *          - Compression ratio: ~4.2:1
 *          - Latency: ~100µs encoding time
 *
 *          Called By: Application_DoAudioEvent() in app_audio.c every 2ms
 *
 *          Return Values:
 *          - AUDIO_FAIL_NO_DATA: No data available (should never happen in AMIC mode)
 *          - 16: ADPCM frame size (typical)
 *          - 57: mSBC frame size (typical)
 *********************************************************************/
uint8_t Audio_getCompressedBuffer(uint8_t* encBuf)
{
    uint8_t dataLength = AUDIO_FAIL_NO_DATA;
    uint8_t decimated_size = 0;

    /* Processing Path 1: Using Digital Filter (CIC decimation + codec) */
    #ifdef USE_ANALOG_FILTER

    #ifdef OVERSAMPLING
    // Optional: DC notch filter to remove DC bias (currently commented out)
    //DC_notch(fullBuffer, ADCSAMPLESIZE);
    // Decimate 128kHz → 16kHz using CIC filter
    decimated_size = Audio_Decimate(fullBuffer, decimated_dcnotch_buffer);
    #else
    decimated_size = ADCSAMPLESIZE;  // No decimation needed (already 16kHz)
    #endif

    #ifdef USE_ADPCM_CODEC
    /* ADPCM Encode: 16-bit PCM → 4-bit ADPCM (4:1 compression) */
    #ifdef OVERSAMPLING
    dataLength = Codec1_encodeBuff(encBuf, decimated_dcnotch_buffer, decimated_size, &si, &pv);
    #endif
    #ifndef OVERSAMPLING
    dataLength = Codec1_encodeBuff(encBuf, fullBuffer, decimated_size, &si, &pv);
    #endif

    #endif
    #ifdef USE_MSBC_CODEC
    /* mSBC Encode: 16-bit PCM → mSBC compressed (~4.2:1 compression) */
    dataLength = Codec2_encodeBuff(encBuf, fullBuffer, AUDIO_ENC_FRAME_SIZE, AUDIO_PCM_FRAME_SIZE);
    #endif

    #endif // USE_ANALOG_FILTER

    /* Processing Path 2: Without Digital Filter (direct codec encoding) */
    #ifndef USE_ANALOG_FILTER
    #ifdef USE_ADPCM_CODEC
    /* ADPCM Encode: 16-bit PCM → 4-bit ADPCM (4:1 compression) */
    dataLength = Codec1_encodeBuff(encBuf, fullBuffer, ADCSAMPLESIZE, &si, &pv);
    #endif
    #ifdef USE_MSBC_CODEC
    /* mSBC Encode: 16-bit PCM → mSBC compressed (~4.2:1 compression) */
    dataLength = Codec2_encodeBuff(encBuf, fullBuffer, AUDIO_ENC_FRAME_SIZE, AUDIO_PCM_FRAME_SIZE);
    #endif

    #endif

    return dataLength;
}
#endif

#ifdef USE_DMIC

#include "voice_processing.h"
#include <ti/drivers/SPI.h>
#include <ti/drivers/utils/List.h>
#include "ti_drivers_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ti/drivers/GPIO.h>
#ifdef USE_ADPCM_CODEC
#include <voice/adpcm/Codec1.h>
#else
#ifdef USE_MSBC_CODEC
#include <sbc/Codec2.h>
#endif
#endif

#define SPI_NUMBUFS 6
#ifdef USE_BW_24KHz
#define SPI_BUFSIZE 256*3
#define PCM_BUF_SIZE 32*3
#else
#define SPI_BUFSIZE 256
#define PCM_BUF_SIZE 32
#endif



List_List pdmFilledSpiTransactionsList;

// SPI buffers hold PDM data (1-bit, 1 MHz)
uint8_t buf1[SPI_BUFSIZE];
uint8_t buf2[SPI_BUFSIZE];
uint8_t buf3[SPI_BUFSIZE];
uint8_t buf4[SPI_BUFSIZE];
uint8_t buf5[SPI_BUFSIZE];
uint8_t buf6[SPI_BUFSIZE];

uint8_t* spiBufTable[SPI_NUMBUFS] = {buf1, buf2, buf3, buf4, buf5, buf6};

SPI_Transaction spiTransaction1;
SPI_Transaction spiTransaction2;
SPI_Transaction spiTransaction3;
SPI_Transaction spiTransaction4;
SPI_Transaction spiTransaction5;
SPI_Transaction spiTransaction6;
SPI_Transaction* spiTransactionTable[SPI_NUMBUFS] = {&spiTransaction1, &spiTransaction2, &spiTransaction3, &spiTransaction4, &spiTransaction5, &spiTransaction6};

SPI_Handle spiHandle;

extern void Application_DoAudioEvent();

extern uint8_t bleStreamIsEnabled;

// This is for the decimation filter (decimation from PDM data to PCM data)
#define FILTER_STATE_SIZE 16
int32_t filterState[FILTER_STATE_SIZE + 4];

// Example biquad coefficient definition
const int32_t pBqCoeffs[] = {
    // Halfband, DC notch, CIC droop compensation (2*Fs)
    689,   121,  1024,  -209,   919,    // First stage of correction filter
    407,   765,  1024,  -376,   470,    // Second stage of correction filter
    392,     0, -1024, -1265,   245,    // Third stage of correction filter
    450,  1345,  1024,  -495,   269,    // Fourth stage of correction filter
    507,   400,  1024,  -296,   690,    // Fifth stage of correction filter
    554,   209,  1024,  -240,   870,    // Sixth stage of correction filter
    0,                                  // Terminate correction filter
    // No EQ filter implemented (Fs)
    0                                   // Terminate EQ filter and coefficient list
};
#ifdef USE_ADPCM_CODEC
// The ADPCM buffers hold compressed PCM data (compression ratio is 4:1)
static uint8_t sequenceNumber = 0;
int8_t            si = 0;
int16_t           pv = 0;
// The PCM buffers hold PCM data (16-bit, 16 kHz)
int16_t pcmData[PCM_BUF_SIZE];
uint8_t pcmDataIndex = 0;
#endif

#ifdef USE_MSBC_CODEC
#ifdef USE_BW_24KHz
//int16_t pcmData[432];
int16_t pcmData[144];
#else
int16_t pcmData[144];
#endif
uint16_t pcmDataIndex = 0;
#endif

extern bool pdm2pcm16k(const void* pIn, int32_t* pState, int32_t* pBqCoeffs, int16_t* pOut);

/*********************************************************************
 * @fn      spiCallbackFxn
 *
 * @brief   SPI completion callback for PDM data capture (DMIC)
 *
 * @param   handle - SPI driver handle
 * @param   transaction - Pointer to completed SPI transaction
 *
 * @return  None
 *
 * @note    Called by SPI driver when a buffer completes receiving PDM data.
 *
 *          Timing:
 *          - 256 bytes PDM @ 1.024 MHz = 2ms per callback
 *          - Callback rate: 500 Hz (same as AMIC)
 *          - Each PDM byte contains 8 bits of 1-bit PDM samples
 *          - Total samples per buffer: 256 * 8 = 2048 PDM samples
 *
 *          Circular Buffer Management:
 *          - 6 SPI transactions in circular queue
 *          - Completed transaction added to pdmFilledSpiTransactionsList
 *          - Processing happens in Application_DoAudioEvent()
 *          - After processing, transaction requeued for next capture
 *
 *          PDM (Pulse Density Modulation):
 *          - 1-bit digital audio format from MEMS microphone
 *          - Sigma-delta modulation at 1.024 MHz
 *          - Requires decimation to 16kHz PCM (64:1 ratio)
 *          - Higher frequency = better SNR (oversampling)
 *
 *          Integration: Invokes Application_DoAudioEvent() to trigger
 *          PDM-to-PCM conversion and BLE transmission.
 *********************************************************************/
void spiCallbackFxn(SPI_Handle handle, SPI_Transaction *transaction)
{
    if(NULL != transaction)
    {
            List_put(&pdmFilledSpiTransactionsList, (List_Elem*)transaction);
            /* Trigger the treatment of the data */
            BLEAppUtil_invokeFunctionNoData(Application_DoAudioEvent);
    }
}

/*********************************************************************
 * @fn      Audio_init
 *
 * @brief   Initialize audio subsystem for DMIC capture
 *
 * @param   None
 *
 * @return  0 on success
 *
 * @note    Called once during system initialization before voice interaction starts.
 *          Initializes:
 *          - SPI driver (for PDM data capture from digital microphone)
 *          - mSBC codec (if USE_MSBC_CODEC defined)
 *
 *          ADPCM codec initialization is deferred until Audio_enable()
 *          because ADPCM is stateless (no persistent state between sessions).
 *
 *          DMIC Architecture:
 *          - Digital MEMS microphone outputs PDM data
 *          - SPI configured in slave mode to receive PDM stream
 *          - Clock from microphone: 1.024 MHz (or 3 MHz for 24kHz bandwidth)
 *          - Data format: 1-bit PDM, MSB first
 *
 *          Integration: Called by Application_onSystemInitialized() in app_audio.c
 *********************************************************************/
uint8_t Audio_init(void)
{
    #ifdef USE_ADPCM_CODEC
    // ADPCM is stateless, no initialization needed
    #else
    #ifdef USE_MSBC_CODEC
        Codec2_init();
    #endif
    #endif

    SPI_init();  // Initialize SPI driver for PDM capture

    return 0;
}

/*********************************************************************
 * @fn      Audio_enable
 *
 * @brief   Start audio capture with SPI and power on digital microphone (DMIC)
 *
 * @param   None
 *
 * @return  AUDIO_SUCCESS (0) on success, AUDIO_FAIL_NOT_OPENED on failure
 *
 * @note    Called when voice interaction starts (PTT/HTT/On-request).
 *
 *          SPI Configuration:
 *          - Frame format: SPI_POL1_PHA1 (clock polarity 1, phase 1)
 *          - Bit rate: 1.024 MHz (or 3 MHz for 24kHz bandwidth)
 *          - Transfer mode: Callback (spiCallbackFxn invoked on completion)
 *          - RX only (no TX buffer needed for PDM capture)
 *          - Buffer size: 256 bytes per transaction
 *
 *          Circular Buffer Setup:
 *          - 6 SPI transactions queued simultaneously (SPI_NUMBUFS = 6)
 *          - Each transaction captures 256 bytes of PDM data
 *          - Total buffering: 6 * 256 = 1536 bytes (12ms @ 1.024 MHz)
 *          - Prevents buffer overrun during BLE transmission delays
 *
 *          PDM Decimation Filter Initialization:
 *          - filterState[]: Zero-initialized for pdm2pcm16k() function
 *          - 16-element state buffer for biquad filter stages
 *          - Biquad coefficients (pBqCoeffs) compensate for CIC droop
 *
 *          Microphone Power:
 *          - CONFIG_GPIO_MIC set HIGH to enable digital microphone
 *          - Microphone requires ~1ms settling time after power-on
 *          - LED turned on for visual feedback (CONFIG_GPIO_LED_GREEN)
 *
 *          Safety Check:
 *          - Only starts capture if bleStreamIsEnabled == 1
 *          - Prevents starting SPI without active BLE connection
 *
 *          Integration: Called by Voice_startStream() in voice_interaction.c
 *********************************************************************/
uint8_t Audio_enable(void)
{
    SPI_Params spiParams;
    /* Open SPI for PDM capture from digital microphone */
    SPI_Params_init(&spiParams);
    spiParams.frameFormat = SPI_POL1_PHA1;  // Clock polarity 1, phase 1
    #ifdef USE_BW_24KHz
    spiParams.bitRate = 3000000;  // 3 MHz for 24kHz bandwidth
    #else
    spiParams.bitRate = 1024000;  // 1.024 MHz for standard 16kHz bandwidth
    #endif
    spiParams.transferMode = SPI_MODE_CALLBACK;
    spiParams.transferCallbackFxn = spiCallbackFxn;
    spiHandle = SPI_open(CONFIG_SPI_0, &spiParams);

    GPIO_write(CONFIG_GPIO_MIC_DTM,1);
    uint8_t retVal = AUDIO_FAIL_NOT_OPENED;

    if (bleStreamIsEnabled == 1)
    {
        if(spiHandle != 0)
        {
            List_clearList(&pdmFilledSpiTransactionsList);  // Clear any stale transactions

            /* Prepare 6 SPI transactions for circular buffering */
            int k;
            for(k = 0; k < SPI_NUMBUFS; k++) {
                spiTransactionTable[k]->count = SPI_BUFSIZE;           // 256 bytes per transaction
                spiTransactionTable[k]->txBuf = (void *) NULL;         // RX only (no TX needed)
                spiTransactionTable[k]->rxBuf = (void *) spiBufTable[k];  // PDM data buffer
            }

            /* Zero-initialize PDM decimation filter state */
            int j = 0;
            for(j = 0; j < FILTER_STATE_SIZE; j++)
            {
                filterState[j] = 0;
            }

            /* Start all 6 SPI transactions (fills circular buffer pipeline) */
            for(k = 0; k < SPI_NUMBUFS; k++)
            {
               SPI_transfer(spiHandle, spiTransactionTable[k]);
            }

            retVal = AUDIO_SUCCESS;
        }
    }

    return retVal;
}

/*********************************************************************
 * @fn      Audio_disable
 *
 * @brief   Stop audio capture and power down digital microphone (DMIC)
 *
 * @param   None
 *
 * @return  0 on success
 *
 * @note    Called when voice interaction ends (button release, timeout, or error).
 *
 *          Shutdown Sequence:
 *          1. Cancel all ongoing SPI transfers (stops DMA)
 *          2. Close SPI driver (releases resources)
 *          3. Power down microphone (CONFIG_GPIO_MIC set LOW)
 *
 *          Safety:
 *          - Only cancels/closes SPI if bleStreamIsEnabled == 1
 *          - SPI_transferCancel() is safe to call even if no transfers active
 *          - Microphone power-down does not require settling time
 *
 *          Power Savings:
 *          - Digital microphone: ~500µA-1mA when powered
 *          - SPI+DMA: ~300µA when active
 *          - Total savings: ~0.8-1.3mA when disabled
 *
 *          Integration: Called by Voice_stopStream() in voice_interaction.c
 *********************************************************************/
uint8_t Audio_disable(void)
{
    if (bleStreamIsEnabled == 1){
        SPI_transferCancel(spiHandle);  // Cancel all ongoing SPI transfers
        SPI_close(spiHandle);           // Release SPI driver resources
    }
        GPIO_write(CONFIG_GPIO_MIC_DTM, 0);  // Power down digital microphone
    return 0;
}

extern uint8_t stop_audio;

/*********************************************************************
 * @fn      Audio_getCompressedBuffer
 *
 * @brief   Compress audio signal using ADPCM or mSBC codec (DMIC)
 *
 * @param   encBuf - Output buffer for compressed audio data
 *
 * @return  Length of compressed data in bytes, or AUDIO_FAIL_NO_DATA
 *
 * @note    This function processes PDM data captured by SPI and compresses it.
 *
 *          Processing Pipeline:
 *          1. Get completed SPI transaction from pdmFilledSpiTransactionsList
 *          2. pdm2pcm16k(): Decimate 256 bytes PDM → 32 samples PCM @ 16kHz
 *          3. Accumulate PCM samples until frame size reached (144 for mSBC)
 *          4. Codec1_encodeBuff() or Codec2_encodeBuff(): Compress to encBuf
 *          5. Shift remaining PCM samples to beginning of buffer
 *          6. Requeue SPI transaction for next capture
 *
 *          PDM-to-PCM Decimation (pdm2pcm16k):
 *          - Input: 256 bytes PDM data (2048 samples @ 1.024 MHz)
 *          - Output: 32 samples PCM @ 16kHz
 *          - Decimation ratio: 64:1 (1.024 MHz / 16 kHz)
 *          - Processing: CIC + Biquad filter for droop compensation
 *          - Latency: ~100µs per call
 *
 *          Frame Accumulation:
 *          - ADPCM: Processes immediately (32 samples = 1 frame)
 *          - mSBC: Accumulates 4.5 callbacks (144 samples total)
 *          - pcmDataIndex tracks current buffer fill level
 *          - Leftover samples preserved for next frame
 *
 *          ADPCM Codec (4:1 compression):
 *          - Input: 32 samples (16-bit PCM) = 64 bytes
 *          - Output: 16 bytes (4-bit per sample)
 *          - State: si (step index), pv (previous value) maintained across calls
 *
 *          mSBC Codec (higher quality):
 *          - Input: 144 samples (16-bit PCM) = 288 bytes
 *          - Output: 57 bytes
 *          - Compression ratio: ~5:1
 *
 *          Circular Buffer Management:
 *          - After processing, transaction removed from list
 *          - Transaction requeued if stop_audio == 0
 *          - Maintains 6-buffer pipeline for continuous capture
 *
 *          Called By: Application_DoAudioEvent() in app_audio.c when
 *          spiCallbackFxn() adds a completed transaction to the list.
 *
 *          Return Values:
 *          - AUDIO_FAIL_NO_DATA: No transaction available (list empty)
 *          - 0: Frame not complete yet (accumulating samples)
 *          - 16: ADPCM frame size (typical)
 *          - 57: mSBC frame size (typical)
 *********************************************************************/
uint8_t Audio_getCompressedBuffer(uint8_t* encBuf)
{
    SPI_Transaction* transactionToTreat = (SPI_Transaction*) List_head(&pdmFilledSpiTransactionsList);
    uint16_t dataLength = AUDIO_FAIL_NO_DATA;

    if(transactionToTreat != NULL)
    {
        dataLength = 0;  // Default: no compressed data yet (accumulating samples)

        /* PDM-to-PCM Decimation: 256 bytes PDM → 32 samples PCM @ 16kHz */
        pdm2pcm16k(transactionToTreat->rxBuf, filterState, pBqCoeffs, &pcmData[pcmDataIndex]);
        pcmDataIndex += PCM_BUF_SIZE;  // Advance buffer index (32 samples added)

        /* Check if enough samples accumulated for codec encoding */
        if(pcmDataIndex >= AUDIO_PCM_FRAME_SIZE)
        {
            #ifdef USE_ADPCM_CODEC
            /* ADPCM Encode: 16-bit PCM → 4-bit ADPCM (4:1 compression) */
            dataLength = Codec1_encodeBuff(encBuf, pcmData, PCM_BUF_SIZE, &si, &pv);
            #else
            #ifdef USE_MSBC_CODEC
            /* mSBC Encode: 16-bit PCM → mSBC compressed (~5:1 compression) */
            #ifdef USE_BW_24KHz
            dataLength = Codec2_encodeBuff(encBuf, pcmData, AUDIO_ENC_FRAME_SIZE, AUDIO_PCM_FRAME_SIZE);
            #else
            dataLength = Codec2_encodeBuff(encBuf, pcmData, AUDIO_ENC_FRAME_SIZE, AUDIO_PCM_FRAME_SIZE);
            #endif

            #endif
            #endif

            /* Shift leftover PCM samples to beginning of buffer for next frame */
            #ifdef USE_BW_24KHz
            memcpy(&pcmData[0], &pcmData[AUDIO_PCM_FRAME_SIZE], (pcmDataIndex - AUDIO_PCM_FRAME_SIZE)*sizeof(int16_t));
            pcmDataIndex -= AUDIO_PCM_FRAME_SIZE;
            #else
            memcpy(&pcmData[0], &pcmData[AUDIO_PCM_FRAME_SIZE], (pcmDataIndex - AUDIO_PCM_FRAME_SIZE)*sizeof(int16_t));
            pcmDataIndex -= AUDIO_PCM_FRAME_SIZE;
            #endif
        }

        /* Requeue transaction for next SPI capture (maintains circular buffer) */
        List_remove(&pdmFilledSpiTransactionsList, (List_Elem*)transactionToTreat);
        transactionToTreat->count = SPI_BUFSIZE;
        if (stop_audio == 0){
            SPI_transfer(spiHandle, transactionToTreat);  // Requeue if audio still active
        }
    }

    return dataLength;
}
#endif
