/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

/* debugging first */
#ifdef DBG_CTRL_BASE
    #undef MEN_DEBUG
    #define MEN_DEBUG
#endif

#include "../helpers/dbg.h"

#include "controller_base.h"

#include "../os/assert.h"

#include "../helpers/error_handling.h"
#include "../helpers/helper.h"

#define DBG_NAME "[controller base] "
#define LOG_PREFIX KBUILD_MODNAME ": " DBG_NAME

static int ctrl_begin_transaction(struct controller_base * self)
{
    DBG_TRACE_BEGIN_FCT;

    int ret = STATUS_OK;

    if (self->handle_begin_transaction != NULL) {
        ret = self->handle_begin_transaction(self);
    }

    DBG_TRACE_END_FCT;
    return ret;
}

static void ctrl_end_transaction(struct controller_base * self)
{
    DBG_TRACE_BEGIN_FCT;

    if (self->handle_end_transaction != NULL) {
        self->handle_end_transaction(self);
    }

    DBG_TRACE_END_FCT;
}

static void ctrl_cleanup_burst_state(struct controller_base * self) {
    self->current_burst_flags = 0;
    self->is_first_shot = false;
    self->is_last_shot = false;
    self->burst_aborted(self);
}

static int ctrl_write_burst(struct controller_base * self, struct burst_header * bh,
    const uint8_t * buf, size_t size)
{
    DBG_TRACE_BEGIN_FCT;
    assert(bh->type == BURST_TYPE_WRITE);

    int ret = STATUS_OK;

    self->current_burst_flags = bh->flags & 0xff;
    ret = self->handle_pre_burst_flags(self, bh->flags);
    if (MEN_IS_ERROR(ret))
        goto error;

    size_t num_remaining_bytes = size;

    int num_queued_commands = 0;

    while (num_remaining_bytes > 0) {
        self->is_first_shot = (num_remaining_bytes == size);
        self->is_last_shot = (num_remaining_bytes <= self->max_bytes_per_write);

        size_t bytes_to_write = min(num_remaining_bytes, self->max_bytes_per_write);

        ret = self->write_shot(self, buf, bytes_to_write);
        if (MEN_IS_ERROR(ret))
            goto error;

        buf += bytes_to_write;
        num_remaining_bytes -= bytes_to_write;
        ++num_queued_commands;

        if (num_queued_commands % self->write_queue_size == 0) {
            ret = self->wait_for_write_queue_empty(self);
            if (MEN_IS_ERROR(ret))
                goto error;
        }
    }

    self->is_first_shot = false;
    self->is_last_shot = false;

    if (num_queued_commands % self->write_queue_size != 0) {
        ret = self->wait_for_write_queue_empty(self);
        if (MEN_IS_ERROR(ret))
            goto error;
    }

    self->handle_post_burst_flags(self, bh->flags);
    self->current_burst_flags = 0;

    DBG_TRACE_END_FCT;
    return STATUS_OK;

error:
    ctrl_cleanup_burst_state(self);
    DBG_TRACE_END_FCT;
    return STATUS_ERROR;
}

