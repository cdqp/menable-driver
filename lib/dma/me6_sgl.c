/************************************************************************
 * Copyright 2023-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "me6_sgl.h"

#include <lib/helpers/error_handling.h>
#include <lib/helpers/bits.h>
#include <lib/helpers/helper.h>

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

/**
 * \brief Represents a field in an SGL block entry.
 *        Used to specify the fields for an entry in a generic way.
 */
typedef struct {
    /**
     * \brief The number of bits for the field.
     */
    size_t numBits;

    /**
     * \brief The value for the field.
     *        Only the lowest `numBits` bits are taken.
     */
    uint64_t value;
} SglBlockField;

static void men_me6sgl_set_block_entry(me6_sgl_block* sgl, uint8_t entry_idx, size_t entry_size,
                                       const SglBlockField* fields, size_t numFields,
                                       size_t first_entry_start_bit_idx) {
    size_t fieldStartBitIdx = first_entry_start_bit_idx + (entry_idx * entry_size);
    for (size_t fieldIdx = 0; fieldIdx < numFields; ++fieldIdx) {
        const SglBlockField* sglBlockField = &fields[fieldIdx];

        const size_t numBits = sglBlockField->numBits;
        const uint64_t value = sglBlockField->value;

        men_set_sgl_block_field(sgl, fieldStartBitIdx, numBits, value);

        /* update for next iteration */
        fieldStartBitIdx += numBits;
    }
}

int men_me6sgl_dev2pc_set_block_entry(me6_sgl_block* sgl, uint8_t entry_idx, uint64_t page_group_address,
                                      uint32_t page_group_size, bool is_last_sgl_entry) {
    /* `entry_idx` must be in [0,4] */
    if (entry_idx > 4) {
        return STATUS_ERR_INVALID_ARGUMENT;
    }

    /* The lowest 2 bits of `page_group_address` must be zero */
    if ((page_group_address & 0x3) != 0) {
        return STATUS_ERR_INVALID_ARGUMENT;
    }

    static const size_t fieldSize_isLast = 1;
    static const size_t fieldSize_pageGroupAddress = 62;
    static const size_t fieldSize_pageGroupSize = 23;

    const size_t entrySize = fieldSize_isLast + fieldSize_pageGroupAddress + fieldSize_pageGroupSize;

    const SglBlockField sglBlockFields[] = {
        {fieldSize_isLast,           (is_last_sgl_entry ? 1 : 0)},
        {fieldSize_pageGroupAddress, (page_group_address >> 2)  },
        {fieldSize_pageGroupSize,    page_group_size            }
    };

    static const size_t firstEntryOffset = 18;
    men_me6sgl_set_block_entry(sgl, entry_idx, entrySize, sglBlockFields, ARRAY_SIZE(sglBlockFields), firstEntryOffset);

    return STATUS_OK;
}

int men_me6sgl_pc2dev_set_block_entry(me6_sgl_block* sgl, uint8_t entry_idx, uint64_t page_group_address,
                                      uint32_t page_group_size) {
    /* `entry_idx` must be in [0,4] */
    if (entry_idx > 4) {
        return STATUS_ERR_INVALID_ARGUMENT;
    }

    /* The lowest 2 bits of `address` must be zero */
    if ((page_group_address & 0x3) != 0) {
        return STATUS_ERR_INVALID_ARGUMENT;
    }

    /* field sizes in bits */
    static const size_t fieldSize_pageGroupAddress = 62;
    static const size_t fieldSize_pageGroupSize = 25;

    const size_t entrySize = fieldSize_pageGroupAddress + fieldSize_pageGroupSize;

    const SglBlockField sglBlockFields[] = {
        {fieldSize_pageGroupAddress, (page_group_address >> 2)},
        {fieldSize_pageGroupSize,    page_group_size          }
    };

    static const size_t firstEntryOffset = 13;
    men_me6sgl_set_block_entry(sgl, entry_idx, entrySize, sglBlockFields, ARRAY_SIZE(sglBlockFields), firstEntryOffset);

    return STATUS_OK;
}