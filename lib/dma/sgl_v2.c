/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "sgl_v2.h"

#include <lib/helpers/error_handling.h>
#include <lib/helpers/helper.h>
#include <lib/helpers/bits.h>
#include <lib/os/assert.h>

#define SGL_ENTRIES_PER_BLOCK 5

#define SGL_BlOCK_FIRST_ENTRY_OFFSET 18
#define SGL_BLOCK_ENTRY_BITS 86
#define SGL_BLOCK_ENTRY_PGA_OFFSET 1
#define SGL_BLOCK_ENTRY_PGA_LENGTH 62
#define SGL_BLOCK_ENTRY_GRPSIZE_OFFSET (SGL_BLOCK_ENTRY_PGA_OFFSET + SGL_BLOCK_ENTRY_PGA_LENGTH)
#define SGL_BLOCK_ENTRY_GRPSIZE_LENGTH 23

#define PCI_EXPRESS_PAGE_SIZE 0x1000

/**
 * Sets a value to a specific bit range witin a memory region.
 */
static int set_bits(uint64_t * target_buf, uint32_t from, uint32_t to, uint64_t value) {
    const uint8_t bits_per_chunk = sizeof(*target_buf) * 8;
    uint8_t num_bits = to + 1 - from;

    // ensure that there are no bits set in value that do not fit into the target range.
    if (GET_BITS_64(value, num_bits, bits_per_chunk - 1) != 0) {
        return STATUS_ERROR;
    }

    while (to >= from) {
        uint8_t target_idx = from / bits_per_chunk;
        uint32_t chunk_offset = target_idx * bits_per_chunk;
        uint8_t chunk_from = from % bits_per_chunk;
        uint8_t chunk_to = MIN(bits_per_chunk - 1, to - chunk_offset);
        uint8_t bits_in_current_chunk = chunk_to + 1 - chunk_from;

        // clear the bits in the target word
        target_buf[target_idx] &= ~GEN_MASK_64(chunk_from, chunk_to);

        // write the bits for the chunk.
        target_buf[target_idx] |= (GET_BITS_64(value, 0, bits_in_current_chunk) << chunk_from);
        
        value >>= bits_in_current_chunk;
        from += bits_in_current_chunk;
    }

    return STATUS_OK;
}

static int set_sgl_block_entry(struct men_sgl_v2_block * block, uint8_t entry_idx, uint64_t page_group_address, uint64_t group_size, bool is_last) {
    const uint32_t entry_offset = SGL_BlOCK_FIRST_ENTRY_OFFSET + (entry_idx * SGL_BLOCK_ENTRY_BITS);
    
    const uint32_t pga_from = entry_offset + SGL_BLOCK_ENTRY_PGA_OFFSET;
    const uint32_t pga_to = pga_from + SGL_BLOCK_ENTRY_PGA_LENGTH - 1;

    const uint32_t grpsize_from = pga_to + 1;
    const uint32_t grpsize_to = grpsize_from + SGL_BLOCK_ENTRY_GRPSIZE_LENGTH - 1;

    set_bits(block->data, entry_offset, entry_offset, is_last ? 1 : 0);
    set_bits(block->data, pga_from, pga_to, page_group_address);
    set_bits(block->data, grpsize_from, grpsize_to, group_size);

    return STATUS_OK;
}

static uint32_t compute_group_size(uint64_t phys_addr, uint32_t chunk_size, uint32_t pci_payload_size) {
        // the size of the chunk + the offset in the first payload block
        uint32_t aligned_chunk_size = chunk_size + (phys_addr & ((uint64_t)pci_payload_size - 1)); 
        uint32_t payload_count = CEIL_DIV(aligned_chunk_size, pci_payload_size) & GEN_MASK_16(0, 14);
        uint32_t last_payload_size = aligned_chunk_size % pci_payload_size;
        if (last_payload_size == 0) {
            last_payload_size += pci_payload_size;
        }
        last_payload_size = (last_payload_size / 4) & GEN_MASK_8(0, 7);
        return payload_count | (last_payload_size << 15);
}

int men_create_sgl_v2(struct men_dma_mem_descriptor * mem_descriptors, uint32_t num_mem_descriptors, uint32_t pci_payload_size,
                      struct men_sgl_v2_block * sgl_blocks, uint64_t * sgl_block_phys_addresses, uint32_t num_sgl_blocks) {
    
    const uint32_t max_chunk_size = (((uint32_t)1) << 15) * pci_payload_size;

    int current_sgl_block = 0;
    int current_sgl_block_entry = 0;

    for (uint32_t i = 0; i < num_mem_descriptors; ++i) {

        if (current_sgl_block_entry == 0 && current_sgl_block > 0) {
            sgl_blocks[current_sgl_block - 1].next_block_ptr = (sgl_block_phys_addresses[current_sgl_block] >> 1) | 1;
        }

        struct men_dma_mem_descriptor * current_descriptor = &mem_descriptors[i];
        uint64_t phys_addr = current_descriptor->physical_address;
        uint64_t chunk_size = current_descriptor->length;

        assert_msg(chunk_size + (phys_addr & 0xFFF) <= 0x1000, "Memory chunk may not exceed a page.");

        /* TODO: This can be optimized further. When the max_chunk_size would be exceeded
         *       by the next descriptor's length, we could take as much as possible from 
         *       that length instead of finishing the current entry.
         */
        for (int j = i + 1;
             j < num_mem_descriptors
                && mem_descriptors[j].physical_address == phys_addr + chunk_size
                && phys_addr + chunk_size > phys_addr // after a wrap around, the page is NOT adjacent!
                && chunk_size + mem_descriptors[j].length <= max_chunk_size;
             ++j, ++i) {
            chunk_size += mem_descriptors[j].length;
        }

        // after merging adjacent pages, update the current descriptor
        // to the last one that was merged.
        current_descriptor = &mem_descriptors[i];

        uint64_t page_group_address = phys_addr >> 2;
        uint64_t page_group_size = compute_group_size(phys_addr, chunk_size, pci_payload_size);

        /* 
         * For is_last we assume that the whole descriptor->length was handled with one SGL block entry.
         * This could theoretically be untrue, if the memory of the last descriptor is larger than max_chunk_size.
         * However, this seems a very theoretical problem, since each descriptor should usually have 
         * a maximum length of a page (4K).
         */
        bool is_last = (i == num_mem_descriptors - 1);
        set_sgl_block_entry(&sgl_blocks[current_sgl_block], current_sgl_block_entry, page_group_address, page_group_size, is_last);

        current_sgl_block_entry = (current_sgl_block_entry + 1) % SGL_ENTRIES_PER_BLOCK;
        if (current_sgl_block_entry == 0)
            current_sgl_block += 1;
    }

    return current_sgl_block + 1;
}
