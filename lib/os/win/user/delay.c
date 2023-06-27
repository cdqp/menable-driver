/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "time_internal.h"
#include <lib/os/assert.h>
#include <lib/os/types.h>

void udelay(unsigned long usecs) {
    uint64_t goal = get_current_usecs() + usecs;
    while (goal > get_current_usecs());
}