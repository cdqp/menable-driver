/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_OS_WIN_USER_PRINT_H
#define LIB_OS_WIN_USER_PRINT_H

#include <stdio.h>

#define pr_info printf
#define pr_warn printf
#define pr_err printf
#ifdef DEBUG
	#define pr_debug printf
#else
	#define pr_debug(...)
#endif

#endif