# Window Covering for SimpleLink CC2340R5

This project implements a "smart" window covering using CC2340R5. The 
application features has various features, listed below, and is suitable for 
battery-powered applications.

## BDC Motor Control Features

This project implements a generic two pole BDC motor with the following functionality:

* Zigbee Window Covering ZCL End Device Node
* Bidirectional and variable speed (PWM) motor control
* Over-current protection (OCP) via ADC sampling
* Configurable end point selection
* Low-power (<10 uA) standby state
* Control from the device-side (Zigbee End Device), remotely (Zigbee Coordinator), 
or optionally via third party smart hub devices such as Amazon Echo.

## Pin Configuration

| Function | CC2340R5 Pin | Configuration | Description |
|----------|-----|---------------|-------------|
| **Motor Control PWM** |||
| M(+) | DIO5 | LGPT2 CH2 | Raise the window covering motor control |
| M(-) | DIO1 | LGPT3 CH1 | Lower the window covering motor control |
| **Hall Sensor** |||
| Hall VCC | DIO23 | GPIO Output | Hall sensor power |
| Hall OUT | DIO0 | GPIO Input | Hall sensor output |
| Hall GND | GND | Wire | Common ground connection |
| **ADC Inputs** |||
| Current Sense | DIO24 | ADC Input | Motor current sensing |
| **Control & Status** |||
| Button 0 | DIO10 | GPIO Input | User button 0 |
| Button 1 | DIO9 | GPIO Input | User button 1 |
| **Serial Logging** |||
| UART TX | DIO20 | UART0 TX | Serial communication transmit |

## Configuration Parameters

The application behavior can be customized by modifying the following parameters 
in the source code:
```c
// ADC Definitions
#define ADCSAMPLESIZE       100         // Length of ADCBuf
#define ADCBUF_SAMPLING_FREQ 1000       // Sample ADCBuf 1 kHz
#define OCP_THRESHOLD       1500000      // Threshold for OCP condition, detected by VSENSE

// PWM Definitions
#define PWM_DUTY_INC        1           // Duty Cycle increase per cycle (in us)
#define PWM_PERIOD          100         // Duty Cycle period (in us -> 1000us = 1ms)
#define PWM_START_POINT     0           // Duty Cycle to ramp up from (zero is min)
#define PWM_END_POINT       100         // Duty Cycle to ramp up to (1000 is max)

// Event mask
#define ACTION_LOWER        0x01        // Counter-clockwise action
#define ACTION_RAISE        0x02        // Clockwise action
#define ACTION_STOP         0x04        // Stop the motor
#define ACTION_ADC          0x08        // ADC OCP check

// Other conficurations
#define THREADSTACKSIZE     1024
#define NVS_SIG             0x5AA5      // Signature of NVS
#define EDGE_DELTA          20          // Delta before updating edge counter
#define USE_HALL                        // Determines whether Hall effect logic is used

// Timer configurations
#define STALL_TIMEOUT       1000000     // 1s timeout for no movement detected by hall effect sensor
#define MOVEMENT_TIME       3000000     // 3s timeout for total movement with no hall effect sensor
```

## Dependencies

This project depends on the following software components:
* [SimpleLink Low Power F3 SDK] v9.11.00.18
* [Code Composer Studio] v20.2.0
* [TI CLANG Compiler] v4.0.3
* [SysConfig] v1.23.2

## Hardware Requirements

This project depends on the use of the following hardware:

* 2x [LP-EM-CC2340R5] - CC2340R5 LaunchPad™ development kit for SimpleLink™ Low 
Energy MCU
* 2x [LP-XDS110ET] - XDS110ET LaunchPad™ development kit debugger with 
EnergyTrace™ software
* 1x [DRV8251AEVM] - DRV8251A motor driver evaluation module
* 1x [TMAG5213] - TMAG5213 Hall-effect latch for low-cost applications
* 1x Brushed DC Motor
* Optional: Third party smart hub device

## Peripherals & Driver Configurations

When this project is built, the SysConfig tool generates TI-Driver configurations 
into the `ti_drivers_config.c` and `ti_drivers_config.h` files. Information on 
pins and resources used is present in both generated files. The System Configuration 
file (`window_covering_LP_EM_CC2340R5_freertos_ticlang.syscfg`) can be opened 
with SysConfig's graphical user interface to determine pins and resources used.

The project uses the following peripherals:

* Zigbee - For network and radio configurations
* PWM - For controlling motor speed and direction
* GPIO - For hall sensor interrupts
* ADC - For sensing current BDC motor current
* UART - For (optional) logging functionality
* Timers - For PWM duty cycle changes and various timeouts

## Example Usage

1. Build and flash the project to your CC2340R5 LaunchPad
2. Connect the window covering motor driver and Hall-effect sensor according to 
the pin configuration table
3. Power on the LaunchPad and motor driver
3. Ensure all peripherals and pin assignments are correctly set up
4. Control the motor using either buttons on either the device-side LaunchPad or 
remotely (see the window_controller project for more information on remote shade 
control)

Local push button actions are described here:

Here are local push button commands:

| Button | Held longer than 1s | Double-pressed within 1s | Clicked once |
|----------|-----|---------------|-------------|
| BTN-1 (left/DIO20) | Move motor up until released | Set top end point | Move to top end point |
| BTN-2 (right/DIO0) | Move motor down until released | Set bottom end point | Move to bottom end point |

## Optional Hall-Effect Sensor Setup

The Hall-effect sensor is used to track the motor position. To set up the Hall-effect sensor:
1. Connect the Hall-effect sensor to the CC2340R5 according to the pin configuration table
2. Configure the Hall-effect sensor in the syscfg file
3. Ensure `#define USE_HALL` is set

```c
GPIO6.$name            = "CONFIG_GPIO_HALL_SENSOR";
GPIO6.interruptTrigger = "Rising Edge";
GPIO6.gpioPin.$assign = "boosterpack.10";
```
The Hall-effect sensor is powered using a GPIO output, enabling it to be 
disconnected when the motor is not active. Additionally, the Hall-effect 
sensor can be optionally removed. To do this, comment out `#define USE_HALL`. 
Then, the project will use a time based motor tracking instead of position 
based (defaults to 3 seconds).

## Conclusion

This project demonstrates how to implement a Zigbee window covering based on 
battery power using the CC2340R5. The application features Brushed DC motor 
control and efficient power management for battery-powered applications. The 
Hall-effect sensor is used to track the motor position, and the OCP threshold 
is used to detect overcurrent conditions. The motor control setup includes the 
PWM configuration and the motor driver setup. The logging setup includes the 
serial logging configuration.

<!-- Definition of inline references -->
[LP-EM-CC2340R5]: https://www.ti.com/tool/LP-EM-CC2340R5
[LP-XDS110ET]: https://www.ti.com/tool/LP-XDS110ET
[DRV8251AEVM]: https://www.ti.com/tool/DRV8251AEVM
[TMAG5213]: https://www.ti.com/product/TMAG5213

[SimpleLink Low Power F3 SDK]: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/9.11.00.18
[Code Composer Studio]: https://www.ti.com/tool/download/CCSTUDIO/20.2.0
[TI CLANG Compiler]: https://www.ti.com/tool/download/ARM-CGT-CLANG/4.0.3.LTS
[SysConfig]: https://www.ti.com/tool/download/SYSCONFIG/1.23.2.4156
