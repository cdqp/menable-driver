/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "me6_impulse.h"
#include "me6_defines.h"

struct i2c_master_core_declaration me6_impulse_i2c_declaration = {
    .address_register = ME6_REG_I2C_ADDRESS,
    .write_register = ME6_REG_I2C_WRITE,
    .read_register = ME6_REG_I2C_READ,
    .num_required_safety_writes = ME6_I2C_NUM_SAFETY_WRITES,
    .firmware_clock_frequency = ME6_FW_CLOCK_FREQ,
    .bus_count = ME6_IMPULSE_NUM_I2C_BUSSES,
    .bus_declarations = {
        {
            .id = ME6_I2C0_PERIPHERAL_ID,
            .name = "mE6 impulse i2c 0",
            .bank_number = 0,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = (1 << 6),
            .bus_frequency = ME6_I2C_FREQ
        },
        {
            .id = ME6_I2C1_PERIPHERAL_ID,
            .name = "mE6 impulse i2c 1",
            .bank_number = 1,
            .bank_activation_bitmask = (1 << 7),
            .write_enable_bitmask = 0,
            .bus_frequency = ME6_I2C_FREQ_EXT
        }
    }
};

struct spi_v2a_declaration me6_impulse_spi_declaration = {
    .id = ME6_SPI0_PEIPHERAL_ID,
    .control_register = ME6_REG_SPI_CONTROL,
    .target_device = 0
};

messaging_dma_declaration me6_impulse_messaging_dma_declaration = {
    .control_register = 0x134,
    .init_register = 0x135,
    .num_buffers = 128
};

struct uiq_declaration me6_impulse_cxp_uiq_declaration[ME6_IMPULSE_CXP_NUM_UIQS] = {
    {.name = "control packet rx channel 0", .type = UIQ_TYPE_READ,      .id = 0x100, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 0",    .type = UIQ_TYPE_READ,      .id = 0x101, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 0",   .type = UIQ_TYPE_READ,      .id = 0x102, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 0", .type = UIQ_TYPE_WRITE_CXP, .id = 0x200, .reg_offs = ME6_IMPULSE_CXP_REG_CMD_DATA_0, .write_burst = 508},
    {.name = "control packet rx channel 1", .type = UIQ_TYPE_READ,      .id = 0x103, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 1",    .type = UIQ_TYPE_READ,      .id = 0x104, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 1",   .type = UIQ_TYPE_READ,      .id = 0x105, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 1", .type = UIQ_TYPE_WRITE_CXP, .id = 0x201, .reg_offs = ME6_IMPULSE_CXP_REG_CMD_DATA_1, .write_burst = 508},
    {.name = "control packet rx channel 2", .type = UIQ_TYPE_READ,      .id = 0x106, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 2",    .type = UIQ_TYPE_READ,      .id = 0x107, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 2",   .type = UIQ_TYPE_READ,      .id = 0x108, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 2", .type = UIQ_TYPE_WRITE_CXP, .id = 0x202, .reg_offs = ME6_IMPULSE_CXP_REG_CMD_DATA_2, .write_burst = 508},
    {.name = "control packet rx channel 3", .type = UIQ_TYPE_READ,      .id = 0x109, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 3",    .type = UIQ_TYPE_READ,      .id = 0x10a, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 3",   .type = UIQ_TYPE_READ,      .id = 0x10b, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 3", .type = UIQ_TYPE_WRITE_CXP, .id = 0x203, .reg_offs = ME6_IMPULSE_CXP_REG_CMD_DATA_3, .write_burst = 508},
};

struct uiq_declaration me6_impulse5_cxp_uiq_declaration[ME6_IMPULSE5_CXP_NUM_UIQS] = {
    {.name = "control packet rx channel 0", .type = UIQ_TYPE_READ,      .id = 0x100, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 0",    .type = UIQ_TYPE_READ,      .id = 0x101, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 0",   .type = UIQ_TYPE_READ,      .id = 0x102, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 0", .type = UIQ_TYPE_WRITE_CXP, .id = 0x200, .reg_offs = ME6_IMPULSE_CXP_REG_CMD_DATA_0, .write_burst = 508},
    {.name = "control packet rx channel 1", .type = UIQ_TYPE_READ,      .id = 0x103, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 1",    .type = UIQ_TYPE_READ,      .id = 0x104, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 1",   .type = UIQ_TYPE_READ,      .id = 0x105, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 1", .type = UIQ_TYPE_WRITE_CXP, .id = 0x201, .reg_offs = ME6_IMPULSE_CXP_REG_CMD_DATA_1, .write_burst = 508},
    {.name = "control packet rx channel 2", .type = UIQ_TYPE_READ,      .id = 0x106, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 2",    .type = UIQ_TYPE_READ,      .id = 0x107, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 2",   .type = UIQ_TYPE_READ,      .id = 0x108, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 2", .type = UIQ_TYPE_WRITE_CXP, .id = 0x202, .reg_offs = ME6_IMPULSE_CXP_REG_CMD_DATA_2, .write_burst = 508},
    {.name = "control packet rx channel 3", .type = UIQ_TYPE_READ,      .id = 0x109, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 3",    .type = UIQ_TYPE_READ,      .id = 0x10a, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 3",   .type = UIQ_TYPE_READ,      .id = 0x10b, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 3", .type = UIQ_TYPE_WRITE_CXP, .id = 0x203, .reg_offs = ME6_IMPULSE_CXP_REG_CMD_DATA_3, .write_burst = 508},
    {.name = "control packet rx channel 4", .type = UIQ_TYPE_READ,      .id = 0x10c, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 4",    .type = UIQ_TYPE_READ,      .id = 0x10d, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 4",   .type = UIQ_TYPE_READ,      .id = 0x10e, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 4", .type = UIQ_TYPE_WRITE_CXP, .id = 0x204, .reg_offs = ME6_IMPULSE_CXP_REG_CMD_DATA_4, .write_burst = 508},
};

struct jtag_declaration me6_impulse_jtag_declaration = {
    .id = ME6_JTAG_PEIPHERAL_ID,
    .jtag_control_register = ME6_JTAG_CTR_REG,
    .devices_counts = ME6_JTAG_DEVICES_COUNTS,
};
