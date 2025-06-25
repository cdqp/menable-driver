/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/



#ifndef KERNEL_MACROS_H_
#define KERNEL_MACROS_H_

#include "types.h"

#ifndef __KERNEL__

#define container_of(member_ptr, container_type, member_name) (container_type*)((char*)(member_ptr) - (char*)&((container_type*)0)->member_name)
#define WRITE_ONCE(reg, value) ((*(volatile uint32_t *)(reg)) = (value))
#define READ_ONCE(reg) (*(volatile uint32_t *)(reg))

#define KBUILD_MODNAME "menable"
#define KBUILD_BASENAME __FILE__

#if defined(__i386__)
/*
 * Some non-Intel clones support out of order store. wmb() ceases to be a
 * nop for these.
 */
#define mb()    asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#define rmb()   asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#define wmb()   asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#elif defined(__x86_64__)
#define mb()    __asm volatile("mfence":::"memory")
#define rmb()   __asm volatile("lfence":::"memory")
#define wmb()   __asm volatile("sfence" ::: "memory")
#endif
#endif


#endif /* KERNEL_MACROS_H_ */