static int ctrl_read_burst(struct controller_base * self,
    struct burst_header * bh,
    uint8_t * buf, size_t size)
{
    DBG_TRACE_BEGIN_FCT;
    assert(bh->type == BURST_TYPE_READ);

    int ret = STATUS_OK;

    self->current_burst_flags = bh->flags & 0xff;
    ret = self->handle_pre_burst_flags(self, bh->flags);
    if (MEN_IS_ERROR(ret))
        goto error;

    size_t bytes_to_request = size;
    size_t bytes_to_read = 0;

    /* queue as many read operations as possible */
    for (size_t i = 0; i < self->read_queue_size && bytes_to_request > 0; ++i) {
        size_t num_bytes = min(bytes_to_request, self->max_bytes_per_read);
        bytes_to_request -= num_bytes;
        bytes_to_read += num_bytes;

        self->is_first_shot = (i == 0);
        self->is_last_shot = (bytes_to_read == size);

        ret = self->request_read(self, num_bytes);
        if (MEN_IS_ERROR(ret))
            goto error;

    }

    /* read data, request further data if required */
    self->is_first_shot = true;
    while (bytes_to_read > 0) {
        self->is_last_shot = (bytes_to_request == 0 && bytes_to_read <= self->max_bytes_per_read);
        size_t num_bytes = min(bytes_to_read, self->max_bytes_per_read);

        ret = self->read_shot(self, buf, num_bytes);
        if (MEN_IS_ERROR(ret))
            goto error;

        buf += num_bytes;
        bytes_to_read -= num_bytes;
        self->is_first_shot = false;

        if (bytes_to_request > 0) {
            num_bytes = min(bytes_to_request, self->max_bytes_per_read);

            ret = self->request_read(self, num_bytes);
            if (MEN_IS_ERROR(ret))
                goto error;

            bytes_to_request -= num_bytes;
            bytes_to_read += num_bytes;
        }
    }
    self->is_last_shot = false;

    assert(bytes_to_request == 0);
    assert(bytes_to_read == 0);

    self->handle_post_burst_flags(self, bh->flags);
    self->current_burst_flags = 0;

    DBG_TRACE_END_FCT;
    return STATUS_OK;

error:
    ctrl_cleanup_burst_state(self);
    DBG_TRACE_END_FCT;
    return STATUS_ERROR;
}

static int ctrl_state_change_burst(struct controller_base * self, struct burst_header * bh) {
    int ret = self->handle_pre_burst_flags(self, bh->flags);

    /* TODO: What happens if the state change is not successful? */
    if (ret != STATUS_OK) {
        return ret;
    }

    return self->handle_post_burst_flags(self, bh->flags);
}

static int ctrl_command_execution_burst(struct controller_base * self, struct burst_header * burst_header, uint8_t * buffer, size_t num_bytes) {
    if (num_bytes < sizeof(command_burst_header)) {
        pr_err(LOG_PREFIX "Command burst buffer is too small.");
        return STATUS_ERR_INVALID_ARGUMENT;
    }

    command_burst_header * cmd_header = (command_burst_header*)(buffer);

    uint8_t * command_data = buffer + sizeof(command_burst_header);
    const size_t command_data_size = num_bytes - sizeof(command_burst_header);

    return self->execute_command(self, cmd_header, command_data, command_data_size);
}

static bool ctrl_are_burst_flags_set(struct controller_base * self, uint8_t flags) {
    return (self->current_burst_flags & flags) == flags;
}

static void ctrl_destroy(struct controller_base * self) {
    self->cleanup(self);
}

void controller_base_init(struct controller_base * ctrl,
                          struct register_interface * ri,
                          user_mode_lock * lock,
                          int read_queue_size,
                          int max_bytes_per_read,
                          int write_queue_size,
                          int max_bytes_per_write,
                          int (*handle_begin_transaction)(struct controller_base * ctrl),
                          void (*handle_end_transaction)(struct controller_base * ctrl),
                          int (*handle_pre_burst_flags)(struct controller_base * ctrl, uint32_t flags),
                          int (*handle_post_burst_flags)(struct controller_base * ctrl, uint32_t flags),
                          int (*write_shot)(struct controller_base * ctrl, const uint8_t * buf, int num_bytes),
                          int (*request_read)(struct controller_base * ctrl, size_t num_bytes),
                          int (*read_shot)(struct controller_base * ctrl, uint8_t * buffer, size_t num_bytes),
                          int (*execute_command)(struct controller_base * self, command_burst_header * header, uint8_t * command_data, size_t command_data_size),
                          int (*wait_for_write_queue_empty)(struct controller_base * ctrl),
                          void (*burst_aborted)(struct controller_base * self),
                          void (*cleanup)(struct controller_base * self))
{
    DBG_TRACE_BEGIN_FCT;
    ctrl->register_interface = ri;
    ctrl->lock = lock;

    ctrl->read_queue_size = read_queue_size;
    ctrl->max_bytes_per_read = max_bytes_per_read;

    ctrl->write_queue_size = write_queue_size;
    ctrl->max_bytes_per_write = max_bytes_per_write;

    ctrl->begin_transaction = ctrl_begin_transaction;
    ctrl->end_transaction = ctrl_end_transaction;
    ctrl->write_burst = ctrl_write_burst;
    ctrl->read_burst = ctrl_read_burst;
    ctrl->state_change_burst = ctrl_state_change_burst;
    ctrl->command_execution_burst = ctrl_command_execution_burst;
    ctrl->are_burst_flags_set = ctrl_are_burst_flags_set;
    ctrl->destroy = ctrl_destroy;

    ctrl->handle_begin_transaction = handle_begin_transaction;
    ctrl->handle_end_transaction = handle_end_transaction;
    ctrl->handle_pre_burst_flags = handle_pre_burst_flags;
    ctrl->handle_post_burst_flags = handle_post_burst_flags;
    ctrl->write_shot = write_shot;
    ctrl->request_read = request_read;
    ctrl->read_shot = read_shot;
    ctrl->execute_command = execute_command;
    ctrl->wait_for_write_queue_empty = wait_for_write_queue_empty;
    ctrl->burst_aborted = burst_aborted;
    ctrl->cleanup = cleanup;

    ctrl->current_burst_flags = 0;
    ctrl->is_first_shot = false;
    ctrl->is_last_shot = false;
    DBG_TRACE_END_FCT;
}

