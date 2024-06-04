/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_OS_WIN_KERNEL_TYPES_H_
#define LIB_OS_WIN_KERNEL_TYPES_H_

#include <intsafe.h>
#include <ntdef.h>
#include <wdm.h>

typedef INT64 int64_t;
typedef INT32 int32_t;
typedef INT16 int16_t;
typedef INT8  int8_t;

typedef UINT64 uint64_t;
typedef UINT32 uint32_t;
typedef UINT16 uint16_t;
typedef UINT8  uint8_t;

#ifndef __cplusplus
#include <stdbool.h>
#endif

typedef FAST_MUTEX user_mode_lock;

#endif /* LIB_OS_WIN_KERNEL_TYPES_H_ */
