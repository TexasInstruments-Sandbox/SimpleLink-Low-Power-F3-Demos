/*
 * Copyright (c) 2016-2017, Texas Instruments Incorporated
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
 *  @file       tmp116.h
 *
 *  @brief      TMP116 driver
 *
 *  The TMP116 header file should be included in an application as
 *  follows:
 *  @code
 *  #include <tmp116.h>
 *  @endcode
 *
 *  # Operation #
 *  The TMP116 driver simplifies using a TMP116 sensor to perform temperature
 *  readings. The board's I2C peripheral and pins must be configured and then
 *  initialized by using Board_initI2C(). Similarly, any GPIO pins must be
 *  configured and then initialized by using Board_initGPIO(). A TMP116_config
 *  array should be defined by the application. The TMP116_config array should
 *  contain pointers to a defined TMP116_HWAttrs and allocated array for the
 *  TMP116_Object structures. TMP116_init() must be called prior to using
 *  TMP116_open().
 *
 *  The APIs in this driver serve as an interface to a DPL(Driver Porting Layer)
 *  The specific implementations are responsible for creating all the RTOS
 *  specific primitives to allow for thread-safe operation.
 *
 *  For accurate operation, calibration may be necessary. Refer to SBOU142.pdf
 *  for calibration instructions.
 *
 *  This driver has no dynamic memory allocation.
 *
 *  ## Defining TMP116_Config, TMP116_Object and TMP116_HWAttrs #
 *  Each structure must be defined by the application. The following
 *  example is for a MSP432 in which two TMP116 sensors are setup.
 *  The following declarations are placed in "MSP_EXP432P401R.h"
 *  and "MSP_EXP432P401R.c" respectively. How the gpioIndices are defined
 *  are detailed in the next example.
 *
 *  "MSP_EXP432P401R.h"
 *  @code
 *  typedef enum MSP_EXP432P491RLP_TMP116Name {
 *      TMP116_ROOMTEMP = 0, // Sensor measuring room temperature
 *      TMP116_OUTDOORTEMP,  // Sensor measuring outside temperature
 *      MSP_EXP432P491RLP_TMP116COUNT
 *  } MSP_EXP432P491RLP_TMP116Name;
 *
 *  @endcode
 *
 *  "MSP_EXP432P401R.c"
 *  @code
 *  #include <tmp116.h>
 *
 *  TMP116_Object TMP116_object[MSP_EXP432P491RLP_TMP116COUNT];
 *
 *  const TMP116_HWAttrs TMP116_hwAttrs[MSP_EXP432P491RLP_TMP116COUNT] = {
 *  {
 *      .slaveAddress = TMP116_SA1, // 0x48
 *      .gpioIndex = MSP_EXP432P401R_TMP116_0,
 *  },
 *  {
 *      .slaveAddress = TMP116_SA2, // 0x49
 *      .gpioIndex = MSP_EXP432P401R_TMP116_1,
 *  },
 *  };
 *
 *  const TMP116_Config TMP116_config[] = {
 *  {
 *      .hwAttrs = (void *)&TMP116_hwAttrs[0],
 *      .objects = (void *)&TMP116_object[0],
 *  },
 *  {
 *      .hwAttrs = (void *)&TMP116_hwAttrs[1],
 *      .objects = (void *)&TMP116_object[1],
 *  },
 *  {NULL, NULL},
 *  };
 *  @endcode
 *
 *  ##Setting up GPIO configurations #
 *  The following example is for a MSP432 in which two TMP116 sensors
 *  each need a GPIO pin for alarming. The following definitions are in
 *  "MSP_EXP432P401R.h" and "MSP_EXP432P401R.c" respectively. This
 *  example uses GPIO pins 1.5 and 4.3. The GPIO_CallbackFxn table must
 *  contain as many indices as GPIO_CallbackFxns that will be specified by
 *  the application. For each ALERT pin used, an index should be allocated
 *  as NULL.
 *
 *  "MSP_EXP432P401R.h"
 *  @code
 *  typedef enum MSP_EXP432P401R_GPIOName {
 *      MSP_EXP432P401R_TMP116_0, // ALERT pin for the room temperature
 *      MSP_EXP432P401R_TMP116_1, // ALERT pin for the outdoor temperature
 *      MSP_EXP432P401R_GPIOCOUNT
 *  } MSP_EXP432P401R_GPIOName;
 *  @endcode
 *
 *  "MSP_EXP432P401R.c"
 *  @code
 *  #include <gpio.h>
 *
 *  GPIO_PinConfig gpioPinConfigs[] = {
 *      GPIOMSP432_P1_5 | GPIO_CFG_INPUT | GPIO_CFG_IN_INT_FALLING,
 *      GPIOMSP432_P4_3 | GPIO_CFG_INPUT | GPIO_CFG_IN_INT_FALLING
 *  };
 *
 *  GPIO_CallbackFxn gpioCallbackFunctions[] = {
 *      NULL,
 *      NULL
 *  };
 *  @endcode
 *
 *  ## Opening a I2C Handle #
 *  The I2C controller must be in blocking mode. This will seamlessly allow
 *  for multiple I2C endpoints without conflicting callback functions. The
 *  I2C_open() call requires a configured index. This index must be defined
 *  by the application in accordance to the I2C driver. The default transfer
 *  mode for an I2C_Params structure is I2C_MODE_BLOCKING. In the example
 *  below, the transfer mode is set explicitly for clarity. Additionally,
 *  the TMP116 hardware is capable of communicating at 400kHz. The default
 *  bit rate for an I2C_Params structure is I2C_100kHz.
 *
 *  @code
 *  #include <ti/drivers/I2C.h>
 *
 *  I2C_Handle      i2cHandle;
 *  I2C_Params      i2cParams;
 *
 *  I2C_Params_init(&i2cParams);
 *  i2cParams.transferMode = I2C_MODE_BLOCKING;
 *  i2cParams.bitRate = I2C_400kHz;
 *  i2cHandle = I2C_open(someI2C_configIndexValue, &i2cParams);
 *  @endcode
 *
 *  ## Opening a TMP116 sensor with default parameters #
 *  The TMP116_open() call must be made in a task context.
 *
 *  @code
 *  #include <tmp116.h>
 *
 *  TMP116_Handle tmp116Handle;
 *
 *  TMP116_init();
 *  tmp116Handle = TMP116_open(TMP116_ROOMTEMP, i2cHandle, NULL);
 *  @endcode
 *
 *  ## Opening a TMP116 sensor to ALERT #
 *  In the following example, a callback function is specified in the
 *  tmp116Params structure. This indicates to the module that
 *  the ALERT pin will be used. Additionally, a user specific argument
 *  is passed in. The sensor will assert the ALERT pin whenever the
 *  temperature exceeds 35 (C). The low limit is ignored. No ALERT will be
 *  generated until TMP116_enableAlert() is called.
 *
 *  @code
 *  #include <tmp116.h>
 *
 *  TMP116_Handle tmp116Handle;
 *  TMP116_Params tmp116Params;
 *
 *  tmp116Params.callback = gpioCallbackFxn;
 *  tmp116Handle = TMP116_open(TMP116_ROOMTEMP, i2cHandle, &tmp116Params);
 *
 *  TMP116_setTempLimit(tmp116Handle, TMP116_CELSIUS, 35, TMP116_IGNORE);
 *  @endcode
 *  ============================================================================
 */

