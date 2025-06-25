/************************************************************************
 * Copyright 2022-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "image_buffer_manager.h"

static size_t size_of(image_buffer_deque* deque) {
    return deque->size(deque);
}

static void clear_all(image_buffer_manager* self) {
    self->free.clear(&self->free);
    self->free.clear(&self->ready);
    self->free.clear(&self->hot);
    self->free.clear(&self->grabbed);
    self->free.clear(&self->done);
}

int image_buffer_manager_init(image_buffer_manager* manager) {
    image_buffer_deque_init(&manager->free);
    image_buffer_deque_init(&manager->ready);
    image_buffer_deque_init(&manager->hot);
    image_buffer_deque_init(&manager->grabbed);
    image_buffer_deque_init(&manager->done);

    manager->size_of = size_of;
    manager->clear_all = clear_all;

    return STATUS_OK;
}