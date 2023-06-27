/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "basler_ic.h"
#include "me6_defines.h"

#include <multichar.h>

struct i2c_master_core_declaration basler_cxp12_ic_i2c_declaration = {
    .address_register = 0x1005,
    .write_register = 0x1006,
    .read_register = 0x1006,
    .num_required_safety_writes = 2,
    .firmware_clock_frequency = 300000000, // 300 MHz
    .bus_count = BASLER_CXP12_IC_1C_NUM_I2C_BUSSES,
    .bus_declarations = {
        {
            .id = MULTICHAR32('I', '2', 'C', '0'),
            .name = "cxp12-ic-1c i2c 0",
            .bank_number = 0,
            .bank_activation_bitmask = 0,
            .write_enable_bitmask = (1 << 6),
            .bus_frequency = 400000 // 400 KHz
        }
    }
};

struct uiq_declaration basler_cxp12_ic_1c_uiq_declaration[BASLER_CXP12_IC_1C_NUM_UIQS] = {
    {.name = "control packet rx channel 0", .type = UIQ_TYPE_READ,      .id = 0x100, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 0",    .type = UIQ_TYPE_READ,      .id = 0x101, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 0",   .type = UIQ_TYPE_READ,      .id = 0x102, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 0", .type = UIQ_TYPE_WRITE_CXP, .id = 0x200, .reg_offs = BASLER_CXP12_IC_CXP_REG_CMD_DATA_0, .write_burst = 508},
};

struct uiq_declaration basler_cxp12_ic_2c_uiq_declaration[BASLER_CXP12_IC_2C_NUM_UIQS] = {
    {.name = "control packet rx channel 0", .type = UIQ_TYPE_READ,      .id = 0x100, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 0",    .type = UIQ_TYPE_READ,      .id = 0x101, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 0",   .type = UIQ_TYPE_READ,      .id = 0x102, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 0", .type = UIQ_TYPE_WRITE_CXP, .id = 0x200, .reg_offs = BASLER_CXP12_IC_CXP_REG_CMD_DATA_0, .write_burst = 508},
    {.name = "control packet rx channel 1", .type = UIQ_TYPE_READ,      .id = 0x103, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 1",    .type = UIQ_TYPE_READ,      .id = 0x104, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 1",   .type = UIQ_TYPE_READ,      .id = 0x105, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 1", .type = UIQ_TYPE_WRITE_CXP, .id = 0x201, .reg_offs = BASLER_CXP12_IC_CXP_REG_CMD_DATA_1, .write_burst = 508},
};

struct uiq_declaration basler_cxp12_ic_4c_uiq_declaration[BASLER_CXP12_IC_4C_NUM_UIQS] = {
    {.name = "control packet rx channel 0", .type = UIQ_TYPE_READ,      .id = 0x100, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 0",    .type = UIQ_TYPE_READ,      .id = 0x101, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 0",   .type = UIQ_TYPE_READ,      .id = 0x102, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 0", .type = UIQ_TYPE_WRITE_CXP, .id = 0x200, .reg_offs = BASLER_CXP12_IC_CXP_REG_CMD_DATA_0, .write_burst = 508},
    {.name = "control packet rx channel 1", .type = UIQ_TYPE_READ,      .id = 0x103, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 1",    .type = UIQ_TYPE_READ,      .id = 0x104, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 1",   .type = UIQ_TYPE_READ,      .id = 0x105, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 1", .type = UIQ_TYPE_WRITE_CXP, .id = 0x201, .reg_offs = BASLER_CXP12_IC_CXP_REG_CMD_DATA_1, .write_burst = 508},
    {.name = "control packet rx channel 2", .type = UIQ_TYPE_READ,      .id = 0x106, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 2",    .type = UIQ_TYPE_READ,      .id = 0x107, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 2",   .type = UIQ_TYPE_READ,      .id = 0x108, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 2", .type = UIQ_TYPE_WRITE_CXP, .id = 0x202, .reg_offs = BASLER_CXP12_IC_CXP_REG_CMD_DATA_2, .write_burst = 508},
    {.name = "control packet rx channel 3", .type = UIQ_TYPE_READ,      .id = 0x109, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "error event rx channel 3",    .type = UIQ_TYPE_READ,      .id = 0x10a, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "event packet rx channel 3",   .type = UIQ_TYPE_READ,      .id = 0x10b, .reg_offs = ME6_REG_IRQ_EVENT_DATA},
    {.name = "control packet tx channel 3", .type = UIQ_TYPE_WRITE_CXP, .id = 0x203, .reg_offs = BASLER_CXP12_IC_CXP_REG_CMD_DATA_3, .write_burst = 508},
};

messaging_dma_declaration basler_cxp12_ic_messaging_dma_declaration = {
    .control_register = 0x134,
    .init_register = 0x135,
    .num_buffers = 128
};