# Window Controller for SimpleLink CC2340R5

This project implements a window controller with Zigbee communication for the 
CC2340R5. The application features button-controlled commands and is designed 
to be connected to mains power as this device is in an "always-on" state.

## Features

* Zigbee Controller which forms the network
* Button-controlled commands
* Zigbee communication for remote control
* Support for window covering devices

## Pin Configuration

| Function | CC2340R5 Pin | Configuration | Description |
|----------|--------------|---------------|-------------|
| **Button 1** |||
| Button 1 | DIO10 | GPIO Input | User button 1 |
| **LED** |||
| LED | DIO15 | GPIO Output | LED indicator |
| **UART** |||
| UART TX | DIO20 | UART0 TX | Serial communication transmit |

## Dependencies

This project depends on the following software components:
* [SimpleLink Low Power F3 SDK] v9.11.00.18
* [Code Composer Studio] v20.2.0
* [TI CLANG Compiler] v4.0.3
* [SysConfig] v1.23.2

## Hardware Requirements

* [LP-EM-CC2340R5](https://www.ti.com/tool/LP-EM-CC2340R5) - CC2340R5 LaunchPad™ 
development kit
* [LP-XDS110ET](https://www.ti.com/tool/LP-XDS110ET) - XDS110ET LaunchPad™ 
development kit debugger

## Example Usage

1. Build and flash the project to your CC2340R5 LaunchPad
2. Follow the steps in the window_covering project to build and flash the device, 
as well as set up the hardware correctly
2. Power on the window_controller board to see initiate it as a Zigbee Coordinator 
(ZC) (or Zigbee Router if using external Zigbee gateway hub as a ZC)
3. Press the left button to toggle a window covering up/down command to the window 
covering device

## Button Press Handler

The button press handler is used to send commands to the window covering device. 
To set up the button press handler:
1. Configure the button press handler in the source code:

```c
void send_up_open_req(zb_uint8_t param)
{
  zb_uint16_t addr = 0;

  ZB_ASSERT(param);

  if (ZB_JOINED()  && !cmd_in_progress)
  {
    cmd_in_progress = ZB_TRUE;
    Log_printf(LogModule_Zigbee_App, Log_INFO, "send_up_open_req %d - send up/open", param);

    ZB_ZCL_WINDOW_COVERING_SEND_UP_OPEN_REQ(
    param,
    addr,
    ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
    0,
    ZB_WINDOW_CONTROLLER_ENDPOINT,
    ZB_AF_HA_PROFILE_ID,
    ZB_FALSE, NULL);
  }
  else
  {
    Log_printf(LogModule_Zigbee_App, Log_INFO, "send_up_open_req %d - not joined", param);
    zb_buf_free(param);
  }
}

void send_down_close_req(zb_uint8_t param)
{
  zb_uint16_t addr = 0;

  ZB_ASSERT(param);

  if (ZB_JOINED()  && !cmd_in_progress)
  {
    cmd_in_progress = ZB_TRUE;
    Log_printf(LogModule_Zigbee_App, Log_INFO, "send_down_close_req %d - send down/close", param);

    ZB_ZCL_WINDOW_COVERING_SEND_DOWN_CLOSE_REQ(
    param,
    addr,
    ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
    0,
    ZB_WINDOW_CONTROLLER_ENDPOINT,
    ZB_AF_HA_PROFILE_ID,
    ZB_FALSE, NULL);
  }
  else
  {
    Log_printf(LogModule_Zigbee_App, Log_INFO, "send_down_close_req %d - not joined", param);
    zb_buf_free(param);
  }
}

void button_press_handler(zb_uint8_t param)
{
  if (!param)
  {
    /* Button is pressed, gets buffer for outgoing command */
    zb_buf_get_out_delayed(button_press_handler);
  }
  else
  {
    Log_printf(LogModule_Zigbee_App, Log_INFO, "button_press_handler %d", param);
    if (toggle) send_up_open_req(param);
    else send_down_close_req(param);
#ifndef ZB_USE_BUTTONS
    /* Do not have buttons in simulator - just start periodic on/off sending */
    ZB_SCHEDULE_APP_ALARM(button_press_handler, 0, 7 * ZB_TIME_ONE_SECOND);
#endif
  }
}
```

## Example Code

The example code demonstrates how to use the button press handler to send 
commands to the window covering device.

## Conclusion

This project demonstrates how to implement a window controller with Zigbee 
communication for the CC2340R5. The application features button-controlled 
commands. The button press handler is used to send commands to the window 
covering device, and the Zigbee communication is used to send commands to the 
window covering device.

<!-- Definition of inline references -->

[SimpleLink Low Power F3 SDK]: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/9.11.00.18
[Code Composer Studio]: https://www.ti.com/tool/download/CCSTUDIO/20.2.0
[TI CLANG Compiler]: https://www.ti.com/tool/download/ARM-CGT-CLANG/4.0.3.LTS
[SysConfig]: https://www.ti.com/tool/download/SYSCONFIG/1.23.2.4156