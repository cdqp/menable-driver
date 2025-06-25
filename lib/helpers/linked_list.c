/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "linked_list.h"

#include <lib/helpers/error_handling.h>
#include <lib/os/types.h>

static void men_list_push_front(struct men_list * self, struct men_list_entry * entry) {
    entry->next = self->head;
    self->head = entry;

    if (self->tail == NULL) {
        self->tail = entry;
    }

    self->size += 1;
}

static void men_list_push_back(struct men_list * self, struct men_list_entry * entry) {
    entry->next = NULL;

    if (self->head == NULL) {
        /* empty queue */
        self->head = entry;
        self->tail = entry;
    } else {
        self->tail->next = entry;
        self->tail = entry;
    }

    self->size += 1;
}

static struct men_list_entry * men_list_peek_front(struct men_list * self) {
    return self->head;
}

static struct men_list_entry * men_list_peek_back(struct men_list * self){
    return self->tail;
}


struct men_list_entry * men_list_pop_front(struct men_list * self) {
    
    struct men_list_entry * entry = NULL;

    if (self->size > 0) {
        entry = self->head;
        self->head = entry->next;
        if (self->tail == entry) {
            /* this was the last entry */
            self->tail = NULL;
        }

        entry->next = NULL;
        self->size -= 1;
    }

    return entry;
}

static struct men_list_entry * men_list_pop_back(struct men_list * self) {
    /* TODO: This function is more complex than the user might expect.
     *       If performance is an issue, this should be removed (if not needed)
     *       or the list should be a doubly linked list. 
     */

    if (self->size == 1) {
        /* there is only one entry -> head and tail are the same and both have no follower */
        struct men_list_entry * entry = self->head;
        self->head = NULL;
        self->tail = NULL;
        self->size = 0;
        return entry;
    } else if (self->size > 1) {
        struct men_list_entry * entry = self->head;

        /* find the last entry before the tail */
        while (entry->next != self->tail) {
            entry = entry->next;
        }
        
        /* entry becomes the new tail */
        self->tail = entry;

        /* set entry to the former tail for returning it */
        entry = entry->next;

        /* the new tail should not have a follower */
        self->tail->next = NULL;

        self->size -= 1;
        return entry;
    } else {
        return NULL;
    }
}

int men_list_init(struct men_list * list) {

    list->size = 0;
    list->head = NULL;
    list->tail = NULL;

    list->push_front = men_list_push_front;
    list->push_back = men_list_push_back;

    list->peek_front = men_list_peek_front;
    list->peek_back = men_list_peek_back;

    list->pop_front= men_list_pop_front;
    list->pop_back = men_list_pop_back;

    return STATUS_OK;
}
