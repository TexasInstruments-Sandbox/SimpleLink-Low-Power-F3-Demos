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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ti/drivers/dpl/DebugP.h>
#include <ti/drivers/dpl/HwiP.h>
#include <ti/drivers/dpl/SemaphoreP.h>
#include <ti/drivers/dpl/SwiP.h>

#include <AudioHAL/AudioHALCodec.h>

extern const AudioHAL_Config AudioHAL_config[];
extern const uint_least8_t AudioHAL_count;

/* AudioHALCodec functions */
AudioHAL_Handle AudioHALCodec_open(AudioHAL_Handle handle, const AudioHAL_Params *params);
AudioHAL_Status AudioHALCodec_startStream(AudioHAL_Handle handle,
                                            AudioHALDir dirToStart);
AudioHAL_Status AudioHALCodec_stopStream(AudioHAL_Handle handle,
                                            AudioHALDir dirToStop);
AudioHAL_Status AudioHALCodec_readBufGet(AudioHAL_Handle handle, void *frame);
void AudioHALCodec_init(AudioHAL_Handle handle);
AudioHAL_Status AudioHALCodec_writeBufPut(AudioHAL_Handle handle, const void *frame);
void AudioHALCodec_close(AudioHAL_Handle handle);

/* AudioHALCodec callback functions plugged to I2S driver */
static void i2sWriteCallback(I2S_Handle handle, int_fast16_t status, I2S_Transaction *transactionPtr);
static void i2sReadCallback(I2S_Handle handle, int_fast16_t status, I2S_Transaction *transactionPtr);
static void i2sErrorCallback(I2S_Handle handle, int_fast16_t status, I2S_Transaction *transactionPtr);


/* AudioHAL function table for TLV320AIC3254 implementation  */
const AudioHAL_FxnTable AudioHALCodec_fxnTable = {
    &AudioHALCodec_open,
    &AudioHALCodec_startStream,
    &AudioHALCodec_stopStream,
    &AudioHALCodec_readBufGet,
    &AudioHALCodec_init,
    &AudioHALCodec_writeBufPut,
    &AudioHALCodec_close
};

/*
 *  ======== convertToI2SChannels ========
 */
static uint16_t convertToI2SChannels(uint16_t numChannels, bool invertMono)
{
    if(numChannels == 8)
    {
        return (I2S_8_CHANNELS);
    }

    if(((AudioHALChannels)numChannels == AUDIOHAL_CHANNELS_MONO) && (invertMono == true))
    {
        return(I2S_CHANNELS_MONO_INV);
    }

    uint16_t defaultMask = (uint16_t)I2S_CHANNELS_ALL;
    uint16_t numIter = 8 - numChannels;

    while(numIter > 0)
    {
        defaultMask >>= (uint16_t)1U;
        numIter--;
    }

    return (defaultMask);
}

/*
 *  ======== i2sHandleToAudioHandle ========
 */
static AudioHAL_Handle i2sHandleToAudioHandle(I2S_Handle handle)
{
    AudioHALCodec_Object *object;
    uint8_t idx;

    /*
     * When receiving a callback from the I2S driver it is necessary to map
     * The I2S handle received to a given handle of the AudioHAL
     */
    for(idx = 0; idx < AudioHAL_count; idx++)
    {
        object  = (AudioHALCodec_Object  *)AudioHAL_config[idx].object;
        if(object->i2sHandle == handle)
        {
            return (AudioHAL_Handle)(&(AudioHAL_config[idx]));
        }
    }

    return (NULL);
}

/*
 *  ======== i2sWriteCallback ========
 */
