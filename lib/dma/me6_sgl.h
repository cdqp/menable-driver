/************************************************************************
 * Copyright 2023-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_DMA_ME6_SGL_H_
#define LIB_DMA_ME6_SGL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lib/os/types.h"

/**
 * \brief Represents a single SGL block for mE6 SGLs.
 */
typedef struct me6_sgl_block {
    /**
     * Pointer to the next block, if this is not the last one.
     */
    uint64_t next;

    /**
     * Data words to hold the SGL block entries.
     * Note that entries have a very odd size in bits so that
     * they tend to start / end somewhere in the middle of a word.
     */
    uint64_t data[7];
} me6_sgl_block;

/**
 * \brief Sets a single entry inside an SGL block for a PC to Device SGL.
 * \param sgl Pointer to the SGL block struct.
 * \param entry_idx The index of the entry to set. Valid values are 0 to 4.
 * \param page_group_address The value for the page group address. The address is expected
 *                           as a whole 64 bit address. Any required adjustments are performed
 *                           inside this function.
 * \param page_group_size The page group size, as specified in the SGL specification document.
 * \param is_last_sgl_entry Flag to indicate whether or not this is the last entry in the SGL.
 * \return STATUS_OK on success or an error code otherwise.
 */
int men_me6sgl_dev2pc_set_block_entry(me6_sgl_block* sgl, uint8_t entry_idx, uint64_t page_group_address,
                                      uint32_t page_group_size, bool is_last_sgl_entry);

/**
 * \brief Sets a single entry inside an SGL block for a Device to PC SGL.
 * \param sgl Pointer to the SGL block struct.
 * \param entry_idx The index of the entry to set. Valid values are 0 to 4.
 * \param page_group_address The value for the page group address. The address is expected
 *                           as a whole 64 bit address. Any required adjustments are performed
 *                           inside this function.
 * \param page_group_size The page group size, as specified in the SGL specification document.
 * \return STATUS_OK on success or an error code otherwise.
 */
int men_me6sgl_pc2dev_set_block_entry(me6_sgl_block* sgl, uint8_t entry_idx, uint64_t page_group_address,
                                      uint32_t page_group_size);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // include guard
