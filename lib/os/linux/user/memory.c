/************************************************************************
 * Copyright 2022-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include <lib/helpers/memory.h>

#include <stdlib.h>
#include <string.h> //for memset

void* alloc_pageable_cacheable_small(size_t size, uint32_t tag) {
    return alloc_pageable_cacheable_large(size, tag);
}

void* alloc_pageable_cacheable_small_zeros(size_t size, uint32_t tag) {
    return alloc_pageable_cacheable_large_zeros(size, tag);
}

void free_pageable_cacheable_small(void* mem, uint32_t tag) {
    free_pageable_cacheable_small(mem, tag);
}

void* alloc_pageable_cacheable_large(size_t size, uint32_t tag) {
    return malloc(size);
}

void* alloc_pageable_cacheable_large_zeros(size_t size, uint32_t tag) {
    void* mem = malloc(size);
    if (mem != NULL) {
        memset(mem, 0, size);
    }
    return mem;
}

void free_pageable_cacheable_large(void* mem, uint32_t tag) {
    free(mem);
}

void* alloc_nonpageable_cacheable_small(size_t size, uint32_t tag) {
    return malloc(size);
}

void* alloc_nonpageable_cacheable_small_zeros(size_t size, uint32_t tag) {
    return calloc(size, 1);
}

void free_nonpageable_cacheable_small(void* mem, uint32_t tag) {
    free(mem);
}

void copy_mem(void* destination, const void* source, size_t length) {
    memcpy(destination, source, length);
}

void fill_mem(void * mem, size_t length, int fill_value) {
    memset(mem, fill_value, length);
}