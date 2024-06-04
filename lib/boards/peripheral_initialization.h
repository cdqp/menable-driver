/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_BOARDS_PERIPHERAL_INITIALIZATION_H
#define LIB_BOARDS_PERIPHERAL_INITIALIZATION_H

#include "peripheral_declaration.h"

#include <lib/controllers/i2c_master_core.h>
#include <lib/controllers/i2c_bus_controller.h>
#include <lib/controllers/spi_v2_controller.h>
#include <lib/controllers/spi_dual_controller.h>

#ifdef __cplusplus
extern "C" {
#endif

int men_init_i2c_master_core(struct i2c_master_core * master_core,
                             struct i2c_bus_controller ** i2c_busses,
                             struct register_interface * reg_interface,
                             user_mode_lock * lock,
                             struct i2c_master_core_declaration * decl);

#ifdef __cplusplus
}
#endif

#endif