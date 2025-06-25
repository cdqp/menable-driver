/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_CONTROLLERS_I2C_BUS_CONTROLLER_H_
#define LIB_CONTROLLERS_I2C_BUS_CONTROLLER_H_

#include "controller_base.h"
#include "i2c_master_core.h"

#ifdef __cplusplus 
extern "C" {
#endif

/**
 * Represents a single bus on a i2c master core.
 * This is merely a wrapper around a i2c_master_core to hide the bank switching.
 * So for one i2c_master_core instance there can reasonably be up to 8 i2c_bus_controller instances,
 * namely one per bank.
 */
struct i2c_bus_controller {
    DERIVE_FROM(controller_base);

    struct i2c_master_core * i2c_core;
    uint8_t bank_number;
};

int i2c_bus_controller_init(struct i2c_bus_controller * ctrl, struct i2c_master_core * i2c_core, uint8_t bank_number);


#ifdef __cplusplus 
} // extern "C"
#endif

#endif /* LIB_CONTROLLERS_I2C_BUS_CONTROLLER_H_ */
