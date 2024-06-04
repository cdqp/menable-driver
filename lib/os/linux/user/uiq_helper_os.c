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

#include <time.h>

void men_get_uiq_timestamp(uiq_timestamp* out_timestamp) {

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    out_timestamp->data[0] = GET_BITS_32(ts.tv_sec, 16, 31);
    out_timestamp->data[1] = GET_BITS_32(ts.tv_sec, 0, 15);
    out_timestamp->data[2] = GET_BITS_32(ts.tv_nsec, 16, 31);
    out_timestamp->data[3] = GET_BITS_32(ts.tv_nsec, 0, 15);
}