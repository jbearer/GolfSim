/**
 * \file errors.h
 *
 * Error handling without having to check return codes.
 */

#ifndef GOLF_ERRORS_H
#define GOLF_ERRORS_H

#include <stdlib.h>

typedef enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
} LogLevel;

typedef enum {
    WARNING,
    FATAL,
} ErrorLevel;

typedef enum {
    ERR_OUT_OF_MEMORY,
    ERR_IO,
    ERR_INVALID_SHADER,
    ERR_TIME,
} Error;

/**
 * \brief Handle an error.
 *
 * \param level The severity of the error. If `level` is `FATAL`, `Error_Raise`
 *              will terminate the program without returning.
 * \param error The error code.
 * \param arg   Error-dependent extra information to help in reporting and/or
 *              recovery. Usually a NULL-terminated string with a message.
 */
void Error_Raise(ErrorLevel level, Error error, void *arg);

/**
 * \brief Get notified when a fatal error occurs.
 *
 * \param f     Function to be called when a fatal error occurs. The first
 *              argument will be the code of the error. The second argument will
 *              be the final argument passed to `Error_Raise`. The third
 *              argument can be arbitrary data.
 * \param arg   Third argument to be passed to `f`.
 */
void Error_SetFatalErrorCallback(void(*f)(Error, void *, void *), void *arg);

/**
 * \brief Log a message.
 *
 * Accepts variadic arguments a la printf.
 */
void Error_SetMinimumLogLevel(LogLevel level);
void Error_Log(LogLevel level, const char *fmt, ...);
#ifndef NDEBUG
#define trace(fmt, ...) Error_Log(LOG_TRACE, fmt, __VA_ARGS__)
#define debug(fmt, ...) Error_Log(LOG_DEBUG, fmt, __VA_ARGS__)
#else
#define trace(fmt, ...)
#define debug(fmt, ...)
#endif
#define info(fmt, ...) Error_Log(LOG_INFO, fmt, __VA_ARGS__)
#define warn(fmt, ...) Error_Log(LOG_WARN, fmt, __VA_ARGS__)
#define error(fmt, ...) Error_Log(LOG_ERROR, fmt, __VA_ARGS__)

#ifdef NDEBUG
#define ASSERT(x) do { (void)sizeof(x); } while (0)
    // Silence unused variable warnings for variables which are only used in an
    // assert, but don't evaluate the expression `x` at runtime.
#else
#include <assert.h>
#define ASSERT(x) assert(x)
#endif

/**
 * \brief Allocate memory, terminating the program on failure.
 *
 * \details
 *      This is a thin wrapper around system malloc, which calls `Error_Raise`
 *      with a `FATAL` error if the requested amount of memory is not available.
 */
static inline void *Malloc(size_t size)
{
    void *ret = malloc(size);
    if (ret == NULL) {
        Error_Raise(FATAL, ERR_OUT_OF_MEMORY, NULL);
    }

    return ret;
}

#endif
