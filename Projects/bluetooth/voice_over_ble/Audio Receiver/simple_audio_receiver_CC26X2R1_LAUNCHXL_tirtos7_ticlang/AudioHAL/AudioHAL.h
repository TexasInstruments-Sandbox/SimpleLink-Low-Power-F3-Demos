/*
 * Copyright (c) 2016-2020, Texas Instruments Incorporated
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

/** ============================================================================
 *  @file       AudioHAL.h
 *
 *  @defgroup AudioHAL AudioHAL
 *
 *  @brief      Generic interface for audio input/output
 *
 *  # Include #
 *  The AudioHAL header file should be included in an application as follows:
 *  @code
 *  #include <ti/audio/hal/AudioHAL.h>
 *  @endcode
 *
 *  @anchor ti_audio_AudioHAL_Overview
 *  # Overview #
 *
 *  The AudioHAL is a hardware abstraction layer for various audio interfaces.
 *  The AudioHAL provides APIs for sampling and streaming audio from an
 *  application. This enables development of audio applications  independent of
 *  the underlying driver that is used for sampling the Audio data itself.
 *  It is compatible across both multiple interfaces and devices.
 *
 *  The HAL is responsible for initializing, calling, and managing the underlying
 *  driver allowing for a minimal set of application code changes when switching
 *  between various audio interfaces.
 *
 *  The HAL provides a stream based API for getting audio data into and out
 *  from the application. It is well suited for low latency RF enabled audio
 *  applications.
 *
 *  # Opening the interface #
 *
 *  The AudioHAL reuses the same design paradigm as the TI drivers or Display
 *  middleware. API calls operate on an instance of the HAL which is specified
 *  by a handle. A handle is really a pointer to an instance of a specific
 *  @ref AudioHAL_Config. A call to open initializes and sets up an instance
 *  of the HAL and close will shut it down and close it. Open should be called
 *  when the application wishes to use a specific configuration of the HAL and
 *  close should be called when the application no longer needs access to the
 *  HAL.
 *
 *  Configurations of the HAL are defined in the board file. This enables a
 *  decoupling of the application and the hardware configuration.
 *
 *  Unlike TI drivers, the entries in the AudioHAL config table correspond
 *  to external hardware (e.g. BoosterPacks or breakout boards) that have audio
 *  capability. Each AudioHAL object has the same member variables
 *  (HW Attrs, object, function table pointer) but their implementation is
 *  different. For example PDM is unidirectional while I2S can be bidirectional.
 *  In both cases, the entire application can be built on AudioHAL without changing APIs.
 *
 *  A sample board file entry is shown below:
 *
 *  @code
 *  AudioHALCodec_Object AudioHALCodecObject;
 *
 *  const AudioHALCodec_HWAttrsV1 AudioHALCodecHWAttrs[] = {
 *      {
 *          .i2cIdx = CC26X2R1_LAUNCHXL_I2C0,
 *          .i2sIdx = CC26X2R1_LAUNCHXL_I2S0,
 *          .i2cAddr = TI3254_I2C_SLAVE_ADDR,
 *          .i2cPageSelAddr = TI3254_PAGE_SEL_REG,
 *          .openScript =  &TLV320AIC3254_openScript,
 *          .closeScript =  &TLV320AIC3254_closeScript,
 *          .startScript =  &TLV320AIC3254_startScript,
 *          .stopScript =  &TLV320AIC3254_stopScript,
 *          .clkCfg =  TLV320AIC3254_clockConfig,
 *          .clkCfgSize = TI3254_NUM_CLK_CFG,
 *          .intfCfg =  TLV320AIC3254_interfaceConfig,
 *          .intfCfgSize = TI3254_NUM_INTF_CFG,
 *          .mclkOpenFxn = MCLKCC26XX_open,
 *          .mclkStartFxn = MCLKCC26XX_start,
 *          .mclkStopFxn = MCLKCC26XX_stop,
 *          .mclkCloseFxn = MCLKCC26XX_close
 *      }
 *  };
 *  const AudioHAL_Config AudioHAL_config[] = {
 *      {
 *          .fxnTablePtr    = &AudioHALCodec_fxnTable,
 *          .object         = &AudioHALCodecObject,
 *          .hwAttrs        = &AudioHALCodecHWAttrs
 *      },
 *  };
 *  @endcode
 *
 *
 *  The structure above tells us that the AudioHAL on this particular board file
 *  supports interfacing to external codec devices via I2S. It can be opened
 *  from the application as below
 *
 *  @code
 *  AudioHAL_Handle tlv320Handle = AudioHAL_open(Board_AUDIOHAL_CODEC, &audioHalParams);
 *  @endcode
 *
 *  It is always recommended to check for a valid handle before using any
 *  further APIs, an example is shown below:
 *
 *  @code
 *  if(tlv320Handle == NULL)
 *  {
 *      while (1);
 *  }
 *  @endcode
 *
 *  # Parameters #
 *
 *  The parameters are passed in as part of the open call and are common to
 *  all HAL implementations. They will configure the HAL for a given set of
 *  audio parameters and will setup the necessary hardware to provide a stream
 *  with the given parameters. See @ref AudioHAL_Params for more information
 *  about the various parameters. Parameters should be initialized to a known
 *  value using @ref AudioHAL_Params_init. The default parameters are defined
 *  in AudioHAL.c, but can be set as below:
 *
 *  @code
 *  AudioHAL_Params audioHalParams;
 *  audioHalParams.frameSize = FRAME_SIZE;
 *  audioHalParams.numChannels = NUM_CHAN;
 *  audioHalParams.readBuf = readBuf;
 *  audioHalParams.writeBuf = writeBuf;
 *  audioHalParams.bitDepth = AUDIOHAL_BITDEPTH_24;
 *  audioHalParams.samplingFrequency = SAMPLE_RATE;
 *  audioHalParams.readBufDepth = AUDIOHAL_DEFAULT_BUF_DEPTH;
 *  audioHalParams.writeBufDepth = AUDIOHAL_DEFAULT_BUF_DEPTH;
 *  audioHalParams.dataNeededCB = audioDataNeeded;
 *  audioHalParams.dataReadyCB = audioDataReady;
 *  audioHalParams.eventCB = audioEvent;
 *  @endcode
 *
 *  The parameters above are required for normal operation of the interface.
 *  The HAL also relies on the user providing valid parameters. For example if
 *  2 channels are specified (e.g. @ref AUDIOHAL_CHANNELS_STEREO), the HAL
 *  expects that the buffer pointed to by the HAL will hold enough memory
 *  for numChannels*frameSize samples. The buffering system is described below.
 *
 *  ## Setting underlying driver parameters #
 *
 *  As mentioned above, the AudioHAL ultimately owns and manages the underlying
 *  drivers required to implement an interface (e.g. I2S, PDM), however, there
 *  may be a need to modify the driver parameters directly to modify
 *  the defaults used by the driver. The AudioHAL supports this by way of an
 *  `arg` parameter. The `arg` parameter is interpreted by the low level
 *  implementation as a pointer to the params structure for that driver.
 *  For example when using `AudioHALCodec` which is designed for connecting to
 *  I2S based codec devices, the arg parameters will be interpreted as
 *  `I2S_Params`.
 *
 *  See below for an example of using the arg parameter to override specific
 *  parameters in the underlying driver. It should be noted that this usually
 *  isn't necessary, but is a feature available to users who need this level
 *  of customization
 *
 *
 *  @code
 *  I2S_Params driverParams;
 *  I2S_Params_init(&driverParams);
 *  driverParams.moduleRole = I2S_MASTER;
 *  audioHalParams.arg = (void *)&driverParams;
 *  @endcode
 *
 *  @warning It is never recommend to override the driver with any parameter
 *  that can be set by the AudioHAL. For example this includes bitDepth and
 *  number of channels. The AudioHAL implementation will manage these parameters
 *  and relies on them for normal operation. The same is true for callbacks.
 *  The AudioHAL will always be responsible for plugging the callbacks as
 *  needed by the lower level driver.
 *
 *  ## Callbacks #
 *
 *  The AudioHAL notifies the application of activity via callback.
 *  These callbacks are usually executed from the underlying driver callbacks,
 *  and thus execute in an elevated context (e.g. SWI, HWI). Callbacks should
 *  be optimized for short execution and wherever possible critical sections
 *  should be avoided in the callbacks. The section below will explain the role
 *  of each of the callbacks. As mentioned above, the callbacks are required for
 *  the HAL to function properly and must be implemented by the application.
 *
 *  ### Data Needed Callback #
 *
 *  The data needed callback is invoked when the low level audio interface
 *  is configured for output (e.g. @ref AUDIOHAL_OUTPUT or
 *  @ref AUDIOHAL_INPUT_OUTPUT) and requires new audio data from the application
 *  to be written to the low level interface. It also means that a buffer slot
 *  is available in the HAL to accept this data.
 *
 *  On reception of the data needed callback the application is recommended
 *  to call @ ref AudioHAL_writeBufPut. This will queue a buffer up to be
 *  written by the underlying interface at the tail of the queue.
 *  @ref AudioHAL_writeBufPut must be called sufficiently often by the
 *  application in order to ensure that underflow does not occur inside the
 *  HAL. The @ref AudioHAL_dataNeededCB will notify the application as buffers
 *  are consumed (i.e. as driver consumes frames) so based on the
 *  @ref AudioHAL_Params.writeBufDepth the application can discern how likely
 *  underflow is and mitigate it.
 *
 *  ### Data Ready Callback #
 *
 *  The data ready callback is invoked when the low level audio interface
 *  is configured for input (e.g. @ref AUDIOHAL_INPUT or
 *  @ref AUDIOHAL_INPUT_OUTPUT) and the interface has new Audio data ready for
 *  consumption by the application. It also means that a buffer slot inside the
 *  HAL has been consumed by an incoming audio frame.
 *
 *  On reception of the data ready callback, the application is recommended to
 *  call @ref AudioHAL_readBufGet. This will remove a buffer from the head of
 *  the input queue.  @ref AudioHAL_readBufGet must be called sufficiently often
 *  to ensure that the queue is not overflow by the low level driver.
 *  The @ref AudioHAL_dataReadyCB will notify the application as the low level
 *  driver fills up the queue.  @ref AudioHAL_Params.readBufDepth determines
 *  how many buffer entries are in the queue, and determines how many
 *  buffers may be queued up by the low level driver before data will be lost
 *  due to overflow.
 *
 *  # Buffer Management #
 *
 *  The AudioHAL is responsible for initializing and managing the actual
 *  sample buffers that store the samples that make up an audio frame.
 *  A frame is a single buffer of samples. The size and properties of the
 *  frame are determined by the following @ref AudioHAL_Params
 *
 *  - @ref AudioHAL_Params.frameSize    // The number of samples per channel in the frame
 *  - @ref AudioHAL_Params.numChannels  // The number of channels in frame (mono, stereo)
 *  - @ref AudioHAL_Params.bitDepth     // The size of each sample in bits
 *
 *  An Audio Frame must be a contiguous block of memory. It is the application's
 *  responsibility to allocate the memory for the frames. For example, a single
 *  frame with two channels would look like this where N is the frame size.
 *
 *  | chan1        | chan2
 *  |--------------|--------------|
 *  | chan1Samp[0] | chan2Samp[0] |
 *  | chan1Samp[1] | chan2Samp[1] |
 *  | chan1Samp[2] | chan2Samp[2] |
 *  | ...          | ...          |
 *  | chan1Samp[N] | chan2Samp[N] |
 *
 *  Each sample can represent multiple bits. Samples are stored little endian.
 *  For example, a 24-bit sample look be shown as below where low blocks
 *  are low addresses.
 *
 * | 24 bit sample|
 * |--------------|
 * | samp0[7:0]   |
 * | samp0[15:8]  |
 * | samp0[23:16] |
 *
 *  With the data above, a single frame with samples of uint16_t type and two
 *  channels could be allocated using the following static buffer
 *
 *  @code
 *  int16_t readBuf[NUM_CHAN][FRAME_SIZE]
 *  @endcode
 *
 *  It could also be dynamically allocated using the following call
 *
 *  @code
 *  uint16_t *frame = malloc(sizeof(uint16_t)*NUM_CHAN*FRAME_SIZE);
 *  @endcode
 *
 *  A single buffer of this size will be sent to/received from the HAL with
 *  a call to @ref AudioHAL_writeBufPut or @ref AudioHAL_readBufGet
 *  respectively.
 *
 *  However the HAL requires multiple frames to be buffered up at once to
 *  enable the application to operate on a buffer while another is being used
 *  by the driver. This is discussed in the next section.
 *
 *  ## Buffer Depth #
 *
 *  The buffer depth is defined as the number of audio frames that the HAL
 *  can queue for each direction (e.g. read, write). The depth is configurable
 *  at initialization, but cannot be changed during stream. This allows for a
 *  constant, predictable audio latency. In the HAL the latency is defined
 *  as the buffer depth times the length of each frame. Increasing the latency
 *  frees up processing time for the main CPU before it needs to provide another
 *  frame, but is done at the cost of RAM allocation.
 *
 *  The buffer depth is configured with the @ref AudioHAL_Params.readBufDepth
 *  and @ref  AudioHAL_Params.writeBufDepth parameters.
 *
 *  ### Creating read and write buffers
 *
 *  The parameters @ref AudioHAL_Params.readBuf and
 *  @ref AudioHAL_Params.writeBuf are pointers to the buffer of audio frames
 *  for the read and write interface. These must point to a contiguous
 *  memory block that can hold at least `bufDepth` number of frames.
 *
 *  For example static creation of read and write buffers for a `bitDepth` of
 *  16 can be set as below
 *
 *  @code
 *  int16_t readBuf[NUM_CHAN][FRAME_SIZE][AUDIOHAL_DEFAULT_BUF_DEPTH];
 *  int16_t writeBuf[NUM_CHAN][FRAME_SIZE][AUDIOHAL_DEFAULT_BUF_DEPTH];
 *  @endcode
 *
 *  the same buffers could be allocated using a dynamic approach
 *
 *  @code
 *  int16_t *readBuf = malloc(sizeof(uint16_t)*NUM_CHAN*FRAME_SIZE*AUDIOHAL_DEFAULT_BUF_DEPTH);
 *  int16_t *writeBuf = malloc(sizeof(uint16_t)*NUM_CHAN*FRAME_SIZE*AUDIOHAL_DEFAULT_BUF_DEPTH);
 *  @endcode
 *
 *  The buffers above would use the following parameters to initialize the HAL.
 *
 *  @code
 *  audioHalParams.frameSize = FRAME_SIZE;
 *  audioHalParams.numChannels = NUM_CHAN;
 *  audioHalParams.readBuf = readBuf;
 *  audioHalParams.writeBuf = writeBuf;
 *  audioHalParams.bitDepth = AUDIOHAL_BITDEPTH_24;
 *  audioHalParams.samplingFrequency = SAMPLE_RATE;
 *  audioHalParams.readBufDepth = AUDIOHAL_DEFAULT_BUF_DEPTH;
 *  audioHalParams.writeBufDepth = AUDIOHAL_DEFAULT_BUF_DEPTH;
 *  @endcode
 *
 *  ### A note about 24-bit and 32-bit samples
 *
 *  There is not a type for 24-bit data in C, so it must be represented as uint8_t.
 *  In order to write code that is portable across devices, it is recommended to use the
 *  macro @ ref SIZEOF_AUDIOHAL_BITDEPTH_24 to set the size of these buffers.
 *
 *  The need for this is because the memory slot length a 24-bit sample differs
 *  between CC32xx and CC26xx/CC13xx.
 *
 *  - CC32xx: 24-bit sample consumes 32-bits
 *  - CC26xx/CC13xx: 24-bit sample consumes 24-bits
 *
 *  @warning 32-bit samples are not supported on CC26xx/CC13xx. The HAL will
 *  return NULL if the device is configured for this bitDepth. The CC32xx
 *  supports 32-bit samples.
 *
 *  It is important to note that the bitDepth parameter only configures
 *  the memory slot length of a sample. The HAL will configure the
 *  underlying serial interface (e.g. I2S) accordingly. However, if fine grained
 *  tweaks are needed (i.e. before or after word padding). This should be
 *  done via the @ref AudioHAL_Params.arg parameter.
 *
 *  ## Buffer Ring List #
 *
 *  Buffers will be queued up in a List using the TI List module.
 *  The list will be circular, meaning the head and tail point to one another.
 *
 *  A sample list of buffers when bufDepth is set to 3 is shown below.
 *
 *  @verbatim
 * /---------------------------------------------------------------------------------\
 * |  /---------------------------------------------------------------------------\  |
 * |  |                                                                           |  |
 * |  |  |----------------|        |----------------|        |----------------|   |  |
 * |  \--+   List Elem    |<-------+   List Elem    |<-------+   List Elem    |<--/  |
 * \---->|   pBuf         +------->|   pBuf         +------->|   pBuf         +------/
 *       |-------+--------|        |-------+--------|        |-------+--------|
 *               |                         |                         |
 *               |                         |                         |
 *               V                         V                         V
 *       |----------------|        |----------------|        |----------------|
 *       |   Frame        |        |   Frame        |        |   Frame        |
 *       |   Buf          |        |   Buf          |        |   Buf          |
 *       |                |        |                |        |                |
 *       |                |        |                |        |                |
 *       |                |        |                |        |                |
 *       |                |        |                |        |                |
 *       |----------------|        |----------------|        |----------------|
 *
 *  @endverbatim
 *
 *  # Starting and stopping the stream
 *
 *  The HAL is stream based, meaning that once the flow of data is enabled
 *  it runs continuously until stopped. The direction of the stream is
 *  defined by the direction parameter in the @ref AudioHAL_Params.
 *
 *  The stream can be started as below:
 *
 *  @code
 *  AudioHAL_Status stat = AudioHAL_startStream(tlv320Handle);
 *  @endcode
 *
 *  The stream can be stopped as below
 *P
 *  @code
 *  AudioHAL_Status stat = AudioHAL_stopStream(tlv320Handle);
 *  @endcode
 *
 *  # Processing Stream data
 *
 *  Once a stream is enabled the user provided callbacks will be invoked
 *  and data can be put into and read from the stream using the
 *  @ref AudioHAL_readBufGet and @ref AudioHAL_writeBufPut APIs.
 *
 *  Here is an example of reading a buffer from the stream
 *
 *
 *  @code
 *  int16_t audioIn[NUM_CHAN][FRAME_SIZE];
 *
 *  // ...
 *  AudioHAL_readBufGet(tlv320Handle, (void *)audioIn);
 *
 *  // Process audioIn here
 *  @endcode
 *
 *  Note that the put/get functions perform a deep copy, which means they
 *  require an input buffer and output buffer to operate on.
 *
 *  ============================================================================
 *  @{
 */
