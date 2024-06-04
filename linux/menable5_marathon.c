/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef ME5_MARATHON_C_
#define ME5_MARATHON_H_

#include "menable5.h"
#include "lib/boards/me5_defines.h"
#include "lib/boards/me5_marathon.h"

#include "men_i2c_bus.h"
#include "lib/helpers/dbg.h"
#include "lib/os/types.h"

/**
 * Struct to declare i2c devices for which the linux driver
 * shall be loaded. Since this is linux specific,
 * it is separated from the peripheral declaration.
 */
struct men_i2c_device_declaration {
    uint8_t bus_number;
    uint8_t device_count;
    struct i2c_board_info board_infos[8];
};

#ifndef NO_SYSTEM_I2C
static struct men_i2c_device_declaration devices[] = {
    {
        .bus_number = 0,
        .device_count = 1,
        .board_infos = {
            {
                 I2C_BOARD_INFO("tmp103", 0x70)
            }
        }
    }
};
#endif

static struct controller_base *
me5_marathon_get_controller(struct siso_menable * men, uint32_t peripheral) {
    struct me5_marathon * self = downcast(men->d5, struct me5_marathon);
    for (int i = 0; i < ARRAY_SIZE(self->i2c_busses); ++i) {
        if (self->i2c_busses[i].id == peripheral)
            return upcast(&self->i2c_busses[i].controller);
    }
    return NULL;
}

static int
me5_marathon_init_peripherals(struct me5_data * me5) {
    DBG_TRACE_BEGIN_FCT;

    struct me5_marathon * self = downcast(me5, struct me5_marathon);
    men_init_i2c_peripherals(me5->men, &me5_marathon_i2c_declaration, 1,
                             self->i2c_cores, self->i2c_busses);

#ifndef NO_SYSTEM_I2C

    for (int bus_idx = 0; bus_idx < ARRAY_SIZE(devices); ++bus_idx) {
        struct men_i2c_device_declaration * decl = &devices[bus_idx];
        struct men_i2c_bus * bus = &self->i2c_busses[bus_idx];
        struct i2c_adapter * adapter = &bus->adapter;

        for (int dev_idx = 0; dev_idx < decl->device_count; ++dev_idx) {
            struct i2c_board_info * board_info = &decl->board_infos[dev_idx];
            dev_info(&me5->men->dev, "Add i2c device '%s' to bus '%s'\n",
                     board_info->type, bus->adapter.name);

            struct i2c_client * client = i2c_new_device(adapter, board_info);
            if (IS_ERR(client)) {
                dev_err(&me5->men->dev, "Failed to add i2c device '%s' to bus '%s'\n",
                        board_info->type, bus->adapter.name);
            }

            bus->clients[dev_idx] = client;
        }
    }

#endif

    DBG_TRACE_END_FCT;
    return 0;
}

static int
me5_marathon_cleanup_peripherals(struct me5_data * me5) {
    DBG_TRACE_BEGIN_FCT;
    struct me5_marathon * self = downcast(me5, struct me5_marathon);
    men_i2c_cleanup_busses(self->i2c_busses, ARRAY_SIZE(self->i2c_busses));

    /* TODO */
    DBG_TRACE_END_FCT;
    return 0;
}

int
me5_marathon_init(struct siso_menable * men, struct me5_marathon * me5_marathon) {
    DBG_TRACE_BEGIN_FCT;
    struct me5_data * me5 = upcast(me5_marathon);
    men->get_controller = me5_marathon_get_controller;
    me5->init_peripherals = me5_marathon_init_peripherals;
    me5->cleanup_peripherals = me5_marathon_cleanup_peripherals;
    DBG_TRACE_END_FCT;
    return 0;
}

#endif /* ME5_MARATHON_C_ */
