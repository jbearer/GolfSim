/**
 * \file clock.h
 * \brief Interacting with real time.
 */

#ifndef GOLF_TIME_H
#define GOLF_TIME_H

#include <stdint.h>

/**
 * \brief Get the current time in milliseconds since the epoch.
 */
uint64_t Clock_GetTimeMS(void);

/**
 * \brief Suspend execution for `ms` milliseconds.
 */
void Clock_SleepMS(uint32_t ms);

#endif
