/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#include "../../../fpga/menable_register_interface.h"
#include "../../../helpers/helper.h"
#include "../../../os/kernel_macros.h"

#include "../../../helpers/dbg.h"

static void write(struct register_interface * self, uint32_t address, uint32_t value) {
    DBG_REG_WRITE(address, value);
    WRITE_ONCE(self->base_address + address, value);
}

static uint32_t read(struct register_interface * self, uint32_t address) {
	uint32_t val = READ_ONCE(self->base_address + address);
    DBG_REG_READ(address, val);
    return val;
}

static void b2b_barrier(struct register_interface * self) {
    DBG1(pr_debug("~~~ back-to-back barrier ~~~\n"));
    IGNORE_RETVAL(READ_ONCE(self->base_address + 0x0012));
}

static void reorder_barrier(struct register_interface * self) {
    DBG1(pr_debug("~~~ reorder barrier ~~~\n"));
    mb();
}

static void reorder_b2b_barrier(struct register_interface * self) {
    DBG1(pr_debug("~~~ full barrier ~~~\n"));
    b2b_barrier(self);
}

void menable_register_interface_init(struct register_interface * ri, uint32_t * base_address) {
    register_interface_init(ri, base_address, write, read,
                            b2b_barrier, reorder_barrier, reorder_b2b_barrier);
}
