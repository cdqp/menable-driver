/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_BOARDS_PERIPHERAL_DECLARATION_H
#define LIB_BOARDS_PERIPHERAL_DECLARATION_H

#include <lib/controllers/i2c_master_core.h>
#include <lib/controllers/i2c_bus_controller.h>
#include <lib/controllers/spi_v2_controller.h>
#include <lib/uiq/uiq_defines.h>
#include <lib/controllers/bpi_controller.h>
#include <lib/controllers/jtag_controller.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Declares a single i2c bus.
 * There are usually multiple i2c busses on one i2c master core.
 */
struct men_i2c_bus_declaration {
    uint32_t id;
    const char * name;
    uint8_t bank_number;
    uint8_t bank_activation_bitmask;
    uint8_t write_enable_bitmask;
    uint32_t bus_frequency;
};

/**
 * Declares a i2c master core instance including all its busses.
 */
struct i2c_master_core_declaration {
    uint32_t address_register;
    uint32_t write_register;
    uint32_t read_register;
    uint32_t firmware_clock_frequency;
    uint32_t num_required_safety_writes;
    uint8_t bus_count;
    struct men_i2c_bus_declaration bus_declarations[8];
};

struct spi_v2a_declaration {
    uint32_t id;
    uint32_t control_register;
    uint32_t target_device;
};

typedef struct uiq_declaration {
    const char* name;
    uint8_t type;
    uint16_t id;
    uint16_t write_burst;
    uint32_t reg_offs;
} uiq_declaration;

struct spi_dual_declaration {
    uint32_t id;
    uint32_t control_register;
    uint32_t flash_select_register;
};

struct bpi_controller_declaration {
    uint32_t id;
    uint32_t address_register;
    uint32_t data_register;
    uint32_t bank_register;
    uint32_t address_width;
    uint32_t bank_width;
};

struct jtag_declaration {
    uint32_t id;
    uint32_t jtag_control_register;
    uint32_t devices_counts;
};

typedef struct _messaging_dma_declaration {
    uint32_t num_buffers;
    uint32_t control_register;
    uint32_t init_register;
} messaging_dma_declaration;

#ifdef __cplusplus
}
#endif

#endif
