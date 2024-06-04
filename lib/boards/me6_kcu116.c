/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "me6_kcu116.h"
#include "me6_defines.h"

struct i2c_master_core_declaration me6_kcu116_i2c_declaration[4] = {
{
    .address_register = ME6_REG_I2C_ADDRESS,
    .write_register = ME6_REG_I2C_WRITE,
    .read_register = ME6_REG_I2C_READ,
    .num_required_safety_writes = ME6_I2C_NUM_SAFETY_WRITES,
    .firmware_clock_frequency = ME6_FW_CLOCK_FREQ,
    .bus_count = 1,
    .bus_declarations = {
        {
            .id = ME6_I2C0_PERIPHERAL_ID,
            .name = "mE6 kcu116 i2c 0",
            .bank_number = 0,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = (1 << 6),
            .bus_frequency = ME6_I2C_FREQ
        }
    }
},
{
    .address_register = ME6_REG_I2C_1_ADDRESS,
    .write_register = ME6_REG_I2C_1_WRITE,
    .read_register = ME6_REG_I2C_1_READ,
    .num_required_safety_writes = ME6_I2C_NUM_SAFETY_WRITES,
    .firmware_clock_frequency = ME6_FW_CLOCK_FREQ,
    .bus_count = 1,
    .bus_declarations = {
        {
            .id = ME6_I2C1_PERIPHERAL_ID,
            .name = "mE6 kcu116 i2c 1",
            .bank_number = 0,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = (1 << 6),
            .bus_frequency = ME6_I2C_FREQ
        }
    }
},
{
    .address_register = ME6_REG_I2C_2_ADDRESS,
    .write_register = ME6_REG_I2C_2_WRITE,
    .read_register = ME6_REG_I2C_2_READ,
    .num_required_safety_writes = ME6_I2C_NUM_SAFETY_WRITES,
    .firmware_clock_frequency = ME6_FW_CLOCK_FREQ,
    .bus_count = 1,
    .bus_declarations = {
        {
            .id = ME6_I2C2_PERIPHERAL_ID,
            .name = "mE6 kcu116 i2c 2",
            .bank_number = 0,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = (1 << 6),
            .bus_frequency = ME6_I2C_FREQ
        }
    }
},
{
    .address_register = ME6_REG_I2C_3_ADDRESS,
    .write_register = ME6_REG_I2C_3_WRITE,
    .read_register = ME6_REG_I2C_3_READ,
    .num_required_safety_writes = ME6_I2C_NUM_SAFETY_WRITES,
    .firmware_clock_frequency = ME6_FW_CLOCK_FREQ,
    .bus_count = 2,
    .bus_declarations = {
        {
            .id = ME6_I2C3_PERIPHERAL_ID,
            .name = "mE6 kcu116 i2c 3",
            .bank_number = 0,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = (1 << 6),
            .bus_frequency = ME6_I2C_FREQ
        },
        {
            .id = ME6_I2C4_PERIPHERAL_ID,
            .name = "mE6 kcu116 i2c 4",
            .bank_number = 1,
            .bank_activation_bitmask = (1 << 7),
            .write_enable_bitmask = 0,
            .bus_frequency = ME6_I2C_FREQ
        }
    }
}
};

struct spi_v2a_declaration me6_kcu116_spi_declaration = {
    .id = ME6_SPI0_PEIPHERAL_ID,
    .control_register = ME6_REG_SPI_CONTROL,
    .target_device = 0
};

messaging_dma_declaration me6_kcu116_messaging_dma_declaration = {
    .control_register = 0x134,
    .init_register = 0x135,
    .num_buffers = 128
};

struct uiq_declaration me6_kcu116_uiq_declaration[ME6_KCU116_NUM_UIQS] = {
    {.name = "GPIO camera status rx channel 0",  .type = UIQ_TYPE_READ,      .id = 0x100, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "GPIO camera status rx channel 1",  .type = UIQ_TYPE_READ,      .id = 0x101, .reg_offs = ME6_REG_IRQ_EVENT_DATA}
};

struct jtag_declaration me6_kcu116_jtag_declaration = {
    .id = ME6_JTAG_PEIPHERAL_ID,
    .jtag_control_register = ME6_JTAG_CTR_REG,
    .devices_counts = ME6_JTAG_DEVICES_COUNTS,
};
