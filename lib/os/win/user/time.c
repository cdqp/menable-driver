/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include <Windows.h>
#include <lib/os/types.h>
#include <lib/os/time.h>

uint64_t get_current_usecs() {
    static LARGE_INTEGER pc_freq = { 0 };
    if (pc_freq.QuadPart == 0) {
        if (!QueryPerformanceFrequency(&pc_freq)) {
            /* Well... yeah... what now?!? Change the API to return an error code? Abort execution? */
        }
    }

    LARGE_INTEGER pc;
    if (!QueryPerformanceCounter(&pc)) {
            /* see above... */
    }

    return pc.QuadPart * 1000000 / pc_freq.QuadPart;
}

uint64_t get_current_microsecs(void) {
    return get_current_usecs();
}

uint64_t get_current_msecs() {
    return get_current_usecs() / 1000;
}