/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "time.h"

void micros_wait(uint64_t micro_seconds) {
    uint64_t start = get_current_microsecs();
    while (get_current_microsecs() - start < micro_seconds);
}

void millis_wait(uint64_t milli_seconds) {
    uint64_t start = get_current_msecs();
    while (get_current_msecs() - start < milli_seconds);
}
