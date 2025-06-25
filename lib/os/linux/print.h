/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef LIB_OS_LINUX_IO_LINUX_H_
#define LIB_OS_LINUX_IO_LINUX_H_

#ifdef __KERNEL__
#include <linux/printk.h>
#include <linux/bug.h>

#else
#include "user/print.h"
#endif

#endif /* LIB_OS_LINUX_PRINT_H_ */