#ifndef TMP116_H_
#define TMP116_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Driver Header files */
#include <ti/drivers/I2C.h>
#include <ti/drivers/GPIO.h>

/* TMP116 Register Addresses */
#define TMP116_RST                  0x00U  /*! Reset Command register                */
#define TMP116_TEMP                 0x00U  /*! Sensor voltage result register        */
#define TMP116_CONFIG               0x01U  /*! Configuration register                */
#define TMP116_HI_LIM               0x02U  /*! Temp High limit register              */
#define TMP116_LO_LIM               0x03U  /*! Object Temp Low limit register        */
#define TMP116_EEPROM_UNLOCK        0x04U  /*! EEPROM access register                */
#define TMP116_EEPROM1              0x05U  /*! EEPROM1 register                      */
#define TMP116_EEPROM2              0x06U  /*! EEPROM2 register                      */
#define TMP116_EEPROM3              0x07U  /*! EEPROM3 register                      */
#define TMP116_EEPROM4              0x08U  /*! EEPROM4 register                      */
#define TMP116_DEVICE_ID            0x0FU  /*! Manufacturer ID register              */

/* Status Mask and Enable Register Bits */
#define TMP116_HI_ALRT              0x8000U  /*! High Alert Flag Bit                 */
#define TMP116_LO_ALRT              0x4000U  /*! Low Alert Flag Bit                  */
#define TMP116_DATA_RDY             0x2000U  /*! Object Temp High Limit Enable       */
#define TMP116_EEPROM_BUSY          0x1000U  /*! Object Temp Low Limit Enable        */
#define TMP116_MOD                  0x0C00U  /*! DIE Temp High Limit Enable          */
#define TMP116_CONV                 0x0700U  /*! DIE Temp Low Limit Enable           */
#define TMP116_AVG                  0x00C0U  /*! Data Invalid Flag Enable Bit        */
#define TMP116_THERM_ALERT_MODE     0x0010U  /*! Memory Corrupt Enable Bit           */
#define TMP116_PIN_POL              0x0008U  /*! Data Invalid Flag Enable Bit        */
#define TMP116_PIN_MODE             0x0004U  /*! Memory Corrupt Enable Bit           */

