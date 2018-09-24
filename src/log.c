#include <stdio.h>
#include <stdarg.h>
#include "log.h"

void __debug_print(char* module_name, const char* message)
{
    #ifdef DEBUG
        printf("DEBUG: %s: %s\n", module_name, message);
    #endif
}

void __error_print(char* module_name, const char* message)
{
    printf("ERROR: %s: %s\n", module_name, message);
}

void log_debug(char* module_name, const char* message, ...)
{
    char buffer[MAX_MESSAGE_LENGTH];

    va_list args;
    va_start(args, message);

    vsnprintf(buffer, sizeof(buffer), message, args);

    va_end(args);

    __debug_print(module_name, buffer);
}

void log_error(char* module_name, const char* message, ...)
{
    char buffer[MAX_MESSAGE_LENGTH];

    va_list args;
    va_start(args, message);

    vsnprintf(buffer, sizeof(buffer), message, args);

    va_end(args);

    __error_print(module_name, buffer);
}
