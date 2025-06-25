/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef _LIB_UIQ_UIQ_BASE_H_
#define _LIB_UIQ_UIQ_BASE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "uiq_defines.h"
#include <lib/os/types.h>

/**
 * A common base for the OS specific UIQ implementations.
 */
typedef struct uiq_base {

    uint32_t id;

    uint32_t channel_index;

    /**
        * The UIQ's type (i.e. read, write legacy).
        * For write UIQs, the data transmission protocol is also specified in the
        * type (i.e. write legacy, write CXP).
        *
        * TODO: [RKN] This field combines the informaion read/write and, for write UIQs,
        *       the transmission protocol. For read UIQs, the protocol is specified
        *       in a separate field. This should be made consistent. Either add
        *       the protocol information to the type for read UIQs or move it to
        *       the protocol field for the write UIQs.
        */
    uint32_t type;

    /**
        * The protocol for data transmissions.
        *
        * @attention
        * Currently we only determine the protocol for read UIQs (device -> host).
        * For write UIQs this member has no meaning.
        */
    uiq_protocol read_protocol;

    uint32_t capacity;
    uint32_t fill;
    uint32_t read_index;
    uint32_t * data;

    uint32_t lost_words_count;  /** number of words that were lost in this uiq */
    uint32_t irq_count;         /** number of intterupts for this uiq */

    bool is_running;
    bool last_words_were_lost;

    /* Indicates wether or not the last processed word has been an end of packet
     * word. In that case, the next word can be expected to be a new header.
     */
    bool last_word_was_eop; // TODO: Rename to 'expecting_header'?

    /* Firmware Interface */
    uint32_t data_register_offset;
    uint32_t fpga_fifo_depth;

    bool (*is_empty)(struct uiq_base * self);
    bool (*is_full)(struct uiq_base * self);
    uint32_t (*get_fill_level)(struct uiq_base * self);
    uint32_t (*get_free_capacity)(struct uiq_base * self);
    uint32_t (*get_lost_words_count)(struct uiq_base * self);
    bool (*get_last_word_lost)(struct uiq_base * self);
    void (*replace_buffer)(struct uiq_base * self, uint32_t * new_buffer, uint32_t new_buffer_size, uint32_t ** out_old_buffer);
    int (*pop_front)(struct uiq_base * self, uint32_t * target_buffer, uint32_t num_words_to_read);
    int (*push_back)(struct uiq_base * self, const uint32_t * data, uint32_t num_words);
    int (*push_back_decorated_with_timestamp)(struct uiq_base * self, uint32_t data, const uiq_timestamp * timestamp);
    void (*write_from_grabber)(struct uiq_base * self, uint32_t value, uiq_timestamp * timestamp, bool * is_timestamp_initialized);
    void (*record_discarded_words)(struct uiq_base * self, uint32_t num_discarded_words);
    void (*reset)(struct uiq_base * self);

} uiq_base;

int uiq_base_init(uiq_base * uiq, uint32_t data_register_offset, uint32_t * buffer, uint32_t capacity,
                  uint32_t id, uint32_t type, uiq_protocol read_protocol, uint32_t fpga_fifo_depth, uint32_t channel_index);

#ifdef __cplusplus
}
#endif

#endif /* Include Guard */
