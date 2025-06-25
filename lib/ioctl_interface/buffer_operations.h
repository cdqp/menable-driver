/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_IOCTL_INTERFACE_BUFFER_OPERATIONS_H_
#define LIB_IOCTL_INTERFACE_BUFFER_OPERATIONS_H_

#include <lib/os/types.h>

// To be sure that the following structs have the same layout across compiler versions,
// all padding is removed. Ordering of members might be optimized manually for clean alignment
// in favor over semantic blocks.
#pragma pack(push, 1)

typedef struct queue_buffer_args {
    uint32_t _size;    /* Size of this header */
    uint32_t _version; /* Version of this header */
    uint64_t bytes_to_transmit;
    uint64_t buffer_index;
    uint32_t head_index;
    /* Version 1 */
} queue_buffer_args;

#pragma pack(pop)

#endif