/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#include <linux/mm.h>
#include <linux/io.h>

#include "../../../helpers/type_hierarchy.h"
#include "../../../fpga/menable_register_interface.h"

#include "../../../helpers/dbg.h"

static void menable_write_register(struct register_interface * ri, uint32_t address, uint32_t value) {
    DBG_REG_WRITE(address, value);
    if (ri->is_active) {
        iowrite32(value, ri->base_address + address);
    }
}

static uint32_t menable_read_register(struct register_interface * ri, uint32_t address) {
    uint32_t val = 0xffffffff;
    if (ri->is_active) {
        val = ioread32(ri->base_address + address);
    }
    DBG_REG_READ(address, val);
    return val;
}

static void menable_b2b_barrier(struct register_interface * ri) {
    /* read from a dummy register to prevent back to back transfer */
    if (ri->is_active) {
        (void) ioread32(ri->base_address);
    }
    DBG_REG_READ(0, 0);
}

static void menable_reorder_barrier(struct register_interface * ri) {
    /* nop, ioread32 and iowrite32 contain memory barriers */
}

static void menable_reorder_b2b_barrier(struct register_interface * ri) {
    /* reordering is prevented by io[read|write]32 anyway */
    menable_b2b_barrier(ri);
}

void menable_register_interface_init(struct register_interface * ri,
                                     uint32_t __iomem * base_address) {
    register_interface_init(ri, base_address,
                            menable_write_register, menable_read_register,
                            menable_b2b_barrier, menable_reorder_barrier,
                            menable_reorder_b2b_barrier);
}
