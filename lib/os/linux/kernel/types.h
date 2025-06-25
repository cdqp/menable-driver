/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef LIB_OS_LINUX_KERNEL_TYPES_H_
#define LIB_OS_LINUX_KERNEL_TYPES_H_

#include <linux/kernel.h>
#include <linux/mutex.h>

#define UINT8_MAX   U8_MAX
#define INT8_MAX    S8_MAX
#define INT8_MIN    S8_MIN
#define UINT16_MAX  U16_MAX
#define INT16_MAX   S16_MAX
#define INT16_MIN   S16_MIN
#define UINT32_MAX  U32_MAX
#define INT32_MAX   S32_MAX
#define INT32_MIN   S32_MIN
#define UINT64_MAX  U64_MAX
#define INT64_MAX   S64_MAX
#define INT64_MIN   S64_MIN

typedef struct mutex user_mode_lock;

#endif /* LIB_OS_LINUX_KERNEL_TYPES_H_ */
