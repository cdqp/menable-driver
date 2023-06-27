/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include <lib/os/types.h>

#include "timeout.h"

void timeout_init(struct timeout * timeout, uint32_t msecs) {
    timeout->target_time = (msecs != TIMEOUT_INIFINITE)
                             ? get_current_msecs() + msecs
                             : UINT64_MAX;
}

bool timeout_has_elapsed(struct timeout * timeout) {
    return (timeout->target_time != UINT64_MAX
            && get_current_msecs() >= timeout->target_time);
}

