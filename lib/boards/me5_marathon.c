/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "me5_marathon.h"
#include "me5_defines.h"

struct i2c_master_core_declaration me5_marathon_i2c_declaration = {
    .address_register = ME5_I2C_ADDRESS_REG,
    .write_register = ME5_I2C_WRITE_REG,
    .read_register = ME5_MARATHON_I2C_READ_REG,
    .num_required_safety_writes = ME5_I2C_NUM_SAFETY_WRITES,
    .firmware_clock_frequency = ME5_FW_CLOCK_FREQ,
    .bus_count = ME5_MARATHON_NUM_I2C_BUSSES,
    .bus_declarations = {
        {
            .id = ME5_I2C0_PERIPHERAL_ID,
            .name = "mE5 i2c 0",
            .bank_number = 0,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = 0,
            .bus_frequency = ME5_I2C_FREQ
        },
        {
            .id = ME5_I2C1_PERIPHERAL_ID,
            .name = "mE5 i2c 1",
            .bank_number = 1,
            .bank_activation_bitmask = (1 << 6),
            .write_enable_bitmask = 0,
            .bus_frequency = ME5_I2C_FREQ_EXT
        }
    }
};

struct bpi_controller_declaration me5_marathon_bpi_declaration = {
    .id                     = ME5_BPI_PEIPHERAL_ID,
    .address_register       = ME5_MARATHON_BPI_ADDRESS_REG,
    .data_register          = ME5_MARATHON_BPI_WRITE_READ_REG,
    .bank_register          = ME5_MARATHON_BPI_BANK_SELECTION_STATUS_REG,
    .address_width          = 25,
    .bank_width             = 3,
};

struct jtag_declaration me5_marathon_jtag_declaration = {
    .id = ME5_JTAG_PEIPHERAL_ID,
    .jtag_control_register = ME5_JTAG_CTR_REG,
    .devices_counts = ME5_JTAG_DEVICES_COUNTS,
};
