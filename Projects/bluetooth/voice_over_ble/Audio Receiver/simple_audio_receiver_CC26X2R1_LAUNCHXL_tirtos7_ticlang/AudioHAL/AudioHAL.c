/*
 * Copyright (c) 2015-2020, Texas Instruments Incorporated
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
 *  ======== AudioHAL.c ========
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <ti/drivers/dpl/HwiP.h>
#include <AudioHAL/AudioHAL.h>

extern const AudioHAL_Config AudioHAL_config[];
extern const uint_least8_t AudioHAL_count;

const AudioHAL_Params AudioHAL_defaultParams = {
    .samplingFrequency = 16000U,
    .bitDepth = AUDIOHAL_BITDEPTH_16,
    .frameSize = 320U,
    .numChannels = AUDIOHAL_CHANNELS_STEREO,
    .direction = AUDIOHAL_INPUT_OUTPUT,
    .invertMono = (bool)true,
    .role = AUDIOHAL_MASTER,
    .readBuf = NULL,
    .writeBuf = NULL,
    .writeBufDepth = AUDIOHAL_DEFAULT_BUF_DEPTH,
    .readBufDepth = AUDIOHAL_DEFAULT_BUF_DEPTH,
    .dataReadyCB = NULL,
    .dataNeededCB = NULL,
    .eventCB = NULL,
    .arg = NULL
};

static bool isInitialized = false;

/*
 *  ======== AudioHAL_close ========
 */
void AudioHAL_close(AudioHAL_Handle handle)
{
    handle->fxnTablePtr->closeFxn(handle);
}

/*
 *  ======== AudioHAL_readBufGet ========
 */
AudioHAL_Status AudioHAL_readBufGet(AudioHAL_Handle handle, void *frame)
{
    return (handle->fxnTablePtr->readBufGetFxn(handle, frame));
}

/*
 *  ======== AudioHAL_init ========
 */
void AudioHAL_init(void)
{
    uint_least8_t i;
    uintptr_t key;

    key = HwiP_disable();

    if (!isInitialized) {
        isInitialized = (bool) true;

        /* Call each driver's init function */
        for (i = 0; i < AudioHAL_count; i++) {
            AudioHAL_config[i].fxnTablePtr->initFxn((AudioHAL_Handle) &(AudioHAL_config[i]));
        }
    }

    HwiP_restore(key);
}

/*
 *  ======== AudioHAL_open ========
 */
AudioHAL_Handle AudioHAL_open(uint_least8_t index, const AudioHAL_Params *params)
{
    AudioHAL_Handle handle = NULL;

    if (isInitialized && (index < AudioHAL_count)) {
        /* If params are NULL use defaults */
        if (params == NULL) {
            params = (AudioHAL_Params *) &AudioHAL_defaultParams;
        }

        /* Get handle for this driver instance */
        handle = (AudioHAL_Handle)&(AudioHAL_config[index]);
        handle = handle->fxnTablePtr->openFxn(handle, params);
    }

    return (handle);
}

/*
 *  ======== AudioHAL_writeBufPut ========
 */
AudioHAL_Status AudioHAL_writeBufPut(AudioHAL_Handle handle, const void *frame)
{
    return (handle->fxnTablePtr->writeBufPutFxn(handle, frame));
}

/*
 *  ======== AudioHAL_Params_init ========
 */
void AudioHAL_Params_init(AudioHAL_Params *params)
{
    *params = AudioHAL_defaultParams;
}

/*
 *  ======== AudioHAL_startStream ========
 */
AudioHAL_Status AudioHAL_startStream(AudioHAL_Handle handle,
                                        AudioHALDir dirToStart)
{
    return (handle->fxnTablePtr->startStreamFxn(handle, dirToStart));
}

/*
 *  ======== AudioHAL_startStream ========
 */
AudioHAL_Status AudioHAL_stopStream(AudioHAL_Handle handle,
                                    AudioHALDir dirToStop)
{
    return (handle->fxnTablePtr->stopStreamFxn(handle, dirToStop));
}