#ifndef ti_audio_AudioHAL__include
#define ti_audio_AudioHAL__include

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

/* DeviceFamily Includes */
#include <ti/devices/DeviceFamily.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 *                                          Constants
 * -----------------------------------------------------------------------------
 */

/*!
 *  @brief      The default buffer depth to be used by the AudioHAL.
 *
 *  This may be changed via @ref AudioHAL_Params.readBufDepth
 */
#define AUDIOHAL_DEFAULT_BUF_DEPTH  16

/* -----------------------------------------------------------------------------
 *                                          Typedefs
 * ------------------------------------------------------------------------------
 */
/*!
 *  @brief      A handle to an AudioHAL object
 *
 *  This structure hold the various variables needed for one instance of\
 *  an AudioHAL object
 */
typedef struct AudioHAL_Config_ * AudioHAL_Handle;

/*!
 *  @brief AudioHAL Events.
 *
 *  Notify application of changes to the stream state
 */
typedef enum {
    AUDIOHAL_STREAM_STARTED = 0x01U,    /*!< Stream has started successfully */
    AUDIOHAL_STREAM_STOPPED = 0x02U,    /*!< Stream stopped successfully */
    AUDIOHAL_STREAM_ERROR   = 0x03U,    /*!< Low level driver reported error */
} AudioHALEvent;

