/*
 * audio_player.c
 *
 *  Created on: Nov 25, 2022
 *      Author: a0132664
 */

/* C standard library includes */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Audio HAL top level include */
#include <AudioHAL/AudioHAL.h>
#include <ti/drivers/UART2.h>

/* Board Header file */
#include "ti_drivers_config.h"

#include <minimal_board.h>

/* Audio Frame settings */
#ifdef USE_BW_24KHz
#define SAMPLE_RATE         48000
#else
#define SAMPLE_RATE         16000
#endif


#define NUM_CHAN            AUDIOHAL_CHANNELS_MONO

#ifdef USE_ADPCM_CODEC
#include <adpcm/Codec1.h>
#define ADPCM_ENC_FRAME_SIZE               (16U)
#define ADPCM_PCM_FRAME_SIZE               (32U)
#define AUDIO_PCM_FRAME_SIZE          ADPCM_PCM_FRAME_SIZE
#define AUDIO_ENC_FRAME_SIZE          ADPCM_ENC_FRAME_SIZE

#else
#ifdef USE_MSBC_CODEC
/* Codec and filters Header files */
#include <sbc/Codec2.h>
#define MSBC_ENC_FRAME_SIZE               (57U)
#define MSBC_PCM_FRAME_SIZE               (120U)


#define AUDIO_PCM_FRAME_SIZE          MSBC_PCM_FRAME_SIZE
#define AUDIO_ENC_FRAME_SIZE          MSBC_ENC_FRAME_SIZE

#endif
#endif


/* Codec param and object */
#ifdef USE_ADPCM_CODEC
static int8_t            si_dec = 0;
static int16_t           pv_dec = 0;
#endif

AudioHAL_Handle tlv320Handle = NULL;

static uint8_t rxBitStream[AUDIO_ENC_FRAME_SIZE];

static void uartReadCallback(UART2_Handle handle, void *rxBuf, size_t size, void *userArg, int_fast16_t status)
{
}

static void uartWriteCallback(UART2_Handle handle, void *txBuf, size_t size, void *userArg, int_fast16_t status)
{
}

/* These are the actual sample buffers that are used by the driver below the HAL */
static int16_t writeBuf[NUM_CHAN][AUDIO_PCM_FRAME_SIZE][AUDIOHAL_DEFAULT_BUF_DEPTH];

/* Single frame buffer used to copy data in from HAL to application space */
int16_t audioOut[NUM_CHAN][AUDIO_PCM_FRAME_SIZE];
int8_t uartbufferaudioOut[25*AUDIO_PCM_FRAME_SIZE*2];

/*
 *  ======== audioDataNeededCB ========
 *  Callback function executed when audio data are expected by the AudioHAL
 */
static void audioDataNeededCB(AudioHAL_Handle handle, int_fast16_t status)
{
    /* Nothing to do */
}

/*
 *  ======== audioEventCB ========
 *  Callback function executed when an audio event occurs
 */
static void audioEventCB(AudioHAL_Handle handle, AudioHALEvent event)
{
    /* Nothing to do */
}

// UART Interface
static UART2_Handle uartHandle = NULL;

void AudioPlayer_init(void)
{
    /* Audio HAL related variables */
    AudioHAL_Status stat;
    AudioHAL_Params audioHalParams;

    AudioHAL_init();

    /* Set AudioHAL settings */
    AudioHAL_Params_init(&audioHalParams);
    audioHalParams.frameSize = AUDIO_PCM_FRAME_SIZE;
    audioHalParams.numChannels = NUM_CHAN;
    audioHalParams.writeBuf = writeBuf;
    audioHalParams.bitDepth = AUDIOHAL_BITDEPTH_16;
    audioHalParams.samplingFrequency = SAMPLE_RATE;
    audioHalParams.writeBufDepth = AUDIOHAL_DEFAULT_BUF_DEPTH;
    audioHalParams.dataNeededCB = audioDataNeededCB;
    audioHalParams.eventCB = audioEventCB;
    audioHalParams.direction = AUDIOHAL_OUTPUT;

    /* Open AudioHAL interface to TLV320 */
    tlv320Handle = AudioHAL_open(Board_AUDIOHAL_CODEC, &audioHalParams);

    if(tlv320Handle == NULL)
    {
        /* AudioHAL_open(Board_AUDIOHAL_CODEC, ...) failed */
        while (1);
    }

        // Initialize UART
    UART2_Params uartParams;

    // Open UART in callback mode for both read and write
    UART2_Params_init(&uartParams);
    uartParams.readReturnMode = UART2_ReadReturnMode_PARTIAL;
    uartParams.readMode = UART2_Mode_CALLBACK;
    uartParams.writeMode = UART2_Mode_CALLBACK;
    uartParams.readCallback = uartReadCallback;
    uartParams.writeCallback = uartWriteCallback;
    uartParams.baudRate = 3000000;

    uartHandle = UART2_open(CONFIG_UART2_0, &uartParams);

    if (uartHandle == NULL)
    {
        // UART_open() failed
        while (1);
    }

    /* Open codec instance */
    #ifdef USE_ADPCM_CODEC
    /* Prepare ADPCM codec */
    si_dec = 0;
    pv_dec = 0;

    #else
    #ifdef USE_MSBC_CODEC
    /* Prepare MSBC codec */
    Codec2_init();

    #endif
    #endif


    stat = AudioHAL_startStream(tlv320Handle, AUDIOHAL_OUTPUT);

    if(stat != AUDIOHAL_SUCCESS)
    {
        /* AudioHAL_startStream(...) failed */
        while (1);

    }
}


volatile uint8_t numberOfSeqNumFault = 0;
uint8_t rep;

void AudioPlayer_addData(uint8_t *rxBitStream, uint16_t bufferLength)
{
    /* Loop on the buffer received */
    int indexEncodedBuffer = 0;

    while((indexEncodedBuffer + AUDIO_ENC_FRAME_SIZE) <= bufferLength)
    {
        /* Decode the frame received */
        #ifdef USE_ADPCM_CODEC

        Codec1_decodeBuff((int16_t *)audioOut,
                          &rxBitStream[indexEncodedBuffer],
                          AUDIO_PCM_FRAME_SIZE * sizeof(int16_t),
                          &si_dec,
                          &pv_dec);
        #else
        #ifdef USE_MSBC_CODEC
        /* Prepare MSBC codec */
        Codec2_decodeBuff((int16_t *)audioOut,
                          &rxBitStream[indexEncodedBuffer],
                          AUDIO_PCM_FRAME_SIZE,
                          AUDIO_ENC_FRAME_SIZE);

        #endif
        #endif

        /* Place decoded buffer in the output */
        AudioHAL_writeBufPut(tlv320Handle, (void *)audioOut);

        indexEncodedBuffer += AUDIO_ENC_FRAME_SIZE;

        if (rep >= 25)
        {
            UART2_write(uartHandle, uartbufferaudioOut, sizeof(uartbufferaudioOut), NULL);
            rep=0;
        }
        else 
        {
            memcpy(&uartbufferaudioOut[AUDIO_PCM_FRAME_SIZE*2*rep], audioOut, AUDIO_PCM_FRAME_SIZE*2);
            rep++;
        }
    }

}
