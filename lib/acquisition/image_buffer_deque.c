/************************************************************************
 * Copyright 2022-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "image_buffer_deque.h"

static void set_follower(image_buffer* item, image_buffer* follower) {
    item->deque_entry.next = follower;

    if (follower != NULL)
        follower->deque_entry.previous = item;
}

static bool is_item_already_linked(image_buffer* buffer) {
    return (buffer->deque_entry.next != NULL || buffer->deque_entry.previous != NULL);
}

static void unlink_item(image_buffer* buffer) {
    buffer->deque_entry.previous = NULL;
    buffer->deque_entry.next = NULL;
}

int image_buffer_init(image_buffer* buffer) {
    buffer->deque_entry.previous = NULL;
    buffer->deque_entry.next = NULL;

    return STATUS_OK;
}

static bool is_empty(image_buffer_deque* self) {
    return (self->head.deque_entry.next == NULL);
}

static image_buffer* pop_front(image_buffer_deque* self) {
    image_buffer* item = self->head.deque_entry.next;

    if (item != NULL) {
        set_follower(&self->head, item->deque_entry.next);
        unlink_item(item);
    }

    return item;
}

static image_buffer* get_last_entry(image_buffer_deque* self) {
    image_buffer* last = &self->head;
    while (last->deque_entry.next != NULL)
        last = last->deque_entry.next;

    return last;
}

static int push_back(image_buffer_deque* self, image_buffer* buffer) {

    if (buffer == NULL || is_item_already_linked(buffer)) {
        return STATUS_ERR_INVALID_OPERATION;
    }

    image_buffer* last = get_last_entry(self);
    set_follower(last, buffer);

    return STATUS_OK;
}

static size_t size(image_buffer_deque* self) {
    /* The number of buffers will be rather small, so we save the complexity of maintaining a counter and iterate over all items here. */
    size_t count = 0;

    for (image_buffer* current = self->head.deque_entry.next; current != NULL; current = current->deque_entry.next) {
        count += 1;
    }

    return count;
}

static void clear(image_buffer_deque* self) {
    while (!is_empty(self))
        self->pop_front(self);
}

static void pop_all(image_buffer_deque* self, image_buffer_deque* target) {
    target->head = self->head;
    unlink_item(&self->head);
}

static void move_items_from(struct image_buffer_deque* self, struct image_buffer_deque* source) {
    set_follower(get_last_entry(self), source->head.deque_entry.next);
    unlink_item(&source->head);
}

static int remove(struct image_buffer_deque* self, image_buffer* buffer) {
    if (is_empty(self))
        return STATUS_ERR_INVALID_OPERATION;

    if (buffer == NULL)
        return STATUS_ERR_INVALID_ARGUMENT;

    for (image_buffer* current = self->head.deque_entry.next; current != NULL; current = current->deque_entry.next) {
        if (current == buffer) {
            set_follower(buffer->deque_entry.previous, buffer->deque_entry.next);
            unlink_item(buffer);
            return STATUS_OK;
        }
    }

    return STATUS_ERROR;
}

int image_buffer_deque_init(image_buffer_deque * deque) {

    image_buffer_init(&deque->head);

    deque->is_empty = is_empty;
    deque->size = size;
    deque->pop_front = pop_front;
    deque->push_back = push_back;
    deque->clear = clear;
    deque->pop_all = pop_all;
    deque->move_items_from = move_items_from;
    deque->remove = remove;

    return STATUS_OK;
}