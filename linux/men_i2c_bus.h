/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef MEN_I2C_BUS_H_
#define MEN_I2C_BUS_H_

#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/i2c.h>

#include "lib/boards/peripheral_declaration.h"
#include "lib/controllers/i2c_master_core.h"
#include "lib/controllers/i2c_bus_controller.h"

struct men_i2c_bus {
    uint32_t id;
    struct i2c_bus_controller controller;
    struct i2c_adapter adapter;
    struct i2c_client * clients[8];
};

struct siso_menable;

extern const struct i2c_algorithm men_i2c_algorithm;

u32 men_i2c_func (struct i2c_adapter * adapter);

int men_i2c_master_xfer(struct i2c_adapter * adapter, struct i2c_msg *msgs, int num);

void men_i2c_cleanup_busses(struct men_i2c_bus * busses, int bus_count);

int men_init_i2c_peripherals(struct siso_menable * men,
                             struct i2c_master_core_declaration * declarations,
                             uint8_t num, struct i2c_master_core* i2c_cores,
                             struct men_i2c_bus* i2c_busses);

#endif /* MEN_I2C_BUS_H_ */
