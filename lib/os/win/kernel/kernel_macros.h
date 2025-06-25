/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_OS_WIN_KERNEL_MACROS_KERNEL_H_
#define LIB_OS_WIN_KERNEL_MACROS_KERNEL_H_

#define container_of(member_ptr, container_type, member_name) (container_type*)((char*)(member_ptr) - (char*)&((container_type*)0)->member_name)
#define WRITE_ONCE(reg, value) ((*(volatile uint32_t *)(reg)) = (value))
#define READ_ONCE(reg) (*(volatile uint32_t *)(reg))

#define KBUILD_MODNAME "menable"
#define KBUILD_BASENAME __FILE__

#endif /* LIB_OS_WIN_KERNEL_MACROS_KERNEL_H_ */
