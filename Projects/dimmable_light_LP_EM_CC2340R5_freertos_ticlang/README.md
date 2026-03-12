# Dimmable Light with DALI for SimpleLink CC2340R5

This project implements a Zigbee-to-DALI bridge using the CC2340R5, enabling 
wireless control of DALI-compliant lighting fixtures. The application acts as 
a Zigbee Router Dimmable Light device and translates Zigbee commands to DALI 
protocol for controlling commercial lighting systems.  This project is 
based on the existing [MSPM0 DALI UG] and [TIDA-010963].

## DALI Communication Features

This project implements DALI (Digital Addressable Lighting Interface) IEC 62386 
communication with the following functionality:

* Zigbee Router Dimmable Light ZCL Node
* DALI protocol control via bit-banged GPIO communication
* Variable dimming control (0-254 levels)
* On/Off control with level restoration
* Acts as *Target* for Finding and Binding
* Optional PWM-based control for non-DALI fixtures
* Hardware-agnostic DALI HAL using ClockP and GPIO

## Pin Configuration

| Function | CC2340R5 Pin | Configuration | Description |
|----------|-----|---------------|-------------|
| **DALI Communication** |||
| DALI TX | DIO9 | GPIO Output | DALI transmit line (bit-banged) |
| DALI RX | DIO12 | GPIO Input | DALI receive line with edge detection |
| **Optional PWM Control** |||
| PWM Output | DIO0 | PWM (LGPT) | PWM dimming output (when USE_PWM_CONTROL defined) |
| **Control & Status** |||
| Button 1 | DIO10 | GPIO Input | User button 1 |
| Button 2 | DIO8 | GPIO Input | User button 2 |
| Red LED | DIO14 | GPIO Output | Status LED |

## Configuration Parameters

The application behavior can be customized by modifying the following 
parameters in the source code:

```c
// DALI Configuration
#define ZB_DIMMABLE_LIGHT_ENDPOINT          5       // Zigbee endpoint
#define LIGHT_LEVEL_MAX                     250     // Maximum light level (DALI 0-254 range)

// DALI Timing (in dali_hal_simple.h)
#define DALI_BIT_TIME_US                    416     // DALI bit time: 416.67 µs
#define DALI_RX_TIMEOUT_US                  2000    // RX timeout: 2 ms
#define DALI_FR_PERIOD_US                   10000   // Free running period: 10 ms

// PWM Configuration (optional, when USE_PWM_CONTROL is defined)
#define PWM_DUTY_FRACTION_MAX               100     // PWM duty cycle range

// DALI Command Structure
typedef struct {
    uint8_t addrByte;       // DALI address byte (0x02 for broadcast)
    uint8_t opcodeByte;     // DALI command/level byte
    bool isSendTwiceCmd;    // Send command twice (per DALI spec)
} DALI_Command;
```

## DALI Protocol Overview

DALI (Digital Addressable Lighting Interface) is a two-wire digital 
communication protocol specifically designed for lighting control:

* **Bit Rate:** 1200 baud (416.67 µs per bit)
* **Encoding:** Manchester encoding (edge transitions represent data)
* **Voltage Levels:** 0V (low) and 24V (high)
* **Addressing:** Individual device addresses (0-63) or broadcast (254)
* **Commands:** 256 standard commands including dimming, on/off, and configuration

### DALI Frame Format
```
[Start Bit] [Address Byte: 8 bits] [Command/Data Byte: 8 bits]
```

### Common DALI Commands Used
* **Direct Arc Power:** 0x00-0xFE (dimming level 0-254)
* **Off:** 0x00 with address
* **Recall Max Level:** 0x05
* **Recall Min Level:** 0x06

## Dependencies

This project depends on the following software components:
* [SimpleLink Low Power F3 SDK] v9.14.00.39
* [Code Composer Studio] v20.4
* [TI CLANG Compiler] v4.0.3.LTS
* [SysConfig] v1.23.2

## Hardware Requirements

This project depends on the use of the following hardware:

* 1x [LP-EM-CC2340R5] - CC2340R5 LaunchPad™ development kit for SimpleLink™ Low Energy MCU
* 1x [LP-XDS110ET] - XDS110ET LaunchPad™ development kit debugger with EnergyTrace™ software
* 1x DALI-compliant lighting fixture or DALI control gear: a MSPM0 LaunchPad from the [MSPM0 DALI UG]
* 2x [TIDA-010963] Boosterpacks
* 1x DALI bus power supply (24V DC)

**Note:** The CC2340R5 GPIO operates at 3.3V logic levels. A DALI interface 
circuit is required to translate between 3.3V logic and 24V DALI bus voltage 
levels for production systems.

## Peripherals & Driver Configurations

When this project is built, the SysConfig tool generates TI-Driver 
configurations into the `ti_drivers_config.c` and `ti_drivers_config.h` files. 
Information on pins and resources used is present in both generated files. The 
System Configuration file (`dimmable_light.syscfg`) can be opened with 
SysConfig's graphical user interface to determine pins and resources used.

The project uses the following peripherals:

* Zigbee - For network and radio configurations
* GPIO - For DALI TX/RX bit-banging communication
* ClockP - For precise DALI timing (416.67 µs bit periods)
* PWM (optional) - For direct PWM dimming control
* HwiP - For interrupt management

## DALI HAL Architecture

The DALI Hardware Abstraction Layer (HAL) uses a software-based approach:

* **TX Path:** ClockP timer triggers GPIO bit transitions at 416.67 µs intervals
* **RX Path:** GPIO edge detection captures incoming DALI frames with timeout
* **Timing:** All timing implemented via ClockP (no hardware timer dependencies)
* **Bit-Banging:** Manchester encoding/decoding in software

