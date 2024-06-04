/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#include <lib/os/time.h>

#include <stdint.h>

uint64_t get_current_microsecs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return NANOS_2_MICROS(ts.tv_nsec) + SECS_2_MICROS(ts.tv_sec);
}
uint64_t get_current_msecs() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return NANOS_2_MILLIS(ts.tv_nsec) + SECS_2_MILLIS(ts.tv_sec);
}

