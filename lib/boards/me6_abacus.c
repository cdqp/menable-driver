/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "me6_abacus.h"
#include "me6_defines.h"

struct i2c_master_core_declaration me6_abacus_i2c_declaration = {
    .address_register = ME6_REG_I2C_ADDRESS,
    .write_register = ME6_REG_I2C_WRITE,
    .read_register = ME6_REG_I2C_READ,
    .num_required_safety_writes = ME6_I2C_NUM_SAFETY_WRITES,
    .firmware_clock_frequency = ME6_FW_CLOCK_FREQ,
    .bus_count = ME6_ABACUS_NUM_I2C_BUSSES,
    .bus_declarations = {
        {
            .id = ME6_I2C0_PERIPHERAL_ID,
            .name = "mE6 abacus i2c 0",
            .bank_number = 0,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = (1 << 6),
            .bus_frequency = ME6_I2C_FREQ
        },
        {
            .id = ME6_I2C1_PERIPHERAL_ID,
            .name = "mE6 abacus i2c 1",
            .bank_number = 1,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = 0,
            .bus_frequency = ME6_I2C_FREQ_POE
        }
    }
};

struct spi_v2a_declaration me6_abacus_spi_declaration = {
    .id = ME6_SPI0_PEIPHERAL_ID,
    .control_register = ME6_REG_SPI_CONTROL,
    .target_device = 0
};

struct uiq_declaration me6_abacus_uiq_declaration[ME6_ABACUS_NUM_UIQS] = {
    {.name = "frame corrupted channel 0",    .type = UIQ_TYPE_READ,         .id = 0x100, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "frame corrupted channel 1",    .type = UIQ_TYPE_READ,         .id = 0x101, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "frame corrupted channel 2",    .type = UIQ_TYPE_READ,         .id = 0x102, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "frame corrupted channel 3",    .type = UIQ_TYPE_READ,         .id = 0x103, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "frame lost channel 0",         .type = UIQ_TYPE_READ,         .id = 0x104, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "frame lost channel 1",         .type = UIQ_TYPE_READ,         .id = 0x105, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "frame lost channel 2",         .type = UIQ_TYPE_READ,         .id = 0x106, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "frame lost channel 3",         .type = UIQ_TYPE_READ,         .id = 0x107, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "control packet channel 0",     .type = UIQ_TYPE_READ,         .id = 0x108, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "control packet channel 1",     .type = UIQ_TYPE_READ,         .id = 0x109, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "control packet channel 2",     .type = UIQ_TYPE_READ,         .id = 0x10a, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "control packet channel 3",     .type = UIQ_TYPE_READ,         .id = 0x10b, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "action command lost channel 0",.type = UIQ_TYPE_READ,         .id = 0x10c, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "action command lost channel 1",.type = UIQ_TYPE_READ,         .id = 0x10d, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "action command lost channel 2",.type = UIQ_TYPE_READ,         .id = 0x10e, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "action command lost channel 3",.type = UIQ_TYPE_READ,         .id = 0x10f, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "spi data in",                  .type = UIQ_TYPE_READ,         .id = 0x110, .reg_offs = ME6_REG_IRQ_EVENT_DATA },
    {.name = "spi data out",                 .type = UIQ_TYPE_WRITE_LEGACY, .id = 0x211, .reg_offs = ME6_ABACUS_REG_SPI_DATA_OUT, .write_burst = 16 }
};
