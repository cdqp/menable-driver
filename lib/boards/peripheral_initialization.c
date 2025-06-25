/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "peripheral_declaration.h"
#include "peripheral_initialization.h"

#include <lib/helpers/error_handling.h>

int men_init_i2c_master_core(struct i2c_master_core * master_core,
                             struct i2c_bus_controller ** i2c_busses,
                             struct register_interface * reg_interface,
                             user_mode_lock * lock,
                             struct i2c_master_core_declaration * decl) {

    int ret = i2c_master_core_init(
                master_core, reg_interface, lock,
                decl->address_register, 
                decl->write_register,
                decl->read_register,
                decl->firmware_clock_frequency,
                decl->num_required_safety_writes);

    /* TODO: Cleanup? */
    if (ret != STATUS_OK)
        return ret;

    for (int i = 0; i < decl->bus_count; ++i) {
        struct men_i2c_bus_declaration * bus_declaration = &decl->bus_declarations[i];
        ret = i2c_master_core_configure_bus(
                master_core,
                bus_declaration->bank_number,
                bus_declaration->bank_activation_bitmask,
                bus_declaration->write_enable_bitmask,
                bus_declaration->bus_frequency);

        /* TODO: Cleanup? */
        if (ret != STATUS_OK)
            return ret;

        struct i2c_bus_controller * bus_controller = i2c_busses[i];
        ret = i2c_bus_controller_init(bus_controller, master_core, bus_declaration->bank_number);
        
        /* TODO: Cleanup? */
        if (ret != STATUS_OK)
            return ret;
    }

    return STATUS_OK;
}
