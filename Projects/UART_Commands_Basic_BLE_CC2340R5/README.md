# UART Commands BLE Example

## Introduction

This example demonstrates how to implement UART commands to control BLE functionality using the SimpleLink Low Power F3 SDK. 
The project enables seamless interaction between a BLE-enabled device and a UART interface, allowing users to send specific 
commands via UART to control BLE features. The `BLEAppUtil_invokeFunction()` is a key feature of this implementation, ensuring 
that all UART command function calls occur within the BLE task context for proper synchronization.

## Features

- **UART Interface**: Facilitates data exchange between the BLE-enabled device and the UART interface.
- **BLEAppUtil_invokeFunction()**: Ensures all UART command function calls are executed within the BLE task context, avoiding 
  conflicts between BLE and UART tasks.
- **Customizable Commands**: Users can add additional commands to extend functionality by modifying the 
  `uartFrameProtocol.c` file.

## Prerequisites

### Hardware Requirements

- 1x [LP-EM-CC2340R5 Launchpad](https://www.ti.com/tool/LP-EM-CC2340R5)
- 1x [LP-XDS110 Debugger](https://www.ti.com/tool/LP-XDS110ET)
- 1x USB Type-C to USB-A cable (included with LP-XDS110ET debugger)

### Software Requirements

- **SDK**: `simplelink_lowpower_f3_sdk_8_40_02_01`
- **IDE**: Code Composer Studio (CCS) 20.1.1
- **TI CLang**: `4.0.2`


## Overview

### BLEAppUtil

The `BLEAppUtil` is a utility library provided by the SimpleLink Low Power F3 SDK. It simplifies BLE application development 
by providing APIs for managing BLE events, tasks, and profiles. In this example, `BLEAppUtil_invokeFunction()` is used to ensure 
that UART command function calls are executed within the BLE task context, maintaining synchronization between BLE and UART 
operations. This synchronization is critical for avoiding conflicts between BLE operations and UART communication.

The `BLEAppUtil` library includes APIs for:
- Registering and handling BLE events such as connection, disconnection, and pairing.
- Managing BLE profiles and services.
- Simplifying task management for BLE applications.

For more details, refer to the [SimpleLink Low Power F3 SDK User's Guide](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK).

### UART Commands

The commands in this example are received via UART and processed to control BLE functionality. The default commands enabled 
in the example are defined in `uartFrameProtocol.c` and `uartFrameProtocol.h`. Below is a list of the default commands:

| Command         | Description                                                                 |
|-----------------|-----------------------------------------------------------------------------|
| `adv`           | Toggles BLE advertising on or off.                                         |
| `red`           | Toggles the red LED on the Launchpad.                                      |
| `green`         | Toggles the green LED on the Launchpad.                                    |

### Adding New Commands

To add new UART commands, follow these steps:

1. **Modify `uartFrameProtocol.c`**:
   - Define the new command and its corresponding logic in the `processUARTFrame()` function.
   - Use `BLEAppUtil_invokeFunction()` to ensure the command's logic is executed within the BLE task context.

2. **Update `uartFrameProtocol.h`**:
   - Add any necessary declarations for the new command.

3. **Test the New Command**:
   - Build and flash the updated firmware onto the LP-EM-CC2340R5 Launchpad.
   - Use a UART terminal to send the new command and verify its functionality.

> **Note**: Commands added can also include BLE APIs to perform BLE-specific operations such as starting or stopping advertising, 
connecting to devices, or modifying BLE characteristics.

For example, to add a command `blue` that toggles a blue LED:

```c
void processUARTFrame(char *buffer)
{
    size_t size = buffer[0];    // Extract size
    char *cmd = buffer + 1;     // Provide UART frame

    if (size == 4 && memcmp("blue", cmd, size) == 0)
    {
        GPIO_toggle(CONFIG_GPIO_LED_BLUE);
        UART2_write(uart, "Toggle blue LED!\n\r", 19, NULL);
    }

    BLEAppUtil_free(buffer);
}

