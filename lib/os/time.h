/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_OS_TIME_H_
#define LIB_OS_TIME_H_

#include "types.h"

#if defined(__linux__)

#include "linux/delay.h"

#elif defined(_WIN32) || defined(_WIN64)

#include "win/delay.h"

#endif

#define NANOS_2_MICROS(nanos) ((nanos) / 1000)
#define NANOS_2_MILLIS(nanos) ((nanos) / 1000000)

#define SECS_2_MICROS(secs) ((secs) * 1000000)
#define SECS_2_MILLIS(secs) ((secs) * 1000)

/**
 * Get the current milliseconds relative to an unknown, but fixed point in the past.
 * It is guaranteed that while the system is running, the same point is used.
 *
 * @return the current milliseconds
 */
uint64_t get_current_msecs(void);

/**
* Get the current microseconds relative to an unknown, but fixed point in the past.
* It is guaranteed that while the system is running, the same point is used.
*
* @return the current milliseconds
*/
uint64_t get_current_microsecs(void);

/**
 * Wait actively for a certain time.
 *
 *@param the amount of micro seconds to wait.
 */
void micros_wait(uint64_t micro_seconds);

/**
 * Wait actively for a certain time.
 *
 *@param the amount of micro seconds to wait.
 */
void millis_wait(uint64_t milli_seconds);

#endif /* LIB_OS_TIME_H_ */
