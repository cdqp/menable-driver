/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "../../../helpers/type_hierarchy.h"
#include "../../../fpga/register_interface.h"

#include "../../../helpers/dbg.h"

static void menable_write_register(struct register_interface * ri, uint32_t address, uint32_t value) {
	if (ri->is_active) {
		*(ri->base_address + address) = value;
	} else {
		pr_warn(KBUILD_MODNAME " [REG_IF]: Attempt to write 0x%08x to register 0x%08x while register I/O is deactivated.\n", value, address);
	}
}

static uint32_t menable_read_register(struct register_interface * ri, uint32_t address) {
	uint32_t val = 0xffffffff;
	if (ri->is_active) {
		val = *(ri->base_address + address);
	} else {
		pr_warn(KBUILD_MODNAME " [REG_IF]: Attempt to read register 0x%08x while register I/O is deactivated.\n", address);
	}
    return val;
}


static void menable_b2b_barrier(struct register_interface * ri) {

}

static void menable_reorder_barrier(struct register_interface * ri) {
    /* nop, ioread32 and iowrite32 contain memory barriers */
}

static void menable_reorder_b2b_barrier(struct register_interface * ri) {
    /* reordering is prevented by io[read|write]32 anyway */
    menable_b2b_barrier(ri);
}

void menable_register_interface_init(struct register_interface * ri,
                                     uint32_t * base_address) {
    register_interface_init(ri, base_address,
                            menable_write_register, menable_read_register,
                            menable_b2b_barrier, menable_reorder_barrier,
                            menable_reorder_b2b_barrier);
}
