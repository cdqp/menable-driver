/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "menable5.h"
#include "lib/boards/me5_defines.h"
#include "lib/boards/me5_ironman.h"

#include "men_i2c_bus.h"


static int
me5_ironman_init_peripherals(struct me5_data * me5) {
    struct me5_ironman * self = downcast(me5, struct me5_ironman);
    men_init_i2c_peripherals(me5->men, &me5_ironman_i2c_declaration, 1,
                             self->i2c_cores, self->i2c_busses);

    int ret = spi_dual_controller_init(&self->spi_controller,
                               &me5->men->register_interface,
                               &me5->men->flash_lock,
                               me5_ironman_spi_declaration.control_register,
                               me5_ironman_spi_declaration.flash_select_register);

    return ret;
}

static int
me5_ironman_cleanup_peripherals(struct me5_data * me5) {
    struct me5_ironman * self = downcast(me5, struct me5_ironman);
    
    men_i2c_cleanup_busses(self->i2c_busses, ARRAY_SIZE(self->i2c_busses));

    struct controller_base * spi_base = upcast(&self->spi_controller);
    if (spi_base->destroy != 0) {
        spi_base->destroy(spi_base);
    }

    return 0;
}

static struct controller_base *
me5_ironman_get_controller(struct siso_menable * men, uint32_t peripheral) {
    struct me5_ironman * self = downcast(men->d5, struct me5_ironman);
    for (int i = 0; i < ARRAY_SIZE(self->i2c_busses); ++i) {
        if (self->i2c_busses[i].id == peripheral)
            return upcast(&self->i2c_busses[i].controller);
    }

    if (peripheral == me5_ironman_spi_declaration.id)
        return upcast(&self->spi_controller);

    return NULL;
}

int
me5_ironman_init(struct siso_menable * men, struct me5_ironman * me5_ironman) {
    struct me5_data * me5 = upcast(me5_ironman);
    pr_debug("menable %s\n", __FUNCTION__);
    men->get_controller = me5_ironman_get_controller;
    me5->init_peripherals = me5_ironman_init_peripherals;
    me5->cleanup_peripherals = me5_ironman_cleanup_peripherals;
    return 0;
}

