#include <AudioHAL/AudioHAL.h>
#include <AudioHAL/AudioHALCodec.h>
#include <AudioHAL/AudioHAL_I2C.h>
#include <AudioHAL/MCLKCC26XX.h>
#include <AudioHAL/TLV320AIC3254.h>
#include <minimal_board.h>
#include "ti_drivers_config.h"
AudioHALCodec_Object AudioHALCodecObject;

const AudioHALCodec_HWAttrsV1 AudioHALCodecHWAttrs[] = {
    {
        .i2cIdx = CONFIG_I2C_0,
        .i2sIdx = CONFIG_I2S_0,
        .i2cAddr = TI3254_I2C_SLAVE_ADDR,
        .i2cPageSelAddr = TI3254_PAGE_SEL_REG,
        .openScript =  &TLV320AIC3254_openScript,
        .closeScript =  &TLV320AIC3254_closeScript,
        .startScript =  &TLV320AIC3254_startScript,
        .stopScript =  &TLV320AIC3254_stopScript,
        .clkCfg =  TLV320AIC3254_clockConfig,
        .clkCfgSize = TI3254_NUM_CLK_CFG,
        .intfCfg =  TLV320AIC3254_interfaceConfig,
        .intfCfgSize = TI3254_NUM_INTF_CFG,
        .mclkOpenFxn = MCLKCC26XX_open,
        .mclkStartFxn = MCLKCC26XX_start,
        .mclkStopFxn = MCLKCC26XX_stop,
        .mclkCloseFxn = MCLKCC26XX_close
    }
};

const AudioHAL_Config AudioHAL_config[] = {
    {
        .fxnTablePtr    = &AudioHALCodec_fxnTable,
        .object         = &AudioHALCodecObject,
        .hwAttrs        = &AudioHALCodecHWAttrs
    },
};
const uint_least8_t AudioHAL_count = CC1312R1_LAUNCHXL_AUDIOHALCOUNT;
