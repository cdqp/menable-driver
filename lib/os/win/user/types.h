/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef _DRIVER_USER_CUSTOM_TYPES
#define _DRIVER_USER_CUSTOM_TYPES

#include <stdint.h>

#ifndef NULL
    #define NULL 0
#endif

// Visual Studio 2010 doesn't have stdbool.h
#if defined(_MSC_VER) && (_MSC_VER < 1800)
#define true 1
#define false 0
#else
#include <stdbool.h>
#endif

typedef struct user_mode_lock {
    void* mutex_handle;
} user_mode_lock;

#endif //_DRIVER_USER_CUSTOM_TYPES