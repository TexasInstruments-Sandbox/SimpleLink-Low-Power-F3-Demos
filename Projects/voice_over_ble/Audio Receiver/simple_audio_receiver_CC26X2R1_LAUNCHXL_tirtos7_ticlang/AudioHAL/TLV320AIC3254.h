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
 *  @file       TLV320AIC3254.h
 *
 *  @defgroup TLV320AIC3254 TLV320AIC3254
 *
 *  @brief  I2C register map and scripts for TLV320AIC3254
 *
 * # Include #
 *
 * @code
 * #include <ti/audio/hal/codec/TLV320AIC3254.h>
 * @endcode
 *
 * # Overview #
 *
 * This file defines the relevant registers within the TLV320AIC3254.
 * Register addresses and pages are mapped to defines. That can be used in
 * code. This file is intended to be used in conjunction with the
 * @ref AudioHAL_I2C module which provides a generic I2C interface to a codec
 * device.
 *
 * # Codec Scripts #
 *
 * In addition to defining the register map and pages, the associated C file is responsible
 * for defining the register scripts that are to be executed during AudioHAL
 * usage. A script is a collection of registers to be written and their intended
 * value. Essentially, these scripts are arrays of @ref AudioHAL_I2CReg
 * The AudioHAL layer will invoke these scripts to setup the codec
 * for various streaming states.
 *
 * These scripts are passed to the codec based AudioHAL implementation
 * by way of pointers in the HWAttrs in the board file.
 *
 * @note All codec specific settings are included in this file and if
 * another codec is to be supported a file similar file can be created. The
 * rest of the AudioHAL will not need to be changed.
 *
 *  Required scripts are:
 *      - Init script: Reset the device to a known state. This is run on
 *                      AudioHAL open
 *      - Clock Config script: This is a special structure that defines the
 *                             clock register settings for various sampling
 *                             frequencies. This script is run after init
 *      - Open script: Perform additional initialization after the clock/PLL
 *                      init.
 *      - Start stream script: Script to be executed right before audio
 *                              streaming is enabled
 *      - Stop stream script: Script to be run right as audio streaming is
 *                             stopped
 *      - Close script: Power down the codec, as it is no longer used.
 *
 * The definitions below come from: <a href="http://www.ti.com/lit/an/slaa408a/slaa408a.pdf">TLV320AIC3254 Application Reference Guide</a>
 *  ============================================================================
 *  @{
 */
#ifndef ti_audio_TLV320AIC3254__include
#define ti_audio_TLV320AIC3254__include

#include <AudioHAL/AudioHAL_I2C.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 * Default I2C slave address of the TLV320AIC3254.
 *
 * The codec is controlled via I2C and operates as a slave. This is its address.
 */
#define TI3254_I2C_SLAVE_ADDR  ((0x30 >> 1))

/*!
 * @defgroup Codec_Pages Codec Page Definitions
 * @brief  Defines associated with codec's register pages.
 * These values are programmed into the @ref TI3254_PAGE_SEL_REG to change
 * the active page.
 * @{
 */
#define TI3254_PAGE_0                   0x00    /*!< Codec register page 0  */
#define TI3254_PAGE_1                   0x01    /*!< Codec register page 1  */
#define TI3254_PAGE_8                   0x08    /*!< Codec register page 8  */
#define TI3254_PAGE_44                  0x2C    /*!< Codec register page 44 */
/*! @} End Codec_Pages */


/*!
 * @defgroup Page0_regs Page 0 Register Definitions
 * @brief Defines associated with codec's page 0 registers.
 * @{
 */
#define TI3254_PAGE_SEL_REG             0x00    /*!< Page Select Register  */
#define TI3254_SW_RESET_REG             0x01    /*!< Software Reset Register */
#define TI3254_CLK_MUX_REG              0x04    /*!< Clock Setting Register 1, Multiplexers */
#define TI3254_CLK_PLL_P_R_REG          0x05    /*!< Clock Setting Register 2, PLL P and R Values */
#define TI3254_CLK_PLL_J_REG            0x06    /*!< Clock Setting Register 3, PLL J Values */
#define TI3254_CLK_PLL_D_MSB_REG        0x07    /*!< Clock Setting Register 4, PLL D Values (MSB) */
#define TI3254_CLK_PLL_D_LSB_REG        0x08    /*!< Clock Setting Register 5, PLL D Values (LSB) */
#define TI3254_CLK_NDAC_REG             0x0B    /*!< Clock Setting Register 6, NDAC Values */
#define TI3254_CLK_MDAC_REG             0x0C    /*!< Clock Setting Register 7, MDAC Values */
#define TI3254_DAC_OSR_MSB_REG          0x0D    /*!< DAC OSR Setting Register 1, MSB Value */
#define TI3254_DAC_OSR_LSB_REG          0x0E    /*!< DAC OSR Setting Register 2, LSB Value */
#define TI3254_DSP_D_CTRL_1_REG         0x0F    /*!< miniDSP_D Instruction Control Register 1 */
#define TI3254_DSP_D_CTRL_2_REG         0x10    /*!< miniDSP_D Instruction Control Register 2 */
#define TI3254_DSP_D_INTERPOL_REG       0x11    /*!< miniDSP_D Interpolation Factor Setting Register */
#define TI3254_CLK_NADC_REG             0x12    /*!< Clock Setting Register 8, NADC Values */
#define TI3254_CLK_MADC_REG             0x13    /*!< Clock Setting Register 9, MADC Values */
#define TI3254_ADC_OSR_REG              0x14    /*!< ADC Oversampling (AOSR) Register */
#define TI3254_DSP_A_CTRL_1_REG         0x15    /*!< miniDSP_A Instruction Control Register 1 */
#define TI3254_DSP_A_CTRL_2_REG         0x16    /*!< miniDSP_A Instruction Control Register 2 */
#define TI3254_DSP_A_DEC_FACT_REG       0x17    /*!< miniDSP_A Decimation Factor Setting Register */
#define TI3254_AUDIO_IF_1_REG           0x1B    /*!< Audio Interface Setting Register 1 */
#define TI3254_AUDIO_IF_3_REG           0x1D    /*!< Audio Interface Setting Register 3 */
#define TI3254_BCLK_N_DIV               0x1E    /*!< Clock Setting Register 12, BCLK N Divider */
#define TI3254_DAC_SIG_P_BLK_CTRL_REG   0x3C    /*!< DAC Signal Processing Block Control Register */
#define TI3254_ADC_SIG_P_BLK_CTRL_REG   0x3D    /*!< ADC Signal Processing Block Control Register */
#define TI3254_DAC_CHANNEL_SETUP_1_REG  0x3F    /*!< DAC Channel Setup Register 1 */
#define TI3254_DAC_CHANNEL_SETUP_2_REG  0x40    /*!< DAC Channel Setup Register 2 */
#define TI3254_LEFT_DAC_VOL_CTRL_REG    0x41    /*!< Left DAC Channel Digital Volume Control Register */
#define TI3254_RIGHT_DAC_VOL_CTRL_REG   0x42    /*!< Right DAC Channel Digital Volume Control Register */
#define TI3254_ADC_CHANNEL_SETUP_REG    0x51    /*!< ADC Channel Setup Register */
#define TI3254_ADC_FINE_GAIN_ADJ_REG    0x52    /*!< ADC Fine Gain Adjust Register */
#define TI3254_LEFT_ADC_VOL_CTRL_REG    0x53    /*!< Left ADC Channel Volume Control Register */
#define TI3254_RIGHT_ADC_VOL_CTRL_REG   0x54    /*!< Right ADC Channel Volume Control Register */
/*! @} End Page0_regs */


/*!
 * @defgroup Page1_regs Page 1 Register Definitions
 * @brief Defines associated with codec's page 1 registers.
 * @{
 */
#define TI3254_PWR_CTRL_REG                 0x01    /*!< Power Configuration Register */
#define TI3254_LDO_CTRL_REG                 0x02    /*!< LDO Control Register */
#define TI3254_OP_DRV_PWR_CTRL_REG          0x09    /*!< Output Driver Power Control Register */
#define TI3254_HPL_ROUTING_SEL_REG          0x0C    /*!< HPL Routing Selection Register */
#define TI3254_HPR_ROUTING_SEL_REG          0x0D    /*!< HPR Routing Selection Register */
#define TI3254_LOL_ROUTING_SEL_REG          0x0E    /*!< LOL Routing Selection Register */
#define TI3254_LOR_ROUTING_SEL_REG          0x0F    /*!< LOR Routing Selection Register */
#define TI3254_HPL_DRV_GAIN_CTRL_REG        0x10    /*!< HPL Driver Gain Setting Register */
#define TI3254_HPR_DRV_GAIN_CTRL_REG        0x11    /*!< HPR Driver Gain Setting Register */
#define TI3254_LOL_DRV_GAIN_CTRL_REG        0x12    /*!< LOL Driver Gain Setting Register */
#define TI3254_LOR_DRV_GAIN_CTRL_REG        0x13    /*!< LOR Driver Gain Setting Register */
#define TI3254_HP_DRV_START_UP_CTRL_REG     0x14    /*!< Headphone Driver Startup Control Register */
#define TI3254_MICBIAS_CTRL_REG             0x33    /*!< MICBIAS Configuration Register */
#define TI3254_LEFT_MICPGA_P_CTRL_REG       0x34    /*!< Left MICPGA Positive Terminal Input Routing Configuration Register */
#define TI3254_LEFT_MICPGA_N_CTRL_REG       0x36    /*!< Left MICPGA Negative Terminal Input Routing Configuration Register */
#define TI3254_RIGHT_MICPGA_P_CTRL_REG      0x37    /*!< Right MICPGA Positive Terminal Input Routing Configuration Register */
#define TI3254_RIGHT_MICPGA_N_CTRL_REG      0x39    /*!< Right MICPGA Negative Terminal Input Routing Configuration Register */
#define TI3254_FLOAT_IP_CTRL_REG            0x3a    /*!< Floating Input Configuration Register */
#define TI3254_LEFT_MICPGA_VOL_CTRL_REG     0x3B    /*!< Left MICPGA Volume Control Register */
#define TI3254_RIGHT_MICPGA_VOL_CTRL_REG    0x3C    /*!< Right MICPGA Volume Control Register */
#define TI3254_ANALOG_IP_QCHRG_CTRL_REG     0x47    /*!< Analog Input Quick Charging Configuration Register */
#define TI3254_REF_PWR_UP_CTRL_REG          0x7B    /*!< Reference Power-up Configuration Register */
/*! @} End Page1_regs */

/*!
 * @defgroup Page8_regs Page 8 Register Definitions
 * @brief Defines associated with codec's page 8 registers.
 * @{
 */
#define TI3254_ADC_ADP_FILTER_CTRL_REG  0x01    /*!< ADC Adaptive Filter Configuration Register */
/*! @} End Page8_regs */

/*!
 * @defgroup Page44_regs Page 44 Register Definitions
 * @brief Defines associated with codec's page 44 registers.
 * @{
 */
#define TI3254_DAC_ADP_FILTER_CTRL_REG  0x01    /*!< DAC Adaptive Filter Configuration Register */
/*! @} End Page44_regs */


/*!
 * Default number of interface scripts supported by TLV320AIC3254
 */
#define TI3254_NUM_INTF_CFG              6U

/*!
 * Default number of clock scripts supported by TLV320AIC3254
 */
#define TI3254_NUM_CLK_CFG               10U


/*!
 * TLV320AIC3254 Open script
 *
 * Configures audio path within codec. This includes ADC, DAC, headphone, gain, and other settings
 */
extern AudioHAL_I2CScript  TLV320AIC3254_openScript;

/*!
 * TLV320AIC3254 Close script
 *
 * Powers down PLL and MICBIAS. Mutes and ADC, DAC
 */
extern AudioHAL_I2CScript  TLV320AIC3254_closeScript;

/*!
 * TLV320AIC3254 start streaming script
 *
 * Increases volume so stream can be heard
 */
extern AudioHAL_I2CScript  TLV320AIC3254_startScript;

/*!
 * TLV320AIC3254 stop streaming script
 *
 * Decreases volume so stream can be heard
 */
extern AudioHAL_I2CScript  TLV320AIC3254_stopScript;

/*!
 * TLV320AIC3254 clock config array
 *
 * Specifies an array scripts that can be used to configure the clock settings (Fs) for the chip
 */
extern AudioHAL_I2CclkCfg  TLV320AIC3254_clockConfig[];

/*!
 * TLV320AIC3254 interface config array
 *
 * Specifies an array scripts that can be used to configure the serial interface settings for the codec
 */
extern AudioHAL_I2CintfCfg TLV320AIC3254_interfaceConfig[];

/*!
 * Number of clock config scripts supported by TLV320AIC3254
 *
 * This specifies the number of entries of size @ref AudioHAL_I2CScript in the @ref TLV320AIC3254_clockConfig array
 */
extern uint8_t TLV320AIC3254_clockConfig_count;

/*!
 * Number of interface scripts supported by TLV320AIC3254
 *
 * This specifies the number of entries of size @ref AudioHAL_I2CScript in the @ref TLV320AIC3254_interfaceConfig array
 */
extern uint8_t TLV320AIC3254_interfaceConfig_count;

#ifdef __cplusplus
}
#endif


#endif /* ti_audio_TLV320AIC3254__include */
/*! @} End TLV320AIC3254 */
