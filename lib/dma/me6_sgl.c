/************************************************************************
 * Copyright 2023-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "me6_sgl.h"

#ifdef DBG_ME6_SGL
#  undef MEN_DEBUG
#  define MEN_DEBUG
#endif

#ifdef TRACE_ME6_SGL
#  define DBG_TRACE_ON
#endif

#include <lib/helpers/dbg.h>

#include <lib/helpers/error_handling.h>
#include <lib/helpers/bits.h>
#include <lib/helpers/helper.h>
#include <lib/helpers/memory.h>

#define ME6_PAGES_PER_SGL_BLOCK 5

#ifndef PAGE_SIZE
#  define PAGE_SIZE 4096
#endif

#define LOG_PREFIX KBUILD_MODNAME ":[ME6SGL]: "

#define MEN_ME6SGL_DEV2PC_FIELD_SIZE_IS_LAST            1
#define MEN_ME6SGL_DEV2PC_FIELD_SIZE_PAGE_GROUP_ADDRESS 62
#define MEN_ME6SGL_DEV2PC_FIELD_SIZE_PAGE_GROUP_SIZE    23

#define MEN_ME6SGL_PC2DEV_FIELD_SIZE_PAGE_GROUP_ADDRESS 62
#define MEN_ME6SGL_PC2DEV_FIELD_SIZE_PAGE_GROUP_SIZE    25

/**
 * \brief Represents a field in an SGL block entry.
 *        Used to specify the fields for an entry in a generic way.
 */
typedef struct men_me6sgl_block_field {
    /**
     * \brief The number of bits for the field.
     */
    size_t numBits;

    /**
     * \brief The value for the field.
     *        Only the lowest `numBits` bits are taken.
     */
    uint64_t value;
} men_me6sgl_block_field;

/**
 * \brief Sets the leftmost bits from `source` at the specified position in `target`
 */
static void set_bits(uint64_t* target, uint64_t source, size_t target_start_bit_idx, size_t target_end_bit_idx) {
    SET_BITS_64(*target, source, target_start_bit_idx, target_end_bit_idx);
}

/**
 * \brief Sets a single field for an entry in an SGL block.
 * \param sgl Pointer to the SGL block struct.
 * \param fieldStartBitIdx Global index of the bit, where the field starts.
 *                         This is relative to the beginning of the entries section of the SGL block,
 *                         counting every bit of all data words.
 * \param fieldSizeInBits The size of the field in bits. The function will not touch any bits outside
 *                        the interval [fieldStartBitIdx, fieldStartBitIdx + fieldSizeInBits - 1]
 * \param fieldValue The value to set. Bits are taken from index 0 to `fieldSizeInBits`. Any bits
 *                   beyond that will be ignored.
 */
static void men_set_sgl_block_field(me6_sgl_block* sgl, size_t fieldStartBitIdx, size_t fieldSizeInBits,
                                    uint64_t fieldValue) {
    static const size_t bitsPerArrayItem = BITS_PER_BYTE * sizeof(sgl->data[0]);

    size_t arrayItemIdx = fieldStartBitIdx / bitsPerArrayItem;
    size_t startBitInArrayItem = fieldStartBitIdx % bitsPerArrayItem;

    size_t remainingBits = fieldSizeInBits;
    while (remainingBits > 0) {
        const size_t bitsInChunk = MIN(remainingBits, bitsPerArrayItem - startBitInArrayItem);
        const size_t endBitInArrayItem = startBitInArrayItem + bitsInChunk - 1;

        uint64_t* const targetArrayItem = &sgl->data[arrayItemIdx];
        set_bits(targetArrayItem, fieldValue, startBitInArrayItem, endBitInArrayItem);

        /* update loop variables */
        remainingBits -= bitsInChunk;
        arrayItemIdx += 1;
        startBitInArrayItem = 0;
        fieldValue >>= bitsInChunk;
    }
}

static void set_block_entry(me6_sgl_block* sgl, uint8_t entry_idx, size_t entry_size,
                                       const men_me6sgl_block_field* fields, size_t numFields,
                                       size_t first_entry_start_bit_idx) {
    size_t fieldStartBitIdx = first_entry_start_bit_idx + (entry_idx * entry_size);
    for (size_t fieldIdx = 0; fieldIdx < numFields; ++fieldIdx) {
        const men_me6sgl_block_field* sglBlockField = &fields[fieldIdx];

        const size_t numBits = sglBlockField->numBits;
        const uint64_t value = sglBlockField->value;

        men_set_sgl_block_field(sgl, fieldStartBitIdx, numBits, value);

        /* update for next iteration */
        fieldStartBitIdx += numBits;
    }
}

