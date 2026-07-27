/*
 * Copyright (c) 2015-2019, Texas Instruments Incorporated
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

#include <AudioHAL/TLV320AIC3254.h>
#include <stdint.h>

/*
 *******************************************************************************
 * These scripts assume MCLK = 12MHz, Resource Class = 12
 * The values are generated using the AIC3254 CS Application contained in
 * http://www.ti.com/lit/zip/slac214
 *******************************************************************************
 */
AudioHAL_I2CReg clockRegs8k[] =
{
    {TI3254_PAGE_0, TI3254_CLK_MUX_REG, 0x03},                          /* PLL Clock is CODEC_CLKIN */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x80 | 0x30 | 0x01},        /* PLL is powered up, P=3, R=1 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_J_REG, 0x020},                       /* J=32 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_MSB_REG, 0x00},                    /* Set MSB of D value D=0x0000 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_LSB_REG, 0x00},                    /* Set LSB of D value */
    {TI3254_PAGE_0, TI3254_CLK_NDAC_REG, 0x80 | 0x19},                  /* NDAC divider powered on, NDAC = 25 */
    {TI3254_PAGE_0, TI3254_CLK_MDAC_REG, 0x80 | 0x05},                  /* MDAC divider powered on, MDAC = 5 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_MSB_REG, 0x00},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_LSB_REG, 0x80},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_CLK_NADC_REG, 0x80 | 0x19},                  /* NADC divider powered on, NADC = 25 */
    {TI3254_PAGE_0, TI3254_CLK_MADC_REG, 0x80 | 0x05},                  /* MADC divider powered on, MADC = 5 */
    {TI3254_PAGE_0, TI3254_ADC_OSR_REG, 0x80},                          /* AOSR = 128 ((Use with PRB_R1 to PRB_R6, ADC Filter Type A) */
    {TI3254_PAGE_0, TI3254_BCLK_N_DIV, 0x80 | 0x14},                    /* BCLK divider powered up, val = 20 */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_3_REG, 0x04},                       /* Primary BCLK and Primary WCLK buffers are powered up */
};

AudioHAL_I2CReg clockRegs8k_m[] =
{
    {TI3254_PAGE_0, TI3254_CLK_MUX_REG, 0x13},                          /* PLL Clock is CODEC_CLKIN */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x80 | 0x10 | 0x02},        /* PLL is powered up, P=1, R=2 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_J_REG, 0x008},                       /* J=8 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_MSB_REG, 0x00},                    /* Set MSB of D value D=0x0000 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_LSB_REG, 0x00},                    /* Set LSB of D value */
    {TI3254_PAGE_0, TI3254_CLK_NDAC_REG, 0x80 | 0x01},                  /* NDAC divider powered on, NDAC = 1 */
    {TI3254_PAGE_0, TI3254_CLK_MDAC_REG, 0x80 | 0x04},                  /* MDAC divider powered on, MDAC = 4 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_MSB_REG, 0x00},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_LSB_REG, 0x80},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_CLK_NADC_REG, 0x80 | 0x01},                  /* NADC divider powered on, NADC = 1 */
    {TI3254_PAGE_0, TI3254_CLK_MADC_REG, 0x80 | 0x04},                  /* MADC divider powered on, MADC = 4 */
    {TI3254_PAGE_0, TI3254_ADC_OSR_REG, 0x80},                          /* AOSR = 128 ((Use with PRB_R1 to PRB_R6, ADC Filter Type A) */
};

