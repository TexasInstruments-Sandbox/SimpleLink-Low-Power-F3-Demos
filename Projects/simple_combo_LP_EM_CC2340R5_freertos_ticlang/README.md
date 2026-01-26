# Zigbee Simple Combo for OnOff Light + GP Sink

This project combines both Zigbee OnOff Light and Green Power (GP) Sink 
applications which can be controlled by either an OnOff Switch or Green Power 
Device (GPD) Switch. As the OnOff Light application is already covered in the 
documentation of that project, this Readme will specifically cover the 
Green Power Sink (GPS) addition.

## Zigbee Simple Combo Features

This project implements the following functionality:

* Zigbee Coordinator OnOff Light node
* LED controlled through default OnOff Switch ZR/ZED nodes
* Addition of GPD Switch device control of local LED

## Dependencies

This project depends on the following software components:
* [SimpleLink Low Power F3 SDK] v9.14.00.35
* [Code Composer Studio] v20.2.0
* [TI CLANG Compiler] v4.0.3
* [SysConfig] v1.23.2

## Hardware Requirements

This project depends on the use of the following hardware:

* 3x [LP-EM-CC2340R5] - CC2340R5 LaunchPad™ development kit for SimpleLink™ Low 
Energy MCU (also available is the [LP-EM-CC2755P10])
* 3x [LP-XDS110ET] - XDS110ET LaunchPad™ development kit debugger with 
EnergyTrace™ software

## Configuration Parameters

The following code is added to the `simple_combo.c` which is based from `on_off_light.c`:
```c
#define ZB_ENABLE_ZGP_SINK
#define ZGP_SINK_USE_GP_MATCH_TABLE
//...
zb_uint8_t g_ind_key[ZB_CCM_KEY_SIZE] = {0xCF,0xCE,0xCD,0xCC,0xCB,0xCA,0xC9,0xC8,0xC7,0xC6,0xC5,0xC4,0xC3,0xC2,0xC1,0xC0};
//...
```
The definitions are necessary to enable ZGP Sink functionality.  The `g_ind_key` 
is agreed upon **Data Frame Security Key** between the GP Sink and GPD Switch 
devices.  In the `gpd_switch.syscfg` file, this setting can be found under
**Zigbee** &rarr; **Security**.

The GP Sink application also supports unidirectional GP commissioning, 
therefore the default `gpd_switch` project needs to select 
**Unidirectional Commissioning** inside `gpd_switch.syscfg` &rarr;
**Zigbee** &rarr; **Advanced**.

## Example Usage

1. Import, build, and flash the project to one CC2340R5 LaunchPad
2. Import, build, and flash the `onoff_switch` project from the SDK (can be 
either CC2340R5 or CC2755P10 LaunchPad variants) to the second LaunchPad
3. Import the `gpd_switch` project from the SDK (can be either CC2340R5 or 
CC2755P10 LaunchPad variants)
4. Select **Unidirectional Commissioning** inside `gpd_switch.syscfg` &rarr;
**Zigbee** &rarr; **Advanced**
5. Build and flash the second LaunchPad
6. Power on the ZC Simple Combo LaunchPad, followed by the OnOff Switch and
GPD Switch LaunchPads
7. Use *BTN-1* on the OnOff Switch LaunchPad to control the *Green LED* on the 
Simple Combo
8. Push *BTN-1* on the Simple Combo LaunchPad to enable Green Power 
Commissioning
9. Push *BTN-1* on the GPD Switch LaunchPad to broadcast the Green Power
Commissioning Data Frame which is received by the Simple Combo
10. Push *BTN-1* on the Simple Combo LaunchPad to disable Green Power 
Commissioning
11. Push *BTN-2* on the GPD Switch LaunchPad to broadcast On/Off/Toggle
Green Power Data Frames (GPDF) which affect the *Green LED* of the Simple Combo

## Conclusion

This project demonstrates how to implement a Zigbee Simple Combo application
using the CC2340R5. The application features control of the local device's 
green LED by both separate OnOff Switch and GPD Switch devices.  This is 
a starting point from which users can further expand their Zigbee ZCL and
GPS application.

<!-- Definition of inline references -->
[LP-EM-CC2340R5]: https://www.ti.com/tool/LP-EM-CC2340R5
[LP-EM-CC2755P10]: https://www.ti.com/tool/LP-EM-CC2755P10
[LP-XDS110ET]: https://www.ti.com/tool/LP-XDS110ET

[SimpleLink Low Power F3 SDK]: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/9.14.00.41
[Code Composer Studio]: https://www.ti.com/tool/download/CCSTUDIO/20.2.0
[TI CLANG Compiler]: https://www.ti.com/tool/download/ARM-CGT-CLANG/4.0.3.LTS
[SysConfig]: https://www.ti.com/tool/download/SYSCONFIG/1.23.2.4156