
#include <stdint.h>
#include <stdbool.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/I2C.h>

/* Module Header */
#include <sensors/tmp116/tmp116.h>

/* POSIX Header files */
#include <unistd.h>

/* It is recommended to wait for some time(in few ms, defined by below macro)
before TMP116 module is used after issuing a reset*/
#define WAIT_POST_RESET      2U         /* Wait time in ms after reset        */

#define MAX_CELSIUS          256U        /* Max degrees celsius                */
#define MIN_CELSIUS          -256        /* Min degrees celsius                */
#define CELSIUS_FAHREN_CONST 1.8F        /* Multiplier for conversion          */
#define CELSIUS_FAHREN_ADD   32U         /* Celsius to Fahrenheit added factor */
#define CELSIUS_TO_KELVIN    273.15F     /* Celsius to Kelvin addition factor  */
#define CELSIUS_PER_LSB      0.0078125F  /* Degrees celsius per LSB            */

#define READ_BUF_SIZE        2U
#define WRITE_BUF_SIZE       3U

/* TMP116 Configuration Register Bits */
#define CMD_SW_RST    0x06U             /* Software reset                      */

/* Default TMP116 parameters structure */
const TMP116_Params TMP116_defaultParams = {
    TMP116_CC_MODE,                     /*!< Continuous Conversion Mode     */
    TMP116_0CONV,                       /*!< Conversion Cycle Time 15.5ms   */
    TMP116_0SAMPLES,                    /*!< 0 Samples per conversion       */
    TMP116_Alert,                       /*!< Alert Mode                     */
    TMP116_AlertFlag,                   /*!< Alert Pin Mode                 */
    TMP116_Active_Low,                  /*!< Active Low on Alert            */
    NULL,                               /*!< Pointer to GPIO callback       */
};

extern TMP116_Config TMP116_config[];


void TMP116_init()
{
    static bool initFlag = false;
    unsigned char i;

    if (initFlag == false) {
        for (i = 0U; TMP116_config[i].object != NULL; i++) {
            ((TMP116_Object *)(TMP116_config[i].object))->i2cHandle = NULL;
        }
        initFlag = true;
    }
}

void TMP116_Params_init(TMP116_Params *params)
{
    *params = TMP116_defaultParams;
}

TMP116_Handle TMP116_open(unsigned int tmp116Index, I2C_Handle i2cHandle,
                                 TMP116_Params *params)
{
    TMP116_Handle handle = &TMP116_config[tmp116Index];
    TMP116_Object *obj = (TMP116_Object*)(TMP116_config[tmp116Index].object);
    TMP116_HWAttrs *hw = (TMP116_HWAttrs*)(TMP116_config[tmp116Index].hwAttrs);
    uint16_t data;

    if (obj->i2cHandle != NULL) {
        return (NULL);
    }

    obj->i2cHandle = i2cHandle;

    if (params == NULL) {
       params = (TMP116_Params *) &TMP116_defaultParams;
    }

    /* Perform a Hardware TMP116 Reset */
    if (TMP116_writeRegister(handle, CMD_SW_RST, TMP116_RST)) {

        usleep(WAIT_POST_RESET * 1000U);

        data = (uint16_t)params->conversionMode | (uint16_t)params->conversionCycle | 
                (uint16_t)params->averaging | (uint16_t)params->alertMode | 
                (uint16_t)params->alertPinMode | (uint16_t)params->alertPinPolarity;

        if (TMP116_writeRegister(handle, data, TMP116_CONFIG)) {

            if (params->callback != NULL) {
                obj->callback = params->callback;
                GPIO_setCallback(hw->gpioIndex, obj->callback);
                GPIO_enableInt(hw->gpioIndex);
            }

            return (handle);
        }
    }

    obj->i2cHandle = NULL;

    return (NULL);
}

bool TMP116_close(TMP116_Handle handle)
{
    TMP116_Object *obj = (TMP116_Object *)(handle->object);

    if (obj->callback != NULL) {
        TMP116_configAlert(handle,TMP116_Therm, TMP116_DataReadyFlag, TMP116_Active_Low);
    }

    if (TMP116_configConversions(handle, TMP116_SD_MODE, TMP116_0CONV, TMP116_0SAMPLES)) {
        obj->i2cHandle = NULL;

        return (true);
    }

    return (false);
}

bool TMP116_configAlert(TMP116_Handle handle, TMP116_AlertMode mode,
                               TMP116_AlertPinMode pinMode,
                               TMP116_AlertPinPolarity pinPol)
{
    uint16_t reg;

    if (!TMP116_readRegister(handle, (uint16_t *) &reg, TMP116_CONFIG)) {

        reg =((reg & ((uint16_t)mode|(uint16_t)pinMode|(uint16_t)pinPol)) | ((~((uint16_t)mode|(uint16_t)pinMode|(uint16_t)pinPol)<<2U) & 
                (TMP116_THERM_ALERT_MODE|TMP116_PIN_POL|TMP116_PIN_MODE)));

        if (!TMP116_writeRegister(handle,reg, TMP116_CONFIG)) {

            return (false);

        }
    }

    return (true);
}

bool TMP116_configConversions(TMP116_Handle handle,
                                     TMP116_ConversionMode mode,
                                     TMP116_ConversionCycle cycle,
                                     TMP116_Averaging avg)
{

    uint16_t reg;

    if (!TMP116_readRegister(handle, (uint16_t *) &reg, TMP116_CONFIG)) {

        reg = ((reg & ((uint16_t)mode|(uint16_t)cycle|(uint16_t)avg)) | ((~((uint16_t)mode|(uint16_t)cycle|(uint16_t)avg)<<6U) &
                (TMP116_MOD|TMP116_CONV|TMP116_AVG)));

        if (!TMP116_writeRegister(handle, reg, TMP116_CONFIG)) {

            return (false);

        }
    }

    return (true);
}