AudioHAL_I2CReg clockRegs16k[] =
{
    {TI3254_PAGE_0, TI3254_CLK_MUX_REG, 0x03},                          /* PLL Clock is CODEC_CLKIN */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x80 | 0x10 | 0x01},        /* PLL is powered up, P=1, R=1 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_J_REG, 0x008},                       /* J=8 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_MSB_REG, 0x07},                    /* Set MSB of D value D=0x0780 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_LSB_REG, 0x80},                    /* Set LSB of D value */
    {TI3254_PAGE_0, TI3254_CLK_NDAC_REG, 0x80 | 0x10},                  /* NDAC divider powered on, NDAC = 16 */
    {TI3254_PAGE_0, TI3254_CLK_MDAC_REG, 0x80 | 0x03},                  /* MDAC divider powered on, MDAC = 3 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_MSB_REG, 0x00},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_LSB_REG, 0x80},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_CLK_NADC_REG, 0x80 | 0x10},                  /* NADC divider powered on, NADC = 16 */
    {TI3254_PAGE_0, TI3254_CLK_MADC_REG, 0x80 | 0x03},                  /* MADC divider powered on, MADC = 3 */
    {TI3254_PAGE_0, TI3254_ADC_OSR_REG, 0x80},                          /* AOSR = 128 ((Use with PRB_R1 to PRB_R6, ADC Filter Type A) */
    {TI3254_PAGE_0, TI3254_BCLK_N_DIV, 0x80 | 0x0C},                    /* BCLK divider powered up, val = 12 */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_3_REG, 0x04},                       /* Primary BCLK and Primary WCLK buffers are powered up */
};

AudioHAL_I2CReg clockRegs16k_m[] =
{
    {TI3254_PAGE_0, TI3254_CLK_MUX_REG, 0x13},                          /* PLL Clock is CODEC_CLKIN */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x80 | 0x10 | 0x04},        /* PLL is powered up, P=1, R=4 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_J_REG, 0x2A},                        /* J=42 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_MSB_REG, 0x00},                    /* D = 0000 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_LSB_REG, 0x00},                    /* D = 0000 */
    {TI3254_PAGE_0, TI3254_CLK_NDAC_REG, 0x80 | 0x0E},                  /* NDAC divider powered up, NDAC = 14 */
    {TI3254_PAGE_0, TI3254_CLK_MDAC_REG, 0x80 | 0x01},                  /* MDAC divider powered up, MDAC = 1 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_MSB_REG, 0x01},                      /* DOSR = 0x0180 = 384 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_LSB_REG, 0x80},                      /* DOSR = 0x0180 = 384 */
    {TI3254_PAGE_0, TI3254_CLK_NADC_REG, 0x80 | 0x15},                  /* NADC divider powered on, NADC = 21 */
    {TI3254_PAGE_0, TI3254_CLK_MADC_REG, 0x80 | 0x02},                  /* MADC divider powered on, MADC = 2 */
    {TI3254_PAGE_0, TI3254_ADC_OSR_REG, 0x80},                          /* AOSR = 128 ((Use with PRB_R1 to PRB_R6, ADC Filter Type A) */
};

AudioHAL_I2CReg clockRegs32k[] =
{
    {TI3254_PAGE_0, TI3254_CLK_MUX_REG, 0x03},                          /* PLL Clock is CODEC_CLKIN */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x80 | 0x10 | 0x01},        /* PLL is powered up, P=1, R=1 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_J_REG, 0x008},                       /* J=8 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_MSB_REG, 0x07},                    /* Set MSB of D value D=0x0780 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_LSB_REG, 0x80},                    /* Set LSB of D value */
    {TI3254_PAGE_0, TI3254_CLK_NDAC_REG, 0x80 | 0x0C},                  /* NDAC divider powered on, NDAC = 12 */
    {TI3254_PAGE_0, TI3254_CLK_MDAC_REG, 0x80 | 0x02},                  /* MDAC divider powered on, MDAC = 2 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_MSB_REG, 0x00},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_LSB_REG, 0x80},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_CLK_NADC_REG, 0x80 | 0x0C},                  /* NADC divider powered on, NADC = 12 */
    {TI3254_PAGE_0, TI3254_CLK_MADC_REG, 0x80 | 0x02},                  /* MADC divider powered on, MADC = 2 */
    {TI3254_PAGE_0, TI3254_ADC_OSR_REG, 0x80},                          /* AOSR = 128 ((Use with PRB_R1 to PRB_R6, ADC Filter Type A) */
    {TI3254_PAGE_0, TI3254_BCLK_N_DIV, 0x80 | 0x08},                    /* BCLK divider powered up, val = 8 */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_3_REG, 0x04},                       /* Primary BCLK and Primary WCLK buffers are powered up */
};