static void i2sWriteCallback(I2S_Handle handle, int_fast16_t status, I2S_Transaction *transactionPtr)
{
    /* Translate I2S handle to AudioHAL handle */
    AudioHAL_Handle  audioHandle  = i2sHandleToAudioHandle(handle);
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)audioHandle->object;

    /* Status to be sent back in user callback */
    AudioHAL_Status stat = AUDIOHAL_SUCCESS;

    if((audioHandle == NULL) || (object == NULL))
    {
        return;
    }

    if(transactionPtr != NULL)
    {
        object->writeHead = (I2S_Transaction*)List_next(&transactionPtr->queueElement);

        /* Verify if this sample has not already been written
         *  To avoid zeroing the frame multiple times, we
         *  only zero it the first time it is required.
         */
        if(transactionPtr->numberOfCompletions == 2){
            memset(transactionPtr->bufPtr, 0x00, transactionPtr->bufSize);
        }

        /* Verify if we are not underflowing */
        if(transactionPtr == object->writeTail)
        {
            stat = AUDIOHAL_DATA_UNDERFLOW;
            object->writeTail = (I2S_Transaction*)List_next(&object->writeHead->queueElement);
        }
    }

    object->dataNeededCB(audioHandle, stat);
}

/*
 *  ======== i2sReadCallback ========
 */
static void i2sReadCallback(I2S_Handle handle, int_fast16_t status, I2S_Transaction *transactionPtr)
{
    /* Translate I2S handle to AudioHAL handle */
    AudioHAL_Handle  audioHandle  = i2sHandleToAudioHandle(handle);
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)audioHandle->object;

    /* Status to be sent back in user callback */
    AudioHAL_Status stat = AUDIOHAL_SUCCESS;

    if((audioHandle == NULL) || (object == NULL))
    {
        return;
    }

    if(transactionPtr != NULL)
    {
        /* We must remove the previous transaction (the current one is not over) */
        I2S_Transaction *transactionFinished = (I2S_Transaction*)List_prev(&transactionPtr->queueElement);
        if(transactionFinished != NULL){
            if(object->readHead == NULL)
            {
                /* This is done at initialization only */
                object->readHead = transactionFinished;
            }
            else
            {
                if((I2S_Transaction*)transactionPtr == object->readHead)
                {
                    stat = AUDIOHAL_DATA_OVERFLOW;
                    object->readHead = (I2S_Transaction*)object->readHead->queueElement.next;
                }

            }
        }

        object->readTail = transactionFinished;
    }

    object->dataReadyCB(audioHandle, stat);
}

/*
 *  ======== i2sErrorCallback ========
 */
static void i2sErrorCallback(I2S_Handle handle, int_fast16_t status, I2S_Transaction *transactionPtr)
{
    /* Translate I2S handle to AudioHAL handle */
    AudioHAL_Handle  audioHandle  = i2sHandleToAudioHandle(handle);
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)audioHandle->object;

    /* Notify application that stream has encountered an error */
    object->eventCB(audioHandle, AUDIOHAL_STREAM_ERROR);
}

/*
 *  ======== bitDepthToBytes ========
 */
static uint8_t bitDepthToBytes(AudioHAL_BitDepth bitDepth)
{
    uint8_t numBytes =  0xFFU;
    switch(bitDepth)
    {
        case AUDIOHAL_BITDEPTH_16:
            numBytes = (uint8_t)sizeof(uint16_t);
            break;
        case AUDIOHAL_BITDEPTH_24:
            numBytes = (uint8_t)SIZEOF_AUDIOHAL_BITDEPTH_24;
            break;
#if (DeviceFamily_PARENT != DeviceFamily_PARENT_CC13X0_CC26X0) && \
    (DeviceFamily_PARENT != DeviceFamily_PARENT_CC13X2_CC26X2)
        /* CC26xx/CC13xx devices do not support 32-bit samples */
        case AUDIOHAL_BITDEPTH_32:
            numBytes = (uint8_t)sizeof(uint32_t);
            break;
#endif
        default:
            break;
    }

    return (numBytes);
}

/*
 *  ======== allocateTransationMem ========
 */
