/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "me6_elegance.h"
#include "me6_defines.h"

struct i2c_master_core_declaration me6_elegance_i2c_declaration = {
    .address_register = ME6_REG_I2C_ADDRESS,
    .write_register = ME6_REG_I2C_WRITE,
    .read_register = ME6_REG_I2C_READ,
    .num_required_safety_writes = ME6_I2C_NUM_SAFETY_WRITES,
    .firmware_clock_frequency = ME6_FW_CLOCK_FREQ,
    .bus_count = ME6_ELEGANCE_NUM_I2C_BUSSES,
    .bus_declarations = {
        {
            .id = ME6_I2C0_PERIPHERAL_ID,
            .name = "mE6 elegance i2c 0",
            .bank_number = 0,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = (1 << 6),
            .bus_frequency = ME6_I2C_FREQ
        }
    }
};

struct uiq_declaration me6_elegance_uiq_declaration[ME6_ELEGANCE_NUM_UIQS] = {
    {.name = "control packet channel 0",          .type = UIQ_TYPE_READ, .id = 0, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "trigger acknowledgement channel 0", .type = UIQ_TYPE_READ, .id = 1, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet channel 1",          .type = UIQ_TYPE_READ, .id = 2, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "trigger acknowledgement channel 1", .type = UIQ_TYPE_READ, .id = 3, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet channel 2",          .type = UIQ_TYPE_READ, .id = 4, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "trigger acknowledgement channel 2", .type = UIQ_TYPE_READ, .id = 5, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet channel 3",          .type = UIQ_TYPE_READ, .id = 6, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "trigger acknowledgement channel 3", .type = UIQ_TYPE_READ, .id = 7, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
};
