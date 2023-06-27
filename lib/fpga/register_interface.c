/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "register_interface.h"

void register_interface_activate(struct register_interface * self)
{
    self->is_active = 1;
}

void register_interface_deactivate(struct register_interface * self)
{
    self->is_active = 0;
}

void register_interface_init(struct register_interface * self, uint32_t __iomem * base_address,
                             void (* write_fct)(struct register_interface * register_interface, uint32_t address, uint32_t value),
                             uint32_t (* read_fct)(struct register_interface * register_interface, uint32_t address),
                             void (*b2b_barrier)(struct register_interface * self),
                             void (*reorder_barrier)(struct register_interface * self),
                             void (*reorder_b2b_barrier)(struct register_interface * self)) {
    self->base_address = base_address;
    self->is_active = 0;

    self->activate = register_interface_activate;
    self->deactivate = register_interface_deactivate;

    self->write = write_fct;
    self->read = read_fct;
    self->b2b_barrier = b2b_barrier;
    self->reorder_barrier = reorder_barrier;
    self->reorder_b2b_barrier = reorder_b2b_barrier;
}