static bool allocateTransationMem(AudioHAL_Handle handle)
{
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)handle->object;

    /* Initialize pointers to known state */
    object->readTransactionBuf = NULL;
    object->readList = NULL;
    object->writeTransactionBuf = NULL;
    object->writeList = NULL;

    if(object->direction & AUDIOHAL_INPUT)
    {
        object->readTransactionBuf = (I2S_Transaction *)malloc(sizeof(I2S_Transaction)*object->readBufDepth);
        object->readList = (List_List *)malloc(sizeof(List_List));
        if((object->readTransactionBuf == NULL)  || (object->readList == NULL))
        {
            return(false);
        }
    }

    if(object->direction & AUDIOHAL_OUTPUT)
    {
        object->writeTransactionBuf = (I2S_Transaction *)malloc(sizeof(I2S_Transaction)*object->writeBufDepth);
        object->writeList = (List_List *)malloc(sizeof(List_List));
        if((object->writeTransactionBuf == NULL) || (object->writeList == NULL))
        {
            return(false);
        }
    }

    return (true);
}

/*
 *  ======== setupBuffers ========
 */
static void setupBuffers(AudioHAL_Handle handle)
{
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)handle->object;
    uint8_t bytesPerSample = bitDepthToBytes(object->bitDepth);
    size_t bufSize = (size_t)(bytesPerSample*object->frameSize*object->numChannels);

    if(object->direction & AUDIOHAL_INPUT)
    {
        List_clearList(object->readList);

        /* Initialize the input buffers based on bitDepth, numChannels, and frameSize */
        uint8_t bufNum;
        for(bufNum = 0; bufNum < object->readBufDepth; bufNum++)
        {
            /* Initialize the I2S Transaction */
            I2S_Transaction_init(&(object->readTransactionBuf[bufNum]));

            /* Initialize the pointer to each buffer, do math based on bytes */
            uint8_t *bufPtr = (uint8_t *)object->readBuf;
            bufPtr = &bufPtr[bufNum*object->frameSize*object->numChannels*bytesPerSample];
            object->readTransactionBuf[bufNum].bufPtr = (void *)bufPtr;
            object->readTransactionBuf[bufNum].bufSize = bufSize;

            /* Now that the I2S transaction is intialized, place it in the list */
            List_put(object->readList, (List_Elem*)&object->readTransactionBuf[bufNum].queueElement);
        }

        /* Read buffers are queued in a ring-list */
        List_tail(object->readList)->next = List_head(object->readList);
        List_head(object->readList)->prev = List_tail(object->readList);

        /* Since list is empty, set head to tail */
        object->readHead = (I2S_Transaction *)List_head(object->readList);
        object->readTail = object->readHead;
    }

    if(object->direction & AUDIOHAL_OUTPUT)
    {
        List_clearList(object->writeList);

        /* Initialize the output buffers based on bitDepth, numChannels, and frameSize */
        uint8_t bufNum;
        for(bufNum = 0; bufNum < object->writeBufDepth; bufNum++)
        {
            /* Initialize the I2S Transaction */
            I2S_Transaction_init(&(object->writeTransactionBuf[bufNum]));
            uint8_t *bufPtr = (uint8_t *)object->writeBuf;
            bufPtr = &bufPtr[bufNum*object->frameSize*object->numChannels*bytesPerSample];
            object->writeTransactionBuf[bufNum].bufPtr = bufPtr;
            object->writeTransactionBuf[bufNum].bufSize = bufSize;

            /* Now that the I2S transaction is intialized, place it in the list */
            List_put(object->writeList, (List_Elem*)&object->writeTransactionBuf[bufNum].queueElement);
        }

        /* Write buffers are queued in a ring-list */
        List_tail(object->writeList)->next = List_head(object->writeList);
        List_head(object->writeList)->prev = List_tail(object->writeList);

        /* Since list is empty, set head to tail */
        object->writeHead = (I2S_Transaction *)List_head(object->writeList);
        object->writeTail = object->writeHead;
    }
}

/*
 *  ======== setupI2cInterface ========
 */
