#include <stdio.h>
#include <stdlib.h>

#include "errors.h"

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
        exit(1);
    }
}
