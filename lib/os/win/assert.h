/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef LIB_OS_WIN_ASSERT_H_
#define LIB_OS_WIN_ASSERT_WIN_H_


#ifdef _KERNEL_MODE
#include <wdm.h>
#define assert_msg(condition, message) ASSERTMSG((message), (condition))
#define assert(condition) ASSERTMSG("", (condition))
#else
#include <assert.h>
#define assert_msg(condition, message) assert(((void)(message), (condition)))
#endif

#endif /* LIB_OS_WIN_ASSERT_H_ */