/*! @defgroup AudioHALCBs AudioHAL Callbacks
 * @brief    AudioHAL application callback functions
 * @{
 */
/*!
 *  @brief AudioHAL Data Ready Callback
 *
 *  This callback is invoked when the low layer audio implementation has
 *  received an audio frame that is ready for processing in the application.
 *
 *  Frames can be read from the HAL via @ref AudioHAL_readBufGet
 */
typedef void (*AudioHAL_dataReadyCB)(AudioHAL_Handle handle, int_fast16_t status);

/*!
 *  @brief AudioHAL Data Needed Callback
 *
 *  This callback is invoked when the low layer audio needs a frame from the
 *  application to be transmitted.
 *
 *  Frames can be input to the HAL via @ref AudioHAL_writeBufPut
 */
typedef void (*AudioHAL_dataNeededCB)(AudioHAL_Handle handle, int_fast16_t status);

/*!
 *  @brief AudioHAL Event Callback
 *
 *  This callback is invoked when the low layer audio implementation has changed
 *  the stream state.
 */
typedef void (*AudioHAL_eventCB)(AudioHAL_Handle handle, AudioHALEvent event);
/** @} End AudioHALCBs */

/*!
 *  @brief Audio HAL Bit Depth
 *
 *  This represents the size of each sample in bits.
 *
 *  @note The buffers passed into the HAL must be sized according to the
 *  bitDepth*numberofchannels
 */