AudioHAL_I2CReg clockRegs32k_m[] =
{
    {TI3254_PAGE_0, TI3254_CLK_MUX_REG, 0x13},                          /* PLL Clock is CODEC_CLKIN */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x80 | 0x10 | 0x03},        /* PLL is powered up, P=1, R=3 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_J_REG, 0x020},                       /* J=32 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_MSB_REG, 0x00},                    /* Set MSB of D value D=0x0000 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_LSB_REG, 0x00},                    /* Set LSB of D value */
    {TI3254_PAGE_0, TI3254_CLK_NDAC_REG, 0x80 | 0x08},                  /* NDAC divider powered on, NDAC = 8 */
    {TI3254_PAGE_0, TI3254_CLK_MDAC_REG, 0x80 | 0x03},                  /* MDAC divider powered on, MDAC = 3 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_MSB_REG, 0x00},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_LSB_REG, 0x80},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_CLK_NADC_REG, 0x80 | 0x08},                  /* NADC divider powered on, NADC = 8 */
    {TI3254_PAGE_0, TI3254_CLK_MADC_REG, 0x80 | 0x03},                  /* MADC divider powered on, MADC = 3 */
    {TI3254_PAGE_0, TI3254_ADC_OSR_REG, 0x80},                          /* AOSR = 128 ((Use with PRB_R1 to PRB_R6, ADC Filter Type A) */
};

AudioHAL_I2CReg clockRegs44p1k[] =
{
    {TI3254_PAGE_0, TI3254_CLK_MUX_REG, 0x03},                          /* PLL Clock is CODEC_CLKIN */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x80 | 0x10 | 0x01},        /* PLL is powered up, P=1, R=1 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_J_REG, 0x008},                       /* J=8 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_MSB_REG, 0x12},                    /* Set MSB of D value D=0x1240 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_LSB_REG, 0x40},                    /* Set LSB of D value */
    {TI3254_PAGE_0, TI3254_CLK_NDAC_REG, 0x80 | 0x06},                  /* NDAC divider powered on, NDAC = 6 */
    {TI3254_PAGE_0, TI3254_CLK_MDAC_REG, 0x80 | 0x03},                  /* MDAC divider powered on, MDAC = 3 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_MSB_REG, 0x00},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_LSB_REG, 0x80},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_CLK_NADC_REG, 0x80 | 0x06},                  /* NADC divider powered on, NADC = 6 */
    {TI3254_PAGE_0, TI3254_CLK_MADC_REG, 0x80 | 0x03},                  /* MADC divider powered on, MADC = 3 */
    {TI3254_PAGE_0, TI3254_ADC_OSR_REG, 0x80},                          /* AOSR = 128 ((Use with PRB_R1 to PRB_R6, ADC Filter Type A) */
    {TI3254_PAGE_0, TI3254_BCLK_N_DIV, 0x80 | 0x0C},                    /* BCLK divider powered up, val = 12 */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_3_REG, 0x04},                       /* Primary BCLK and Primary WCLK buffers are powered up */
};

AudioHAL_I2CReg clockRegs44p1k_m[] =
{
    {TI3254_PAGE_0, TI3254_CLK_MUX_REG, 0x13},                          /* PLL Clock is CODEC_CLKIN */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x80 | 0x10 | 0x02},        /* PLL is powered up, P=1, R=2 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_J_REG, 0x020},                       /* J=32 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_MSB_REG, 0x00},                    /* Set MSB of D value D=0x0000 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_LSB_REG, 0x00},                    /* Set LSB of D value */
    {TI3254_PAGE_0, TI3254_CLK_NDAC_REG, 0x80 | 0x04},                  /* NDAC divider powered on, NDAC = 4 */
    {TI3254_PAGE_0, TI3254_CLK_MDAC_REG, 0x80 | 0x04},                  /* MDAC divider powered on, MDAC = 4 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_MSB_REG, 0x00},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_LSB_REG, 0x80},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_CLK_NADC_REG, 0x80 | 0x04},                  /* NADC divider powered on, NADC = 4 */
    {TI3254_PAGE_0, TI3254_CLK_MADC_REG, 0x80 | 0x04},                  /* MADC divider powered on, MADC = 4 */
    {TI3254_PAGE_0, TI3254_ADC_OSR_REG, 0x80},                          /* AOSR = 128 ((Use with PRB_R1 to PRB_R6, ADC Filter Type A) */
};

