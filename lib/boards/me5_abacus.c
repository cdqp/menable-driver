/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "me5_abacus.h"
#include "me5_defines.h"

struct i2c_master_core_declaration me5_abacus_i2c_declaration = {
    .address_register = ME5_I2C_ADDRESS_REG,
    .write_register = ME5_I2C_WRITE_REG,
    .read_register = ME5_ABACUS_I2C_READ_REG,
    .num_required_safety_writes = ME5_I2C_NUM_SAFETY_WRITES,
    .firmware_clock_frequency = ME5_FW_CLOCK_FREQ,
    .bus_count = ME5_ABACUS_NUM_I2C_BUSSES,
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
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = 0,
            .bus_frequency = ME5_I2C_FREQ_POE
        }
    }
};

struct spi_v2a_declaration me5_abacus_spi_declaration = {
    .id = ME5_SPI0_PEIPHERAL_ID,
    .control_register = ME5_ABACUS_SPI_CONTROL_REG,
    .target_device = 0
};