/* TMP116 Status Register Bits */
#define TMP116_STAT_ALR_HI          0x8000U  /*! High Alert Flag                     */
#define TMP116_STAT_ALR_LO          0x4000U  /*! Low Alert Flag                      */
#define TMP116_STAT_DATA_RDY        0x2000U  /*! Data Ready Flag                     */
#define TMP116_STAT_EEPROM_BUSY     0x1000U  /*! EEPROM Busy Flag                    */

/*!
 *  @brief    A handle that is returned from a TMP116_open() call.
 */
typedef struct TMP116_Config    *TMP116_Handle;

/*!
 *  @brief    TMP116 Conversion Mode
 *
 *  This enumeration defines the Conversion Mode of the Device
 *  The MOD[0:1] bits control in what mode does the TMP116 operate in
 *  There are three modes: Continuous Conversion, Shutdown, and one-shot
 *  Continuous Mode [00] takes continuous samples of the temperature and
 *  stores them on the temperature register.
 *  Shutdown Mode [01] instantly aborts the currently running conversion
 *  and enters in low-power shutdown mode
 *  One-Shot Mode [11] wakes up form shutdown mode and makes a single conversion
 *  the time it takes to do that single conversion will depend on what is the
 *  Conversion Cycle and Average setting on the device, once done it will go
 *  back to Shutdown Mode.
 *
 */
typedef enum TMP116_ConversionMode {
    TMP116_CC_MODE  = 0x0000U,   /* Continuous Conversion Mode */
    TMP116_SD_MODE  = 0x0200U,   /* Shutdown Mode */
    TMP116_OS_MODE  = 0x0600U    /* One-Shot Mode */
} TMP116_ConversionMode;

/*!
 *  @brief    TMP116 Conversion Cycle Time in CC Mode
 *
 *  This enumeration defines the Conversion Cycle time, the following table shows the times
 *
 *  | TMP116_ConversionCycle |  TMP116_0AVG |  TMP116_1AVG |  TMP116_2AVG |  TMP116_3AVG  |
 *  |----------------------------------------------------------------------------------------|
 *  |     TMP116_0CONV       |     15.5ms    |      125ms    |      500ms    |       1s      |
 *  |                        |               |               |               |               |
 *  |     TMP116_1CONV       |      125ms    |      125ms    |      500ms    |       1s      |
 *  |                        |               |               |               |               |
 *  |     TMP116_2CONV       |      250ms    |      500ms    |      500ms    |       1s      |
 *  |                        |               |               |               |               |
 *  |     TMP116_3CONV       |      500ms    |      500ms    |      500ms    |       1s      |
 *  |                        |               |               |               |               |
 *  |     TMP116_4CONV       |       1s      |       1s      |       1s      |       1s      |
 *  |                        |               |               |               |               |
 *  |     TMP116_5CONV       |       4s      |       4s      |       4s      |       4s      |
 *  |                        |               |               |               |               |
 *  |     TMP116_6CONV       |       8s      |       8s      |       8s      |       8s      |
 *  |                        |               |               |               |               |
 *  |     TMP116_7CONV       |      16s      |      16s      |      16s      |      16s      |
 *
 */
