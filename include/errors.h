/**
 * \file errors.h
 *
 * Error handling without having to check return codes.
 */

#ifndef GOLF_ERRORS_H
#define GOLF_ERRORS_H

typedef enum {
    FATAL
} ErrorLevel;

typedef enum {
    ERR_OUT_OF_MEMORY,
    ERR_IO,
    ERR_INVALID_SHADER,
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
