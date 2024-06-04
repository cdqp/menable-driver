/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef _LIB_HELPERS_MEMORY_H_
#define _LIB_HELPERS_MEMORY_H_

#include <lib/os/types.h>

/*
 * Tags are windows only.
 * On linux or if no tag is desired, use the following macro.
 */
#define DUMMY_ALLOC_TAG 0

/*
 * Implementation is located inside `os`
 */

/**
 * Pageable memory up to one page
 */
void * alloc_pageable_cacheable_small(size_t size, uint32_t tag);
void * alloc_pageable_cacheable_small_zeros(size_t size, uint32_t tag);
void free_pageable_cacheable_small(void * mem, uint32_t tag);

/**
 * Pageable memory with arbitrary size.
 */
void * alloc_pageable_cacheable_large(size_t size, uint32_t tag);
void * alloc_pageable_cacheable_large_zeros(size_t size, uint32_t tag);
void free_pageable_cacheable_large(void * mem, uint32_t tag);

/**
 * Pageable memory up to one page
 */
void * alloc_nonpageable_cacheable_small(size_t size, uint32_t tag);
void * alloc_nonpageable_cacheable_small_zeros(size_t size, uint32_t tag);
void free_nonpageable_cacheable_small(void * mem, uint32_t tag);

void copy_mem(void * destination, const void * source, size_t length);
void fill_mem(void * mem, size_t length, int fill_value);

#endif /* _LIB_HELPERS_MEMORY_H_ */