typedef enum TMP116_ConversionCycle {
    TMP116_0CONV =    0x0000U,
    TMP116_1CONV =    0x0080U,
    TMP116_2CONV =    0x0100U,
    TMP116_3CONV =    0x0180U,
    TMP116_4CONV =    0x0200U,
    TMP116_5CONV =    0x0280U,
    TMP116_6CONV =    0x0300U,
    TMP116_7CONV =    0x0380U
} TMP116_ConversionCycle;

/*!
 *  @brief    TMP116 Conversion Average
 *
 *  This enumeration defines the Number of averages the device
 *
 */
typedef enum TMP116_Averaging {
    TMP116_0SAMPLES  =    0x0000U,  /* No averaging,15.5-ms active conversion time */
    TMP116_8SAMPLES  =    0x0080U,  /* 8 averages, 125-ms active conversion time   */
    TMP116_32SAMPLES =    0x0100U,  /* 32 averages, 500-ms active conversion time  */
    TMP116_64SAMPLES =    0x0180U   /* 64 averages, 1-s active conversion time     */
} TMP116_Averaging;

/*!
 *  @brief    TMP116 Alert Mode
 *
 *  The T/nA bit in the Configuration Register.
 *  In Alert Mode the bit is (0) and it compares the conversion result with the
 *  High Limit register and Low Limit register contents at the end of every conversion.
 *  If the temperature result exceeds the value in the high limit register, the
 *  HIGH_Alert status flag in the configuration register is set. On the other hand,
 *  if the temperature result is lower than the value in the low limit register,
 *  the LOW_Alert status flag in the configuration register is set.
 *  In Therm Mode the bit is (1), In this mode, the device compares the conversion result
 *  at the end of every conversion with the values in the low limit register and high limit
 *  register and sets the HIGH_Alert status flag in the configuration register if the
 *  temperature exceeds the value in the high limit register. When set, the device clears
 *  the HIGH_Alert status flag if the conversion result goes below the value in the low
 *  limit register. Thus, the difference between the high and low limits effectively acts
 *  like a hysteresis. In this mode, the LOW_Alert status flag is disabled and always
 *  reads 0. Unlike the alert mode,I2C reads of the configuration register do not affect
 *  the status bits. The HIGH_Alert status flag is only set or cleared at the end of conversions
 *   based on the value of the temperature result compared to the high and low limits.
 *
 */
typedef enum TMP116_AlertMode {
    TMP116_Alert =  0x0000U,
    TMP116_Therm =  0x0010U
} TMP116_AlertMode;

/*!
 *  @brief    TMP116 DR/Alert Pin Config
 *
 *  Data ready, ALERT flag select bit.
 *  1: ALERT pin reflects the status of the data ready flag
 *  0: ALERT pin reflects the status of the alert flags
 *
 */
typedef enum TMP116_AlertPinMode {
    TMP116_DataReadyFlag = 0x0004U,
    TMP116_AlertFlag     = 0x0000U
} TMP116_AlertPinMode;

/*!
 *  @brief    TMP116 ALERT pin polarity bit
 *
 * ALERT pin polarity bit.
 * 1: Active high
 * 0: Active low
 *
 */
typedef enum TMP116_AlertPinPolarity {
    TMP116_Active_High = 0x0008U,
    TMP116_Active_Low  = 0x0000U
} TMP116_AlertPinPolarity;

