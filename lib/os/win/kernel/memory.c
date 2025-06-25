/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

/* include order matters */
#include <ntddk.h>
#include <wdm.h>

#include "types.h"
#include "..\..\..\helpers\memory.h"

void * alloc_pageable_cacheable_small(size_t size, uint32_t tag) {
	// TODO: [RKN] Is there a windows kernel function to allocate small (< 1 page) buffers (like kmalloc on linux)?
	return alloc_pageable_cacheable_large(size, tag);
}

void * alloc_pageable_cacheable_small_zeros(size_t size, uint32_t tag) {
	return alloc_pageable_cacheable_large_zeros(size, tag);
}

void free_pageable_cacheable_small(void * mem, uint32_t tag) {
	ExFreePoolWithTag(mem, tag);
}

void * alloc_pageable_cacheable_large(size_t size, uint32_t tag) {
	return ExAllocatePoolWithTag(PagedPool, size, tag);
}

void * alloc_pageable_cacheable_large_zeros(size_t size, uint32_t tag) {
	void * mem = ExAllocatePoolWithTag(PagedPool, size, tag);
	if (mem != NULL) {
		RtlZeroMemory(mem, size);
	}
	return mem;
}

void free_pageable_cacheable_large(void * mem, uint32_t tag) {
	ExFreePoolWithTag(mem, tag);
}

void * alloc_nonpageable_cacheable_small(size_t size, uint32_t tag)
{
	return ExAllocatePoolWithTag(NonPagedPool, size, tag);
}

void * alloc_nonpageable_cacheable_small_zeros(size_t size, uint32_t tag)
{
	void * mem = ExAllocatePoolWithTag(NonPagedPool, size, tag);
	if (mem != NULL) {
		RtlZeroMemory(mem, size);
	}
	return mem;
}

void free_nonpageable_cacheable_small(void * mem, uint32_t tag)
{
	ExFreePoolWithTag(mem, tag);
}

void copy_mem(void * destination, const void * source, size_t length) {
	RtlCopyMemory(destination, source, length);
}

void fill_mem(void * mem, size_t length, int fill_value) {
    RtlFillMemory(mem, length, fill_value);
}

uint64_t get_bus_address(void* virtual_address) {
    return MmGetPhysicalAddress(virtual_address).QuadPart;
}