static bool setupI2cInterface(AudioHAL_Handle handle, bool isMaster)
{
    AudioHALCodec_HWAttrsV1 const *hwAttrs = (AudioHALCodec_HWAttrsV1 const *)handle->hwAttrs;
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)handle->object;
    bool matchFound = false;
    uint32_t index = 0U;

    if((hwAttrs->openScript == NULL) &&
        (hwAttrs->closeScript == NULL) &&
        (hwAttrs->startScript == NULL) &&
        (hwAttrs->stopScript == NULL) &&
        (hwAttrs->clkCfg == NULL) &&
        (hwAttrs->intfCfg == NULL))
    {
        /* If all the scripts are NULL, the user doesn't wish to use I2C */
        return (true);
    }

    /* Open the control interface via I2C */
    object->i2cHandle =  AudioHAL_I2C_open(hwAttrs->i2cIdx);
    if(object->i2cHandle == NULL)
    {
        DebugP_log0("AudioHALCodec.c: Can't open I2C");
        return (false);
    }

    /* Initialize interface based on bitDepth */
    if(hwAttrs->intfCfg != NULL)
    {
        for(index = 0U; index < hwAttrs->intfCfgSize; index++)
        {
            /* If we found a matching bitDepth in the table, run the script */
            if(object->bitDepth == hwAttrs->intfCfg[index].bitDepth &&
               isMaster == hwAttrs->intfCfg[index].master)
            {
                /* If the script failed, return false, else break from loop */
                if (!AudioHAL_I2C_runScript(object->i2cHandle, hwAttrs->i2cAddr,
                                            hwAttrs->i2cPageSelAddr,
                                            hwAttrs->intfCfg[index].script))
                {
                    AudioHAL_I2C_close(object->i2cHandle);
                    return (false);
                }
                else
                {
                    matchFound = true;
                    break;
                }
            }
        }
    }

    /* Ensure that the there was an entry in the interfaceConfig that
     * matched the user params
     */
    if(!matchFound)
    {
        AudioHAL_I2C_close(object->i2cHandle);
        return (false);
    }

    if(hwAttrs->clkCfg != NULL)
    {
        /* Initialize clock settings based on Fs*/
        for(index = 0U, matchFound = false; index < hwAttrs->clkCfgSize; index++)
        {
            /* If we found a matching frequency in the table run the script */
            if((object->samplingFrequency == hwAttrs->clkCfg[index].samplingFrequency) &&
                (isMaster == hwAttrs->clkCfg[index].master))
            {
                /* If the script failed, return NULL, else break from loop */
                if (!AudioHAL_I2C_runScript(object->i2cHandle, hwAttrs->i2cAddr,
                                            hwAttrs->i2cPageSelAddr,
                                            hwAttrs->clkCfg[index].script))
                {
                    AudioHAL_I2C_close(object->i2cHandle);
                    return (NULL);
                }
                else
                {
                    matchFound = true;
                    break;
                }
            }
        }
    }

    /* Ensure that the there was an entry in the clockConfig that
     * matched the user params
     */
    if(!matchFound)
    {
        AudioHAL_I2C_close(object->i2cHandle);
        return (false);
    }


    if(hwAttrs->openScript != NULL)
    {
        /* Initialize codec clocks and power on based on user script */
        if (!AudioHAL_I2C_runScript(object->i2cHandle, hwAttrs->i2cAddr,
                                    hwAttrs->i2cPageSelAddr,
                                    hwAttrs->openScript))
        {
            AudioHAL_I2C_close(object->i2cHandle);
            return (false);
        }
    }

    return (true);
}

/*
 *  ======== AudioHALCodec_open ========
 */