/*!
 *  @brief    TMP116 I2C slave addresses.
 *
 *  The TMP116 Slave Address is determined by the input to the ADR0 and ADR1
 *  input pins of the TMP116 hardware. A '1' indicates a supply voltage of
 *  up to 7.5 V while '0' indicates ground. In some cases, the ADR0 pin may
 *  be coupled with the SDA or SCL bus to achieve a particular slave address.
 *  TMP116 sensors on the same I2C bus cannot share the same slave address.
 */
typedef enum TMP116_SlaveAddress {
    TMP116_SA1 = 0x48U,  /*!< ADR0 = 0   */
    TMP116_SA2 = 0x49U,  /*!< ADR0 = 1   */
    TMP116_SA3 = 0x4AU,  /*!< ADR0 = SDA */
    TMP116_SA4 = 0x4BU,  /*!< ADR0 = SCL */
} TMP116_SlaveAddress;

/*!
 *  @brief    TMP116 temperature settings
 *
 *  This enumeration defines the scaling for reading and writing temperature
 *  values with a TMP116 sensor.
 */
typedef enum TMP116_TempScale {
    TMP116_CELSIUS = 0U,
    TMP116_KELVIN  = 1U,
    TMP116_FAHREN  = 2U
} TMP116_TempScale;

/*!
 *  @brief    TMP116 configuration
 *
 *  The TMP116_Config structure contains a set of pointers used to characterize
 *  the TMP116 driver implementation.
 *
 *  This structure needs to be defined and provided by the application.
 */
typedef struct TMP116_Config {
    /*! Pointer to a driver specific hardware attributes structure */
    void const    *hwAttrs;

    /*! Pointer to an uninitialized user defined TMP116_Object struct */
    void          *object;
} TMP116_Config;

/*!
 *  @brief    Hardware specific settings for a TMP116 sensor.
 *
 *  This structure should be defined and provided by the application. The
 *  gpioIndex should be defined in accordance of the GPIO driver. The pin
 *  must be configured as GPIO_CFG_INPUT and GPIO_CFG_IN_INT_FALLING.
 */
typedef struct TMP116_HWAttrs {
    TMP116_SlaveAddress    slaveAddress;    /*!< I2C slave address */
    unsigned int           gpioIndex;       /*!< GPIO configuration index */
} TMP116_HWAttrs;

/*!
 *  @brief    Members should not be accessed by the application.
 */
typedef struct TMP116_Object {
    I2C_Handle          i2cHandle;
    GPIO_CallbackFxn    callback;
} TMP116_Object;

/*!
 *  @brief  TMP116 Parameters
 *
 *  TMP116 parameters are used with the TMP116_open() call. Default values for
 *  these parameters are set using TMP116_Params_init(). The GPIO_CallbackFxn
 *  should be defined by the application only if the ALERT functionality is
 *  desired. A gpioIndex must be defined in the TMP116_HWAttrs for the
 *  corresponding tmp116Index. The GPIO_CallbackFxn is in the context of an
 *  interrupt handler.
 *
 *  @sa     TMP116_Params_init()
 */
typedef struct TMP116_Params {
    TMP116_ConversionMode   conversionMode;         /*!< TMP116 Conversion Mode                  */
    TMP116_ConversionCycle  conversionCycle;        /*!< TMP116 Conversion Cycle Time in CC Mode */
    TMP116_Averaging        averaging;              /*!< TMP116 Conversion Average               */
    TMP116_AlertMode        alertMode;              /*!< TMP116 Alert Mode                       */
    TMP116_AlertPinMode     alertPinMode;           /*!< TMP116 DR/Alert Pin Config              */
    TMP116_AlertPinPolarity alertPinPolarity;       /*!< TMP116 ALERT pin polarity bit           */
    GPIO_CallbackFxn        callback;               /*!< Pointer to GPIO callback                */
} TMP116_Params;

/*!
 *  @brief  Function to initialize TMP116 driver.
 *
 *  This function will initialize the TMP116 driver. This should be called
 *  before BIOS_start().
 */
extern void TMP116_init();

