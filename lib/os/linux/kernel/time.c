/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/



#ifndef LIB_OS_LINUX_KERNEL_TIME_C_
#define LIB_OS_LINUX_KERNEL_TIME_H_

#include "../../time.h"

#include <linux/ktime.h>
#include "types.h"

uint64_t get_current_microsecs(void) {
    return ktime_get_ns() / 1000;
}

uint64_t get_current_msecs(void) {
    return ktime_get_ns() / 1000000;
}

#endif /* LIB_OS_LINUX_KERNEL_TIME_C_ */
