/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_IOCTL_INTERFACE_TRANSACTION_H_
#define LIB_IOCTL_INTERFACE_TRANSACTION_H_

#include "../os/types.h"
#include <multichar.h>

// A dummy peripheral Id. Access to this peripheral is ignored by the driver without further notice.
// Changing this id would break compatibility with older driver / runtime versions, so: Don't touch this!
#define DUMMY_PERIPHERAL_ID MULTICHAR32('D','U','M','Y')

/**
 * The possible types for bursts.
 */
enum burst_type
{
    BURST_TYPE_NONE,         //!< Invalid value
    BURST_TYPE_WRITE,        //!< A burst for writing data
    BURST_TYPE_READ,         //!< A burst for reading data
    BURST_TYPE_STATE_CHANGE, //!< A burst for changing the state of the target
    BURST_TYPE_COMMAND       //!< A burst for executing a command
};

// To be sure that the following structs have the same layout across compiler versions,
// all padding is removed.
#pragma pack(push, 1)

/**
 * Metadata for read/write bursts.
 */
typedef struct burst_header
{
    uint32_t type;
    uint32_t flags;

    /**
     * Userspace address of the burst's buffer.
     * 
     * @remark
     * This is stored as a uint64_t instead of a pointer to have a fixed
     * size on all machines, especially for a 32 bit process on a 64 bit machine.
     */
    uint64_t buffer_address;
    uint32_t len;

    /* Version 1 */

    /* Attention: This struct is packed. When adding new members, take care of alignment. */
} burst_header;

typedef struct transaction_header
{
    uint32_t _size;
    uint32_t _version;
    uint32_t peripheral;
    uint32_t num_bursts;

    /**
     * Userspace address of the burst headers array.
     * 
     * @remark
     * This is stored as a uint64_t instead of a pointer to have a fixed
     * size on all machines, especially for a 32 bit process on a 64 bit machine.
     */
    uint64_t burst_headers_address;

    /* Version 1 */

    /* Attention: This struct is packed. When adding new members, take care of alignment. */
} transaction_header;

/**
 * The header of a command transaction.
 * The buffer of a command burst must start with such a struct.
 */
typedef struct command_burst_header
{
    uint32_t command_id;

    // The command header's size must be divisible by 64,
    // so that any following data in the user buffer will be 64 bit aligned.
    // Therefore we reserve the remaining bytes for future use.
    uint8_t _reserved[4];

    /* Version 1 */

    /* Attention: This struct is packed. When adding new members, take care of alignment. */

} command_burst_header;

#pragma pack(pop)

#endif /* LIB_IOCTL_INTERFACE_TRANSACTION_H_ */
