/*
 *  ======== cli_driver.c ========
 *  This file implements a basic CLI driver that allows dynamic registration
 *  of commands and processes input to execute associated functions.
 */

#include "cli_driver.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ti/drivers/UART2.h>

// Maximum number of commands that can be registered
#define MAX_COMMANDS 10
#define MAX_ARGS 5 // Maximum number of arguments a command can take



// Array to store registered commands
static CLI_Command *commandList[MAX_COMMANDS];

// Number of registered commands
static size_t commandCount = 0;

// UART handle for sending messages
extern UART2_Handle uart;

// Function to clear the UART terminal
static void CLI_clearScreen(void)
{
    // ANSI escape sequence to clear the screen and move the cursor to the top-left corner
    const char clearSequence[] = "\033[2J\033[H";
    UART2_write(uart, clearSequence, sizeof(clearSequence) - 1, NULL);
}

// Wrapper for CLI_clearScreen to match the expected function signature
static void CLI_clearScreenWrapper(int argc, char **argv)
{
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    CLI_clearScreen();
}

// Function to print help information
static void CLI_printHelp(void)
{
    UART2_write(uart, "\r\nAvailable Commands:\r\n", 24, NULL);

    // Iterate through all registered commands and print their details
    for (size_t i = 0; i < commandCount && i < MAX_COMMANDS; i++) // Ensure we don't exceed MAX_COMMANDS
    {
        CLI_Command *cmd = commandList[i];

        // Check if the command is valid
        if (cmd == NULL || cmd->keyword == NULL || cmd->helpMessage == NULL)
        {
            continue; // Skip invalid commands
        }

        // Print the command's keyword and help message directly
        UART2_write(uart, cmd->keyword, strlen(cmd->keyword), NULL);
        UART2_write(uart, ": ", 2, NULL);
        UART2_write(uart, cmd->helpMessage, strlen(cmd->helpMessage), NULL);
        UART2_write(uart, "\r\n", 2, NULL);
    }
}

// Wrapper for CLI_printHelp to match the expected function signature
static void CLI_printHelpWrapper(int argc, char **argv)
{
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    CLI_printHelp();
}

// Initialize the CLI driver
void CLI_init(void)
{
    commandCount = 0; // Reset the command count
    memset(commandList, 0, sizeof(commandList)); // Clear the command list

    // Add the built-in "help" command using the wrapper
    CLI_addCommand("help", CLI_printHelpWrapper, "\r\nHelp Command Executed\r\n", "Displays all available commands and their descriptions", 0);

    // Add the built-in "clear" command using the wrapper
    CLI_addCommand("clear", CLI_clearScreenWrapper, "\r\nScreen Cleared\r\n", "Clears the UART terminal screen", 0);
}

// Add a command to the CLI
void CLI_addCommand(const char *keyword, void (*function)(int argc, char **argv), const char *resultMessage, const char *helpMessage, int argCount)
{
#ifdef DEBUG
    char debugBuffer[128];

    // Debug: Log the command being added
    snprintf(debugBuffer, sizeof(debugBuffer), "Adding command: %s\r\n", keyword);
    UART2_write(uart, debugBuffer, strlen(debugBuffer), NULL);

    // Debug: Log the current command count
    snprintf(debugBuffer, sizeof(debugBuffer), "Current commandCount: %zu\r\n", commandCount);
    UART2_write(uart, debugBuffer, strlen(debugBuffer), NULL);
#endif

    // Validate inputs
    if (commandCount >= MAX_COMMANDS || keyword == NULL || function == NULL || resultMessage == NULL || helpMessage == NULL)
    {
        UART2_write(uart, "Invalid command parameters or max commands reached\r\n", 52, NULL);
        return;
    }

    // Allocate memory for the new command
    CLI_Command *newCommand = (CLI_Command *)malloc(sizeof(CLI_Command));
    if (newCommand == NULL)
    {
        UART2_write(uart, "Memory allocation failed for new command\r\n", 42, NULL);
        return;
    }

    // Directly assign the pointers instead of duplicating the strings
    newCommand->keyword = keyword;
    newCommand->resultMessage = resultMessage;
    newCommand->helpMessage = helpMessage;
    newCommand->function = function;
    newCommand->argCount = argCount;

    // Add the command to the list and increment the command count
    if (commandCount < MAX_COMMANDS) // Ensure we don't exceed MAX_COMMANDS
    {
        commandList[commandCount++] = newCommand;

#ifdef DEBUG
        // Debug: Log successful addition
        snprintf(debugBuffer, sizeof(debugBuffer), "Command added successfully. New commandCount: %zu\r\n", commandCount);
        UART2_write(uart, debugBuffer, strlen(debugBuffer), NULL);
#endif
    }
    else
    {
        UART2_write(uart, "Max commands reached. Cannot add command\r\n", 42, NULL);

        // Free memory if the command cannot be added
        free(newCommand);
    }
}

// Process input and execute matching command
void CLI_processInput(uint8_t *inputBuffer, size_t bufferSize)
{

#ifdef DEBUG
    char debugBuffer[128];

    // Debug: Log the received input
    snprintf(debugBuffer, sizeof(debugBuffer), "Processing input: %.*s\r\n", (int)bufferSize, inputBuffer);
    UART2_write(uart, debugBuffer, strlen(debugBuffer), NULL);
#endif

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

#ifdef DEBUG
    // Debug: Log the parsed arguments
    snprintf(debugBuffer, sizeof(debugBuffer), "Parsed arguments (argc=%d):\r\n", argc);
    UART2_write(uart, debugBuffer, strlen(debugBuffer), NULL);
    for (int i = 0; i < argc; i++)
    {
        snprintf(debugBuffer, sizeof(debugBuffer), "  argv[%d]: %s\r\n", i, argv[i]);
        UART2_write(uart, debugBuffer, strlen(debugBuffer), NULL);
    }
#endif

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

#ifdef DEBUG
        // Debug: Log the command being checked
        snprintf(debugBuffer, sizeof(debugBuffer), "Checking command: %s\r\n", cmd ? cmd->keyword : "NULL");
        UART2_write(uart, debugBuffer, strlen(debugBuffer), NULL);
#endif

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
