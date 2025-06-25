/************************************************************************
 * Copyright 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "uiq_base.h"
#include "uiq_defines.h"
#include "uiq_helper.h"

#include <lib/helpers/bits.h>
#include <lib/helpers/error_handling.h>
#include <lib/helpers/helper.h>
#include <lib/helpers/memory.h>
#include <lib/os/types.h>

static bool is_empty(uiq_base * self) {
    return (self->fill == 0);
}

static bool is_full(uiq_base * self) {
    return (self->fill == self->capacity);
}

static uint32_t get_fill_level(uiq_base * self) {
    return self->fill;
}

static uint32_t get_free_capacity(struct uiq_base * self) {
    return self->capacity - self->fill;
}

static void replace_buffer(uiq_base * self, uint32_t * new_buffer, uint32_t new_buffer_size, uint32_t ** out_old_buffer) {

    const unsigned int words_to_copy = MIN(self->fill, new_buffer_size);

    if (self->fill > words_to_copy)
        self->lost_words_count += (self->fill - words_to_copy);

    const unsigned int words_before_wraparound = MIN(self->capacity - self->read_index, words_to_copy);
    const unsigned int bytes_before_wraparound = words_before_wraparound * sizeof(*self->data);

    copy_mem(new_buffer, self->data + self->read_index, bytes_before_wraparound);

    if (words_before_wraparound < words_to_copy) {
        const unsigned int words_after_wraparound = words_to_copy - words_before_wraparound;
        const unsigned int bytes_after_wraparound = words_after_wraparound * sizeof(*self->data);

        copy_mem(new_buffer + words_before_wraparound, self->data, bytes_after_wraparound);
    }

    *out_old_buffer = self->data;

    self->data = new_buffer;
    self->fill = words_to_copy;
    self->read_index = 0;
    self->capacity = new_buffer_size;
}

static int pop_front(uiq_base * self, uint32_t * target_buffer, uint32_t num_words_to_read) {

    num_words_to_read = MIN(num_words_to_read, self->fill);

    for (int i = 0; i < num_words_to_read; ++i) {
        target_buffer[i] = self->data[self->read_index];
        self->read_index = (self->read_index + 1) % self->capacity;
    }

    self->fill -= num_words_to_read;

    return num_words_to_read;
}

static void reset(struct uiq_base * self)
{
    self->fill = 0;
    self->read_index = 0;
    self->lost_words_count = 0;
}

static void record_discarded_words(struct uiq_base * self, uint32_t num_discarded_words) {
    self->lost_words_count += num_discarded_words;
    self->last_words_were_lost = true;
}

static int push_back(uiq_base * self, const uint32_t * data, uint32_t num_words) {

    const size_t words_free = self->capacity - self->fill;
    const size_t words_lost = (words_free < num_words) 
        ? (num_words - words_free) 
        : 0;
    
    num_words = MIN(num_words, words_free);

    for (uint32_t i = 0; i < num_words; ++i) {
        const size_t write_index = (self->read_index + self->fill) % self->capacity;
        self->data[write_index] = data[i];
        self->fill += 1;
    }

    if (words_lost == 0) {
        self->last_words_were_lost = false;
    } else {
        record_discarded_words(self, words_lost);
    }

    return num_words;
}

static void split_timestamp_into_16bit_chunks(uint64_t timestamp, uint32_t out_chunks[]) {
    static const uint32_t bits_per_chunk = 16;
    static const uint32_t num_chunks = sizeof(timestamp) * 8 / 16;

    for (uint32_t i = 0; i < num_chunks; ++i) {
        const uint32_t chunk_offset = (num_chunks - i - 1) * bits_per_chunk;
        out_chunks[i] = GET_BITS_64(timestamp, chunk_offset, chunk_offset + bits_per_chunk - 1);
    }
}

static int push_back_decorated_with_timestamp_unchecked(uiq_base * self, uint32_t data, const uiq_timestamp * timestamp) {

    uint32_t write_buffer[5];

    bool data_contained_eot_flag = (data & UIQ_CONTROL_EOT);
    write_buffer[0] = data & ~UIQ_CONTROL_EOT;

    const size_t bytes_to_copy = sizeof(timestamp->data[0]) * ARRAY_SIZE(timestamp->data);
    copy_mem(&write_buffer[1], timestamp->data, bytes_to_copy);
    if (data_contained_eot_flag)
        write_buffer[4] |= UIQ_CONTROL_EOT;

    return push_back(self, write_buffer, ARRAY_SIZE(write_buffer));
}

static int push_back_decorated_with_timestamp(uiq_base * self, uint32_t data, const uiq_timestamp * timestamp) {

    if (get_free_capacity(self) < 5)
        return push_back(self, &data, 1);
    else
        return push_back_decorated_with_timestamp_unchecked(self, data, timestamp);
}

static void write_from_grabber_raw(struct uiq_base * self, uint32_t value, uiq_timestamp * timestamp, bool * is_timestamp_initialized) {
    self->push_back(self, &value, 1);
}

static void init_timestamp_if_uninitialized(uiq_timestamp * timestamp, bool * is_timestamp_initialized) {
    if (!(*is_timestamp_initialized)) {
        men_get_uiq_timestamp(timestamp);
        *is_timestamp_initialized = true;
    }
}

static void write_word_with_dataloss_flag_if_applicable(struct uiq_base * self, uint32_t value, uiq_timestamp * timestamp, bool * is_timestamp_initialized) {
    if (self->last_words_were_lost)
        value |= UIQ_CONTROL_DATALOSS;

    if (UIQ_CONTROL_SHALL_INSERT_TIMESTAMP(value)) {
        init_timestamp_if_uninitialized(timestamp, is_timestamp_initialized);
        push_back_decorated_with_timestamp(self, value, timestamp);
    }
    else {
        push_back(self, &value, 1);
    }
}

static void write_from_grabber_legacy(struct uiq_base * self, uint32_t value, uiq_timestamp * timestamp, bool * is_timestamp_initialized) {

    if (!UIQ_CONTROL_IS_INVALID(value)) {
        write_word_with_dataloss_flag_if_applicable(self, value, timestamp, is_timestamp_initialized);
    }
}

static void write_from_grabber_va_event(struct uiq_base * self, uint32_t value, uiq_timestamp * timestamp, bool * is_timestamp_initialized) {

    if (!UIQ_CONTROL_IS_INVALID(value)) {

        if (self->last_words_were_lost && !self->last_word_was_eop)
            record_discarded_words(self, 1);
        else
            write_word_with_dataloss_flag_if_applicable(self, value, timestamp, is_timestamp_initialized);

        self->last_word_was_eop = UIQ_CONTROL_IS_END_OF_TRANSMISSION(value);
    }
}

static uint32_t get_lost_words_count(struct uiq_base * self)
{
    return self->lost_words_count;
}

static bool get_last_word_lost(struct uiq_base * self)
{
    return self->last_words_were_lost;
}

int uiq_base_init(uiq_base * uiq, uint32_t data_register_offset, uint32_t * buffer, uint32_t capacity,
                  uint32_t id, uint32_t type, uiq_protocol read_protocol, uint32_t fpga_fifo_depth, uint32_t channel_index)
{
    uiq->is_empty = is_empty;
    uiq->is_full = is_full;
    uiq->get_fill_level = get_fill_level;
    uiq->get_free_capacity = get_free_capacity;
    uiq->get_lost_words_count = get_lost_words_count;
    uiq->get_last_word_lost = get_last_word_lost;
    uiq->replace_buffer = replace_buffer;
    uiq->pop_front = pop_front;
    uiq->push_back = push_back;
    uiq->push_back_decorated_with_timestamp = push_back_decorated_with_timestamp;
    uiq->record_discarded_words = record_discarded_words;
    uiq->reset = reset;

    switch (read_protocol) {
    case UIQ_PROTOCOL_RAW:
        uiq->write_from_grabber = write_from_grabber_raw;
        break;

    case UIQ_PROTOCOL_LEGACY:
        uiq->write_from_grabber = write_from_grabber_legacy;
        break;

    case UIQ_PROTOCOL_VA_EVENT:
        uiq->write_from_grabber = write_from_grabber_va_event;
        break;
    }

    uiq->is_running = false;
    uiq->last_words_were_lost = false;
    uiq->channel_index = channel_index;
    uiq->id = id;
    uiq->lost_words_count = 0;
    uiq->fill = 0;
    uiq->irq_count = 0;
    uiq->read_index = 0;
    uiq->type = type;
    uiq->data_register_offset = data_register_offset;
    uiq->last_word_was_eop = 0;
    uiq->data = buffer;
    uiq->capacity = capacity;
    uiq->read_protocol = read_protocol;

    /* The fifo on the board is only used by write UIQs */
    uiq->fpga_fifo_depth = (UIQ_TYPE_IS_WRITE(type))
        ? fpga_fifo_depth
        : 0;

    return STATUS_OK;
}