/*!
 *  @brief  Function to initialize a TMP116_Params struct to its defaults
 *
 *   Default values are:
 *      TMP116_CC_MODE,                     !< Continuous Conversion Mode
 *      TMP116_0CONV,                       !< Conversion Cycle Time 15.5ms
 *      TMP116_0SAMPLES,                    !< 0 Samples per conversion
 *      TMP116_Alert,                       !< Alert Mode
 *      TMP116_AlertFlag,                   !< Alert Pin Mode
 *      TMP116_Active_Low,                  !< Active Low on Alert
 *      NULL,                               !< Pointer to GPIO callback
 *
 *  @param  params      A pointer to TMP116_Params structure for
 *                      initialization.
 *
 */
extern void TMP116_Params_init(TMP116_Params *params);

/*!
 *  @brief  Function to open a given TMP116 sensor
 *
 *  Function to initialize a given TMP116 sensor specified by the particular
 *  index value. This function must be called from a task context.
 *  If one intends to use the ALERT pin, a callBack function must be specified
 *  in the TMP116_Params structure. Additionally, a gpioIndex must be setup
 *  and specified in the TMP116_HWAttrs structure.
 *
 *  The I2C controller must be operating in BLOCKING mode. Failure to ensure
 *  the I2C_Handle is in BLOCKING mode will result in undefined behavior.
 *
 *  The user should ensure that each sensor has its own slaveAddress,
 *  gpioIndex (if alarming) and tmp116Index.
 *
 *  @pre    TMP116_init() has to be called first
 *
 *  @param  tmp116Index    Logical sensor number for the TMP116 indexed into
 *                         the TMP116_config table
 *
 *  @param  i2cHandle      An I2C_Handle opened in BLOCKING mode
 *
 *  @param  *params        A pointer to TMP116_Params structure. If NULL, it
 *                         will use default values.
 *
 *  @return  A TMP116_Handle on success, or a NULL on failure.
 *
 *  @sa      TMP116_init()
 *  @sa      TMP116_Params_init()
 *  @sa      TMP116_close()
 */
extern TMP116_Handle TMP116_open(unsigned int tmp116Index,
        I2C_Handle i2cHandle, TMP116_Params *params);

/*!
 *  @brief  Function to close a TMP116 sensor specified by the TMP116 handle
 *
 *  The TMP116 hardware will be placed in a low power state in which ADC
 *  conversions are disabled. If the pin is configured to alarm, the ALERT pin
 *  and GPIO pin interrupts will be disabled. The I2C handle is not affected.
 *
 *  @pre    TMP116_open() had to be called first.
 *
 *  @param  handle    A TMP116_Handle returned from TMP116_open()
 *
 *  @return true on success or false upon failure.
 */
extern bool TMP116_close(TMP116_Handle handle);

/*!
 *  @brief  Function to configure the ALERT pin and GPIO interrupt.
 *
 *  This function will configure the ALERT pin on the specified TMP116 sensor.
 *  Interrupts on the TMP116 specific GPIO index will be disabled. This
 *  function is not thread safe when used in conjunction with
 *  TMP116_disableAlert() (ie. A task is executing TMP116_enableAlert()
 *  but preempted by a higher priority task that calls TMP116_disableAlert()
 *  on the same handle).
 *
 *  @param  handle      A TMP116_Handle
 *  @param  mode        A TMP116_AlertMode
 *  @param  pinMode     A TMP116_AlertPinMode
 *  @param  pinPol      A TMP116_AlertPinPolarity
 *
 *  @return true on success or false upon failure.
 */
extern bool TMP116_configAlert(TMP116_Handle handle, TMP116_AlertMode mode,
               TMP116_AlertPinMode pinMode, TMP116_AlertPinPolarity pinPol);

/*!
 *  @brief  Function to configure conversion mode on the TMP116
 *
 *  This function will configure the conversion cycle, mode and averaging
 *
 *
 *  @param  handle      A TMP116_Handle
 *  @param  mode        A TMP116_ConversionMode
 *  @param  cycle       A TMP116_ConversionCycle
 *  @param  avg         A TTMP116_Averaging
 *
 *  @return true on success or false upon failure.
 */
extern bool TMP116_configConversions(TMP116_Handle handle, TMP116_ConversionMode mode,
             TMP116_ConversionCycle cycle, TMP116_Averaging avg);