static int command_execution_burst_not_supported(struct controller_base * self, struct burst_header * burst_header, uint8_t * buffer, size_t num_bytes) {
    return STATUS_ERR_INVALID_OPERATION;
}

void controller_base_super_init(
    struct controller_base * ctrl,
    struct register_interface * ri,
    user_mode_lock * lock,
    int read_queue_size,
    int max_bytes_per_read,
    int write_queue_size,
    int max_bytes_per_write,
    int(*begin_transaction)(struct controller_base *),
    void(*end_transaction)(struct controller_base *),
    int(*read_burst)(struct controller_base *, struct burst_header *, uint8_t *, size_t),
    int(*write_burst)(struct controller_base *, struct burst_header *, const uint8_t *, size_t),
    int(*state_change_burst)(struct controller_base *, struct burst_header *),
    int(*command_execution_burst)(struct controller_base *, struct burst_header *, uint8_t *, size_t),
    bool(*are_burst_flags_set)(struct controller_base *, uint8_t),
    void(*destroy)(struct controller_base * self))
{
    DBG_TRACE_BEGIN_FCT;
    ctrl->register_interface = ri;
    ctrl->lock = lock;

    ctrl->read_queue_size = read_queue_size;
    ctrl->max_bytes_per_read = max_bytes_per_read;

    ctrl->write_queue_size = write_queue_size;
    ctrl->max_bytes_per_write = max_bytes_per_write;

    ctrl->begin_transaction = begin_transaction;
    ctrl->end_transaction = end_transaction;
    ctrl->write_burst = write_burst;
    ctrl->read_burst = read_burst;
    ctrl->state_change_burst = state_change_burst;
    ctrl->are_burst_flags_set = are_burst_flags_set;
    ctrl->destroy = destroy;

    if (command_execution_burst != NULL)
        ctrl->command_execution_burst = command_execution_burst;
    else
        ctrl->command_execution_burst = command_execution_burst_not_supported;

    ctrl->handle_begin_transaction = NULL;
    ctrl->handle_end_transaction = NULL;
    ctrl->handle_pre_burst_flags = NULL;
    ctrl->handle_post_burst_flags = NULL;
    ctrl->write_shot = NULL;
    ctrl->request_read = NULL;
    ctrl->read_shot = NULL;
    ctrl->execute_command = NULL;
    ctrl->wait_for_write_queue_empty = NULL;
    ctrl->burst_aborted = NULL;
    ctrl->cleanup = NULL;

    ctrl->current_burst_flags = 0;
    ctrl->is_first_shot = false;
    ctrl->is_last_shot = false;
    DBG_TRACE_END_FCT;
}

int controller_base_execute_command_not_supported(struct controller_base * self, command_burst_header * header, uint8_t * command_data, size_t command_data_size)
{
    return STATUS_ERR_INVALID_OPERATION;
}