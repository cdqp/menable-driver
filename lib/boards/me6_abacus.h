/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
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

#define ME6_ABACUS_NUM_I2C_BUSSES 2
extern struct i2c_master_core_declaration me6_abacus_i2c_declaration;

extern struct spi_v2a_declaration me6_abacus_spi_declaration;

#define ME6_ABACUS_NUM_UIQS 18
extern struct uiq_declaration me6_abacus_uiq_declaration[ME6_ABACUS_NUM_UIQS];

#define ME6_ABACUS_REG_SPI_MODE 0x0900
#define ME6_ABACUS_REG_SPI_DATA_OUT 0x0901

#define ME6_ABACUS_REG_SPI_DISABLE_PIC_INTERFACE 0x0902
#define ME6_ABACUS_ENABLE_PIC_INTERFACE 0x0
#define ME6_ABACUS_DISABLE_PIC_INTERFACE 0x1

#define ME6_ABACUS_REG_SLOT_ID 0x1008

#ifdef __cplusplus
}
#endif

#endif