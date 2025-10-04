# RF Echo TX with Stepper Motor Control

---

Example Summary
---------------
This example demonstrates the integration of radio frequency (RF) communication with stepper motor control on the Texas Instruments CC2340R5 platform. The system consists of two main components:

1. **RF Echo TX/RX**: Transmits packets and receives echoed responses, similar to the standard RF Echo TX example
2. **Stepper Motor Control**: Controls a bipolar stepper motor based on commands received via RF

The RF Echo TX component transmits packets and waits for echoed responses from another device. When specific command values are received in the echoed packet, the stepper motor is commanded to turn either clockwise or counter-clockwise.

Hardware Requirements
--------------------
* Texas Instruments [LP-EM-CC2340R5] LaunchPad (2 boards for full functionality)
* [LP-XDS110ET]
* Bipolar stepper motor
* [DRV8411EVM] (or similar stepper motor driver 4xPWM, nSLEEP, and nFAULT pins)
* Appropriate power supply for the motor

Software Requirements
--------------------
* [SimpleLink Low Power F3 SDK] v9.11
* [SysConfig] v1.23.2
* [TI CLANG Compiler] v4.0.3.LTS

Board Specific Settings
-----------------------
1. The default frequency is:
    - 2440 MHz on the LP_EM_CC2340R5
In order to change frequency, modify the FREQUENCY define in rfEchoTx.c.

2. Stepper motor connections are defined in the Pin Map table below.

### Pin Map

| Function | Pin | Configuration | Description |
|----------|-----|---------------|-------------|
| **Stepper Motor Control** |||
| CONFIG_GPIO_AOUT1 | DIO23 | GPIO Output | Stepper motor phase A+ |
| CONFIG_GPIO_AOUT2 | DIO2 | GPIO Output | Stepper motor phase A- |
| CONFIG_GPIO_BOUT1 | DIO5 | GPIO Output | Stepper motor phase B+ |
| CONFIG_GPIO_BOUT2 | DIO25 | GPIO Output | Stepper motor phase B- |
| CONFIG_GPIO_nSLEEP | DIO1 | GPIO Output | Motor driver enable pin (active high) |
| CONFIG_GPIO_nFAULT | DIO18 | GPIO Input | Motor driver fault detection (active low) |
| **ADC Inputs** |||
| CONFIG_ADCBUF_VSENA | DIO24 | ADC | ADC input with DMA for sampling |
| CONFIG_ADC_VSENB | DIO0 | ADC | ADC input for measurements |
| **User Interface** |||
| CONFIG_GPIO_BTN_LEFT | DIO10 | GPIO Input | User button 1 - counter-clockwise |
| CONFIG_GPIO_BTN_RIGHT | DIO9 | GPIO Input | User button 2 - clockwise |
| CONFIG_GPIO_RLED | DIO14 | GPIO Output | Red LED |
| CONFIG_GPIO_GLED | DIO15 | GPIO Output | Green LED |
| **Timer** |||
| CONFIG_LGPTIMER_0 | LGPT3 | Timer | Timer for ADC triggering |

Example Usage
-------------
Run this example on one board (Board_1). On another board, run the matching Echo RX example (Board_2).

1. Board_2 should be started first, followed by Board_1
2. Board_1 transmits packets periodically, and Board_2 echoes them back with command values
3. When Board_1 receives an echoed packet with:
   - Value 0 in the last byte (BTN-1 on Board_2): The stepper motor turns counter-clockwise
   - Value 1 in the last byte (BTN-2 on Board_2): The stepper motor turns clockwise
4. The green LED (CONFIG_GPIO_GLED) toggles when packets are successfully transmitted and received
5. The motor will move for a predetermined number of steps when commanded

Alternatively, the motor can be controlled using the onboard buttons:
- Left button (BTN-1): Move the stepper motor counter-clockwise
- Right button (BTN-2): Move the stepper motor clockwise

Application Design Details
--------------------------
This example consists of two threads:
1. **Radio Thread**: Handles RF communication
2. **Main/Motor Thread**: Controls the stepper motor

### Radio Thread
The radio thread:
1. Configures the radio for Proprietary mode
2. Gets access to the radio via RCL_open
3. Configures RCL_CmdGenericTx and RCL_CmdGenericRx commands
4. Sets output power and frequency
5. Creates packets with random content
6. Transmits packets and receives echoed responses
7. Parses command values from the received packets and triggers the appropriate motor actions

### Main/Motor Thread
The motor thread:
1. Initializes GPIO pins for motor control
2. Sets up ADC for current monitoring
3. Configures timers for motor stepping and CPU load measurement
4. Processes events from radio thread and button inputs
5. Implements different stepping modes (Full-step or Half-step)
6. Provides protection features including over-current detection

### Motor Control Features
The stepper motor control includes:
1. **Three stepping modes** (selectable via defines):
   - FULL_STEP: 4-step sequence
   - HALF_STEP_SLOW: 8-step sequence with higher torque
   - HALF_STEP_FAST: 8-step sequence with higher speed
2. **Protection features**:
   - Over-current detection via ADC window comparator
   - Driver fault detection via nFAULT pin
3. **Performance monitoring**:
   - CPU load measurement through custom power policy

### Events System
The application uses a semaphore-based event system to communicate between the radio and motor threads:
- ACTION_CCLOCK (0x01): Command to move counter-clockwise
- ACTION_CLOCK (0x02): Command to move clockwise
- ACTION_START (0x04): Start the motor
- ACTION_STOP (0x08): Stop the motor

### Default Radio Configuration Constants
- MAX_LENGTH: 10 - Length of data in radio packets (must match rfEchoRx)
- PACKET_INTERVAL: 5s - Time between sending data requests to rfEchoRx
- RX_TIMEOUT: 20 ms - Timeout waiting for response from rfEchoRx

### Default Motor Configuration Constants
- STEP_FREQUENCY: 400 Hz - Defines the step rate
- NUMBER_STEPS: 400 - Number of steps per rotation (may vary based on motor)
- ADC_SAMPLE_SIZE: 100 - Buffer size for ADC samples
- ADC_PER_STEP: 50 - ADC measurements per step
- WINDOW_HIGH: 1500 - ADC threshold for over-current protection

Switching PHYs
--------------
The PHY used for TX operations can be configured in SysConfig by opening the
rfEchoTx.syscfg file and navigating to the "Custom" RF Stack. The PHY used
can be selected from the provided drop downs. The example only supports one PHY
at a time.  Be sure to modify the rfEchoRx project accordingly if any PHY
changes are applied.

In addition, several register fields must be exposed as defines so that
the example can appropriately set the length field in the packet header. This
can be accomplished by navigating in SysConfig to the Custom RF Stack --> Your
Selected PHY --> Code Export Configuration --> Register Field Defines and setting
its value to "PBE_GENERIC_RAM_LENCFG_LENPOS,PBE_GENERIC_RAM_LENCFG_NUMLENBITS".


<!-- Definition of inline references -->
[LP-EM-CC2340R5]: https://www.ti.com/tool/LP-EM-CC2340R5
[LP-XDS110ET]: https://www.ti.com/tool/LP-XDS110ET
[DRV8411EVM]: https://www.ti.com/tool/DRV8411EVM

[SimpleLink Low Power F3 SDK]: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/9.11.00.18
[Code Composer Studio]: https://www.ti.com/tool/download/CCSTUDIO/20.2.0
[TI CLANG Compiler]: https://www.ti.com/tool/download/ARM-CGT-CLANG/4.0.3.LTS
[SysConfig]: https://www.ti.com/tool/download/SYSCONFIG/1.23.2.4156