bool TMP116_setTempLimit(TMP116_Handle handle, TMP116_TempScale scale,
                                float high, float low)
{
    int16_t hi_temp, lo_temp;

    /* Convert from scale to Celsius */
    switch (scale) {
        case TMP116_KELVIN:
            high = high - CELSIUS_TO_KELVIN;
            low = low - CELSIUS_TO_KELVIN;
            break;

        case TMP116_FAHREN:
            high = (high - CELSIUS_FAHREN_ADD) / CELSIUS_FAHREN_CONST;
            low = (low - CELSIUS_FAHREN_ADD) / CELSIUS_FAHREN_CONST;
            break;

        default:
            break;
    }

    if (high > MAX_CELSIUS) {
        high = MAX_CELSIUS;
    }

    if (low > MAX_CELSIUS) {
        low = MAX_CELSIUS;
    }

    if (high < MIN_CELSIUS) {
        high = MIN_CELSIUS;
    }

    if (low < MIN_CELSIUS) {
        low = MIN_CELSIUS;
    }

    hi_temp = (int16_t)(high * 128U); /* 0.0078125 (C) per LSB and truncate to integer */
    lo_temp = (int16_t)(low * 128U); /* 0.0078125 (C) per LSB and truncate to integer */

    /* Write Temp Lim */
    if (!(TMP116_writeRegister(handle, hi_temp, TMP116_HI_LIM)) & 
        !(TMP116_writeRegister(handle, lo_temp, TMP116_LO_LIM))) {
        return (false);
    }

    return (true);
}


bool TMP116_readStatus(TMP116_Handle handle, uint16_t *data){
    uint16_t reg;

    /* Read Temperature Limit */
    if (!(TMP116_readRegister(handle, (uint16_t *) &reg, TMP116_CONFIG))) {
        return (false);
    }

    *data = reg;

    return (true);
}

bool TMP116_getTempLimit(TMP116_Handle handle, TMP116_TempScale scale,
                                float* data, float *high, float *low)
{
    int16_t hi_temp, lo_temp;

    /* Read Temperature Limit */
    if (!(TMP116_readRegister(handle, (uint16_t *) &hi_temp, TMP116_HI_LIM)) & 
        !(TMP116_readRegister(handle, (uint16_t *) &lo_temp, TMP116_LO_LIM))) {
        return (false);
    }

    /* Right 6 registers are don't care bits*/
    *high = ((float)(hi_temp * CELSIUS_PER_LSB));
    *low = ((float)(lo_temp * CELSIUS_PER_LSB));

    switch (scale) {
        case TMP116_KELVIN:
            *high = *high - CELSIUS_TO_KELVIN;
            *low = *low - CELSIUS_TO_KELVIN;
            break;

        case TMP116_FAHREN:
            *high = (*high  * CELSIUS_FAHREN_CONST) + CELSIUS_FAHREN_ADD;
            *low = (*low  * CELSIUS_FAHREN_CONST) + CELSIUS_FAHREN_ADD;
            break;

        default:
            break;
    }

    return (true);
}

bool TMP116_getTemp(TMP116_Handle handle, TMP116_TempScale unit,
                    float *data)
{
    int16_t temp;

    if (TMP116_readRegister(handle, (uint16_t *) &temp, TMP116_TEMP)) {
        /* Shift out first 2 don't care bits, convert to Celsius */
        *data = ((float)(temp * CELSIUS_PER_LSB));

        switch (unit) {
            case TMP116_KELVIN:
                *data += CELSIUS_TO_KELVIN;
                break;

            case TMP116_FAHREN:
                *data = *data * CELSIUS_FAHREN_CONST + CELSIUS_FAHREN_ADD;
                break;

            default:
                break;
        }

        return (true);
    }

    return (false);
}

bool TMP116_readRegister(TMP116_Handle handle, uint16_t *data,
                                uint8_t registerAddress)
{
    I2C_Transaction i2cTransaction;
    unsigned char writeBuffer = registerAddress;
    unsigned char readBuffer[READ_BUF_SIZE];

    i2cTransaction.writeBuf = &writeBuffer;
    i2cTransaction.writeCount = 1U;
    i2cTransaction.readBuf = readBuffer;
    i2cTransaction.readCount = READ_BUF_SIZE;
    i2cTransaction.targetAddress = ((TMP116_HWAttrs *)(handle->hwAttrs))
            ->slaveAddress;

    if (!I2C_transfer(((TMP116_Object *)(handle->object))->i2cHandle,
            &i2cTransaction)) {
        return (false);
    }

    *data = (readBuffer[0U] << 8U) | readBuffer[1U];

    return (true);
}

bool TMP116_writeRegister(TMP116_Handle handle, uint16_t data,
                                 uint8_t registerAddress)
{
    I2C_Transaction i2cTransaction;
    uint8_t writeBuffer[WRITE_BUF_SIZE] = {registerAddress, (data >> 8), data};

    i2cTransaction.writeBuf = writeBuffer;
    i2cTransaction.writeCount = WRITE_BUF_SIZE;
    i2cTransaction.readCount = 0U;
    i2cTransaction.targetAddress = ((TMP116_HWAttrs *)(handle->hwAttrs))
            ->slaveAddress;

    /* If transaction success */
    if (!I2C_transfer(((TMP116_Object *)(handle->object))
            ->i2cHandle, &i2cTransaction)) {
        return (false);
    }

    return (true);
}





