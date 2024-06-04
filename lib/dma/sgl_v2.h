/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_DMA_SGL_V2_H_
#define LIB_DMA_SGL_V2_H_

#include <lib/os/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Descriptor for a chunk of DMA memory.
 */
struct men_dma_mem_descriptor {
    /**
     * The pyhscial address of the memory chunk.
     * 
     * @attention It has to be ensured that the memory can not be paged out during DMA operations!
     */
    uint64_t physical_address;

    /**
     * The length of the memory chunk in bytes.
     * 
     * @attebtion The length may not exceed the page size!
     */
    uint32_t length;
};

/* TODO: Do we need to mark this to be packed? */
struct men_sgl_v2_block {
    uint64_t next_block_ptr;
    uint64_t data[7];
};

/**
 * creates the SGL (v2) for a given array of memory descriptors.
 * 
 * @param mem_descriptors An array of memory descriptors that represent the memory
 *                        to be managed by the SGL.
 * 
 * @param num_mem_descriptors
 *        The number of memory descriptors in the array.
 * 
 * @param pci_payload_size
 *        The trained pci payload size in bytes.
 * 
 * @param sgl_blocks
 *        An array of SGL blocks where the SGL should be created.
 *        The caller has to ensure that the array is large enough
 *        for the memory to be managed by the SGL.
 * 
 * @param sgl_block_phys_addresses
 *        The physical addresses of the SGL blocks.
 *        This is required to set the next poiner for the SGL blocks.
 * 
 * @param num_sgl_blocks
 *        The number of SGL blocks in the array.
 * 
 * @return The number of SGL blocks that are actually used 
 *         or an error code < 0 if the SGL can not be created.
 */
int men_create_sgl_v2(struct men_dma_mem_descriptor * mem_descriptors, uint32_t num_mem_descriptors, uint32_t pci_payload_size,
                      struct men_sgl_v2_block * sgl_blocks, uint64_t * sgl_block_phys_addresses, uint32_t num_sgl_blocks);

#ifdef __cplusplus
}
#endif

#endif