AudioHAL_I2CReg clockRegs48k[] =
{
    {TI3254_PAGE_0, TI3254_CLK_MUX_REG, 0x03},                          /* PLL Clock is CODEC_CLKIN */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x80 | 0x10 | 0x01},        /* PLL is powered up, P=1, R=1 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_J_REG, 0x007},                       /* J=7 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_MSB_REG, 0x1A},                    /* Set MSB of D value D=0x1A90 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_LSB_REG, 0x90},                    /* Set LSB of D value */
    {TI3254_PAGE_0, TI3254_CLK_NDAC_REG, 0x80 | 0x05},                  /* NDAC divider powered on, NDAC = 5 */
    {TI3254_PAGE_0, TI3254_CLK_MDAC_REG, 0x80 | 0x03},                  /* MDAC divider powered on, MDAC = 3 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_MSB_REG, 0x00},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_LSB_REG, 0x80},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_CLK_NADC_REG, 0x80 | 0x05},                  /* NADC divider powered on, NADC = 5 */
    {TI3254_PAGE_0, TI3254_CLK_MADC_REG, 0x80 | 0x03},                  /* MADC divider powered on, MADC = 3 */
    {TI3254_PAGE_0, TI3254_ADC_OSR_REG, 0x80},                          /* AOSR = 128 ((Use with PRB_R1 to PRB_R6, ADC Filter Type A) */
    {TI3254_PAGE_0, TI3254_BCLK_N_DIV, 0x80 | 0x0C},                    /* BCLK divider powered up, val = 12 */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_3_REG, 0x04},                       /* Primary BCLK and Primary WCLK buffers are powered up */
};

AudioHAL_I2CReg clockRegs48k_m[] =
{
    {TI3254_PAGE_0, TI3254_CLK_MUX_REG, 0x13},                          /* PLL Clock is CODEC_CLKIN */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x80 | 0x10 | 0x02},        /* PLL is powered up, P=1, R=2 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_J_REG, 0x020},                       /* J=32 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_MSB_REG, 0x00},                    /* Set MSB of D value D=0x0000 */
    {TI3254_PAGE_0, TI3254_CLK_PLL_D_LSB_REG, 0x00},                    /* Set LSB of D value */
    {TI3254_PAGE_0, TI3254_CLK_NDAC_REG, 0x80 | 0x04},                  /* NDAC divider powered on, NDAC = 4 */
    {TI3254_PAGE_0, TI3254_CLK_MDAC_REG, 0x80 | 0x04},                  /* MDAC divider powered on, MDAC = 4 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_MSB_REG, 0x00},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_DAC_OSR_LSB_REG, 0x80},                      /* DOSR = 128 */
    {TI3254_PAGE_0, TI3254_CLK_NADC_REG, 0x80 | 0x04},                  /* NADC divider powered on, NADC = 4 */
    {TI3254_PAGE_0, TI3254_CLK_MADC_REG, 0x80 | 0x04},                  /* MADC divider powered on, MADC = 4 */
    {TI3254_PAGE_0, TI3254_ADC_OSR_REG, 0x80},                          /* AOSR = 128 ((Use with PRB_R1 to PRB_R6, ADC Filter Type A) */
};

AudioHAL_I2CScript clockScript8k =
{
    .size = sizeof(clockRegs8k)/ sizeof(AudioHAL_I2CReg),
    .data = clockRegs8k,
};

AudioHAL_I2CScript clockScript8k_m =
{
    .size = sizeof(clockRegs8k_m)/ sizeof(AudioHAL_I2CReg),
    .data = clockRegs8k_m,
};

AudioHAL_I2CScript clockScript16k =
{
    .size = sizeof(clockRegs16k)/ sizeof(AudioHAL_I2CReg),
    .data = clockRegs16k,
};

