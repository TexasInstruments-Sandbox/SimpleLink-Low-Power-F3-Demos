/*
 *  ======== cli_app.c ========
*  This file implements a basic UART-based CLI driver.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include "cli_driver.h"

// Max Command Length
#define MAX_COMMAND_LEN 64

/* Driver configuration */
#include "ti_drivers_config.h"

// UART handle and parameters
UART2_Handle uart;
UART2_Params uartParams;

// Semaphore for UART read synchronization
static sem_t sem;

void callbackFxn(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if (status != UART2_STATUS_SUCCESS)
    {
        // If an error occurs during UART read, halt the program
        while (1) {}
    }
    // Signal the semaphore to indicate data is ready
    sem_post(&sem);
}

// Function to toggle the green LED
void greenLEDToggle()
{
    GPIO_toggle(CONFIG_GPIO_LED_GREEN);
}

// Function to toggle the red LED
void redLEDToggle()
{
    GPIO_toggle(CONFIG_GPIO_LED_RED);
}

// Function to toggle both LEDs
void bothLEDToggle()
{
    GPIO_toggle(CONFIG_GPIO_LED_RED);
    GPIO_toggle(CONFIG_GPIO_LED_GREEN);
}

// Function to blink the red LED with a specified duration and frequency
void redBlinkTimed(int argc, char **argv)
{
    int duration = atoi(argv[0]);
    int frequency = atoi(argv[1]);

    for (int i = 0; i < duration * frequency; i++)
    {
        GPIO_toggle(CONFIG_GPIO_LED_RED);
        usleep(1000000 / frequency);
    }

    GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_GPIO_LED_OFF); // Ensure LED is off after blinking
}

// Function to blink the green LED 4 times per second for a specified duration
void greenBlinkTimed(int argc, char **argv)
{
    int duration = atoi(argv[0]);
    int frequency = 4; // Fixed frequency of 4 times per second

    for (int i = 0; i < duration * frequency; i++)
    {
        GPIO_toggle(CONFIG_GPIO_LED_GREEN);
        usleep(1000000 / frequency);
    }

    GPIO_write(CONFIG_GPIO_LED_GREEN, CONFIG_GPIO_LED_OFF); // Ensure LED is off after blinking
}

// Function to blink both LEDs with specified frequency, duration, and mode
void bothBlinkTimed(int argc, char **argv)
{
    int frequency = atoi(argv[0]);
    int duration = atoi(argv[1]);
    int mode = atoi(argv[2]);

    // Set initial states based on the mode
    if (mode == 0) // Both start off (unison blinking)
    {
        GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_GPIO_LED_OFF);
        GPIO_write(CONFIG_GPIO_LED_GREEN, CONFIG_GPIO_LED_OFF);
    }
    else if (mode == 1) // Red starts on, Green starts off (alternating)
    {
        GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_GPIO_LED_ON);
        GPIO_write(CONFIG_GPIO_LED_GREEN, CONFIG_GPIO_LED_OFF);
    }
    else if (mode == 2) // Green starts on, Red starts off (alternating)
    {
        GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_GPIO_LED_OFF);
        GPIO_write(CONFIG_GPIO_LED_GREEN, CONFIG_GPIO_LED_ON);
    }

    for (int i = 0; i < duration * frequency; i++)
    {
        GPIO_toggle(CONFIG_GPIO_LED_RED);
        GPIO_toggle(CONFIG_GPIO_LED_GREEN);
        usleep(1000000 / frequency);
    }

    // Ensure both LEDs are off after blinking
    GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_GREEN, CONFIG_GPIO_LED_OFF);
}

