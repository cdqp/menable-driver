/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef LIB_OS_WIN_TYPES_WIN_H_
#define LIB_OS_WIN_TYPES_WIN_H_

#ifdef _KERNEL_MODE
#include "kernel/types.h"
#else
#include "user/types.h"
#endif

static inline uint16_t swab16(uint16_t val)
{
    return ((val & 0x00FF) << 8)
        | ((val & 0xFF00U) >> 8);
}

static inline uint32_t swab32(uint32_t val)
{
    return ((val & 0x000000FF) << 24)
        | ((val & 0x0000FF00) << 8)
        | ((val & 0x00FF0000) >> 8)
        | ((val & 0xFF000000) >> 24);
}

static inline uint64_t swab64(uint64_t val)
{
    return ((val & 0x00000000000000FF) << 56)
        | ((val & 0x000000000000FF00) << 40)
        | ((val & 0x0000000000FF0000) << 24)
        | ((val & 0x00000000FF000000) << 8)
        | ((val & 0x000000FF00000000) >> 8)
        | ((val & 0x0000FF0000000000) >> 24)
        | ((val & 0x00FF000000000000) >> 40)
        | ((val & 0xFF00000000000000) >> 56);
}

#endif /* LIB_OS_WIN_TYPES_H_ */