typedef enum {
    AUDIOHAL_BITDEPTH_16       = 16U,   /*!<   Each sample is 16-bits */
    AUDIOHAL_BITDEPTH_24       = 24U,   /*!<   Each sample is 24-bits */
    AUDIOHAL_BITDEPTH_32       = 32U,   /*!<   Each sample is 32-bits */
} AudioHAL_BitDepth;

/*!
 *  @brief SIZEOF_AUDIOHAL_BITDEPTH_24
 *
 *  This is a convience macro for determing the side of samples of 24-bit type
 *
 *  Using this macro is the best way to make the code portable across platforms
 *  because different devies will use different memory slots to store a 24-bit
 *  sample.
 *
 *  For example
 *
 *   - CC32xx : 24-bit samples consume 32-bits of memory
 *   - CC26xx/CC13xx : 24-bit sample consume 24-bits of memory
 *
 *  When using 24-bit samples, always use the uint8_t type. For example
 *  this can be used to declare the read and write buffers as below:
 *  @code
 *  int8_t readBuf[NUM_CHAN*SIZEOF_AUDIOHAL_BITDEPTH_24][FRAME_SIZE][AUDIOHAL_DEFAULT_BUF_DEPTH];
 *  int8_t writeBuf[NUM_CHAN*SIZEOF_AUDIOHAL_BITDEPTH_24][FRAME_SIZE][AUDIOHAL_DEFAULT_BUF_DEPTH];
 *  @endcode
 *
 *  Later these buffers will be consumed by the HAL by way of the @ref AudioHAL_Params
 *  see below for example
 *
 *  @code
 *  audioHalParams.readBuf = readBuf;
 *  audioHalParams.writeBuf = writeBuf;
 *  audioHalParams.bitDepth = AUDIOHAL_BITDEPTH_24;
 *  @endcode
 */