AudioHAL_Handle AudioHALCodec_open(AudioHAL_Handle handle, const AudioHAL_Params *params)
{
    AudioHALCodec_HWAttrsV1 const *hwAttrs = (AudioHALCodec_HWAttrsV1 const *)handle->hwAttrs;
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)handle->object;
    I2S_Params i2sParams;
    uint16_t i2sDrvChnlMask;
    uint8_t bytesPerSample = 0xFFU;
    bool isMaster = false;

    /* Initialize the AudioHAL object */
    object->samplingFrequency = params->samplingFrequency;
    object->bitDepth = params->bitDepth;
    object->frameSize = params->frameSize;
    object->numChannels = params->numChannels;
    object->direction = params->direction;
    object->role = params->role;
    object->readBuf = params->readBuf;
    object->writeBuf = params->writeBuf;
    object->readBufDepth = params->readBufDepth;
    object->writeBufDepth = params->writeBufDepth;
    object->dataReadyCB = params->dataReadyCB;
    object->dataNeededCB = params->dataNeededCB;
    object->eventCB = params->eventCB;
    object->activeStreams = AUDIOHAL_NONE;

    bytesPerSample = bitDepthToBytes(object->bitDepth);

    /* Not every platform supports 32-bit samples, check for this case here */
    DebugP_assert(0xFF != bytesPerSample);

    /* The number of supported channels is limited */
    DebugP_assert(2 >= object->numChannels);

    /* I2S has an interesting channel mask, convert to it */
    i2sDrvChnlMask = convertToI2SChannels((uint16_t)object->numChannels, params->invertMono);

    /* Setup I2S Params and open */
    I2S_Params_init(&i2sParams);

    /* If the user wishes to override the default I2S params, they can pass
      them in via arg */
    if(params->arg != NULL)
    {
        memcpy((void *)&i2sParams, params->arg, sizeof(I2S_Params));
    }

    /* Always set callbacks */
    i2sParams.samplingFrequency     =  object->samplingFrequency;
    i2sParams.bitsPerWord           =  (I2S_MemoryLength)object->bitDepth;
    i2sParams.SD1Channels           =  (I2S_ChannelConfig)i2sDrvChnlMask;
    i2sParams.SD0Channels           =  (I2S_ChannelConfig)i2sDrvChnlMask;
    i2sParams.memorySlotLength      =  (I2S_MemoryLength)object->bitDepth;
    i2sParams.fixedBufferLength     =  (uint16_t)(bytesPerSample*params->numChannels*params->frameSize);
    i2sParams.moduleRole            =  (object->role == AUDIOHAL_SLAVE)? I2S_SLAVE : I2S_MASTER;
    i2sParams.writeCallback         =  &i2sWriteCallback;
    i2sParams.readCallback          =  &i2sReadCallback;
    i2sParams.errorCallback         =  &i2sErrorCallback;

    /* Set whether codec is master based on I2S params */
    isMaster = i2sParams.moduleRole == I2S_MASTER;

    /* Setup the I2C control interface by configuring the clks, interface, and running the open script */
    if(!setupI2cInterface(handle, isMaster))
    {
        /* If I2C init failed, then return NULL */
        return (NULL);
    }

    /* Open MCLK interface */
    if(hwAttrs->mclkOpenFxn != NULL)
    {
        if(!hwAttrs->mclkOpenFxn())
        {
            if(object->i2cHandle) { AudioHAL_I2C_close(object->i2cHandle); }
            /* If MCLK generation failed, then return NULL */
            return (NULL);
        }
    }

    /* Open the driver and validate the handle */
    object->i2sHandle = I2S_open(hwAttrs->i2sIdx, &i2sParams);
    if(object->i2sHandle == NULL)
    {
        DebugP_log0("AudioHALCodec.c: Can't open I2S");
        if(object->i2cHandle) { AudioHAL_I2C_close(object->i2cHandle); }
        return (NULL);
    }

    /* Attempt to allocate memory for input/output List, I2S transactions
     * this is a oneshot allocation on open, and will be free'd on close
     */
    if(!allocateTransationMem(handle))
    {
        DebugP_log0("AudioHALCodec.c: I2S buffer allocation failed");
        if(object->i2cHandle) { AudioHAL_I2C_close(object->i2cHandle); }
        I2S_close(object->i2sHandle);
        return (NULL);
    }

    /* Setup buffer memory to be used by I2S driver */
    setupBuffers(handle);

    return (handle);
}

/*
 *  ======== AudioHALCodec_startStream ========
 */
