/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "me5_ironman.h"
#include "me5_defines.h"

struct i2c_master_core_declaration me5_ironman_i2c_declaration = {
    .address_register = ME5_I2C_ADDRESS_REG,
    .write_register = ME5_I2C_WRITE_REG,
    .read_register = ME5_IRONMAN_I2C_READ_REG,
    .num_required_safety_writes = ME5_I2C_NUM_SAFETY_WRITES,
    .firmware_clock_frequency = ME5_FW_CLOCK_FREQ,
    .bus_count = ME5_IRONMAN_NUM_I2C_BUSSES,
    .bus_declarations = {
        {
            .id = ME5_I2C0_PERIPHERAL_ID,
            .name = "mE5 i2c 0",
            .bank_number = 0,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = 0,
            .bus_frequency = ME5_I2C_FREQ
        }
    }
};

struct spi_dual_declaration me5_ironman_spi_declaration = {
    .id = ME5_SPI0_PEIPHERAL_ID,
    .control_register = ME5_IRONMAN_SPI_CONTROL_REG,
    .flash_select_register = ME5_IRONMAN_SPI_SELECT_REG,
};

struct jtag_declaration me5_ironman_jtag_declaration = {
    .id = ME5_JTAG_PEIPHERAL_ID,
    .jtag_control_register = ME5_JTAG_CTR_REG,
    .devices_counts = ME5_JTAG_DEVICES_COUNTS,
};