#if (DeviceFamily_PARENT == DeviceFamily_PARENT_CC13X0_CC26X0) || \
    (DeviceFamily_PARENT == DeviceFamily_PARENT_CC13X2_CC26X2)
        #define SIZEOF_AUDIOHAL_BITDEPTH_24   (3*sizeof(uint8_t))
#else
        #define SIZEOF_AUDIOHAL_BITDEPTH_24   (sizeof(uint32_t))
#endif

/*!
 *  @brief    AudioHAL Channels
 *
 *  Describes the number of channels to be stored in the read and write buffers
 *  used by the HAL. When using stereo, data is interleaved.
 *
 */
typedef enum {
    AUDIOHAL_CHANNELS_MONO      = 0x01U,    /*!< Store 1 channel of data */
    AUDIOHAL_CHANNELS_STEREO    = 0x02U,    /*!< Store 2 channels of data */
} AudioHALChannels;

/*!
 *  @brief    AudioHAL Direction
 *
 *  Describes whether the HAL is to be used to read, write, or read and write
 *  audio data to the lower layer driver
 */
typedef enum {
    AUDIOHAL_NONE          = 0x00U,     /*!< Audio HAL no input, no output*/
    AUDIOHAL_INPUT         = 0x01U,     /*!< Audio HAL is used for input only */
    AUDIOHAL_OUTPUT        = 0x02U,     /*!< Audio HAL is used for output only */
    AUDIOHAL_INPUT_OUTPUT  = 0x03U,     /*!< Audio HAL is used for both input and output */
} AudioHALDir;

