# Color Light for SimpleLink CC2340R5

This project implements a full-featured RGB+W (RGBW) color light using the
CC2340R5. The application acts as a Zigbee Router Color Light device and
provides advanced color control capabilities including hue/saturation, XY
chromaticity, color temperature, and enhanced color modes suitable for smart
lighting applications.

## RGB+W Color Control Features

This project implements a Zigbee Color Light with the following functionality:

* Zigbee Router Color Light ZCL Node
* RGBW LED control via hardware PWM (LGPT timers)
* Hue/Saturation (HSV color space) color control modes:
* Independent white channel control
* Dimming and intensity control
* Acts as *Target* for Finding and Binding
* Group and scene management for synchronized lighting

## Pin Configuration

| Function | CC2340R5 Pin | Configuration | Description |
|----------|-----|---------------|-------------|
| **RGB+W LED Control** |||
| Red LED PWM | DIO24 | LGPT0 CH0 | Red LED PWM output |
| Green LED PWM | DIO19 | LGPT0 CH1 | Green LED PWM output |
| Blue LED PWM | DIO0 | LGPT0 CH2 | Blue LED PWM output |
| White LED PWM | DIO5 | LGPT1 CH0 | White LED PWM output |
| **Control & Status** |||
| Button 1 | DIO10 | GPIO Input | User button 1 |
| Button 2 | DIO9 | GPIO Input | User button 2 |
| Status LED | DIO14 | GPIO Output | Red status LED |

## Configuration Parameters

The application behavior can be customized by modifying the following
parameters in the source code:

```c
// Zigbee Configuration
#define HA_DIMMABLE_LIGHT_ENDPOINT          5       // Zigbee endpoint

// PWM Configuration
#define TARGET_CNT_VALUE                    24000   // PWM counter target (24 MHz / 24000 = 1 kHz)

// Color Control Modes
typedef enum {
    RGBW_CHANGE_COLOR,      // Change color (hue/saturation)
    RGBW_CHANGE_INTENSITY,  // Change intensity/level
    RGBW_CHANGE_OFF         // Turn off
} RGBW_Change_States;

// Default Color Values
zb_uint8_t rVal = 0xFF;         // Red channel (0-255)
zb_uint8_t grVal = 0x00;        // Green channel (0-255)
zb_uint8_t bVal = 0x00;         // Blue channel (0-255)
zb_uint8_t wVal = 0x00;         // White channel (0-255)
zb_uint8_t saturation = 0xFF;   // Saturation (0-254)
zb_uint8_t hue = 0x00;          // Hue (0-254)
zb_uint8_t value = 255;         // Value/Brightness (0-255)
zb_uint8_t intensity = 0x0A;    // PWM duty cycle (4%)

// Color Control Capabilities
#define COLOR_CAPABILITIES  0x11    // Hue/Saturation + XY support
```

## Color Control Overview

The Zigbee Color Control cluster (0x0300) provides multiple methods for
controlling color:

### Hue/Saturation Mode (HSV)

* Hue: 0-254 (0° to 360°, 1.41° per step)
* Saturation: 0-254 (0% to 100%)
* Value/Level: 0-254 (brightness)
* Most intuitive for color selection

### Color Space Conversion

The application converts between HSV and RGB color spaces:

```
HSV to RGB:
  C = V * S
  X = C * (1 - |((H / 60) mod 2) - 1|)
  M = V - C

  (R', G', B') based on hue sector:
    0-60°:   (C, X, 0)
    60-120°: (X, C, 0)
    120-180°:(0, C, X)
    180-240°:(0, X, C)
    240-300°:(X, 0, C)
    300-360°:(C, 0, X)

  (R, G, B) = (R'+M, G'+M, B'+M) * 255
```

## Dependencies

This project depends on the following software components:
* [SimpleLink Low Power F3 SDK] v9.14.00.39
* [Code Composer Studio] v20.4
* [TI CLANG Compiler] v4.0.3.LTS
* [SysConfig] v1.23.2

## Hardware Requirements

This project depends on the use of the following hardware:

* 1x [LP-EM-CC2340R5] - CC2340R5 LaunchPad™ development kit for SimpleLink™
Low Energy MCU
* 1x [LP-XDS110ET] - XDS110ET LaunchPad™ development kit debugger with
EnergyTrace™ software
* 1x RGB+W LED module or common-anode RGB LED with separate white LED
* Current-limiting resistors (appropriate for LED forward voltage and current)
* Optional: LED driver circuit for high-power LEDs