AudioHAL_I2CScript clockScript16k_m =
{
    .size = sizeof(clockRegs16k_m)/ sizeof(AudioHAL_I2CReg),
    .data = clockRegs16k_m,
};

AudioHAL_I2CScript clockScript32k =
{
    .size = sizeof(clockRegs32k)/ sizeof(AudioHAL_I2CReg),
    .data = clockRegs32k,
};

AudioHAL_I2CScript clockScript32k_m =
{
    .size = sizeof(clockRegs32k_m)/ sizeof(AudioHAL_I2CReg),
    .data = clockRegs32k_m,
};

AudioHAL_I2CScript clockScript44p1k =
{
    .size = sizeof(clockRegs44p1k)/ sizeof(AudioHAL_I2CReg),
    .data = clockRegs44p1k,
};

AudioHAL_I2CScript clockScript44p1k_m =
{
    .size = sizeof(clockRegs44p1k_m)/ sizeof(AudioHAL_I2CReg),
    .data = clockRegs44p1k_m,
};

AudioHAL_I2CScript clockScript48k =
{
    .size = sizeof(clockRegs48k)/ sizeof(AudioHAL_I2CReg),
    .data = clockRegs48k,
};

AudioHAL_I2CScript clockScript48k_m =
{
    .size = sizeof(clockRegs48k_m)/ sizeof(AudioHAL_I2CReg),
    .data = clockRegs48k_m,
};

AudioHAL_I2CclkCfg TLV320AIC3254_clockConfig[TI3254_NUM_CLK_CFG] =
{
    {
        .samplingFrequency = 8000,
        .master = false,
        .script = &clockScript8k,
    },
    {
        .samplingFrequency = 8000,
        .master = true,
        .script = &clockScript8k_m,
    },
    {
        .samplingFrequency = 16000,
        .master = false,
        .script = &clockScript16k,
    },
    {
        .samplingFrequency = 16000,
        .master = true,
        .script = &clockScript16k_m,
    },
    {
        .samplingFrequency = 32000,
        .master = false,
        .script = &clockScript32k,
    },
    {
        .samplingFrequency = 32000,
        .master = true,
        .script = &clockScript32k_m,
    },
    {
        .samplingFrequency = 44100,
        .master = false,
        .script = &clockScript44p1k,
    },
    {
        .samplingFrequency = 44100,
        .master = true,
        .script = &clockScript44p1k_m,
    },
    {
        .samplingFrequency = 48000,
        .master = false,
        .script = &clockScript48k,
    },
    {
        .samplingFrequency = 48000,
        .master = true,
        .script = &clockScript48k_m,
    }
};

uint8_t TLV320AIC3254_clockConfig_count = sizeof(TLV320AIC3254_clockConfig) / sizeof(TLV320AIC3254_clockConfig[0]);

/*
 *******************************************************************************
 * These scripts assume that codec generates BCLK, WCLK, and I2S mode as slave
 * The register values are taken from http://www.ti.com/lit/an/slaa408a/slaa408a.pdf
 * Configure clock and PLL of TLV320AIC3254 on CC3200AUDBOOST
 *
 * Note this script is responsible for resetting the interface to a known state
 *******************************************************************************
 */
AudioHAL_I2CReg configRegs16Bit[] =
{
    {TI3254_PAGE_0, TI3254_SW_RESET_REG, 0x01},                         /* Reset the codec */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_1_REG, 0x0C},                       /* 16bit, I2S, WCLK, BCLK is output to the device */
};

AudioHAL_I2CReg configRegs16Bit_m[] =
{
    {TI3254_PAGE_0, TI3254_SW_RESET_REG, 0x01},                         /* Reset the codec */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_1_REG, 0x00},                       /* 16bit, I2S, WCLK, BCLK is input to the device */
};

AudioHAL_I2CReg configRegs24Bit[] =
{
    {TI3254_PAGE_0, TI3254_SW_RESET_REG, 0x01},                         /* Reset the codec */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_1_REG, 0x2C},                       /* 24bit, I2S, WCLK, BCLK is output to the device */
};

