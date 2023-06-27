/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_HELPERS_LINKED_LIST_H_
#define LIB_HELPERS_LINKED_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <lib/os/types.h>

/**
 * To get arbitrary data into a list, derive from
 * men_list_entry using the helpers in helpers/type_hierarchy.h.
 */
struct men_list_entry {
    struct men_list_entry * next;
};

/**
 * A linked list that can be used as a queue, stack etc.
 */
struct men_list {
    
    void(*push_front)(struct men_list * self, struct men_list_entry * entry);
    void(*push_back)(struct men_list * self, struct men_list_entry * entry);

    struct men_list_entry * (*peek_front)(struct men_list * self);
    struct men_list_entry * (*peek_back)(struct men_list * self);

    struct men_list_entry * (*pop_front)(struct men_list * self);
    struct men_list_entry * (*pop_back)(struct men_list * self);

    uint32_t size;

    /* These are PRIVATE members, do not use directly! */
    struct men_list_entry * head;
    struct men_list_entry * tail;
};

int men_list_init(struct men_list * list);

#define MEN_LIST_FOREACH(list) for(struct men_list_entry * entry = list.head; entry != NULL; entry = entry->next)

#ifdef __cplusplus
}
#endif

#endif