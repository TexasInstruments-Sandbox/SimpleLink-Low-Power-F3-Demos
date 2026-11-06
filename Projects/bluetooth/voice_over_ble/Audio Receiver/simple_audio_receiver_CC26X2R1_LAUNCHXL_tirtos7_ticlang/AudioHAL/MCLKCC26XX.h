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
/** ============================================================================
 *  @file       MCLKCC26XX.h
 *
 *  @defgroup MCLKCC26XX Master Clock for CC26xx
 *
 *  @brief  MCLK signal derived from SCLK_HF on CC26xx/CC13xx based devices
 *
 * # Include #
 *
 * @code
 * #include <ti/audio/hal/codec/MCLKCC26XX.h>
 * @endcode
 *
 * # Overview #
 *
 * This module will generate an MCLK signal on CC26xx that is operating
 * in I2S slave mode. If operating  in I2S master mode, this should not be used
 * but instead let the I2S module generate the clocks.
 * This targets a specific case where the CC26xx is to be an I2S slave and also
 * generate an MCLK.
 *
 * The underlying implementation is GPTimer based as the IOC driverlib
 * By default GPT3 will be used to generate a 4Mhz MCLK signal on IOID_28.
 * This is changable by changing the @ref GPT_IDX, @ref MCLK_PIN_ID, and
 * @ref MCLK_FREQ.
 *
 * # General Behavior #
 * At a high level this is a abstraction for the GPTimerCC26XX IOC calls
 * required to generate an MCLK signal. This code depends does not check
 * for PIN collisions on a given IO. It assumes that the MCLK IO is owned by
 * this module.
 *
 * On @ref MCLKCC26XX_open <br>
 * In order to generate the MCLK, the SW will setup the GP timer in periodic
 * mode, with the compare event being routed to the IOC pin associated with
 * @ref MCLK_PIN_ID. On compare events, the GPT module will trigger
 * @ref MCLK_PIN_ID to toggle. By default, the pin is cleared.
 *
 * On @ref MCLKCC26XX_start <br>
 * The compare register will be loaded with the compare value need to generate
 * an MCLK with @ref MCLK_FREQ. The timer will be started and a dependency
 * will be placed on XOSC_HF for precise timing
 *
 * If a lighter weight interface is needed, the user should replace
 * the GPT functions their driverlib equivalents from Timer.h
 * However, then the user must guarantee that no other SW is using these
 * peripherals.
 *
 * # Error handling #
 *
 * Errors are returned if the module fails to open. Since there are no logic
 * errors possible in the module, if the module failed to open the it is
 * because one of the lower level drivers failed to open.
 *
 *
 * # Use Cases  #
 * This is intended to be called from the @ref AudioHALCodec module. These
 * files implements the register and driver calls necessary to generate an MCLK
 * signal. It is intended to remove device specific calls from the HAL
 * implementation.
 *
 *  ============================================================================
 *  @{
 */
#ifndef ti_audio_AudioHAL_MCLKCC26XX__include
#define ti_audio_AudioHAL_MCLKCC26XX__include

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @defgroup MCLKCC26XX_Settings MCLK settings
 *  @brief  Settings for clock generation
 *
 * @note NB! It is important that MCLK is defined as IOID unused in I2S hwAttrs
 * On CC26xx/CC13xx we use GPT to derive MCLK when in I2S slave mode because it
 * is not possible to output MCLK directly. See the snippet below for info
 * on how to set I2S hwAttrs.
 *
 * @code
 *   .pinMCLK     =  (PIN_Id)IOID_UNUSED,
 * @endcode
 * @{
 */

/*!
 * The instance of GP Timer to use to generate the MCLK. This should be
 * pulled from Board.h
 */
#define GPT_IDX     (Board_GPTIMER3A)
/*!
 * The PIN ID from IOC to output the MCLK signal on.
 *
 * @warning Driverlib APIs will be used directly on this pin. This means
 * that the MCLKCC26xx module assumes complete ownership of this pin.
 * If other code accesses this pin, unexpected behavior will occur
 */
#define MCLK_PIN_ID (IOID_23)
/*!
 * Desired MCLK frequency. This must be within limits of what is possible with
 * GPTimer as well as what the Codec will accept.
 */
#define MCLK_FREQ   (12000000U)
/*! @} End MCLKCC26XX_Settings */

/*!
 *  @brief  Open interface to generate MCLK signals on CC26xx
 *
 *  @return true if success, false otherwise.
 *
 */
extern bool MCLKCC26XX_open(void);

/*!
 *  @brief  Start generating MCLK signal
 *
 *  This function will start the GP timer to generate the MCLK signal
 *
 */
extern void MCLKCC26XX_start(void);

/*!
 *  @brief  Stop generating MCLK signal
 *
 *  This function will stop the GP timer that was used to generate MCLK
 *
 */
extern void MCLKCC26XX_stop(void);

/*!
 *  @brief  Close the MCLK interface
 *
 *  This function will close the underlying PIN and GPTimer interfaces used
 *  to generate the MCLK.
 *
 */
extern void MCLKCC26XX_close(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_audio_AudioHAL_MCLKCC26XX__include */
/*! @} End MCLKCC26XX */
