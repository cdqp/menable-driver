/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_BOARDS_ME6_IMPULSE_H
#define LIB_BOARDS_ME6_IMPULSE_H

#include "peripheral_declaration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ME6_IMPULSE_NUM_I2C_BUSSES 2
extern struct i2c_master_core_declaration me6_impulse_i2c_declaration;

extern struct spi_v2a_declaration me6_impulse_spi_declaration;

extern messaging_dma_declaration me6_impulse_messaging_dma_declaration;

#define ME6_IMPULSE_CXP_NUM_UIQS 16
extern struct uiq_declaration me6_impulse_cxp_uiq_declaration[ME6_IMPULSE_CXP_NUM_UIQS];

#define ME6_IMPULSE5_CXP_NUM_UIQS 20
extern struct uiq_declaration me6_impulse5_cxp_uiq_declaration[ME6_IMPULSE5_CXP_NUM_UIQS];

extern struct jtag_declaration me6_impulse_jtag_declaration;
extern struct jtag_declaration me6_impulse_jtag_declaration;
/* CXP data registers */
#define ME6_IMPULSE_CXP_REG_CMD_DATA_0 0x809
#define ME6_IMPULSE_CXP_REG_CMD_DATA_1 0x80b
#define ME6_IMPULSE_CXP_REG_CMD_DATA_2 0x80d
#define ME6_IMPULSE_CXP_REG_CMD_DATA_3 0x80f
#define ME6_IMPULSE_CXP_REG_CMD_DATA_4 0x819

/* DSN Parts */
#define ME6_IMPULSE_DSN_LOW_STATIC_MAJOR_VERSION_SHIFT 0
#define ME6_IMPULSE_DSN_LOW_STATIC_MAJOR_VERSION_MASK 0x7
#define ME6_IMPULSE_DSN_LOW_STATIC_MINOR_VERSION_SHIFT 11
#define ME6_IMPULSE_DSN_LOW_STATIC_MINOR_VERSION_MASK 0xf

#define ME6_IMPULSE_STATIC_VERSION_MAJOR(dsn_low) ((dsn_low >> ME6_IMPULSE_DSN_LOW_STATIC_MAJOR_VERSION_SHIFT) & ME6_IMPULSE_DSN_LOW_STATIC_MAJOR_VERSION_MASK)
#define ME6_IMPULSE_STATIC_VERSION_MINOR(dsn_low) ((dsn_low >> ME6_IMPULSE_DSN_LOW_STATIC_MINOR_VERSION_SHIFT) & ME6_IMPULSE_DSN_LOW_STATIC_MINOR_VERSION_MASK)

#define ME6_IMPULSE_IS_STATIC_VERSION_GREATER_EQUAL(dsn_low, major, minor) ( \
    (ME6_IMPULSE_STATIC_VERSION_MAJOR(dsn_low) > (major)) \
      || (ME6_IMPULSE_STATIC_VERSION_MAJOR(dsn_low) == (major) && (ME6_IMPULSE_STATIC_VERSION_MINOR(dsn_low)) >= (minor)) \
)

/* Feature Checks - CXP only*/
#define ME6_IMPULSE_CXP_SUPPORTS_IDLE_VIOLATION_FIX(dsn_low) ME6_IMPULSE_IS_STATIC_VERSION_GREATER_EQUAL(dsn_low, 0, 1)

#ifdef __cplusplus
}
#endif

#endif