/*!
 *  @brief    AudioHAL Direction
 *
 *  Describes whether the HAL is to be used as slave or as master
 */
typedef enum {
    AUDIOHAL_SLAVE         = 0x01U,     /*!< Audio HAL is used as slave */
    AUDIOHAL_MASTER        = 0x02U,     /*!< Audio HAL is used as master */
} AudioHALRole;
/*!
 *  @brief    AudioHAL status
 *
 *  The AudioHAL Status is used to flag the different errors as return codes to
 *  AudioHAL API calls
 */
typedef enum {
    AUDIOHAL_SUCCESS            = 0x00U,  /*!< Operation succeeded */
    AUDIOHAL_DATA_OVERFLOW      = 0x01U,  /*!< Discarded old data in circular buffer */
    AUDIOHAL_DATA_UNDERFLOW     = 0x02U,  /*!< Ran out of data to write in circular buffer */
    AUDIOHAL_CMD_FAILED         = 0x03U,  /*!< A command returned failure */
    AUDIOHAL_INVALID_PARAM      = 0x04U,  /*!< Invalid parameters */
} AudioHAL_Status;

/*!
 *  @brief AudioHAL Parameters
 *
 *  AudioHAL parameters are used with the AudioHAL_open() call. Default values
 *  for these parameters are set using AudioHAL_Params_init().
 *
 *  @sa         AudioHAL_Params_init()
 */
typedef struct {
    uint32_t                samplingFrequency;  /*!< Sampling frequency (Fs) */
    AudioHAL_BitDepth       bitDepth;           /*!< Number of bits in each sample @ref AudioHAL_BitDepth*/
    uint32_t                frameSize;          /*!< Number of samples per channel in each frame */
    AudioHALChannels        numChannels;        /*!< Number of channels in each frame @ref AudioHALChannels */
    AudioHALDir             direction;          /*!< Directionality of the interface @ref AudioHALDir */
    AudioHALRole            role;               /*!< Role (Slave/Master) of the interface @ref AudioHALRole */
    bool                    invertMono;         /*!< Selects whether to sample mono data from channel 1 (invert = false) or 2 (invert = true) */
    void                    *readBuf;           /*!< Pointer to memory to hold input frames */
    void                    *writeBuf;          /*!< Pointer to memory to hold output frames */
    uint8_t                 readBufDepth;       /*!< Number of frames to hold in input buffer */
    uint8_t                 writeBufDepth;      /*!< Number of frames to hold in output buffer */
    AudioHAL_dataReadyCB    dataReadyCB;        /*!< Callback invoked when data is added to input buffer */
    AudioHAL_dataNeededCB   dataNeededCB;       /*!< Callback invoked when data is needed on output buffer */
    AudioHAL_eventCB        eventCB;            /*!< Callback to notify about state changes */
    void                    *arg;               /*!< Contains low level driver params (overriding defaults) */
} AudioHAL_Params;