AudioHAL_I2CReg configRegs24Bit_m[] =
{
    {TI3254_PAGE_0, TI3254_SW_RESET_REG, 0x01},                         /* Reset the codec */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_1_REG, 0x20},                       /* 24bit, I2S, WCLK, BCLK is input to the device */
};

AudioHAL_I2CReg configRegs32Bit[] =
{
    {TI3254_PAGE_0, TI3254_SW_RESET_REG, 0x01},                         /* Reset the codec */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_1_REG, 0x3C},                       /* 32bit, I2S, WCLK, BCLK is output to the device */
};

AudioHAL_I2CReg configRegs32Bit_m[] =
{
    {TI3254_PAGE_0, TI3254_SW_RESET_REG, 0x01},                         /* Reset the codec */
    {TI3254_PAGE_0, TI3254_AUDIO_IF_1_REG, 0x30},                       /* 32bit, I2S, WCLK, BCLK is input to the device */
};

AudioHAL_I2CScript configScript16Bit =
{
    .size = sizeof(configRegs16Bit)/ sizeof(AudioHAL_I2CReg),
    .data = configRegs16Bit,
};

AudioHAL_I2CScript configScript16Bit_m =
{
    .size = sizeof(configRegs16Bit_m)/ sizeof(AudioHAL_I2CReg),
    .data = configRegs16Bit_m,
};

AudioHAL_I2CScript configScript24Bit =
{
    .size = sizeof(configRegs24Bit)/ sizeof(AudioHAL_I2CReg),
    .data = configRegs24Bit,
};

AudioHAL_I2CScript configScript24Bit_m =
{
    .size = sizeof(configRegs24Bit_m)/ sizeof(AudioHAL_I2CReg),
    .data = configRegs24Bit_m,
};

AudioHAL_I2CScript configScript32Bit =
{
    .size = sizeof(configRegs32Bit)/ sizeof(AudioHAL_I2CReg),
    .data = configRegs32Bit,
};

AudioHAL_I2CScript configScript32Bit_m =
{
    .size = sizeof(configRegs32Bit_m)/ sizeof(AudioHAL_I2CReg),
    .data = configRegs32Bit_m,
};

AudioHAL_I2CintfCfg TLV320AIC3254_interfaceConfig[TI3254_NUM_INTF_CFG] =
{
    {
        .bitDepth = AUDIOHAL_BITDEPTH_16,
        .master = false,
        .script = &configScript16Bit,
    },
    {
        .bitDepth = AUDIOHAL_BITDEPTH_16,
        .master = true,
        .script = &configScript16Bit_m,
    },
    {
        .bitDepth = AUDIOHAL_BITDEPTH_24,
        .master = false,
        .script = &configScript24Bit,
    },
    {
        .bitDepth = AUDIOHAL_BITDEPTH_24,
        .master = true,
        .script = &configScript24Bit_m,
    },
    {
        .bitDepth = AUDIOHAL_BITDEPTH_32,
        .master = false,
        .script = &configScript32Bit,
    },
    {
        .bitDepth = AUDIOHAL_BITDEPTH_32,
        .master = true,
        .script = &configScript32Bit_m,
    }
};

uint8_t TLV320AIC3254_interfaceConfig_count = sizeof(TLV320AIC3254_interfaceConfig) / sizeof(TLV320AIC3254_interfaceConfig[0]);

/*
 *******************************************************************************
 * These scripts are generic scripts to be run at open, close, start, and stop
 * of stream. They should not be sample rate or bitDepth dependent
 *******************************************************************************
 */
