/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#pragma once

#ifndef LIB_BOARDS_BOARDS_H
#define LIB_BOARDS_BOARDS_H

#include <sisoboards.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "peripheral_declaration.h"

unsigned int men_get_uiq_declaration(uint16_t device_id, uiq_declaration ** out_uiq_declaration);

void men_get_messaging_dma_declaration(uint16_t device_id, uint32_t pcieDsnLow, messaging_dma_declaration ** out_messaging_dma_declaration);

#ifdef __cplusplus
}
#endif

#endif // LIB_BOARDS_BOARDS_H
