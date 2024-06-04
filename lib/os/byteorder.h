/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef LIB_OS_BYTEORDER_H_
#define LIB_OS_BYTEORDER_H_

#if defined(__linux__)

#include <asm/byteorder.h>

#elif defined(_WIN32) || defined(_WIN64)
/* todo */
#endif

#endif /* LIB_OS_TYPES_H_ */
