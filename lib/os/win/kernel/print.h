/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_OS_WIN_PRINT_KERNEL_H_
#define LIB_OS_WIN_PRINT_KERNEL_H_

#include <wdm.h>
#include <stdio.h>
#include <varargs.h>

#define snprintf _snprintf

extern ULONG logComponentId;

#if defined(DEBUG)
#define pr_debug(...) DbgPrintEx(logComponentId, DPFLTR_INFO_LEVEL, __VA_ARGS__);
#else
/* Define a function that does not print anyhting but takes the arguments like pr_debug does. 
   This way we still get compiler errors for the arguments of pr_debug statements when debugging output is turned off. */
static inline void _men_pr_nop(const char* fmt, ...) {
    (void)fmt;
}
#define pr_debug(fmt, ...) _men_pr_nop((fmt), __VA_ARGS__)
#endif
#define pr_err(...)   DbgPrintEx(logComponentId, DPFLTR_ERROR_LEVEL,   __VA_ARGS__);
#define pr_warn(...)  DbgPrintEx(logComponentId, DPFLTR_WARNING_LEVEL, __VA_ARGS__);
#define pr_info(...)  DbgPrintEx(logComponentId, DPFLTR_INFO_LEVEL,    __VA_ARGS__);

#endif /* LIB_OS_WIN_PRINT_KERNEL_H_ */