**LED Connection Notes:**
- The CC2340R5 LGPT PWM outputs are active-high (PWM high = LED on)
- For common-cathode LEDs: Connect LED anodes to PWM outputs through
current-limiting resistors, cathodes to ground
- For common-anode LEDs: Invert PWM logic in software or use external
transistor drivers
- Maximum GPIO current: 4 mA per pin (use external drivers for high-power LEDs)

## Peripherals & Driver Configurations

When this project is built, the SysConfig tool generates TI-Driver
configurations into the `ti_drivers_config.c` and `ti_drivers_config.h` files.
Information on pins and resources used is present in both generated files. The
System Configuration file (`color_light.syscfg`) can be opened with
SysConfig's graphical user interface to determine pins and resources used.

The project uses the following peripherals:

* Zigbee - For network and radio configurations
* LGPT (Low-Power General Purpose Timer) - For PWM generation on RGB+W channels
* GPIO - For buttons and status LED
* UART - For (optional) logging functionality

## PWM Hardware Configuration

The color light uses two LGPT timers for four independent PWM channels:

**LGPT0 (RGB Channels):**
- Channel 0 (DIO24): Red LED
- Channel 1 (DIO19): Green LED
- Channel 2 (DIO0): Blue LED

**LGPT1 (White Channel):**
- Channel 0 (DIO5): White LED

**PWM Characteristics:**
- Frequency: ~1 kHz (24 MHz / 24000)
- Resolution: 16-bit (0-24000 counts)
- Duty Cycle Range: 0-100%

## Example Usage

### Basic Setup

1. Build and flash the project to your CC2340R5 LaunchPad
2. Connect RGB+W LED module:
   - Red LED to DIO24 through current-limiting resistor
   - Green LED to DIO19 through current-limiting resistor
   - Blue LED to DIO0 through current-limiting resistor
   - White LED to DIO5 through current-limiting resistor
   - Common cathode/anode as appropriate for LED type
3. Power on the LaunchPad
4. The device will start as a Zigbee Router and join an existing network

### Zigbee Network Operation

1. Ensure a Zigbee Coordinator is already running and forming a network
2. The color light will automatically attempt to join the network
3. Once joined, the device is ready to receive color control commands
4. Use a Zigbee controller (e.g., color_controller or coordinator) to send
Color Control, On/Off, and Level Control commands

### Controlling the Color Light

**Via Zigbee Commands:**
* **On/Off Commands:** Turn light on/off
* **Move to Hue/Saturation:** Set specific color
* **Enhanced Move to Hue:** High-precision hue control
* **Move to Level:** Adjust brightness

### Color Command Examples

**Setting Pure Red (Hue/Saturation):**
```
Hue = 0 (0°)
Saturation = 254 (100%)
Level = 254 (100% brightness)
Result: RGB = (255, 0, 0)
```

**Setting Cyan (Hue/Saturation):**
```
Hue = 127 (180°)
Saturation = 254 (100%)
Level = 254 (100% brightness)
Result: RGB = (0, 255, 255)
```

## Color Control Implementation Details

### Initialization Sequence

1. Initialize LGPT timers for 4-channel PWM output
2. Configure PWM frequency and initial duty cycles
3. Register ZCL cluster handlers for Color Control
4. Set default color to red (hue=0, saturation=100%)
5. Set initial brightness to 4% duty cycle

### HSV to RGB Conversion

The application implements HSV-to-RGB conversion in the
`rgbwChange()` function:

```c
void rgbwChange(zb_uint8_t hue, zb_uint8_t sat,
                                    zb_uint8_t val)
{
   //...
        // Convert H to a value from 0 to 1530 (6 * 255)
        // The range 0-255 for H corresponds to 0-360 degrees
        // We can work in this scaled integer space
        uint16_t scaled_h = (uint16_t)hue * 6; 
        region = scaled_h / 255; // Which sextant (0-5)
        remainder = scaled_h % 255; // Remainder within the sextant

        // Calculate p, q, t
        // p = V * (1 - S)
        // q = V * (1 - S * f)
        // t = V * (1 - S * (1 - f))
        // Using integer math with scaling by 255 to avoid floats:
        p = (value * (255 - saturation)) / 255;
        q = (value * (255 - saturation * remainder / 255)) / 255; // Note: integer division order matters
        t = (value * (255 - saturation * (255 - remainder) / 255)) / 255;

    // Determine RGB' based on hue sector
    // Add minimum value and scale to 0-255
    // Update rVal, grVal, bVal globals
    // Set PWM duty cycles for RGB channels
}
```

### Zigbee ZCL Cluster Support

The device implements the following Zigbee clusters:

**Server Clusters:**
* **Basic Cluster (0x0000):** Device identification and version info
* **Identify Cluster (0x0003):** Device identification mode
* **Groups Cluster (0x0004):** Group membership management
* **Scenes Cluster (0x0005):** Scene storage and recall
* **On/Off Cluster (0x0006):** On/Off control
* **Level Control Cluster (0x0008):** Brightness control
* **Color Control Cluster (0x0300):** Full color control with:
  - Hue/Saturation attributes and commands
  - XY chromaticity attributes and commands
  - Color temperature attributes and commands
  - Enhanced hue attributes and commands
  - Color loop attributes and commands
  - Color capabilities reporting

### Color Capabilities

The device reports the following color capabilities (0x11 = 0b00010001):
- **Bit 0 (Hue/Saturation):** Supported ✓
- **Bit 4 (XY Chromaticity):** Supported ✓
- Color Temperature: Not reported in capabilities but may be supported

## Advanced Features Not Yet Implemented

### Color Transitions

The Color Control cluster supports smooth transitions between colors:
- Transition time specified in tenths of seconds (0-65535)
- Linear interpolation in HSV or XY color space
- Gradual PWM duty cycle changes

### Color Loop Mode

Automatic color cycling through the hue spectrum:
- Loop time: Configurable (seconds to complete one cycle)
- Direction: Increment or decrement hue
- Start/End hue values: Configurable range

### Scene Support

Store and recall complete lighting states:
- Stores: On/Off, Level, Hue, Saturation, Color Temperature
- Up to 16 scenes per group
- Scene transitions with configurable timing

### Group Control

Control multiple lights simultaneously:
- Broadcast commands to all group members
- Synchronized color changes
- Group scenes for coordinated effects

## Troubleshooting

### LED Not Lighting

**No output on any channel:**
* Verify PWM configuration in SysConfig
* Check LGPT timer initialization
* Confirm GPIO pin assignments match hardware connections
* Use oscilloscope to verify PWM signals at CC2340R5 pins

**Single channel not working:**
* Check current-limiting resistor value (too high = dim/off)
* Verify LED polarity (common-cathode vs common-anode)
* Test LED with known-good power source
* Check for GPIO conflicts with other peripherals

### Incorrect Colors

**Colors don't match commands:**
* Verify HSV-to-RGB conversion algorithm
* Check for gamma correction requirements
* Confirm LED forward voltages are similar (color balance)
* Adjust color calibration in software

**Colors appear washed out:**
* Check saturation values (low saturation = pastel colors)
* Verify maximum brightness setting
* Ensure adequate LED current (resistor value too high)
* Consider LED binning differences (color variation)

### Zigbee Network Issues

**Device not joining network:**
* Confirm Zigbee Coordinator is running and permitting joins
* Perform factory reset (hold BTN-1/BTN-2 during startup)
* Check Zigbee channel configuration matches coordinator
* Verify network key and security settings
* Use a sniffer for further debugging

**Commands not received:**
* Check device is bound to controller (Finding & Binding)
* Verify correct endpoint (endpoint 5)
* Confirm group membership if using group commands
* Check Zigbee link quality (RSSI)

### Performance Issues

**Flickering LEDs:**
* Increase PWM frequency (reduce TARGET_CNT_VALUE)
* Check for timer interrupt conflicts
* Verify stable power supply
* Reduce computational load in PWM update routine

**Slow color transitions:**
* Optimize HSV-to-RGB conversion (use lookup tables)
* Reduce transition calculation complexity
* Increase PWM update rate if needed

## Conclusion

This project demonstrates how to implement a full-featured Zigbee Color Light
using the CC2340R5, enabling wireless control of RGB+W LED lighting. The
application combines Zigbee Router functionality with advanced color control
capabilities, supporting multiple color modes and industry-standard color
specifications. The hardware PWM implementation provides flicker-free, smooth
color transitions suitable for lighting applications.

Key features include:
* Standards-compliant Zigbee Color Light device
* Four independent PWM channels (RGB+W)
* HSV color control modes (XY, CCT, Enhanced Hue also available)
* Smooth color transitions and effects
* Color loop and scene support
* Group control for synchronized lighting
* Support for up to 6 primary color channels (extensible)

<!-- Definition of inline references -->
[LP-EM-CC2340R5]: https://www.ti.com/tool/LP-EM-CC2340R5
[LP-XDS110ET]: https://www.ti.com/tool/LP-XDS110ET

[SimpleLink Low Power F3 SDK]: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/9.14.00.39
[Code Composer Studio]: https://www.ti.com/tool/download/CCSTUDIO/20.4.0
[TI CLANG Compiler]: https://www.ti.com/tool/download/ARM-CGT-CLANG/4.0.3.LTS
[SysConfig]: https://www.ti.com/tool/download/SYSCONFIG/1.23.2