uint32_t men_generate_page_group_size(const uint64_t offset_in_page, const uint32_t data_size_in_bytes,
                                      const uint32_t num_bytes_per_pci_transfer, const uint16_t bits_in_last_transfer_size_field) {

    const uint32_t bits_in_num_transfers_field = 15;

    const uint32_t offset_in_pci_chunk = (offset_in_page % num_bytes_per_pci_transfer);
    const uint32_t bytes_in_first_chunk = num_bytes_per_pci_transfer - offset_in_pci_chunk;

    uint32_t num_transfers;
    uint32_t num_bytes_in_last_transfer;

    if (data_size_in_bytes <= bytes_in_first_chunk) {
        /* only one chunk to transfer */
        num_transfers = 1;
        num_bytes_in_last_transfer = data_size_in_bytes;
    }
    else {
        const uint32_t num_bytes_beyond_first_chunk = data_size_in_bytes - bytes_in_first_chunk;

        num_transfers = 1 + CEIL_DIV(num_bytes_beyond_first_chunk, num_bytes_per_pci_transfer);

        num_bytes_in_last_transfer = num_bytes_beyond_first_chunk % num_bytes_per_pci_transfer;
        if (num_bytes_in_last_transfer == 0) {
            num_bytes_in_last_transfer = num_bytes_per_pci_transfer;
        }
    }

    // TODO: what if `num_bytes_in_last_chunk` is no multiple of 4? Is that an error?
    const uint32_t num_32bit_words_in_last_transfer = num_bytes_in_last_transfer / 4;

    const uint32_t max_num_transfers = (1 << bits_in_num_transfers_field);
    const uint32_t max_last_transfer_size = (1 << bits_in_last_transfer_size_field);
    if (num_transfers > max_num_transfers) {
        pr_err(LOG_PREFIX "Invalid SGL entry block size. Maximum of %u transfers of %u bytes each.\n",
               max_num_transfers, num_bytes_per_pci_transfer);
    }

    if (num_32bit_words_in_last_transfer > max_last_transfer_size) {
        pr_err(LOG_PREFIX "Invalid SGL entry block size. Maximum of %u 32 bit words in last transfer, but got %u.\n",
               max_last_transfer_size, num_32bit_words_in_last_transfer);
    }
    uint32_t size_value = 0;
    const uint32_t start_bit_for_num_transfers = 0;
    const uint32_t end_bit_for_num_transfers = bits_in_last_transfer_size_field - 1;
    const uint32_t start_bit_for_last_transfer_size = bits_in_num_transfers_field;
    const uint32_t end_bit_for_last_transfer_size = start_bit_for_last_transfer_size + bits_in_last_transfer_size_field - 1;

    SET_BITS_32(size_value, num_transfers, start_bit_for_num_transfers, end_bit_for_num_transfers);
    SET_BITS_32(size_value, num_32bit_words_in_last_transfer, start_bit_for_last_transfer_size,
                end_bit_for_last_transfer_size);

    return size_value;
}

static void link_block_to_next(me6_sgl_block* block, uint64_t bus_address_of_next_block) {
    static const uint64_t valid_flag = 0x1;
    block->next = (bus_address_of_next_block >> 1) | valid_flag;
}

static void create_link_in_previous_block(men_me6sgl* sgl, const size_t block_idx) {
    me6_sgl_block* current_sgl_block = &sgl->blocks[block_idx];
    uint64_t current_sgl_block_addr = get_bus_address(current_sgl_block);
    me6_sgl_block* previous_sgl_block = &sgl->blocks[block_idx - 1];

    link_block_to_next(previous_sgl_block, current_sgl_block_addr);
}

static void init_block_and_create_link_from_previous(men_me6sgl* sgl, const size_t block_index) {
    me6_sgl_block* block = &sgl->blocks[block_index];

    fill_mem(block, sizeof(*block), 0);

    if (block_index > 0) {
        create_link_in_previous_block(sgl, block_index);
    }

}

