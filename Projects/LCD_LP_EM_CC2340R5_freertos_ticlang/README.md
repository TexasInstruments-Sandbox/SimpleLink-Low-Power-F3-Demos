# 7-Segment LCD Driver for SimpleLink CC2340R5

This project implements a 7-segment 1/3 bias, 1/3 duty LCD driver with dynamic 
brightness control based on battery voltage for the CC2340R5. The application 
features automatic contrast adjustment, digit display controlled by buttons or
the IC temperature, and backlight strength adjustment (optional based on LCD 
hardware features avaialable).

For instructions regarding the creation of your own LCD lookup table, see the 
__LCD Lookup Table Design Notes__ section at the end of this document.

## Features

* 1/3 bias, 1/3 duty 7-segment LCD control
* Example for 2-character LCD screen
* Battery monitor notifications for adjusting LCD contrast
* BTN-2 increments the ones and BTN-1 increments the tens with wraparound from 99+ to 0
* Optional backlight control with simple timer-based PWM
* Optional chip temperature readings output on LCD

## Software Dependencies

This project depends on the following software components:

* [SimpleLink Low Power F3 SDK] (v9.11.00.18 or later)
* [Code Composer Studio] (v20.x or later)

## Hardware Requirements

* [LP-EM-CC2340R5](https://www.ti.com/tool/LP-EM-CC2340R5) - CC2340R5 LaunchPad™ 
development kit
* 7-segment LCD display module with common anode/cathode configuration
* Appropriate resistor network for LCD 1/3 bias LCD driver as seen in [figure 1]

![CC2340R5_LCD_schematic][figure 1]

## Pin Configuration

| Function | CC2340R5 Pin | Configuration | Description |
|----------|--------------|---------------|-------------|
| **LCD Backlight** |||
| Backlight | DIO15 | GPIO Output | LCD backlight control |
| **LCD Control** |||
| PWM | DIO11 | GPIO Output | PWM signal for LCD |
| PWM_N | DIO13 | GPIO Output | Inverted PWM signal |
| COM1 | DIO8 | GPIO Output | COM 1 control |
| COM2 | DIO19 | GPIO Output | COM 2 control |
| COM3 | DIO21 | GPIO Output | COM 3 control |
| **LCD Segments** |||
| SEG1 | DIO23 | GPIO Output | Segment 1 control |
| SEG2 | DIO25 | GPIO Output | Segment 2 control |
| SEG3 | DIO7 | GPIO Output | Segment 3 control |
| SEG4 | DIO14 | GPIO Output | Segment 4 control |
| SEG5 | DIO6 | GPIO Output | Segment 5 control |
| SEG6 | DIO12 | GPIO Output | Segment 6 control |
| SEG7 | DIO5 | GPIO Output | Segment 7 control |
| **User Input** |||
| Left Button | DIO10 | GPIO Input | Increment tens digit |
| Right Button | DIO9 | GPIO Input | Increment ones digit |

## Configuration Parameters

The application behavior can be customized by modifying the following parameters 
in the source code:

```c
#define LIGHT_STRENGTH  2               // Backlight intensity (0-4): 0=0%, 1=25%, 2=50%, 3=75%, 4=100%
#define LIGHT_FREQUENCY 1000            // Backlight PWM frequency in Hz
#define REFRESH_RATE    120             // LCD refresh rate in Hz
#define THRESHOLD_DELTA_MILLIVOLT 200   // Voltage threshold for battery monitoring
#define TEMP_UPDATE 10000000            // Number of microseconds before the temperature updates
#define USE_TEMP
```

## Battery Voltage Dependent Contrast

The display's contrast is automatically adjusted based on the battery voltage 
to maintain optimal visibility across different battery charge levels:

| Battery Voltage | Contrast Setting |
|-----------------|------------------|
| ≤ 2.4V          | Very high (1)    |
| 2.4V - 2.8V     | High (2)         |
| 2.8V - 3.2V     | Medium (4)       |
| 3.2V - 3.6V     | Low (16)         |

## Example Usage

1. Build and flash the project to your CC2340R5 LaunchPad
2. Connect the 7-segment LCD display according to the pin configuration table
3. Power on the board to see the default display (00)
4. Press the left button to increment the tens digit
5. Press the right button to increment the ones digit
6. If USE_TEMP is defined, then the chip temperature in degrees Celsius will
update every TEMP_UPDATE microseconds

## LCD Lookup Table Design Notes

The pin configuration for a two-digit LCD with seven special segment  indicators 
is displayed below. For 'x' where x is a character A-G, all 1x segments correspond 
to the "tens" digit, which all 2x segments correspond to the "ones" digit. 
Segments S1-S7 represent the special segment indicators that are located on the 
peripheral of this display. All in, there are 21 segments that can be turned ON or OFF.

| Pin  | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 10 |
|------|----|----|----|----|----|----|----|----|----|----|
| COM1 | 1F | 1A | 1B | 2F | 2A | 2B | S2 |COM1|  - |  - | 
| COM2 | 1E | 1G | 1C | 2E | 2G | 2C | S4 |  - |COM2|  - | 
| COM3 | S1 | 1D | S7 | S6 | 2D | S5 | S3 |  - |  - |COM3| 

The physical layout of this LCD is listed below.

```
 _ 1A _       _ 2A _       * S1
|      |     |      |      * S2
1F     1B   2F      2B     * S3
|_ 1G _|     |_ 2G _|      * S4
|      |     |      |      * S5
1E     1C   2E      2C     * S6
|_ 1D _|     |_ 2D _|      * S7
```

To display the value __95__, as seen below, the following segments must be turned on:

1A, 1B, 1C, 1D, 1F, 1G, 2A, 2C, 2D, 2F, 2G

Additionally, for the LCD used in this project, S7 corresponds to the degree 
symbol (°), so we will turn S7 on to enable this LCD to act as a temperature monitor. 
All other segments digit and special segments are turned off.

```
 ______     ______
|      |   |            
|      |   |         
|______|   |______      
       |          |      
       |          |     
 ______|    ______|   * S7
```

To enable the user to write any value __00 - 99__, a lookup table is created. 
Then, the user can access lookupTable dynamically to automatically update the 
LCD when a new digit needs to be written.

```c
uint8_t lookupTable[NUM_SEGMENTS][NUM_DIGITS] = {
//   0  1  2  3  4  5  6  7  8  9
    {1, 0, 1, 1, 0, 1, 1, 1, 1, 1}, // A  0
    {1, 1, 1, 1, 1, 0, 0, 1, 1, 1}, // B  1
    {1, 1, 0, 1, 1, 1, 1, 1, 1, 1}, // C  2
    {1, 0, 1, 1, 0, 1, 1, 0, 1, 1}, // D  3
    {1, 0, 1, 0, 0, 0, 1, 0, 1, 0}, // E  4
    {1, 0, 0, 0, 1, 1, 1, 0, 1, 1}, // F  5
    {0, 0, 1, 1, 1, 1, 1, 0, 1, 1}, // G  6
};
```

For example, to perform a GPIO write to segment __1A__, one would execute the 
following lines of code. 

```c
GPIO_write(CONFIG_GPIO_PWM, 0);
GPIO_write(CONFIG_GPIO_PWM_N, 1);

GPIO_write(CONFIG_GPIO_COM1, 0); // specifies COM1
GPIO_write(CONFIG_GPIO_COM2, 1);
GPIO_write(CONFIG_GPIO_COM3, 1);

/* COM1, SEG2, corresponds to 1A. Then you match A-G to the numbers 0-7, so you 
are writing to lookupTable[0]. Then, since this segment is the "tens" digit segment, 
you fill it with your user controlled "tens" value. */
GPIO_write(CONFIG_GPIO_SEG2, lookupTable[0][tensValue]);
```

Using this approach, any seven-segment LCD can be configured to display user 
input dynamically.

[figure 1]:CC2340R5_LCD_schematic.png "CC2340R5 LCD schematic"
[SimpleLink Low Power F3 SDK]: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/9.11.00.18
[Code Composer Studio]: https://www.ti.com/tool/download/CCSTUDIO/20.2.0