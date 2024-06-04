/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_BOARDS_BASLER_IC_H
#define LIB_BOARDS_BASLER_IC_H

#include "peripheral_declaration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BASLER_CXP12_IC_1C_NUM_I2C_BUSSES 1
extern struct i2c_master_core_declaration basler_cxp12_ic_i2c_declaration;

#define BASLER_CXP12_IC_1C_NUM_UIQS 4
extern struct uiq_declaration basler_cxp12_ic_1c_uiq_declaration[BASLER_CXP12_IC_1C_NUM_UIQS];

#define BASLER_CXP12_IC_2C_NUM_UIQS 8
extern struct uiq_declaration basler_cxp12_ic_2c_uiq_declaration[BASLER_CXP12_IC_2C_NUM_UIQS];

#define BASLER_CXP12_IC_4C_NUM_UIQS 16
extern struct uiq_declaration basler_cxp12_ic_4c_uiq_declaration[BASLER_CXP12_IC_4C_NUM_UIQS];

extern messaging_dma_declaration basler_cxp12_ic_messaging_dma_declaration;

/* CXP data registers */
#define BASLER_CXP12_IC_CXP_REG_CMD_DATA_0 0x809
#define BASLER_CXP12_IC_CXP_REG_CMD_DATA_1 0x80b
#define BASLER_CXP12_IC_CXP_REG_CMD_DATA_2 0x80d
#define BASLER_CXP12_IC_CXP_REG_CMD_DATA_3 0x80f

/* DSN Parts */
#define BASLER_CXP12_IC_DSN_LOW_STATIC_MAJOR_VERSION_SHIFT 0
#define BASLER_CXP12_IC_DSN_LOW_STATIC_MAJOR_VERSION_MASK 0x7
#define BASLER_CXP12_IC_DSN_LOW_STATIC_MINOR_VERSION_SHIFT 11
#define BASLER_CXP12_IC_DSN_LOW_STATIC_MINOR_VERSION_MASK 0xf

#define BASLER_CXP12_IC_STATIC_VERSION_MAJOR(dsn_low) ((dsn_low >> BASLER_CXP12_IC_DSN_LOW_STATIC_MAJOR_VERSION_SHIFT) & BASLER_CXP12_IC_DSN_LOW_STATIC_MAJOR_VERSION_MASK)
#define BASLER_CXP12_IC_STATIC_VERSION_MINOR(dsn_low) ((dsn_low >> BASLER_CXP12_IC_DSN_LOW_STATIC_MINOR_VERSION_SHIFT) & BASLER_CXP12_IC_DSN_LOW_STATIC_MINOR_VERSION_MASK)

#define BASLER_CXP12_IC_IS_STATIC_VERSION_GREATER_EQUAL(dsn_low, major, minor) ( \
    (BASLER_CXP12_IC_STATIC_VERSION_MAJOR(dsn_low) > (major)) \
      || (BASLER_CXP12_IC_STATIC_VERSION_MAJOR(dsn_low) == (major) && (BASLER_CXP12_IC_STATIC_VERSION_MINOR(dsn_low)) >= (minor)) \
)

#define BASLER_CXP12_IC_IS_STATIC_VERSION_EQUAL(dsn_low, major, minor) ( \
    BASLER_CXP12_IC_STATIC_VERSION_MAJOR(dsn_low) == (major) && (BASLER_CXP12_IC_STATIC_VERSION_MINOR(dsn_low)) == (minor) \
)
/* Feature Checks */
/* TODO: [RKN] Remove FW 0.1 for release */
#define BASLER_CXP12_IC_1C_SUPPORTS_CAMERA_FRONTEND(dsn_low)    (BASLER_CXP12_IC_IS_STATIC_VERSION_GREATER_EQUAL(dsn_low, 1, 1) || BASLER_CXP12_IC_IS_STATIC_VERSION_EQUAL(dsn_low, 0, 1))
#define BASLER_CXP12_IC_1C_SUPPORTS_IDLE_VIOLATION_FIX(dsn_low) (BASLER_CXP12_IC_IS_STATIC_VERSION_GREATER_EQUAL(dsn_low, 1, 2) || BASLER_CXP12_IC_IS_STATIC_VERSION_EQUAL(dsn_low, 0, 1))
#define BASLER_CXP12_IC_1C_SUPPORTS_MESSAGING_DMA(dsn_low)      (BASLER_CXP12_IC_IS_STATIC_VERSION_GREATER_EQUAL(dsn_low, 1, 2) || BASLER_CXP12_IC_IS_STATIC_VERSION_EQUAL(dsn_low, 0, 1))

#ifdef __cplusplus
}
#endif

#endif
