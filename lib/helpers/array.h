/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef MEN_LIB_HELPERS_ARRAY
#define MEN_LIB_HELPERS_ARRAY

#include <lib/os/types.h>

struct men_array {

    /* The internal memory buffer */
    void * _mem;

    /* The size of the buffer in bytes */
    uint32_t size;

    /**
     * Switches the internal buffer to a new memory location which may have a
     * bigger capacity than the current one.
     */
    int(*switch_buffer_to)(struct men_array* self, void * new_mem, uint32_t new_size);
};

int men_array_init(struct men_array * array, void * mem, uint32_t size);

#endif