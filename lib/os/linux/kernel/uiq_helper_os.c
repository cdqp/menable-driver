/************************************************************************
 * Copyright 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include <lib/uiq/uiq_helper.h>
#include <lib/helpers/bits.h>
#include <lib/helpers/helper.h>

#include <linux/ktime.h>
#include <linux/version.h>

/* With Kernel 3.17 the 64 bit time API was introduced */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
    typedef time_t men_time;
    typedef struct timespec men_timespec;

    static inline void men_get_time(men_timespec * ts) {
        ktime_get_ts(ts);
    }
#else
    typedef time64_t men_time;
    typedef struct timespec64 men_timespec;

    static inline void men_get_time(men_timespec * ts) {
        ktime_get_ts64(ts);
    }
#endif

static men_time uiq_timestamp_offset = 0;

void set_uiq_timestamp_offset(int64_t seconds) {
    uiq_timestamp_offset = seconds;
}

static void get_time(men_timespec* time) {
    men_get_time(time);
    time->tv_sec -= uiq_timestamp_offset;
}

void men_get_uiq_timestamp(uiq_timestamp* out_timestamp) {

    men_timespec time;
    get_time(&time);

    out_timestamp->data[0] = GET_BITS_32(time.tv_sec, 0, 15);
    out_timestamp->data[1] = GET_BITS_32(time.tv_sec, 16, 31);
    out_timestamp->data[2] = GET_BITS_32(time.tv_nsec, 0, 15);
    out_timestamp->data[3] = GET_BITS_32(time.tv_nsec, 16, 31);
}