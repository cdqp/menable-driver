/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_BOARDS_ME5_MARATHON_H
#define LIB_BOARDS_ME5_MARATHON_H

#include "peripheral_declaration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ME5_ABACUS_NUM_I2C_BUSSES 2
extern struct i2c_master_core_declaration me5_abacus_i2c_declaration;

extern struct spi_v2a_declaration me5_abacus_spi_declaration;

#define ME5_ABACUS_REG_SPI_MODE 0x0900
#define ME5_ABACUS_REG_SLOT_ID 0x1008


#ifdef __cplusplus
}
#endif

#endif