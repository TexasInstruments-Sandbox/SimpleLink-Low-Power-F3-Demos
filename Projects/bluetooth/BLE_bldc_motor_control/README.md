# BLDC Motor Control for CC2340R5

## Example Summary

This application demonstrates Brushless DC (BLDC) motor control using the Texas 
Instruments CC2340R5 microcontroller with FreeRTOS. The project enables speed 
control, direction control, and monitoring CPU Load, RPMs, and ADC measurements 
of a three-phase BLDC motor through hall sensor feedback.  The default project 
intends to connect to a DRV8329AEVM and use a BLDC motor which includes three 
hall state sensors.

## Features

* 6x PWM outputs for BLDC motor motion
* Current sense ADC input buffer storage synchronized with PWM duty cycle for
overcurrent protection
* UART command or BLE peripheral control of the motor
* Bidirectional speed-controlled movement
* Optional BLE data reporting to the central device

## Hardware Requirements

* Texas Instruments [LP-EM-CC2340R5] LaunchPad
* [LP-XDS110ET]
* BLDC motor with hall sensors
* [DRV8329AEVM] (or other motor driver circuitry compatible with the control signals from CC2340R5)
* Power supply appropriate for the motor

## Software Prerequisites

* [Code Composer Studio] v20.2
* [SimpleLink Low Power F3 SDK] v8.40
* [SysConfig] v1.22
* [TI CLANG Compiler] v4.0.1.LTS

## Peripherals & Pin Assignments

When this project is built, the SysConfig tool generates TI-Driver configurations 
into the `ti_drivers_config.c` and `ti_drivers_config.h` files. Information on pins 
and resources used is present in both generated files. The System Configuration file 
(`bldc_motor_control.syscfg`) can be opened with SysConfig's graphical user interface 
to determine pins and resources used.

### Pin Table

| Function | Pin | Configuration | Description |
|----------|-----|---------------|-------------|
| **Motor Control PWM** |||
| UH (Phase U High) | DIO24_A7 | LGPT1 CH0 | Phase U high-side PWM output |
| UL (Phase U Low) | DIO8 | LGPT1 CH0N | Phase U low-side PWM output |
| VH (Phase V High) | DIO12 | LGPT1 CH1 | Phase V high-side PWM output |
| VL (Phase V Low) | DIO21_A10 | LGPT1 CH1N | Phase V low-side PWM output |
| WH (Phase W High) | DIO6_A1_AR+ | LGPT1 CH2 | Phase W high-side PWM output |
| WL (Phase W Low) | DIO11 | LGPT1 CH2N | Phase W low-side PWM output |
| **Hall Sensors** |||
| Hall A | DIO23_A8 | GPIO Input | Hall sensor A input |
| Hall B | DIO18 | GPIO Input | Hall sensor B input |
| Hall C | DIO13 | GPIO Input | Hall sensor C input |
| **ADC Inputs** |||
| Current Sense | DIO0_A5 | ADC | Motor current sensing |
| Phase A Voltage | DIO1_A4 | ADC | Phase A voltage sensing |
| Phase B Voltage | DIO2_A3 | ADC | Phase B voltage sensing |
| Phase C Voltage | DIO5_A2 | ADC | Phase C voltage sensing |
| Supply Voltage | DIO7_A0_AR- | ADC | Supply voltage sensing |
| **Control & Status** |||
| Fault Input | DIO14 | GPIO Input | Motor driver fault input |
| Button 0 | DIO10 | GPIO Input | User button 0 |
| Button 1 | DIO9 | GPIO Input | User button 1 |
| **Communication** |||
| UART TX | DIO20_A11 | UART0 TX | Serial communication transmit |
| UART RX | DIO22_A9 | UART0 RX | Serial communication receive |

The project uses the following peripherals:

* PWM - For controlling motor phase outputs
* GPIO - For hall sensor inputs and fault detection
* ADC - For current and voltage sensing
* UART (optional) - For debug information and control commands
* Timers - For RPM measurement, PWM duty cycle changes, and various timeouts

## Board Resources & Jumper Settings

For board-specific jumper settings, resources and BoosterPack modifications, 
refer to the Board.html file in your project directory.

The Board.html can also be found in your SDK installation:

```text
<SDK_INSTALL_DIR>/source/ti/boards/LP_EM_CC2340R5
```

## Application Design Details

This application uses FreeRTOS to manage several features:

### Motor Control Features
* Speed control through PWM duty cycle adjustment
* Direction control (forward/reverse)
* Soft start/stop functionality
* Over-current protection
* Fault detection and handling
* RPM monitoring
* CPU utilization monitoring

### Operation
The application operates as follows:

1. Initializes all required peripherals (ADC, GPIO, PWM, timers)
2. Uses hall sensor feedback for proper commutation timing
3. Implements PWM control for speed regulation
4. Provides protection features for motor and system safety
5. Can be controlled via UART commands or BLE interface

### UART Commands
With USE_UART defined, the LP-XDS110(ET) can be connected to a computer COM 
port terminal at a baud rate of 921600 with 8 data bits, 1 stop bit, and no 
parity or flow control.  The motor can be controlled using the following 
keyboard entries:

| Key | Function |
|-----|----------|
| 's' | Start/Stop motor operation |
| 'u' | Increment PWM duty cycle (speed) by 5% |
| 'd' | Decrement PWM duty cycle (speed) by 5% |
| 'f' | Set direction to forward |
| 'r' | Set direction to reverse |
| 'l' | Read RPM and CPU utilization statistics |

### BLE Interface

The application includes BLE functionality with the following features:
* Device name: "BLE/BLDC MOTOR"
* Public address mode
* Supports service discovery by UUID
* Advertises with custom service UUID: 0xFFF0
* Includes a custom 128-bit UUID: B000405104C0C000F0

It connects to a SimpleLink Connect Phone app to control the following:

| Characteristic | Function |
|----------------|----------|
| 1 | Start/Stop motor operation (1 or 0) |
| 2 | Read motor RPM (hex) |
| 3 | Change direction of motor (1 or 0) |
| 4 | Change speed of motor (%) |

### Fault Handling
The application monitors for several fault conditions:

* Over-current protection
* External fault pin detection
* Motor stall detection
* Under/over voltage detection

## Example Usage

1. Build and flash the application to your CC2340R5 board
2. Connect the board to your DRV8329AEVM BLDC motor driver circuit
3. Power on the system
4. Control the motor using either:
   * UART commands (if enabled)
   * BLE connection via a compatible app

## FreeRTOS Configuration

Please view the `FreeRTOSConfig.h` header file for example configuration information.

## References

For more information about motor control concepts and TI's motor control solutions, see:
* [TI Motor Control](https://www.ti.com/applications/industrial/motor-drives/overview.html)
* [CC2340R5 Product Page](https://www.ti.com/product/CC2340R5)

<!-- Definition of inline references -->
[LP-EM-CC2340R5]: https://www.ti.com/tool/LP-EM-CC2340R5
[LP-XDS110ET]: https://www.ti.com/tool/LP-XDS110ET
[DRV8329AEVM]: https://www.ti.com/tool/DRV8329AEVM

[SimpleLink Low Power F3 SDK]: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/8.40.00.61
[Code Composer Studio]: https://www.ti.com/tool/download/CCSTUDIO/20.2.0
[TI CLANG Compiler]: https://www.ti.com/tool/download/ARM-CGT-CLANG/4.0.1.LTS
[SysConfig]: https://www.ti.com/tool/download/SYSCONFIG/1.22.0.3893