// Main thread function
void *mainThread(void *arg0)
{
    // Initialize the CLI driver
    CLI_init();

    // Register commands with the CLI
    CLI_ADD_COMMAND(red, redLEDToggle, "\r\nRed Blink!\r\n", "Toggles the red LED", 0);
    CLI_ADD_COMMAND(green, greenLEDToggle, "\r\nGreen Blink!\r\n", "Toggles the green LED", 0);
    CLI_ADD_COMMAND(both, bothLEDToggle, "\r\nBoth Blink!\r\n", "Toggles both LEDs", 0);
    CLI_ADD_COMMAND(redBlinkTimed, redBlinkTimed, "\r\nRed Blink Timed Executed\r\n", "Blinks the red LED. Args: <duration> <frequency>", 2);
    CLI_ADD_COMMAND(greenBlinkTimed, greenBlinkTimed, "\r\nGreen Blink Timed Executed\r\n", "Blinks the green LED 4 times per second. Args: <duration>", 1);
    CLI_ADD_COMMAND(bothBlinkTimed, bothBlinkTimed, "\r\nBoth Blink Timed Executed\r\n", "Blinks both LEDs. Args: <frequency> <duration> <mode>", 3);

    char input; // Variable to store the current input character
    const char echoPrompt[] = "CLI Demo Initialized:\r\n";

    int32_t semStatus;
    uint32_t status = UART2_STATUS_SUCCESS;

    // Initialize GPIO for LED control
    GPIO_init();

    // Configure the LED pins
    GPIO_setConfig(CONFIG_GPIO_LED_GREEN, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_LED_RED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    // Initialize the semaphore
    semStatus = sem_init(&sem, 0, 0);
    if (semStatus != 0)
    {
        // If semaphore initialization fails, halt the program
        while (1) {}
    }

    // Initialize UART in callback read mode
    UART2_Params_init(&uartParams);
    uartParams.readMode = UART2_Mode_CALLBACK;
    uartParams.readCallback = callbackFxn;
    uartParams.baudRate = 115200;

    uart = UART2_open(CONFIG_UART2_0, &uartParams);
    if (uart == NULL)
    {
        // If UART initialization fails, halt the program
        while (1) {}
    }

    // Turn on LEDs to indicate successful initialization
    GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_GPIO_LED_ON);
    GPIO_write(CONFIG_GPIO_LED_GREEN, CONFIG_GPIO_LED_ON);

    // Send an initial prompt to the terminal
    UART2_write(uart, echoPrompt, sizeof(echoPrompt), NULL);

    // Rolling buffer for storing input characters
    uint8_t inputCLI[MAX_COMMAND_LEN] = {0};
    size_t inputIndex = 0;

    // Main loop to handle input and process commands
    while (1)
    {
        // Pass NULL for bytesRead since it's not used in this example
        status = UART2_read(uart, &input, 1, NULL);

        if (status != UART2_STATUS_SUCCESS)
        {
            // UART2_read() failed
            while (1) {}
        }

        // Pend until write occurs
        sem_wait(&sem);

        // Handle backspace or delete key
        if (input == '\b' || input == 127)
        {
            if (inputIndex > 0)
            {
                inputIndex--; // Remove the last character from the buffer
                UART2_write(uart, "\b \b", 3, NULL); // Erase character on terminal
            }
            continue;
        }

        // Handle line endings ("\r", "\n", "\r\n")
        if (input == '\r' || input == '\n')
        {
            // If the previous character was '\r' and the current is '\n', skip processing
            if (input == '\n' && inputIndex > 0 && inputCLI[inputIndex - 1] == '\r')
            {
                continue; // Ignore the '\n' in "\r\n"
            }

            UART2_write(uart, "\r\n", 2, NULL); // Move to a new line on terminal

            // Process the input buffer as a command
            CLI_processInput(inputCLI, inputIndex);

            // Clear the buffer for the next command
            memset(inputCLI, 0, MAX_COMMAND_LEN);
            inputIndex = 0;
            continue;
        }

        // Add input to the buffer if there's space
        if (inputIndex < MAX_COMMAND_LEN - 1)
        {
            inputCLI[inputIndex++] = input; // Store the input character in the buffer

            // Echo the input character back to the terminal
            UART2_write(uart, &input, 1, NULL);
        }
    }
}