static size_t prepare_sgl_block(men_me6sgl* sgl, char* buffer_chunk_ptr, const uint64_t buffer_chunk_bus_address,
                                size_t remaining_length, const uint32_t pci_payload_size) {
    const size_t max_pci_transfers_per_sgl_entry = (1 << 15);

    // length until end of buffer or end of page
    const size_t page_mask = (PAGE_SIZE - 1);
    const size_t page_offset = (buffer_chunk_bus_address & page_mask);

    // The first pci packet is only filled from addr to the next payload size boundary, so we have to
    // reduce the maximum size accordingly. See internal documentation 'DmaSystemSpecification.pdf' for details.
    const size_t pci_payload_offset = (buffer_chunk_bus_address % pci_payload_size);
    const size_t max_bytes_for_sgl_entry = (max_pci_transfers_per_sgl_entry * pci_payload_size) - pci_payload_offset;
    const size_t max_length = MIN(remaining_length, max_bytes_for_sgl_entry);

    size_t entry_length = MIN(PAGE_SIZE - page_offset, max_length);

    pr_debug(LOG_PREFIX "entry_length: %zu, max_length: %zu\n", entry_length, max_length);

    // Try to join contiguous blocks
#if 0 // The joining of multiple pages is not tested sufficiently
    if (entry_length < max_length) {
        uint64_t current_chunk_address = buffer_chunk_bus_address;

        char* next_buffer_chunk_ptr = buffer_chunk_ptr + entry_length;
        uint64_t next_chunk_address = get_bus_address(next_buffer_chunk_ptr);

        const uint64_t PAGE_ADDRESS_MASK = ~((uint64_t)0xfff);
        while (entry_length < max_length &&
               next_chunk_address == ((current_chunk_address & PAGE_ADDRESS_MASK) + PAGE_SIZE)) {

            entry_length = MIN(entry_length + PAGE_SIZE, max_length);
            pr_debug(LOG_PREFIX "Joining page @0x%016llx Total length: %zu\n", next_chunk_address, entry_length);

            /* loop variable updates */
            current_chunk_address = next_chunk_address;
            next_buffer_chunk_ptr = buffer_chunk_ptr + entry_length;
            next_chunk_address = get_bus_address(next_buffer_chunk_ptr);
        }
    }
#endif

    return entry_length;
}

static size_t fill_blocks(men_me6sgl* self, size_t entry_count_offset, size_t batch_length, void* virt_address,
                          bool is_last_batch) {

    pr_debug(LOG_PREFIX "CreateSgl [entry_count_offset=%zu, batch_lengh=%zu, payload_size=%u, last=%s].\n",
             entry_count_offset, batch_length, self->max_pci_transfer_size, is_last_batch ? "true" : "false");

    size_t current_entry_number = entry_count_offset;
    char* buffer_chunk_ptr = (char*)virt_address;
    char* buffer_end_ptr = buffer_chunk_ptr + batch_length;
    size_t remaining_length = batch_length;

    while (buffer_chunk_ptr < buffer_end_ptr) {
        const size_t block_sgl_idx = current_entry_number / ME6_PAGES_PER_SGL_BLOCK;
        const uint8_t block_entry_idx = current_entry_number % ME6_PAGES_PER_SGL_BLOCK;

#if defined(DBG_ME6_SGL)
        pr_debug(LOG_PREFIX "block_sgl_idx = %zu, block_entry_idx = %d, remaining_length = %zu\n", block_sgl_idx,
                 block_entry_idx, remaining_length);
#endif

        me6_sgl_block* current_sgl_block = &self->blocks[block_sgl_idx];

        if (block_entry_idx == 0) {
            init_block_and_create_link_from_previous(self, block_sgl_idx);
        }

        const uint64_t buffer_chunk_bus_address = get_bus_address(buffer_chunk_ptr);

        size_t entry_length = prepare_sgl_block(self, buffer_chunk_ptr, buffer_chunk_bus_address, remaining_length,
                                                self->max_pci_transfer_size);

        remaining_length -= entry_length;
        buffer_chunk_ptr += entry_length;

#if defined(DBG_ME6_SGL)
        pr_debug(LOG_PREFIX "entry %04zu @0x%llx: address %016llx, length %08zx\n", current_entry_number,
                 get_bus_address(current_sgl_block), buffer_chunk_bus_address, entry_length);
#endif

        const bool is_last_entry = (is_last_batch == true) && (remaining_length == 0);

        const uint32_t size_value =
            men_generate_page_group_size(buffer_chunk_bus_address & 0xfff, (uint32_t)entry_length,
                                       self->max_pci_transfer_size, self->bits_in_last_transfer_size_field);


        men_me6sgl_block_field* fields;
        size_t num_fields;
        self->get_entry_fields_with_values(self, buffer_chunk_bus_address, size_value, is_last_entry, &fields, &num_fields);

        size_t block_entry_length = 0;
        for (size_t i = 0; i < num_fields; ++i) {
            block_entry_length += fields[i].numBits;
        } 

        set_block_entry(current_sgl_block, block_entry_idx, block_entry_length, fields, num_fields,
                        self->start_bit_of_first_block_entry);

        ++current_entry_number;
    }

    pr_debug(LOG_PREFIX "Return block idx %zu.\n", current_entry_number);
    return current_entry_number;
}

