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
/*! ============================================================================
 *  @file       AudioHALCodec.h
 *
 *  @defgroup AudioHALCodec AudioHAL Codec
 *
 *  @brief This module implements communication with codec devices that support
 *  I2C control interfaces and I2S audio interfaces.
 *
 * # Driver include #
 *  The AudioHAL header file should be included in an application as follows:
 *  @code
 *  #include <ti/audio/hal/AudioHAL.h>
 *  #include <ti/audio/hal/codec/AudioHALCodec.h>
 *  @endcode
 *
 *  Refer to @ref AudioHAL.h for a complete description of APIs.
 *  In general it is intended to use the highest level of abstraction
 *  (e.g. AudioHAL.h). It shouldn't be necessary to use the codec layer
 *  directly in the application.
 *
 * # Overview #
 *
 *  The general AudioHAL API should used in application code, i.e. AudioHAL_open()
 *  is used instead of AudioHALCodec_open(). The board file will define the HAL
 *  specific config, and casting in the general API will ensure that the correct
 *  HAL specific functions are called.
 *
 *  The AudioHALCodec is designed to interface to codecs that operate via
 *  I2S as a data interface and I2C as a control interface. AudioHALCodec
 *  is comprised of three components
 *
 *   - AudoHALCodec: Configure codec as necessary via I2C, receive samples via I2S. Setup MCLK signal as needed
 *   - AudioHAL_I2C: Helper functions for writing to/from I2C registers on codec devices
 *   - MCLKCC**XX: Helper functions for generating an MCLK signal on a given device.
 *
 * # General Behavior #
 *
 *  Before using the AudioHALCodec:
 *    - The AudioHAL is initialized by calling AudioHAL_init().
 *    - The HW is configured and flags system dependencies (e.g. IOs,
 *      power, etc.) by calling AudioHAL_open().
 *    - The RX and TX can operate independently of each other.
 *
 *  ## I2C codec configuration
 *
 *  AudioHALCodec is intended to interface to Codec devices that have an I2C
 *  based control interface. These devices contain I2C registers that
 *  control things such as sampling rate, serial format, and clock configuration.
 *
 *  AudioHALCodec will use the functions in `AudioHAL_I2C` to run a string of
 *  I2C register operations defined as a script. See @ref AudioHAL_I2CScript.
 *
 *  I2C scripts are passed into the HAL via entries in
 *  the @ref AudioHALCodec_HWAttrsV1 struct that is defined in the board file.
 *  The various scripts and their mapping to the variables in the HWAttrs
 *  are described below
 *
 * | HWAttrs Parameter                   | Executed During                | Description                                                                                         |
 * |-------------------------------------|--------------------------------|-----------------------------------------------------------------------------------------------------|
 * | AudioHALCodec_HWAttrsV1.clkCfg      | AudioHALCodec_open()           | Array of @ref AudioHAL_I2CScript that sets up the codecs clocking based on Fs and role              |
 * | AudioHALCodec_HWAttrsV1.intfCfg     | AudioHALCodec_open()           | Array of @ref AudioHAL_I2CScript that sets up the codecs interface settings. (e.g. bit depth)       |
 * | AudioHALCodec_HWAttrsV1.openScript  | AudioHALCodec_open()           | @ref AudioHAL_I2CScript that configures codec settings such as mini DSP settings, output path       |
 * | AudioHALCodec_HWAttrsV1.closeScript | AudioHALCodec_close()          | @ref AudioHAL_I2CScript that shuts down codec                                                       |
 * | AudioHALCodec_HWAttrsV1.startScript | AudioHALCodec_startStream()    | @ref AudioHAL_I2CScript that prepares the codec to stream I2S data                                  |
 * | AudioHALCodec_HWAttrsV1.startScript | AudioHALCodec_stopStream()     | @ref AudioHAL_I2CScript that stops the stream of data                                               |
 *
 *
 *  @note The codec may operate in master or slave mode at multiple sample rates. Additionally multiple bit depths may be
 *  supported by the codec. These settings are initialized in the `clkCfg` and `intfCfg` arrays of scripts respectively.
 *  In order to support the use case where the codec supports multiple sample rates and bit depths, an array of scripts
 *  must be used. On open, the AudioHALCodec will search for the clk and intf config that setup the codec based on the
 *  user provided parameters to the HAL. The fields `intfCfgSize` and `clkCfgSize` should be populated with the number of
 *  entries in the script array.
 *
 *  ### Make I2C optional
 *
 *  If the I2C configuration is not needed by the codec, or an external device does the configuration,
 *  the I2C communication may be made entirely optional. In this case, the AudioHALCodec will not
 *  open the I2C or attempt to execute any scripts. To do so, change the `AudioHALCodecHWAttrs` to
 *  null out the I2C entries. Note that if I2C is to be used then at least one `clkCfg` and `intfCfg`
 *  must be provided. Other scripts (open, close, start, stop) are optional.
 *
 *  @code
 *  const AudioHALCodec_HWAttrsV1 AudioHALCodecHWAttrs[] = {
 *      {
 *          .i2cIdx = CC26X2R1_LAUNCHXL_I2C0,
 *          .i2sIdx = CC26X2R1_LAUNCHXL_I2S0,
 *          .i2cAddr = TI3254_I2C_SLAVE_ADDR,
 *          .i2cPageSelAddr = TI3254_PAGE_SEL_REG,
 *          .openScript =  NULL,
 *          .closeScript =  NULL,
 *          .startScript =  NULL,
 *          .stopScript =  NULL,
 *          .clkCfg =  NULL,
 *          .clkCfgSize = 0,
 *          .intfCfg =  NULL,
 *          .intfCfgSize = 0,
 *          .mclkOpenFxn = NULL,
 *          .mclkStartFxn = NULL,
 *          .mclkStopFxn = NULL,
 *          .mclkCloseFxn = NULL
 *      }
 *  };
 *  @endcode
 *
 *  ### Choosing Driver Indicies
 *
 *  The AudioHALCodec requires the I2S and optionally the I2C driver to run. If the target device
 *  supports more than one instance of either of these interfaces, the index to open can be specified
 *  using the `i2sIdx` and `i2cIdx` field of the HWAttrs struct in the board file.
 *
 *  ### Setting Page Select Address
 *
 *  The AudioHALCodec's I2C support is intended to configure the control settings of the codec.
 *  It assumes that that the codec has a paged architecture consisting of multiple register banks
 *  that can be switched between with a write to a page select register. This register's address
 *  can be set with `i2cPageSelAddr`.
 *
 *  ### Setting I2C Address
 *
 *  The AudioHALCodec operates as an I2C master, the codec's slave address can be set with `i2cAddr`.
 *
 *  ## Master Clock (MCLK) Generation
 *
 *  Some devices such as CC13xx/CC26xx cannot generate a master clock signal when operating as an
 *  I2S slave. However, it may be advantageous to have the CCxxxx device operate in slave mode
 *  while generating a MCLK. In this case, the MCLK signal must be generated in SW using a timer
 *  or similar mechanism. If this functionality is desired, the HAL may open, close, start, and
 *  stop the MCLK signal. The MCLK functions are specified via the HW attrs structure in the board
 *  file.
 *
 *  @code
 *  .mclkOpenFxn = MCLKCC26XX_open,
 *  .mclkStartFxn = MCLKCC26XX_start,
 *  .mclkStopFxn = MCLKCC26XX_stop,
 *  .mclkCloseFxn = MCLKCC26XX_close
 *  @endcode
 *
 *  ### MCLK APIs
 *
 * | HWAttrs Parameter                    | Executed During                | Description                                              |
 * |--------------------------------------|--------------------------------|----------------------------------------------------------|
 * | AudioHALCodec_HWAttrsV1.mclkOpenFxn  | AudioHALCodec_open()           | Opens any H/W or SW necessary to generate an MCLK signal |
 * | AudioHALCodec_HWAttrsV1.mclkStartFxn | AudioHALCodec_startStream()    | Starts generation of MCLK signal                         |
 * | AudioHALCodec_HWAttrsV1.mclkStopFxn  | AudioHALCodec_stopStream()     | Stops generation of MCLK signal                          |
 * | AudioHALCodec_HWAttrsV1.mclkCloseFxn | AudioHALCodec_open()           | Closes MCLK, releases resources                          |
 *
 *  ### Disabling MCLK generation.
 *
 *  As with I2C, if the MCLK functons are specified to be NULL, then no signal
 *  will be generated. See the I2C example above for info on how to NULL these
 *  signals.
 *
 * # Supported Functions #
 * | Generic API function   | API function               | Description                                       |
 * |------------------------|----------------------------|---------------------------------------------------|
 * | AudioHAL_init()        | AudioHALCodec_init()       | Initialize member variables and I2C, I2S drivers  |
 * | AudioHAL_close()       | AudioHALCodec_close()      | Deallocate resources, stop clocks, close drivers  |
 * | AudioHAL_control()     | AudioHALCodec_control()    | RFU                                               |
 * | AudioHAL_readBufGet()  | AudioHALCodec_readBufGet() | Copy a read buffer into user specified memory     |
 * | AudioHAL_writeBufPut() | AudioHALCodec_writeBufPut()| Copy a write buffer from user specified memory    |
 * | AudioHAL_startStream() | AudioHALCodec_startStream()| Begin TX,RX using ring list                       |
 * | AudioHAL_stopStream()  | AudioHALCodec_stopStream() | Stop TX, RX, shutdown clocks                      |
 *
 *  @note All calls should go through the generic API
 *
 *  ============================================================================
 *  @{
 */
