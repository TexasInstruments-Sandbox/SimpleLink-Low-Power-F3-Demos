
# CLI Driver for UART-Based Command Line Interface

## Introduction

This project implements a UART-based Command Line Interface (CLI) driver for the SimpleLink Low Power F3 SDK. The CLI allows users to interact with the system through a serial terminal, execute commands, and control hardware components such as LEDs. It supports dynamic registration of commands, argument parsing, and built-in commands like `help` and `clear`.

---

## Features

- **Dynamic Command Registration**: Add or remove commands at runtime.
- **Argument Parsing**: Commands can accept arguments.
- **Built-in Commands**:
  - `help`: Displays all available commands and their descriptions.
  - `clear`: Clears the UART terminal screen (compatible with PuTTY and other ANSI-capable terminals).
- **Line Ending Support**: Handles `\r`, `\n`, and `\r\n` line endings for command detection.
- **LED Control**: Example commands to toggle and blink LEDs.

---

## Prerequisites

### Hardware Requirements

- 1x [LP-EM-CC2340R5 Launchpad](https://www.ti.com/tool/LP-EM-CC2340R5)
- 1x [LP-XDS110 Debugger](https://www.ti.com/tool/LP-XDS110ET)
- 1x USB Type-C to USB-A cable (included with LP-XDS110ET debugger)

### Software Requirements

- **SDK**: `simplelink_lowpower_f3_sdk_8_40_02_01`
- **IDE**: Code Composer Studio (CCS) `20.1.1`
- **TI CLang**: `4.0.2`

---

## How to Use

### 1. **Setup**
1. Connect your embedded system to a serial terminal (e.g., PuTTY, Tera Term).
2. Configure the UART settings:
   - Baud Rate: `115200`
   - Data Bits: `8`
   - Parity: `None`
   - Stop Bits: `1`
3. Build and flash the firmware onto your device.

---

### 2. **Built-in Commands**

#### `help`
Displays all available commands and their descriptions.

**Usage**:
```plaintext
> help
Available Commands:
help: Displays all available commands and their descriptions
clear: Clears the UART terminal screen
red: Toggles the red LED
green: Toggles the green LED
both: Toggles both LEDs
redBlinkTimed: Blinks the red LED. Args: <duration> <frequency>
greenBlinkTimed: Blinks the green LED 4 times per second. Args: <duration>
bothBlinkTimed: Blinks both LEDs. Args: <frequency> <duration> <mode>
```

#### `clear`
Clears the UART terminal screen.

**Usage**:
```plaintext
> clear
```

---

### 3. **Example Commands**

#### Toggle LEDs
- **Red LED**: `red`
- **Green LED**: `green`
- **Both LEDs**: `both`

**Usage**:
```plaintext
> red
Red Blink!
> green
Green Blink!
> both
Both Blink!
```

#### Blink LEDs with Timing
- **Red LED**: `redBlinkTimed <duration> <frequency>`
- **Green LED**: `greenBlinkTimed <duration>`
- **Both LEDs**:  `bothBlinkTimed <frequency> <duration> <mode>`

**Arguments**:
- `<duration>`: Time in seconds.
- `<frequency>`: Blinks per second.
- `<mode>`:
  - `0`: Both LEDs blink in unison.
  - `1`: Red starts ON, Green starts OFF (alternating).
  - `2`: Green starts ON, Red starts OFF (alternating).

**Usage**:
```plaintext
> redBlinkTimed 5 2
Red Blink Timed Executed

> greenBlinkTimed 3
Green Blink Timed Executed

> bothBlinkTimed 4 5 1
Both Blink Timed Executed
```

---

## Adding and Removing Commands

### Adding a Command
To add a new command, use the `CLI_ADD_COMMAND` macro in your code. The macro registers the command with the CLI.

**Syntax**:
```c
CLI_ADD_COMMAND(keyword, function, resultMessage, helpMessage, argCount);
```

- `keyword`: The command name (string).
- `function`: The function to execute when the command is called.
- `resultMessage`: The message displayed after the command is executed.
- `helpMessage`: The description of the command (displayed in `help`).
- `argCount`: The number of arguments the command expects.

**Example**:
```c
void myCommand(int argc, char **argv)
{
    // Your command logic here
    UART2_write(uart, "My Command Executed\r\n", 22, NULL);
}

// Register the command
CLI_ADD_COMMAND(myCommand, myCommand, "\r\nMy Command Executed\r\n", "Executes my custom command", 0);
```

### Removing a Command
Currently, the CLI does not support dynamic removal of commands. To remove a command, you must modify the source code and rebuild the firmware.

---

## Relevant Code Sections

### Command Registration
Commands are registered in the `mainThread` function using the `CLI_ADD_COMMAND` macro:
```c
CLI_ADD_COMMAND(red, redLEDToggle, "\r\nRed Blink!\r\n", "Toggles the red LED", 0);
CLI_ADD_COMMAND(green, greenLEDToggle, "\r\nGreen Blink!\r\n", "Toggles the green LED", 0);
CLI_ADD_COMMAND(both, bothLEDToggle, "\r\nBoth Blink!\r\n", "Toggles both LEDs", 0);
CLI_ADD_COMMAND(redBlinkTimed, redBlinkTimed, "\r\nRed Blink Timed Executed\r\n", "Blinks the red LED. Args: <duration> <frequency>", 2);
CLI_ADD_COMMAND(greenBlinkTimed, greenBlinkTimed, "\r\nGreen Blink Timed Executed\r\n", "Blinks the green LED 4 times per second. Args: <duration>", 1);
CLI_ADD_COMMAND(bothBlinkTimed, bothBlinkTimed, "\r\nBoth Blink Timed Executed\r\n", "Blinks both LEDs. Args: <frequency> <duration> <mode>", 3);
```

### Command Execution
Commands are executed in the `CLI_processInput` function:
```c
// Process input and execute matching command
void CLI_processInput(uint8_t *inputBuffer, size_t bufferSize)
{
    // Cast the input buffer to a string and null-terminate it
    char *input = (char *)inputBuffer;
    input[bufferSize] = '\0'; // Ensure the input is null-terminated

    // Array to store command arguments
    char *argv[MAX_ARGS + 1]; // +1 to include the command keyword itself
    int argc = 0;             // Argument count

    // Tokenize the input string into command and arguments
    char *token = strtok(input, " "); // Split the input by spaces
    while (token != NULL && argc < MAX_ARGS + 1)
    {
        argv[argc++] = token;        // Store each token in the argv array
        token = strtok(NULL, " ");  // Get the next token
    }

    // If no command is entered, return early
    if (argc == 0)
    {
        UART2_write(uart, "No command entered\r\n", 21, NULL);
        return;
    }

    // Iterate through the list of registered commands to find a match
    for (size_t i = 0; i < commandCount && i < MAX_COMMANDS; i++) // Ensure we don't exceed MAX_COMMANDS
    {
        CLI_Command *cmd = commandList[i]; // Get the current command

        // Check if the entered command matches the registered command keyword
        if (cmd != NULL && strcmp(argv[0], cmd->keyword) == 0)
        {
            // Validate the number of arguments provided
            if (argc - 1 != cmd->argCount)
            {
                UART2_write(uart, "Invalid number of arguments\r\n", 30, NULL);
                return;
            }

            // Execute the command's associated function with the arguments
            cmd->function(argc - 1, &argv[1]); // Pass arguments (excluding the command keyword)

            // Send the result message to indicate successful execution
            UART2_write(uart, cmd->resultMessage, strlen(cmd->resultMessage), NULL);
            return; // Exit after executing the command
        }
    }

    // If no matching command is found, send an error message
    UART2_write(uart, "Invalid Command\r\n", 18, NULL);
}

```

---

## Project Structure

```
CLI_driver_LP_EM_CC2340R5_freertos_ticlang/
├── cli_driver.h      # Header file for the CLI driver
├── cli_driver.c      # Implementation of the CLI driver
├── cli_app.c         # Application code for the CLI
├── ti_drivers_config.h # Driver configuration file
```

---

## Notes

- Ensure that the UART terminal supports ANSI escape sequences for the `clear` command to work.
- The maximum number of commands is defined by `MAX_COMMANDS` in cli_driver.c. Increase this value if needed.
- The maximum command length is defined by `MAX_COMMAND_LEN` in cli_app.c.

