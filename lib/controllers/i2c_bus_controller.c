/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "i2c_bus_controller.h"
#include "../os/assert.h"
#include "../helpers/error_handling.h"

static int i2c_begin_transaction(struct controller_base * base) {
    struct i2c_bus_controller * self = downcast(base, struct i2c_bus_controller);
    struct controller_base * core_base = upcast(self->i2c_core);

    self->i2c_core->activate_bank(self->i2c_core, self->bank_number);
    return core_base->begin_transaction(core_base);
}

static void i2c_end_transaction(struct controller_base * base) {
    struct i2c_bus_controller * self = downcast(base, struct i2c_bus_controller);
    struct controller_base * core_base = upcast(self->i2c_core);

    core_base->end_transaction(core_base);
}

static int i2c_read_burst(struct controller_base * base, struct burst_header * bh, uint8_t * buf, size_t size) {
    struct i2c_bus_controller * self = downcast(base, struct i2c_bus_controller);
    struct controller_base * core_base = upcast(self->i2c_core);

    return core_base->read_burst(core_base, bh, buf, size);
}

static int i2c_write_burst(struct controller_base * base, struct burst_header * bh, const uint8_t * buf, size_t size) {
    struct i2c_bus_controller * self = downcast(base, struct i2c_bus_controller);
    struct controller_base * core_base = upcast(self->i2c_core);

    return core_base->write_burst(core_base, bh, buf, size);
}

static int i2c_state_change_burst(struct controller_base * base, struct burst_header * bh) {
    struct i2c_bus_controller * self = downcast(base, struct i2c_bus_controller);
    struct controller_base * core_base = upcast(self->i2c_core);

    return core_base->state_change_burst(core_base, bh);
}

static bool i2c_are_burst_flags_set(struct controller_base * base, uint8_t flags) {
    struct i2c_bus_controller * self = downcast(base, struct i2c_bus_controller);
    struct controller_base * core_base = upcast(self->i2c_core);

    return core_base->are_burst_flags_set(core_base, flags);
}

static void i2c_destroy(struct controller_base * base) {
    struct i2c_bus_controller * self = downcast(base, struct i2c_bus_controller);
    struct controller_base * core_base = upcast(self->i2c_core);

    core_base->destroy(core_base);
}

int i2c_bus_controller_init(struct i2c_bus_controller * ctrl,
                            struct i2c_master_core * i2c_core,
                            uint8_t bank_number) {

    assert_msg(bank_number < 8, "Invalid bank number\n");

    ctrl->i2c_core = i2c_core;
    ctrl->bank_number = bank_number;

    /*
     * redirect then "non-virtual" member functions of the base of this instance
     * to the base of the i2c_master_core instance.
     */
    struct controller_base * base = upcast(ctrl);
    controller_base_super_init(base,
        i2c_core->base_type.register_interface,
        i2c_core->base_type.lock,
        i2c_core->base_type.read_queue_size,
        i2c_core->base_type.max_bytes_per_read,
        i2c_core->base_type.write_queue_size,
        i2c_core->base_type.max_bytes_per_write,
        i2c_begin_transaction,
        i2c_end_transaction,
        i2c_read_burst,
        i2c_write_burst,
        i2c_state_change_burst,
        NULL,
        i2c_are_burst_flags_set,
        i2c_destroy);

    return STATUS_OK;
}
