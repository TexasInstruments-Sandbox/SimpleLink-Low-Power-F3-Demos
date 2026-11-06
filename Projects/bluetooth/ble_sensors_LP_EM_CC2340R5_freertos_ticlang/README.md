## BLE Sensor Suite for SimpleLink CC2340R5

Uses the BP-BASSENSORSMKII boosterpack to collect sensor data and transmit 
it over Bluetooth LE to the SimpleLink Connect mobile phone application.

## Features

* Bluetooth LE 5 peripheral stack solution.
* TI HDC2080 temperature and humidity sensor.
* TI TMP117 ultra-high accuracy temperature sensor with breakout board.
* TI DRV5055 Hall effect sensor.
* TI OPT3001 ambient light sensor.
* Bosch BMI160 inertial measurement unit plus BMM 150 magnetometer.

## Software Dependencies

This project depends on the following software components:

* [SimpleLink Low Power F3 SDK](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK) v9.12.00.19 or later
* [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO) v20.x or later
* [TI SimpleLink Connect on Apple App Store](https://apps.apple.com/app/simplelink-connect/id6445892658) 
**or** [TI SimpleLink Connect on Google App Store](https://play.google.com/store/apps/details?id=com.ti.connectivity.simplelinkconnect)

If not already completed, please follow the 
[SimpleLink basic_ble example SLA](https://dev.ti.com/tirex/explore/node?node=A__AcrwHdZt.SBRoZj0q1LDWQ__SIMPLELINK-ACADEMY-CC23XX__gsUPh5j__LATEST)
as a prerequisite to running and modifying this example.

## Hardware Requirements

* [LP-EM-CC2340R5](https://www.ti.com/tool/LP-EM-CC2340R5) - CC2340R5 LaunchPad™ 
development kit
* [BP-BASSENSORSMKII Sensors BoosterPack™][bp-bassensorsmkii]
* Jumper wires and headers for custom connections

## Peripherals & Pin Assignments

When this project is built, the SysConfig tool will generate the TI-Driver
configurations into the __ti_drivers_config.c__ and __ti_drivers_config.h__
files. Information on pins and resources used is present in both generated
files. Additionally, the System Configuration file (\*.syscfg) present in the
project may be opened with SysConfig's graphical user interface to determine
pins and resources used.

| Name                      | Pin   | Peripheral    | Description        |
|---------------------------|-------|---------------|--------------------|
| CONFIG_ADC_DRV5055        | DIO21 | ADC input     | DRV5055 sensor     |
| CONFIG_GPIO_LED_RED       | DIO25 | GPIO output   | Red LED            |
| CONFIG_GPIO_LED_GREEN     | DIO14 | GPIO output   | Green LED          |
| CONFIG_GPIO_TMP116_PWR    | DIO7  | GPIO output   | TMP117 power       |
| CONFIG_GPIO_OPT3001_POWER | DIO5  | GPIO output   | OPT3001 power      |
| CONFIG_GPIO_DRV5055_POWER | DIO25 | GPIO output   | DRV5005 power      |
| CONFIG_GPIO_BMI160_INT1   | DIO19 | GPIO input    | BMI160 interrupt   |
| CONFIG_GPIO_HDC2010_INT   | DIO23 | GPIO input    | HDC2010 interrupt  |
| CONFIG_GPIO_OPT3001_INT   | DIO1  | GPIO input    | OPT3001 interrupt  |
| CONFIG_GPIO_HDC2080_POWER | DIO11 | GPIO output   | HDC2080 power      |
| CONFIG_GPIO_TMP116_INT    | DIO2  | GPIO input    | TMP117 interrupt   |
| CONFIG_GPIO_I2C_0_SDA     | DIO0  | I2C data      | I2C bus SDA        |
| CONFIG_GPIO_I2C_0_SCL     | DIO24 | I2C clock     | I2C bus SCL        |
| CONFIG_BUTTON_0           | DIO10 | GPIO input    | LaunchPad Button 1 |
| CONFIG_BUTTON_1           | DIO9  | GPIO input    | LaunchPad Button 2 |
| CONFIG_PIN_UART_TX        | DIO20 | UART transmit | XDS110 UART TX     |
| CONFIG_PIN_UART_RX        | DIO22 | UART receive  | XDS110 UART RX     |

```{note}
TMP116 and TMP1117 functionality is identical, as it HDC2010 and HDC2080
```

## BoosterPacks, Board Resources & Jumper Settings

This example requires a
[BP-BASSENSORSMKII Sensors BoosterPack™][bp-bassensorsmkii].  In theory, 
other TFT LCDs could be supported with the graphics library but have not 
been tested.

For board specific jumper settings, resources and BoosterPack modifications,
refer to the __Board.html__ file.

> If you're using an IDE such as Code Composer Studio (CCS) or IAR, please
refer to Board.html in your project directory for resources used and
board-specific jumper settings.

The Board.html can also be found in your SDK installation:

```text
<SDK_INSTALL_DIR>/source/ti/boards/<BOARD>
```

For compatibility with the CC2340R5 LaunchPad, a few jumper connections are 
necessary

| BP-BASSENSORSMKII | LP-EM-CC2340R5 |
|-------------------|----------------|
| D_O               | DIO21          |
| INT1              | DIO19          |
| H_V+              | DIO11          |

![boostxl_sharp128_setup][figure 1]

## Code Description

The sensor libraries are contained within the `sensors` folder

* **bmi160**: BMI160 + BMI150 inertial measurement unit plus magnetometer
* **drv5055**: DRV5055 magnetometer
* **hdc2010**: HDC2010/HDC2080 temperature and humidity sensor
* **opt3001**: OPT3001 ambient light sensor
* **tmp116**: TMP116/TMP117 temperature sensor

Each of these includes variables and notification APIs to update the 
corresponding BLE characteristic.  They also have their own task established 
in the `main_freertos.c` file.  Note that not all tasks are enabled by default 
and thus will not be updated in the Bluetooth view.  Enable only the sensors 
that need to be evaluated to alleviate the I2C bus and Bluetooth communication.

The Bluetooth Simple GATT file, `common/Profiles/simple_gatt/simple_gatt_profile.c`, 
has configured the following Characteristics for view in the SimpleLink Connect 
mobile phone application.

| UUID           | Description           | Property |
|----------------|-----------------------|----------|
| 0xFFF1         | TMP117 Temperature    | Read     |
| 0xFFF2         | HDC2080 Humidity      | Read     |
| 0xFFF3         | HDC2080 Temperature   | Read     |
| 0xFFF4         | OPT3001 Lumosity      | Read     |
| 0xFFF5         | DRV5055 Magnetism     | Read     |
| 0xFFF6         | BMI160 X Acceleration | Notify   |
| 0xFFF7         | BMI160 Y Acceleration | Notify   |
| 0xFFF8         | BMI160 Z Acceleration | Notify   |

## Example Usage

* Run the example on the LP-EM-CC2340R5

* Open the SimpleLink Connect mobile phone application and enable Bluetooth LE Scan

* Find the `BLE Sensor Project` from Available Devices, connect to it and then 
navigate to the `TI Simple Peripheral Service`

* Interact with the sensors as detailed in the above table.  Use hex format as 
decimal uint16 will not be formatted properly

## Application Design Details

FreeRTOS:

* Please view the `FreeRTOSConfig.h` header file for example configuration
information.

[bp-bassensorsmkii]: https://www.ti.com/tool/BP-BASSENSORSMKII
[figure 1]:bp_bassensorsmkii_setup.png "BP-BASSENSORSMKII on LP-EM-CC2340R5"
