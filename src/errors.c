#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"

static void(*fatal_error_callback)(Error, void *, void *);
static void *fatal_error_callback_arg;
static LogLevel minimum_log_level = LOG_TRACE;

void Error_Raise(ErrorLevel level, Error error, void *arg)
{
    switch (error) {
        case ERR_OUT_OF_MEMORY:
            fprintf(stderr, "out of memory\n");
            break;
        case ERR_IO:
            fprintf(stderr, "IO error: %s\n", (char *)arg);
            break;
        case ERR_INVALID_SHADER:
            fprintf(stderr, "Invalid shader: %s\n", (char *)arg);
            break;
        default:
            fprintf(stderr, "Unknown error %d\n", (int)error);
            break;
    }

    if (level == FATAL) {
        fatal_error_callback(error, arg, fatal_error_callback_arg);
        exit(1);
    }
}

void Error_SetFatalErrorCallback(void(*f)(Error, void *, void *), void *arg)
{
    fatal_error_callback = f;
    fatal_error_callback_arg = arg;
}

void Error_SetMinimumLogLevel(LogLevel level)
{
    minimum_log_level = level;
}

void Error_Log(LogLevel level, const char *fmt, ...)
{
    if (level < minimum_log_level) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