AudioHAL_Status AudioHALCodec_startStream(AudioHAL_Handle handle,
                                            AudioHALDir dirToStart)
{
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)handle->object;
    AudioHALCodec_HWAttrsV1 const *hwAttrs = (AudioHALCodec_HWAttrsV1 const *)handle->hwAttrs;

    /* Check that the stream to start is supported */
    if(!(dirToStart & object->direction))
    {
        return (AUDIOHAL_INVALID_PARAM);
    }

    /* First start MCLK as codec will need it */
    if(hwAttrs->mclkStartFxn != NULL)
    {
        hwAttrs->mclkStartFxn();
    }

    /* Run user start stream script if one exists*/
    if(hwAttrs->startScript != NULL)
    {
        if(!AudioHAL_I2C_runScript(object->i2cHandle, hwAttrs->i2cAddr,
                                    hwAttrs->i2cPageSelAddr,
                                    hwAttrs->startScript))
        {
            return (AUDIOHAL_CMD_FAILED);
        }
    }

    /* Start the I2S clocks */
    I2S_startClocks(object->i2sHandle);

    /* Queue the read transactions and start the read */
    if(dirToStart & AUDIOHAL_INPUT)
    {
        /* Read queue starts on first transaction */
        I2S_setReadQueueHead(object->i2sHandle, object->readHead);
        I2S_startRead(object->i2sHandle);
        object->activeStreams |= AUDIOHAL_INPUT;
    }

    /* Queue the write transactions and start the write */
    if(dirToStart & AUDIOHAL_OUTPUT)
    {
        /* Write queue starts on first transaction */
        I2S_setWriteQueueHead(object->i2sHandle, object->writeHead);
        I2S_startWrite(object->i2sHandle);
        object->activeStreams |= AUDIOHAL_OUTPUT;
    }

    return (AUDIOHAL_SUCCESS);
}

/*
 *  ======== AudioHALCodec_stopStream ========
 */
AudioHAL_Status AudioHALCodec_stopStream(AudioHAL_Handle handle,
                                            AudioHALDir dirToStop)
{
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)handle->object;
    AudioHALCodec_HWAttrsV1 const *hwAttrs = (AudioHALCodec_HWAttrsV1 const *)handle->hwAttrs;

    /* Check that the stream to stop is supported and running */
    if(!(dirToStop & object->activeStreams))
    {
        return (AUDIOHAL_INVALID_PARAM);
    }

    /* Cancel the read transactions and stop the read */
    if(dirToStop & AUDIOHAL_INPUT)
    {
        object->activeStreams &= ~(AUDIOHAL_INPUT);
        I2S_stopRead(object->i2sHandle);
    }

    /* Cancel the write transactions and stop the write */
    if(dirToStop & AUDIOHAL_OUTPUT)
    {
        object->activeStreams &= ~(AUDIOHAL_OUTPUT);
        I2S_stopWrite(object->i2sHandle);
    }

    if(object->activeStreams == AUDIOHAL_NONE)
    {
        /* If the user has provided a codec script, run it */
        if(hwAttrs->stopScript != NULL)
        {
            if(!AudioHAL_I2C_runScript(object->i2cHandle, hwAttrs->i2cAddr,
                                        hwAttrs->i2cPageSelAddr,
                                        hwAttrs->stopScript))
            {
                return (AUDIOHAL_CMD_FAILED);
            }
        }

        /* Now that all read/write is cancelled, stop the clocks */
        I2S_stopClocks(object->i2sHandle);

        /* Since I2S is shut down it is safe to shutdown MCLK */
        if(hwAttrs->mclkStopFxn != NULL)
        {
            hwAttrs->mclkStopFxn();
        }
    }

    return (AUDIOHAL_SUCCESS);
}

/*
 *  ======== AudioHALCodec_readBufGet ========
 */