#ifndef ti_audio_AudioHAL_AudioHALCodec__include
#define ti_audio_AudioHAL_AudioHALCodec__include

#include <stdint.h>
#include <stdbool.h>

#include <AudioHAL/AudioHAL.h>
#include <AudioHAL/AudioHAL_I2C.h>
#include <ti/drivers/I2S.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! AudioHAL function table pointer, points to codec specific HAL implementations */
extern const AudioHAL_FxnTable AudioHALCodec_fxnTable;

/*! specifies the function signature for opening MCLK */
typedef bool (*MclkOpenFxn)(void);
/*! specifies the function signature for starting MCLK signal generation */
typedef void (*MclkStartFxn)(void);
/*! specifies the function signature for stopping MCLK signal generation */
typedef void (*MclkStopFxn)(void);
/*! specifies the function signature for closing MCLK */
typedef void (*MclkCloseFxn)(void);

/*!
 *  @brief      AudioHALCodec Hardware1attributes
 *
 *  This structure contains variables for the CODEC implementation of the
 *  AudioHAL.
 *
 *  A sample structure is shown below:
 *  @code
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
 *  @endcode
 *
 */
typedef struct {
    /*! Index of i2s in I2S_config[] */
    uint_least8_t                i2sIdx;
    /*! Index of i2c in I2C_config[] */
    uint_least8_t                i2cIdx;
    /*! I2C Address of the codec */
    uint8_t                     i2cAddr;
    /*! Address of the Page select register in the codec */
    uint8_t                     i2cPageSelAddr;
    /*! A script of I2C operations to be executed on open */
    AudioHAL_I2CScript          *openScript;
    /*! A script of I2C operations to be executed on close */
    AudioHAL_I2CScript          *closeScript;
    /*! A script of I2C operations to be executed on stream start */
    AudioHAL_I2CScript          *startScript;
    /*! A script of I2C operations to be executed on stream stop */
    AudioHAL_I2CScript          *stopScript;
    /*! An array of @ref AudioHAL_I2CScript with necessary scripts for configuring
     *  the codec device for various sampling frequencies
     */
    AudioHAL_I2CclkCfg          *clkCfg;
    /*! An array of @ref AudioHAL_I2CScript with necessary scripts for configuring
     *  the codec device for various serial interface configurations.
     */
    AudioHAL_I2CintfCfg         *intfCfg;
    /*! Number of entries in @ref AudioHALCodec_HWAttrsV1.intfCfg array */
    uint32_t                     intfCfgSize;
    /*! Number of entries in @ref AudioHALCodec_HWAttrsV1.clkCfg array */
    uint32_t                     clkCfgSize;
    /*! Function to open interface for generating MCLK signal (optional) */
    MclkOpenFxn                 mclkOpenFxn;
    /*! Function to start MCLK signal (optional) */
    MclkStartFxn                mclkStartFxn;
    /*! Function to stop MCLK signal (optional) */
    MclkStopFxn                 mclkStopFxn;
    /*! Function to close interface for generating MCLK signal (optional) */
    MclkCloseFxn                mclkCloseFxn;
} AudioHALCodec_HWAttrsV1;