static void create_for_dummy_buffer(men_me6sgl* self, void* dummy_page_address) {
    /* let all entries point to the same page */
    for (size_t block_index = 0; block_index < ARRAY_SIZE(self->blocks->data); ++block_index) {
        self->fill_blocks(self, block_index, PAGE_SIZE, dummy_page_address, false);
    }

    /* let the block point to itself as the next block */
    link_block_to_next(&self->blocks[0], self->first_block_bus_address);
}

static void get_fields_with_values_dev2pc(men_me6sgl* self, uint64_t page_group_address, uint32_t page_group_size,
                                          bool is_last_sgl_entry, men_me6sgl_block_field** out_fields,
                                          size_t* out_num_fields) {

    static men_me6sgl_block_field fields[3] = {
        { MEN_ME6SGL_DEV2PC_FIELD_SIZE_IS_LAST, 0},
        { MEN_ME6SGL_DEV2PC_FIELD_SIZE_PAGE_GROUP_ADDRESS, 0},
        { MEN_ME6SGL_DEV2PC_FIELD_SIZE_PAGE_GROUP_SIZE, 0}
    };

    fields[0].value = (is_last_sgl_entry ? 1 : 0);
    fields[1].value = (page_group_address >> 2);
    fields[2].value = page_group_size;

    *out_fields = fields;
    *out_num_fields = ARRAY_SIZE(fields);
}

static void get_entry_fields_with_values_pc2dev(men_me6sgl* self, uint64_t page_group_address, uint32_t page_group_size,
                                                bool _unused, men_me6sgl_block_field** out_fields,
                                                size_t* out_num_fields) {

    static men_me6sgl_block_field fields[] = {
        {MEN_ME6SGL_PC2DEV_FIELD_SIZE_PAGE_GROUP_ADDRESS, 0},
        {MEN_ME6SGL_PC2DEV_FIELD_SIZE_PAGE_GROUP_SIZE,    0}
    };

    fields[0].value = (page_group_address >> 2);
    fields[1].value = page_group_size;

    *out_fields = fields;
    *out_num_fields = ARRAY_SIZE(fields);
}

static void init_common_members(men_me6sgl* self, me6_sgl_block* block_memory, size_t num_blocks,
                                uint32_t max_pci_transfer_size) {
    self->blocks = block_memory;
    self->num_blocks = num_blocks;
    self->first_block_bus_address = get_bus_address(block_memory);
    self->max_pci_transfer_size = max_pci_transfer_size;

    self->fill_blocks = fill_blocks;
    self->create_for_dummy_buffer = create_for_dummy_buffer;
}

int men_me6sgl_init_dev2pc(men_me6sgl* sgl, me6_sgl_block* block_memory, size_t num_blocks,
                           uint32_t max_pci_transfer_size) {

    init_common_members(sgl, block_memory, num_blocks, max_pci_transfer_size);
    sgl->bits_in_last_transfer_size_field = 8;
    sgl->start_bit_of_first_block_entry = 18;
    sgl->get_entry_fields_with_values = get_fields_with_values_dev2pc;

    return STATUS_OK;
}

int men_me6sgl_init_pc2dev(men_me6sgl* sgl, me6_sgl_block* block_memory, size_t num_blocks,
                           uint32_t max_pci_transfer_size) {

    init_common_members(sgl, block_memory, num_blocks, max_pci_transfer_size);
    sgl->bits_in_last_transfer_size_field = 10;
    sgl->start_bit_of_first_block_entry = 13;
    sgl->get_entry_fields_with_values = get_entry_fields_with_values_pc2dev;

    return STATUS_OK;
}