/*!
 *  @brief      A function pointer to a specific implementation of
 *              AudioHAL_open().
 */
typedef AudioHAL_Handle (*AudioHAL_openFxn)(AudioHAL_Handle handle,
                                            const AudioHAL_Params *params);

/*!
 *  @brief      A function pointer to a specific implementation of
 *              AudioHAL_startStream().
 */
typedef AudioHAL_Status (*AudioHAL_startStreamFxn)(AudioHAL_Handle handle,
                                                    AudioHALDir dirToStart);

/*!
 *  @brief      A function pointer to a specific implementation of
 *              AudioHAL_stopStream().
 */
typedef AudioHAL_Status (*AudioHAL_stopStreamFxn)(AudioHAL_Handle handle,
                                                    AudioHALDir dirToStop);

/*!
 *  @brief      A function pointer to a specific implementation of
 *              AudioHAL_readBufGet().
 */
typedef AudioHAL_Status (*AudioHAL_readBufGetFxn)(AudioHAL_Handle handle,
                                                    void *frame);

/*!
 *  @brief      A function pointer to a specific implementation of
 *              AudioHAL_initFxn().
 */
typedef void (*AudioHAL_initFxn)(AudioHAL_Handle handle);

/*!
 *  @brief      A function pointer to a specific implementation of
 *              AudioHAL_writeBufPut().
 */
typedef AudioHAL_Status (*AudioHAL_writeBufPutFxn)(AudioHAL_Handle handle,
                                                    const void *frame);

/*!
 *  @brief      A function pointer to a specific implementation of
 *              AudioHAL_close().
 */
typedef void (*AudioHAL_closeFxn)(AudioHAL_Handle handle);

/*!
 *  @brief      The definition of a AudioHAL function table that contains the
 *              required set of functions to control a specific AudioHAL
 *              implementation.
 */
typedef struct
{
    /*! Function to open the specified instance of the HAL */
    AudioHAL_openFxn                openFxn;
    /*! Function to begin the stream of audio data to the application */
    AudioHAL_startStreamFxn         startStreamFxn;
    /*! Function to stop the stream of audio data to the application */
    AudioHAL_stopStreamFxn          stopStreamFxn;
    /*! Function to get a frame from the head of the receive buffer */
    AudioHAL_readBufGetFxn          readBufGetFxn;
    /*! Function to initialize the HAL */
    AudioHAL_initFxn                initFxn;
    /*! Function to place a frame a the tail of write buffer */
    AudioHAL_writeBufPutFxn         writeBufPutFxn;
    /*! Function to close and cleanup access to a given instance of the HAL */
    AudioHAL_closeFxn               closeFxn;
} AudioHAL_FxnTable;

/*!
 *  @brief AudioHAL Global configuration
 *
 *  The AudioHAL_Config structure contains a set of pointers used to
 * characterize the AudioHAL interface implementation.
 *
 *  This structure needs to be defined before calling AudioHAL_open() and it
 * must not be changed thereafter.
 *
 */
typedef struct AudioHAL_Config_
{
    /*! Pointer to a table of driver-specific implementations of AudioHAL APIs */
    AudioHAL_FxnTable const *fxnTablePtr;

    /*! Pointer to a driver specific data object */
    void                   *object;

    /*! Pointer to a driver specific hardware attributes structure */
    void const             *hwAttrs;
} AudioHAL_Config;

/* -----------------------------------------------------------------------------
 *                                          Functions
 * -----------------------------------------------------------------------------
 */

/*!
 *  @brief  Function to close a AudioHAL interface specified by the AudioHAL
 * handle
 *
 *  @pre    AudioHAL_open() has been called.
 *  @pre    Ongoing streaming should be stopped using
 *          AudioHAL_stopStream().
 *
 *  @param  handle      A #AudioHAL_Handle returned from AudioHAL_open()
 *
 *  @sa     AudioHAL_open()
 */
extern void AudioHAL_close(AudioHAL_Handle handle);

/*!
 *  @brief  Function to get a frame from the AudioHAL's input buffer
 *
 *  Function to retrieve a frame from the HAL's input buffer.
 *  This will also remove the frame from the buffer, thus freeing up a slot
 *  in the buffer for another entry.
 *
 *
 *  @param  handle      A AudioHAL handle returned from AudioHAL_open()
 *
 *  @param  frame       A frame buffer provided by AudioHal
 *
 *  @return see @ref AudioHAL_Status.
 *
 */
