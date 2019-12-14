#include "os.h"

#include <errno.h>
#include <stdbool.h>
#include <time.h>

#include "clock.h"
#include "errors.h"

#ifdef GOLF_OS_POSIX

uint64_t Clock_GetTimeMS(void)
{
    struct timespec ts = {0};
    clock();
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        switch (errno) {
            case EFAULT:
                ASSERT(false);
                    // Out of bounds access is a programming error, but the
                    // error-handling mechanism is for unexpected errors due to
                    // user input or system misbehavior, so in this case we'll
                    // just crash.
                break;
            case EINVAL:
                Error_Raise(WARNING, ERR_TIME, "no monotonic clock available");
                break;
            default:
                ASSERT(false);
                    // clock_gettime returned an undocumented error.
                break;
        }
    }

    return ts.tv_sec*1000 + ts.tv_nsec/1000000;
}

void Clock_SleepMS(uint32_t ms)
{
    struct timespec ts = { .tv_sec = 0, .tv_nsec = ms*1000000 };
    nanosleep(&ts, NULL);
}

#else
# error "unsupported operating system"
#endif
