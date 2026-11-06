#ifndef CLI_DRIVER_H
#define CLI_DRIVER_H

#include <stdint.h>
#include <stddef.h>

// Command structure
typedef struct
{
    const char *keyword;
    void (*function)(int argc, char **argv); // Updated to support arguments
    const char *resultMessage;
    const char *helpMessage; // Help message for the command
    int argCount;            // Number of arguments the command expects
} CLI_Command;

// CLI Driver API
void CLI_init(void);
void CLI_addCommand(const char *keyword, void (*function)(int argc, char **argv), const char *resultMessage, const char *helpMessage, int argCount);
void CLI_processInput(uint8_t *inputBuffer, size_t bufferSize);

// Macro for adding commands
#define CLI_ADD_COMMAND(keyword, function, resultMessage, helpMessage, argCount) \
    CLI_addCommand(#keyword, function, resultMessage, helpMessage, argCount)

#endif // CLI_DRIVER_H
