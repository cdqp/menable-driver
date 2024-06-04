/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include <wdm.h>
#include "types.h"

uint64_t get_current_microsecs(void) {
    LARGE_INTEGER current_time;
    KeQuerySystemTime(&current_time);
    return current_time.QuadPart / 10;
}

uint64_t get_current_msecs(void) {
    LARGE_INTEGER current_time;
    KeQuerySystemTime(&current_time);
    return current_time.QuadPart / 10000;
}