AudioHAL_I2CReg openScript[] =
{
    /* Configure Analog I/O */
    {TI3254_PAGE_1, TI3254_PWR_CTRL_REG, 0x08},                         /* Disabled weak connection of AVDD with DVDD */
    {TI3254_PAGE_1, TI3254_LDO_CTRL_REG, 0x01},                         /* Over Current detected for AVDD LDO */
    {TI3254_PAGE_1, TI3254_ANALOG_IP_QCHRG_CTRL_REG, 0x32},             /* Analog inputs power up time is 6.4 ms */
    {TI3254_PAGE_1, TI3254_REF_PWR_UP_CTRL_REG, 0x01},                  /* Reference will power up in 40ms when analog blocks are powered up */

    /* Configure DAC */
    {TI3254_PAGE_0, TI3254_DAC_SIG_P_BLK_CTRL_REG, 0x2},                /* DAC Signal Processing Block PRB_P2 */
    {TI3254_PAGE_44, TI3254_DAC_ADP_FILTER_CTRL_REG, 0x04},             /* Adaptive Filtering enabled for DA */
    {TI3254_PAGE_1, TI3254_HP_DRV_START_UP_CTRL_REG, 0x25},             /* Headphone ramps power up time is determined with 6k resistance */
    {TI3254_PAGE_1, TI3254_HPL_ROUTING_SEL_REG, 0x08},                  /* Left Channel DAC reconstruction filter's positive terminal is routed to HPL */
    {TI3254_PAGE_1, TI3254_HPR_ROUTING_SEL_REG, 0x08},                  /* Left Channel DAC reconstruction filter's negative terminal is routed to HPR */
    {TI3254_PAGE_1, TI3254_OP_DRV_PWR_CTRL_REG, 0x30},                  /* HPL and HPR is powered up */
    {TI3254_PAGE_1, TI3254_HPL_DRV_GAIN_CTRL_REG, 0x00},                /* HPL driver is not muted, HPL driver gain is 0dB */
    {TI3254_PAGE_1, TI3254_HPR_DRV_GAIN_CTRL_REG, 0x00},                /* HPR driver is not muted, HPL driver gain is 0dB */

    {TI3254_PAGE_0, TI3254_LEFT_DAC_VOL_CTRL_REG, 0x80},                /* Digital Volume Control = 64.0dB silent Note: As per data sheet its reserved but on setting this value there is silent */
    {TI3254_PAGE_0, TI3254_RIGHT_DAC_VOL_CTRL_REG, 0x80},               /* Digital Volume Control = 64.0dB silent Note: As per data sheet its reserved but on setting this value there is silent */
    {TI3254_PAGE_0, TI3254_DAC_CHANNEL_SETUP_1_REG, 0xD6},              /* Left and Right DAC Channel Powered Up */
    {TI3254_PAGE_0, TI3254_DAC_CHANNEL_SETUP_2_REG, 0x00},              /* When Right DAC Channel is powered down, the data is zero */

    /* Configure ADC */
    {TI3254_PAGE_8, TI3254_ADC_ADP_FILTER_CTRL_REG, 0x04},              /* Adaptive Filtering enabled for ADC */
    {TI3254_PAGE_0, TI3254_ADC_SIG_P_BLK_CTRL_REG, 0x2},                /* ADC Signal Processing Block PRB_P2 */
    {TI3254_PAGE_1, TI3254_MICBIAS_CTRL_REG, 0x40},                     /* MICBIAS powered up */

#ifdef TI3254_MIC_ON_BOARD
    {TI3254_PAGE_1, TI3254_LEFT_MICPGA_P_CTRL_REG, 0x00},               /* IN1L is routed to Left MICPGA with 10k resistance */
    {TI3254_PAGE_1, TI3254_LEFT_MICPGA_N_CTRL_REG, 0x04},               /* CM is routed to Left MICPGA via CM1L with 10k resistance */
    {TI3254_PAGE_1, TI3254_RIGHT_MICPGA_P_CTRL_REG, 0x04},              /* IN1R is routed to Right MICPGA with 10k resistance */
    {TI3254_PAGE_1, TI3254_FLOAT_IP_CTRL_REG, 0x04},
#else
    /* TI3254_LINE_IN */
    {TI3254_PAGE_1, TI3254_LEFT_MICPGA_P_CTRL_REG, 0x40},               /* IN1L is routed to Left MICPGA with 10k resistance */
    {TI3254_PAGE_1, TI3254_LEFT_MICPGA_N_CTRL_REG, 0x40},               /* CM is routed to Left MICPGA via CM1L with 10k resistance */
    {TI3254_PAGE_1, TI3254_RIGHT_MICPGA_P_CTRL_REG, 0x40},              /* IN1R is routed to Right MICPGA with 10k resistance */
    {TI3254_PAGE_1, TI3254_FLOAT_IP_CTRL_REG, 0xC0},                    /* IN1L input is weakly driven to common mode. Use when not routing IN1L to Left and Right MICPGA and HPL, HPR */
#endif /*TI3254_MIC_ON_BOARD */

    {TI3254_PAGE_1, TI3254_RIGHT_MICPGA_N_CTRL_REG, 0x40},              /* CM is routed to Right MICPGA via CM1R with 10k resistance */

    {TI3254_PAGE_1, TI3254_LEFT_MICPGA_VOL_CTRL_REG, 0x00},             /* 0.0dB */
    {TI3254_PAGE_1, TI3254_RIGHT_MICPGA_VOL_CTRL_REG, 0x00},            /* 0.0dB */

    {TI3254_PAGE_0, TI3254_LEFT_ADC_VOL_CTRL_REG, 0x68},                /* -12dB */
    {TI3254_PAGE_0, TI3254_RIGHT_ADC_VOL_CTRL_REG, 0x68},               /* -12dB */
    {TI3254_PAGE_0, TI3254_ADC_CHANNEL_SETUP_REG, 0xC0},                /* Left and Right Channel ADC is powered up */
    {TI3254_PAGE_0, TI3254_ADC_FINE_GAIN_ADJ_REG, 0x00}                 /* Left and Right ADC Channel Un-muted. Left and Right ADC Channel Fine Gain = 0dB */
};

