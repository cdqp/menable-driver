/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef REGISTER_INTERFACE_H_
#define REGISTER_INTERFACE_H_

#include "../os/types.h"

#ifndef __iomem
#define __iomem
#endif

typedef struct register_interface
{
    uint32_t __iomem * base_address;
    uint8_t is_active;

	void (*activate)(struct register_interface * self);
	void (*deactivate)(struct register_interface * self);

    void (*write)(struct register_interface * self, uint32_t address, uint32_t value);
    uint32_t (*read)(struct register_interface * self, uint32_t address);

    /**
     * Prevents back to back transfer among of register writes before and after the barrier.
     */
    void (*b2b_barrier)(struct register_interface * self);

    /**
     * Prevents reordering of register access from crossing the barrier.
     */
    void (*reorder_barrier)(struct register_interface * self);

    /**
     * Prevents reordering and back-to-back transfer across the barrier.
     */
    void (*reorder_b2b_barrier)(struct register_interface * self);
} register_interface;

#ifdef __cplusplus
extern "C" {
#endif

void register_interface_init(struct register_interface* ri, uint32_t __iomem * base_address,
                             void (*write_fct)(struct register_interface * self, uint32_t address, uint32_t value),
                             uint32_t (*read_fct)(struct register_interface * self, uint32_t address),
                             void (*b2b_barrier)(struct register_interface * self),
                             void (*reorder_barrier)(struct register_interface * self),
                             void (*reorder_b2b_barrier)(struct register_interface * self));

#ifdef __cplusplus
}
#endif

#endif /* REGISTER_INTERFACE_H_ */
