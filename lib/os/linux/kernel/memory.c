/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <asm/io.h>

#include <lib/helpers/memory.h>

void * alloc_pageable_cacheable_small(size_t size, uint32_t tag) {
    return alloc_pageable_cacheable_large(size, tag);
}

void * alloc_pageable_cacheable_small_zeros(size_t size, uint32_t tag) {
    return alloc_pageable_cacheable_large_zeros(size, tag);
}

void free_pageable_cacheable_small(void * mem, uint32_t tag) {
    free_pageable_cacheable_small(mem, tag);
}

void * alloc_pageable_cacheable_large(size_t size, uint32_t tag) {
    return vmalloc(size);
}

void * alloc_pageable_cacheable_large_zeros(size_t size, uint32_t tag) {
    void * mem = vmalloc(size);
    if (mem != NULL) {
        memset(mem, 0, size);
    }
    return mem;
}

void free_pageable_cacheable_large(void * mem, uint32_t tag) {
    vfree(mem);
}

void * alloc_nonpageable_cacheable_small(size_t size, uint32_t tag) {
    return kmalloc(size, GFP_KERNEL);
}

void * alloc_nonpageable_cacheable_small_zeros(size_t size, uint32_t tag) {
    return kzalloc(size, GFP_KERNEL);
}

void free_nonpageable_cacheable_small(void * mem, uint32_t tag) {
    kfree(mem);
}

void copy_mem(void * destination, const void * source, size_t length) {
    memcpy(destination, source, length);
}

void fill_mem(void * mem, size_t length, int fill_value) {
    memset(mem, fill_value, length);
}

uint64_t get_bus_address(void* virtual_address) {
    /* TODO: This implementation should be changed to use the DMA api since it won't
     *       work with IOMMUs or architectures, where DMA addresses don't map 1:1 to
     *       physical addresses.
     * 
     *       This might require a DMA mapping abstraction in the driver library.
     */
    return virt_to_phys(virtual_address);
}