/*!
 *  @brief  This function will set the temperature limits.
 *
 *  This function will write the specified high and low die limits to the
 *  specified TMP116 sensor.
 *
 *  @param  handle    A TMP116_Handle
 *
 *  @param  scale     A TMP116_TempScale field specifying the temperature
 *                    return format.
 *
 *  @param  high      A float that specifies the high limit.
 *
 *  @param  low       A float that specifies the low limit. 
 *
 *  @return true on success or false upon failure.
 */
extern bool  TMP116_setTempLimit(TMP116_Handle handle,
        TMP116_TempScale scale, float high, float low);

/*!
 *  @brief  This function will read the temperature limits.
 *
 *  @param  handle    A TMP116_Handle
 *
 *  @param  scale     A TMP116_TempScale field specifying the temperature
 *                    return format.
 *
 *  @param  high      A pointer to a float to store the value of the high
 *                    temperature limit.
 *
 *  @param  low       A pointer to a float to store the value of the low
 *                    temperature limit.
 *
 *  @return true on success or false upon failure.
 *
 */
extern bool TMP116_getTempLimit(TMP116_Handle handle, TMP116_TempScale scale,
        float *data,  float *high, float *low);

/*!
 *  @brief  Function to read object temperature
 *
 *  This function will return the temperature of the object in the field
 *  of view of the specified TMP116 sensor.
 *
 *  @param  handle    A TMP116_Handle
 *
 *  @param  unit      A TMP116_TempScale field specifying the temperature
 *                    format.
 *
 *  @param  data      A pointer to a float in which temperature data will be
 *                    stored in.
 *
 *  @return true on success or false upon failure.
 */
extern bool TMP116_getTemp(TMP116_Handle handle, TMP116_TempScale unit,
        float *data);

/*!
 *  @brief  Read function for advanced users.
 *
 *  @param  handle             A TMP116_Handle
 *
 *  @param  registerAddress    Register address
 *
 *  @param  data               A pointer to a data register in which received
 *                             data will be written to. Must be 16 bits.
 *
 *  @return true on success or false upon failure.
 */
extern bool TMP116_readRegister(TMP116_Handle handle,
        uint16_t *data, uint8_t registerAddress);

/*!
 *  @brief  Write function for advanced users. This is not thread safe.
 *
 *  When writing to a handle, it is possible to overwrite data written
 *  by another task.
 *
 *  For example: Task A and B are writing to the configuration register.
 *  Task A has a higher priority than Task B. Task B is running and
 *  reads the configuration register. Task A then preempts Task B and reads
 *  the configuration register, performs a logical OR and writes to the
 *  configuration register. Task B then resumes execution and performs its
 *  logical OR and writes to the configuration register--overwriting the data
 *  written by Task A.
 *
 *  Such instances can be prevented through the use of Semaphores. Below is an
 *  example which utilizes an initialized Semaphore_handle, tmp116Lock.
 *
 *  @code
 *  if (Semaphore_pend(tmp116Lock, BIOS_NO_WAIT)) {
 *      //Perform read/write operations
 *  }
 *  else {
 *      Task_sleep(1000);
 *  }
 *  Semaphore_post(tmp116Lock);
 *  @endcode
 *
 *  @param  handle             A TMP116_Handle
 *
 *  @param  data               A uint16_t containing the 2 bytes to be written
 *                             to the TMP116 sensor.
 *
 *  @param  registerAddress    Register address.

 *  @return true on success or false upon failure.
 */
extern bool TMP116_writeRegister(TMP116_Handle handle, uint16_t data,
        uint8_t registerAddress);

/*!
 *  @brief  Function to read a TMP116 sensor's status flags in config register.
 *
 *  @param  handle    A TMP116_Handle
 *
 *  @param  data      A pointer to an uint16_t data buffer in which received
 *                    data will be written.
 *
 *  @return true on success or false upon failure.
 */
extern bool TMP116_readStatus(TMP116_Handle handle, uint16_t *data);

#ifdef __cplusplus
}
#endif

#endif /* TMP116_H_ */
