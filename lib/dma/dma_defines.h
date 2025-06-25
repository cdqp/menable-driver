/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_PCI_DMA_DEFINES_H_
#define LIB_PCI_DMA_DEFINES_H_

#include "lib/os/types.h"

typedef enum men_dma_direction {
    men_dma_direction_dev2pc,
    men_dma_direction_pc2dev
} men_dma_direction;

#define MEN_REG_DMA_TYPE_BIT_DEV2PC 0x1
#define MEN_REG_DMA_TYPE_BIT_PC2DEV 0x2

static bool men_dma_capability_matches_direction(uint32_t capability_bits, men_dma_direction direction) {
    uint32_t mask = (1 << (uint32_t)direction);
    return ((capability_bits & mask) != 0);
}

#endif // include guard