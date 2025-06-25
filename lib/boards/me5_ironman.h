/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_BOARDS_ME5_IRONMAN_H
#define LIB_BOARDS_ME5_IRONMAN_H

#include "peripheral_declaration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ME5_IRONMAN_NUM_I2C_BUSSES 1
extern struct i2c_master_core_declaration me5_ironman_i2c_declaration;

extern struct spi_dual_declaration me5_ironman_spi_declaration;

extern struct jtag_declaration me5_ironman_jtag_declaration;

#ifdef __cplusplus
}
#endif

#endif