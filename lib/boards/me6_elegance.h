/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_BOARDS_ME6_ELEGANCE_H
#define LIB_BOARDS_ME6_ELEGANCE_H

#include "peripheral_declaration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ME6_ELEGANCE_NUM_I2C_BUSSES 1
extern struct i2c_master_core_declaration me6_elegance_i2c_declaration;

#define ME6_ELEGANCE_NUM_UIQS 8
extern struct uiq_declaration me6_elegance_uiq_declaration[ME6_ELEGANCE_NUM_UIQS];

/* DSN Parts */
#define ME6_ELEGANCE_DSN_LOW_STATIC_VERSION_SHIFT 0
#define ME6_ELEGANCE_DSN_LOW_STATIC_VERSION_MASK 0x7ff

/* Feature Checks */
#define ME6_ELEGANCE_ECO_SUPPORTS_CAMERA_FRONTEND(dsn_low) (((dsn_low >> ME6_ELEGANCE_DSN_LOW_STATIC_VERSION_SHIFT) & ME6_ELEGANCE_DSN_LOW_STATIC_VERSION_MASK) > 4)

#ifdef __cplusplus
}
#endif

#endif