/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef CRC32_H_
#define CRC32_H_

#include "../os/types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t crc32(void* data, uint32_t size, uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif /* CRC32_H_ */