This approach provides:
* Hardware independence (works on any CC23xx device)
* Flexibility for protocol customization
* No hardware timer resource conflicts with other peripherals

## Example Usage

### Basic Setup

1. Build and flash the project to your CC2340R5 and MSPM0 LaunchPads
2. Connect the two [TIDA-010963] BoosterPacks on top of the LaunchPads
3. Connect the TIDA-010963 RX and TX pins (TX <--> RX and vice versa)
4. Power on the LaunchPad
5. Power the barrel plug 24V supply
6. The device will start as a Zigbee Router and join an existing network

![Zigbee DALI Controller Diagram][figure 1]

### Zigbee Network Operation

1. Ensure a Zigbee Coordinator is already running and forming a network
2. The dimmable light will automatically attempt to join the network
3. Once joined, the device is ready to receive dimming commands
4. Use a Zigbee controller (e.g., dimmer_switch or coordinator) to send On/Off and Level Control commands

### Controlling the DALI Light

**Via Zigbee Commands:**
* **On Command:** Restores light to previous level (or max level)
* **Off Command:** Turns light off (remembers current level)
* **Move to Level:** Sets specific brightness (0-254)

**Local Button Control:**
* Button presses can be configured to trigger local on/off or dimming (implementation dependent)

### DALI Command Flow

```
[Zigbee Controller]
    ↓ (Wireless Zigbee)
[CC2340R5 Dimmable Light]
    ↓ (DALI Protocol via GPIO)
[MSPM0 DALI Control Gear]
    ↓ (Power Control)
[LED/Lamp]
```

## Switching Between DALI and PWM Control

The project supports two control modes:

### DALI Mode (Default)
* Communicates with external MSPM0 DALI control gear
* Uses bit-banged GPIO for DALI protocol
* Comment out `#define USE_PWM_CONTROL` in source

### PWM Mode (Optional)
* Directly controls LED via PWM output
* Useful for testing without DALI hardware
* Uncomment `#define USE_PWM_CONTROL` in [dimmable_light.c:77](dimmable_light.c#L77)

```c
//#define USE_PWM_CONTROL  // Comment out for DALI mode
#define USE_PWM_CONTROL   // Uncomment for PWM mode
```

## DALI Implementation Details

### Initialization Sequence

1. `DALI_HAL_init()` - Initialize GPIO and ClockP resources
2. Configure DALI TX pin as output, RX pin as input with edge detection
3. Register clock callbacks for TX timing, RX timeout, and free-running timer
4. Set initial light level to half (128)

### Command Transmission

```c
void setBulbValue(zb_uint8_t percentage)
{
    if(percentage >= LIGHT_LEVEL_MAX)
        percentage = LIGHT_LEVEL_MAX;

    gUserVar.addrByte = 0x02;           // Broadcast address
    gUserVar.opcodeByte = percentage;    // Direct arc power level
    gUserVar.isSendTwiceCmd = false;

    DALI_CD_sendCmd();                   // Transmit via DALI protocol
}
```

### Zigbee ZCL Command Handling

The device implements standard Zigbee Cluster Library (ZCL) commands:
* **On/Off Cluster (0x0006):** On, Off, Toggle commands
* **Level Control Cluster (0x0008):** Move to Level, On/Off commands
* **Groups Cluster (0x0004):** Group management for synchronized control
* **Scenes Cluster (0x0005):** Scene recall for preset lighting configurations

## Troubleshooting

### DALI Communication Issues

**No response from DALI device:**
* Verify DALI bus power supply (24V DC)
* Check voltage level translation circuit
* Confirm DALI device address matches configuration
* Use oscilloscope to verify TX signal timing (416.67 µs bit periods)

**Erratic dimming behavior:**
* Check for electrical noise on DALI bus
* Verify DALI polarity (DALI is polarity-insensitive, but check connections)
* Ensure proper Manchester encoding (use logic analyzer)

**Device not joining Zigbee network:**
* Confirm Zigbee Coordinator is running and permitting joins
* Perform factory reset (hold button 1 or 2 during startup)
* Check Zigbee channel configuration matches coordinator

## Conclusion

This project demonstrates how to implement a Zigbee-to-DALI bridge using the 
CC2340R5, enabling wireless control of commercial DALI lighting systems. The 
application combines Zigbee Router functionality with DALI protocol 
implementation, providing a complete solution for smart lighting control. The 
flexible architecture supports both DALI communication and direct PWM control, 
making it suitable for various lighting applications.

Key features include:
* Standards-compliant Zigbee Dimmable Light device
* Software-based DALI protocol implementation
* Hardware-independent HAL using ClockP and GPIO
* Support for 254 dimming levels
* Optional PWM mode for non-DALI fixtures

<!-- Definition of inline references -->
[LP-EM-CC2340R5]: https://www.ti.com/tool/LP-EM-CC2340R5
[LP-XDS110ET]: https://www.ti.com/tool/LP-XDS110ET

[SimpleLink Low Power F3 SDK]: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/9.14.00.39
[Code Composer Studio]: https://www.ti.com/tool/download/CCSTUDIO/24.0.0
[TI CLANG Compiler]: https://www.ti.com/tool/download/ARM-CGT-CLANG/4.0.2.LTS
[SysConfig]: https://www.ti.com/tool/download/SYSCONFIG/1.27.0

[TIDA-010963]: https://www.ti.com/tool/TIDA-010963
[MSPM0 DALI UG]: https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__ALq2q1stw.2ChgqHITlfKA__MSPM0-SDK__a3PaaoK__LATEST
[figure 1]:zigbee_dali_diagram.png "Zigbee DALI controller design"