AudioHAL_I2CReg closeScript[] =
{
    {TI3254_PAGE_0, TI3254_LEFT_DAC_VOL_CTRL_REG, 0x80},                /* Digital Volume Control = 64.0dB silent */
    {TI3254_PAGE_0, TI3254_RIGHT_DAC_VOL_CTRL_REG, 0x80},               /* Digital Volume Control = 64.0dB silent */
    {TI3254_PAGE_0, TI3254_DAC_CHANNEL_SETUP_2_REG, 0x0C},              /* Mute L and R channel of DAC */
    {TI3254_PAGE_0, TI3254_DAC_CHANNEL_SETUP_1_REG, 0xC0},              /* Power down DAC */
    {TI3254_PAGE_1, TI3254_MICBIAS_CTRL_REG, 0x00},                     /* MICBIAS powered down */
    {TI3254_PAGE_0, TI3254_CLK_PLL_P_R_REG, 0x00},                      /* PLL is powered down */
};

AudioHAL_I2CReg startScript[] =
{
    {TI3254_PAGE_0, TI3254_LEFT_DAC_VOL_CTRL_REG, 0x10},
    {TI3254_PAGE_0, TI3254_RIGHT_DAC_VOL_CTRL_REG, 0x10},
    {TI3254_PAGE_0, TI3254_LEFT_ADC_VOL_CTRL_REG, 0x0C},
    {TI3254_PAGE_0, TI3254_RIGHT_ADC_VOL_CTRL_REG, 0x0C}
};

AudioHAL_I2CReg stopScript[] =
{
    {TI3254_PAGE_0, TI3254_LEFT_DAC_VOL_CTRL_REG, 0x80},
    {TI3254_PAGE_0, TI3254_RIGHT_DAC_VOL_CTRL_REG, 0x80}
};


AudioHAL_I2CScript TLV320AIC3254_openScript =
{
    .size = sizeof(openScript)/ sizeof(AudioHAL_I2CReg),
    .data = openScript,
};

AudioHAL_I2CScript TLV320AIC3254_closeScript =
{
    .size = sizeof(closeScript)/ sizeof(AudioHAL_I2CReg),
    .data = closeScript,
};

AudioHAL_I2CScript TLV320AIC3254_startScript =
{
    .size = sizeof(startScript)/ sizeof(AudioHAL_I2CReg),
    .data = startScript,
};

AudioHAL_I2CScript TLV320AIC3254_stopScript =
{
    .size = sizeof(stopScript)/ sizeof(AudioHAL_I2CReg),
    .data = stopScript,
};
