/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/



#ifndef LIB_OS_LINUX_ASSERT_H_
#define LIB_OS_LINUX_ASSERT_LINUX_H_

#ifdef __KERNEL__
#include "kernel/assert.h"
#else
#include <assert.h>
#define assert_msg(condition, message) assert(((void)(message), (condition)))
#endif

#endif /* LIB_OS_LINUX_ASSERT_H_ */
