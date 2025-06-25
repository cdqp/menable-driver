/************************************************************************
 * Copyright 2023-2025 Basler AG
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

typedef struct men_me6sgl_block_field men_me6sgl_block_field;

typedef struct men_me6sgl {
    me6_sgl_block* blocks;
    size_t num_blocks;
    uint64_t first_block_bus_address;

    uint16_t bits_in_last_transfer_size_field;

    void (*get_entry_fields_with_values)(struct men_me6sgl* self, uint64_t page_group_address, uint32_t page_group_size,
                          bool is_last_sgl_entry, men_me6sgl_block_field** out_fields, size_t* out_num_fields);

    size_t (*fill_blocks)(struct men_me6sgl* self, size_t entry_count_offset, size_t batch_length, void* virt_address, bool is_last_batch);

    void (*create_for_dummy_buffer)(struct men_me6sgl* self, void* dummy_page_address);

    men_me6sgl_block_field* fields;
    uint32_t num_fields;
    uint32_t max_pci_transfer_size;
    uint32_t start_bit_of_first_block_entry;

} men_me6sgl;

int men_me6sgl_init_dev2pc(men_me6sgl* sgl, me6_sgl_block* block_memory, size_t num_blocks,
                           uint32_t max_pci_transfer_size);

int men_me6sgl_init_pc2dev(men_me6sgl* sgl, me6_sgl_block* block_memory, size_t num_blocks,
                           uint32_t max_pci_transfer_size);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // include guard