AudioHAL_Status AudioHALCodec_readBufGet(AudioHAL_Handle handle, void * frame)
{
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)handle->object;
    uintptr_t hwiKey, swiKey;

    size_t sizeToCopy = (size_t)(bitDepthToBytes(object->bitDepth)*object->frameSize*object->numChannels);

    /* ReadCB may overwrite the contents so protect against */
    swiKey = SwiP_disable();
    hwiKey = HwiP_disable();

    /* Copy the buffer pointer into the return parameter */
    if(object->readHead != NULL)
    {
        object->readTail->numberOfCompletions = 0;
        memcpy(frame, object->readHead->bufPtr, sizeToCopy);
        object->readHead = (I2S_Transaction*)List_next(&object->readHead->queueElement);
    }

    HwiP_restore(hwiKey);
    SwiP_restore(swiKey);

    return (AUDIOHAL_SUCCESS);
}

/*
 *  ======== AudioHALCodec_init ========
 */
void AudioHALCodec_init(AudioHAL_Handle handle)
{
    AudioHALCodec_Object *object;

    /* Get the pointer to the object */
    object = handle->object;

    /* Clear object Handles */
    object->i2cHandle = NULL;
    object->i2sHandle = NULL;

    /* Initialize the necessary drivers: I2C for ctrl, I2S for audio */
    I2S_init();
    I2C_init();
}

/*
 *  ======== AudioHALCodec_writeBufPut ========
 */
AudioHAL_Status AudioHALCodec_writeBufPut(AudioHAL_Handle handle, const void * frame)
{
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)handle->object;
    uintptr_t hwiKey, swiKey;

    size_t sizeToCopy = (size_t)(bitDepthToBytes(object->bitDepth)*object->frameSize*object->numChannels);
    /* ReadCB may overwrite the contents so protect against */
    swiKey = SwiP_disable();
    hwiKey = HwiP_disable();

    if(object->writeTail != NULL)
    {
        if(object->writeTail == (I2S_Transaction*)List_prev(&object->writeHead->queueElement))
        {
            /*
             * Protect against case where user is about to write to a buffer
             * that is currently being used by the H/W.
             * Head and head->prev are actively used by the H/W
             * Instead, we overwrite the oldest sample that is safe to modify
             */
            object->writeTail = (I2S_Transaction*)List_next(&object->writeHead->queueElement);
        }

        object->writeTail->numberOfCompletions = 0;
        /* Copy the buffer pointer into the return parameter */
        memcpy(object->writeTail->bufPtr, frame, sizeToCopy);
        object->writeTail = (I2S_Transaction*)List_next(&object->writeTail->queueElement);
    }

    HwiP_restore(hwiKey);
    SwiP_restore(swiKey);
    return (AUDIOHAL_SUCCESS);
}

/*
 *  ======== AudioHALCodec_close ========
 */
void AudioHALCodec_close(AudioHAL_Handle handle)
{
    AudioHALCodec_Object  *object  = (AudioHALCodec_Object  *)handle->object;
    AudioHALCodec_HWAttrsV1 const *hwAttrs = (AudioHALCodec_HWAttrsV1 const *)handle->hwAttrs;


    if(hwAttrs->closeScript)
    {
        AudioHAL_I2C_runScript(object->i2cHandle, hwAttrs->i2cAddr,
                                    hwAttrs->i2cPageSelAddr,
                                    hwAttrs->closeScript);
    }

    /* Close the drivers used by the module */
    I2S_close(object->i2sHandle);
    if(object->i2cHandle) { AudioHAL_I2C_close(object->i2cHandle); }

    object->i2sHandle = NULL;
    object->i2cHandle = NULL;
    object->activeStreams = AUDIOHAL_NONE;

    /* Close MCLK interface */
    if(hwAttrs->mclkCloseFxn != NULL)
    {
        hwAttrs->mclkCloseFxn();
    }

    /* Free list and transaction memory */
    free(object->readTransactionBuf);
    free(object->readList);
    free(object->writeTransactionBuf);
    free(object->writeList);

    /* NULL out pointers */
    object->readTransactionBuf = NULL;
    object->readList = NULL;
    object->writeTransactionBuf = NULL;
    object->writeList = NULL;
}
