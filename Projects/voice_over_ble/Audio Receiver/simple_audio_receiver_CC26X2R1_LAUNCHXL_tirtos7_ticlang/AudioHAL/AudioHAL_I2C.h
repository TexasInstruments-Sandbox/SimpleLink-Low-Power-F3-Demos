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
 *  @file       AudioHAL_I2C.h
 *
 *  @defgroup AudioHAL_I2C AudioHAL I2C
 *
 *  @brief      I2C control implementation for codecs with paged register banks
 *
 * # Include #
 *
 * @code
 * #include <ti/audio/hal/codec/AudioHAL_I2C.h>
 * @endcode
 *
 * # Overview #
 *
 * Implements control interface to Audio codecs that can be controlled by I2C.
 * This module is built on top of the I2C driver and uses it to implement
 * register writes within the codec.
 *
 * This module also introduces the concept of an I2C register being comprised
 * of page, address, and value. This is combined into a single struct via
 * @ref AudioHAL_I2CReg. Arrays of @ref AudioHAL_I2CReg can be created to make
 * what is referred to as a "script", see @ref AudioHAL_I2CScript.
 * A script is a recipe for a chain of I2C writes.
 * A script may be passed into @ref AudioHAL_I2C_runScript which
 * will execute the chain of writes as described in the script, changing
 * pages along the way as necessary.
 *
 * # General Behavior #
 *
 * This module handles all the required interfacing to the I2C driver and provides
 * abstraction from the individual I2C writes required to execute a script.
 *
 *
 * # Error handling #
 *
 * The APIs here will check returns from the I2C driver and will return true
 * on success for failure if something went wrong.
 *
 * # Supported Functions #
 *
 * |API function                | Description                                       |
 * |----------------------------|---------------------------------------------------|
 * |@ref AudioHAL_I2C_open      | Open and initialize I2C driver                    |
 * |@ref AudioHAL_I2C_writeReg  | Writes a given register on the codec device       |
 * |@ref AudioHAL_I2C_runScript | Iterates through an array of @ref AudioHAL_I2CReg |
 * |                            | and calls @ref AudioHAL_I2C_writeReg              |
 * |@ref AudioHAL_I2C_close     | Close I2C driver                                  |
 *
 *
 * # Use Cases #
 *
 * These functions are intended to be called from the @ref AudioHALCodec
 * module.
 *
 *  ============================================================================
 *  @{
 */
#ifndef ti_audio_AudioHAL_AudioHAL_I2C__include
#define ti_audio_AudioHAL_AudioHAL_I2C__include

#include <stdint.h>
#include <ti/drivers/I2C.h>
#include <AudioHAL/AudioHAL.h>
#include <ti/sysbios/knl/Clock.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Timeout used for the I2C transfers. The value is given in number of system ticks
 */
#define AUDIOHAL_I2C_TIMEOUT                (1000 * (1000/Clock_tickPeriod))

/*!
 * Number of retries allowed when an I2C transfer fails because of a timeout.
 */
#define AUDIOHAL_I2C_RETRIES_ON_TIMEOUT     5 /* times */

/*!
 * Structure defining an I2C register in a paged register map
 *
 * @note arrays of these structures are referred to as scripts and may be
 * executed by @ref AudioHAL_I2C_runScript. @ref AudioHAL_I2CScript
 */
typedef struct AudioHAL_I2CReg_
{
    uint8_t page;   /*!< Page that register resides on*/
    uint8_t addr;   /*!< Address of register within a given @ref page*/
    uint8_t value;  /*!< Value to write to register */
}
AudioHAL_I2CReg;

/*!
 * Structure defining a a chain of I2C codec commands prepended by its size
 */
typedef struct AudioHAL_I2CScript_ {
    /*! Number of elements in the array of @ ref AudioHAL_I2CReg in  @ref data */
    uint32_t             size;
    /*! Array of register writes */
    AudioHAL_I2CReg      *data;
}AudioHAL_I2CScript;

/*!
 * A structure that pairs a sampling frequency (Fs) with the
 * @ref AudioHAL_I2CScript required to setup the codec for the given FS.
 * This is done so that the sampling frequency can be set at runtime. The intent
 *  is that a higher layer can iterate through an array of
 * @ref AudioHAL_I2CclkCfg and find the script needed to configure the codec at
 * a given Fs.
 */
typedef struct AudioHAL_I2CclkCfg_ {
    /*! Sampling frequency */
    uint32_t            samplingFrequency;
    /*! Controls whether script is made for codec as master or slave */
    bool                master;
    /*! @ref AudioHAL_I2CScript consisting of register writes needed to setup codec at given Fs */
    AudioHAL_I2CScript  *script;
} AudioHAL_I2CclkCfg;

/*!
 * A structure that pairs a bitDepth with the @ref AudioHAL_I2CScript required
 * to setup the codec with a given bitDepth as well as other interface settings.
 * This is done so that the bitDepth can be set at runtime. The intent
 * is that a higher layer can iterate through an array of
 * @ref AudioHAL_I2CintfCfg and find the script needed to configure the codec
 * as specific by the params.
 */
typedef struct AudioHAL_I2CintfCfg_ {
    /*! Number of bits per sample, @ref AudioHAL_BitDepth */
    AudioHAL_BitDepth   bitDepth;
    /*! Controls whether script is made for codec as master or slave */
    bool                master;
    /*! @ref AudioHAL_I2CScript consisting of register writes needed to setup codec at given bitDepth */
    AudioHAL_I2CScript  *script;
} AudioHAL_I2CintfCfg;

/*!
 *  @brief  Open I2C index and initialize driver
 *
 *  @param  i2cIdx       Index of I2C instance to open
 *
 *  @return Valid I2C_Handle if success, else NULL
 *
 */
I2C_Handle AudioHAL_I2C_open(uint_least8_t i2cIdx);

/*!
 *  @brief  Write to an I2C register
 *
 *  Function will write a value to a given I2C register within an external
 *  Audio CODEC that accepts I2C as a control interface.
 *
 *
 *  @param  i2cHandle       A valid handle to the I2C driver instance used to
 *                          perform the write
 *  @param  codecI2CAddr    The I2C slave address of the CODEC
 *  @param  pageSelAddr     The the address of the page select register
 *  @param  page            The page of the register to be written, this assumes
 *                          that the device uses a paged register architecture
 *  @param  registerAddr    The address of the register within the page
 *  @param  registerValue   The value to write to the register
 *
 *  @return true if success, false otherwise.
 *
 */
bool AudioHAL_I2C_writeReg(I2C_Handle i2cHandle , uint8_t codecI2CAddr,
                                    uint8_t pageSelAddr, uint8_t page,
                                    uint8_t registerAddr,
                                    uint8_t registerValue);

/*!
 *  @brief  Run a "script" of I2C commands
 *
 *  This function will execute an array of I2C register writes on the CODEC
 *  Each item in the array contains the page, address, and value of the register
 *  to be written. This function just iterates through the array and executes
 *  the I2C writes.
 *
 *
 *  @param  i2cHandle       A valid handle to the I2C driver instance used to
 *                          perform the write
 *  @param  i2cAddr         The I2C slave address of the CODEC
 *  @param  pageSelAddr     The the address of the page select register
 *  @param  script          A pointer to an array of @ref AudioHAL_I2CReg commands
 *                          to be executed in order
 *
 *  @return true if success, false otherwise.
 *
 */
bool AudioHAL_I2C_runScript( I2C_Handle i2cHandle , uint8_t i2cAddr,
                            uint8_t pageSelAddr,
                            const AudioHAL_I2CScript *script);

/*!
 *  @brief  Close I2C driver with given handle
 *
 *  @param  i2cHandle  I2C Handle to close
 *
 */
void AudioHAL_I2C_close(I2C_Handle i2cHandle);


#ifdef __cplusplus
}
#endif

#endif /* ti_audio_AudioHAL_AudioHAL_I2C__include */

/*! @} End AudioHAL_I2C */
