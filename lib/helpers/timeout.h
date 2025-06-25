/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef TIMEOUT_H_
#define TIMEOUT_H_

#include "../os/types.h"
#include "../os/time.h"

enum {
    TIMEOUT_INIFINITE = UINT32_MAX
};

typedef struct timeout {
    uint64_t target_time;
} timeout;

void timeout_init(struct timeout * timeout, uint32_t msecs);

bool timeout_has_elapsed(struct timeout * timeout);

#endif /* TIMEOUT_H_ */