extern AudioHAL_Status AudioHAL_readBufGet(AudioHAL_Handle handle, void *frame);

/*!
 *  @brief  Function to initialize the audio interface
 *
 */
extern void AudioHAL_init(void);

/*!
 *  @brief  Function to open a given AudioHAL implementation
 *
 *  Function to open a given AudioHAL implementation specified by the
 *  particular index value.
 *
 *  @param  index         Logical peripheral number for the AudioHAL indexed into
 *                        the AudioHAL_config table
 *
 *  @param  params        Pointer to a parameter block. If NULL, default
 *                        parameter values will be used. All the fields in
 *                        this structure are RO (read-only).
 *
 *  @return A #AudioHAL_Handle upon success. NULL if an error occurs, or if the
 *          indexed AudioHAL peripheral is already opened.
 *
 *  @sa     AudioHAL_close()
 */
extern AudioHAL_Handle AudioHAL_open(uint_least8_t index, const AudioHAL_Params *params);

/*!
 *  @brief  Function to put a frame in the AudioHAL's output buffer
 *
 *  Function to put a frame in the HAL's output buffer.
 *  This will also consume a slot in the buffer list and copy the buffer into
 *  the slot.
 *
 *
 *  @param  handle      A AudioHAL handle returned from AudioHAL_open()
 *
 *  @param  frame       A pointer to a frame buffer
 *
 *  @return see @ref AudioHAL_Status.
 *
 */
extern AudioHAL_Status AudioHAL_writeBufPut(AudioHAL_Handle handle, const void *frame);

/*!
 *  @brief  Function to initialize the AudioHAL_Params struct to its defaults
 *
 *  @param  params      An pointer to AudioHAL_Params structure for
 *                      initialization.
 *
 *  Defaults values are:
 *
 *  @code
 *  .samplingFrequency = 16000,
 *  .bitDepth = AUDIOHAL_BITDEPTH_16,
 *  .frameSize = 320,
 *  .numChannels = AUDIOHAL_CHANNELS_STEREO,
 *  .direction = AUDIOHAL_INPUT_OUTPUT,
 *  .role = AUDIOHAL_MASTER,
 *  .readBuf = NULL,
 *  .writeBuf = NULL,
 *  .writeBufDepth = AUDIOHAL_DEFAULT_BUF_DEPTH,
 *  .readBufDepth = AUDIOHAL_DEFAULT_BUF_DEPTH,
 *  .dataReadyCB = NULL,
 *  .dataNeededCB = NULL,
 *  .eventCB = NULL,
 *  .arg = NULL
 *  @endcode
 *
 *  @note: It is important to note that these parameters will be used to
 *  setup the parameters for the underlying driver implementation (e.g. I2S).
 *
 */
extern void AudioHAL_Params_init(AudioHAL_Params *params);

/*!
 *  @brief  Function to begin stream of audio
 *
 *  Function to begin an audio stream. The directionality of this stream is
 *  determined by the @ref AudioHAL_Params.direction parameter.
 *  This will invoke @ref AudioHAL_dataReadyCB when data is ready from input
 *  and @ref AudioHAL_dataNeededCB when data is required on the output.
 *
 *  @param  handle      A AudioHAL handle returned from AudioHAL_open()
 *  @param  dirToStart  Which direction to start stream in @ref AudioHALDir
 *
 *  @note: @ref AUDIOHAL_NONE is not a valid option for the `dirToStart`
 *         parameter, a NONE stream cannot be started.
 *
 *  @return see @ref AudioHAL_Status.
 *
 */
extern AudioHAL_Status AudioHAL_startStream(AudioHAL_Handle handle,
                                            AudioHALDir dirToStart);

/*!
 *  @brief  Function to stop stream of audio
 *
 *  Function to stop an audio stream. Callbacks will no longer be invoked
 *  and the hardware will be placed in an idle state
 *
 *  @warning This function must be called in the task context
 *
 *  @param  handle      A AudioHAL handle returned from AudioHAL_open()
 *  @param  dirToStop   Which direction to stop stream in @ref AudioHALDir
 *
 *  @note: @ref AUDIOHAL_NONE is not a valid option for the `dirToStop`
 *         parameter, a NONE stream cannot be stopped.
 *
 *  @return see @ref AudioHAL_Status.
 *
 */
extern AudioHAL_Status AudioHAL_stopStream(AudioHAL_Handle handle,
                                            AudioHALDir dirToStop);


#ifdef __cplusplus
}
#endif

#endif /* ti_audio_AudioHAL__include */
/*! @} End AudioHAL */