/*!
 *  @brief      AudioHALCodec Object
 *
 *  The application must not access any member variables of this structure!
 */
typedef struct {
    /*! Sampling frequency (Fs) */
    uint32_t                samplingFrequency;
    AudioHAL_BitDepth       bitDepth;
    /*!< Number of bits in each sample @ref AudioHAL_BitDepth*/
    uint32_t                frameSize;
    /*!< Number of samples per channel in each frame */
    AudioHALChannels        numChannels;
    /*!< Number of channels in each frame @ref AudioHALChannels */
    AudioHALDir             direction;
    /*!< Directionality of the interface @ref AudioHALDir */
    AudioHALRole            role;
    /*!< Role (Slave/Master) of the interface @ref AudioHALRole */
    void                    *readBuf;
    /*!< Pointer to memory to hold input frames */
    uint8_t                 readBufDepth;
    /*!< Number of frames to hold in input buffer */
    I2S_Transaction         *readHead;
    /*!< Head of the input circular transaction buffer */
    I2S_Transaction         *readTail;
    /*!< Tail of the input circular transaction buffer */
    List_List               *readList;
    /*!< List to hold input transactions */
    void                    *writeBuf;
    /*!< Pointer to memory to hold output frames */
    uint8_t                 writeBufDepth;
    /*!< Number of frames to hold in output buffer */
    I2S_Transaction         *writeHead;
    /*!< Head of output circular transaction buffer */
    I2S_Transaction         *writeTail;
    /*!< Tail of output circular transaction buffer */
    List_List               *writeList;
    /*!< List to hold output transactions */
    AudioHAL_dataReadyCB    dataReadyCB;
    /*!< Callback invoked when data is added to input buffer */
    AudioHAL_dataNeededCB   dataNeededCB;
    /*!< Callback invoked when data is needed on output buffer */
    AudioHAL_eventCB        eventCB;
    /*!< Callback to notify about state changes */
    I2C_Handle              i2cHandle;
    /*!< Handle of I2C driver */
    I2S_Handle              i2sHandle;
    /*!< Handle of I2S driver */
    I2S_Transaction         *readTransactionBuf;
    /*!< Memory to hold input transactions */
    I2S_Transaction         *writeTransactionBuf;
    /*!< Memory to hold output transactions */
    AudioHALDir             activeStreams;
    /*!< Variable holding the stream directions that are active */
} AudioHALCodec_Object;

#ifdef __cplusplus
}
#endif

#endif /* ti_audio_AudioHAL_AudioHALCodec__include */
/*! @} End AudioHALCodec */
