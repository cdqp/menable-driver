/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef LIB_OS_LINUX_KERNEL_ASSERT_H_
#define LIB_OS_LINUX_KERNEL_ASSERT_H_

#include <linux/bug.h>
#define assert(cond) WARN_ON(!(cond))
#define assert_msg(cond, ...) WARN(!(cond), __VA_ARGS__)

#endif /* LIB_OS_LINUX_KERNEL_ASSERT_H_ */
