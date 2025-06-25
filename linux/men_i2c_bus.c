/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "lib/controllers/i2c_defines.h"
#include "lib/controllers/i2c_master_core.h"

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include "men_i2c_bus.h"
#include "menable.h"
#include "lib/helpers/dbg.h"

const struct i2c_algorithm
men_i2c_algorithm = {
    .functionality = men_i2c_func,
    .master_xfer   = men_i2c_master_xfer,
    .smbus_xfer    = NULL
};

u32 men_i2c_func (struct i2c_adapter * adapter) {
    return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

int men_i2c_master_xfer(struct i2c_adapter * adapter, struct i2c_msg *msgs, int num) {
    struct men_i2c_bus * i2c_peripheral = container_of(adapter, struct men_i2c_bus, adapter);
    struct i2c_bus_controller * i2c_bus_ctrl = &i2c_peripheral->controller;

    /* check message flags before doing any I/O */
    for (int i = 0; i < num; ++i) {
        struct i2c_msg * msg = &msgs[i];
        /* Only I2C_M_RD is a valid flag */
        if ((msg->flags & ~I2C_M_RD) != 0) {
            dev_err(adapter->dev.parent, "i2c message %d contains invalid flag: %04x\n", i, msg->flags);
            return -EOPNOTSUPP;
        }
    }

    /* transfer data */
    for (int i = 0; i < num; ++i) {
        struct i2c_msg * msg = &msgs[i];
        struct controller_base * i2c_ctrl = upcast(i2c_bus_ctrl);
        struct burst_header bh;

        bh.type = BURST_TYPE_WRITE;
        bh.len = 1;
        bh.flags = I2C_PRE_BURST_FLAG_START_CONDITION;
        uint8_t i2c_address = (msg->addr << 1) | (msg->flags & I2C_M_RD);
        int ret = i2c_ctrl->write_burst(i2c_ctrl, &bh, &i2c_address, 1);

        if (ret < 0) {
            dev_err(adapter->dev.parent, "no ack received for i2c address %02x\n", msg->addr);
            return -ENXIO;
        }

        bh.type = (msg->flags & I2C_M_RD) ? BURST_TYPE_READ : BURST_TYPE_WRITE;
        bh.len = msg->len;
        bh.flags = I2C_POST_BURST_FLAG_STOP_CONDITION;
        ret = (bh.type == BURST_TYPE_READ)
                ? i2c_ctrl->read_burst(i2c_ctrl, &bh, msg->buf, msg->len)
                : i2c_ctrl->write_burst(i2c_ctrl, &bh, msg->buf, msg->len);

        if (ret < 0) {
            return -EIO;
        }
    }

    return num;
}

/**
 * Cleans up the i2c adapters in a given array in reverse order.
 *
 * @param initialized_busses
 * @param last_bus_idx
 */
void men_i2c_cleanup_busses(struct men_i2c_bus * busses, int bus_count) {
    DBG_TRACE_BEGIN_FCT;

    for (int i = 0; i < bus_count; ++i) {
        struct men_i2c_bus * bus = &busses[i];

        if (bus->id != 0) {

#ifndef NO_SYSTEM_I2C
            dev_info(bus->adapter.dev.parent, "Cleanup i2c bus nr. %d\n", i);

            for (int i = 0; i < ARRAY_SIZE(bus->clients); ++i) {
                if (bus->clients[i]) {
                    dev_info(bus->adapter.dev.parent, "Remove i2c device '%s'\n", bus->clients[i]->name);
                    i2c_unregister_device(bus->clients[i]);
                    bus->clients[i] = NULL;
                }
            }

            if (bus->adapter.nr != 0) {
                dev_info(bus->adapter.dev.parent, "Remove i2c adapter nr. %d\n", bus->adapter.nr);
                i2c_del_adapter(&bus->adapter);
                bus->adapter.nr = 0;
            }
#endif

            struct controller_base * base = upcast(&bus->controller);

            if (base->destroy) {
                // if destroy is NULL, initialization did not happen
                base->destroy(base);
            }
        }
    }

    DBG_TRACE_END_FCT;
}

int men_init_i2c_peripherals(struct siso_menable * men,
                              struct i2c_master_core_declaration * declarations,
                              uint8_t num, struct i2c_master_core* i2c_cores,
                              struct men_i2c_bus* i2c_busses)
{
    DBG_TRACE_BEGIN_FCT;

    for (uint8_t core_idx = 0; core_idx < num; ++core_idx) {
        struct i2c_master_core_declaration* i2c_core_decl = &declarations[core_idx];
        struct i2c_master_core* i2c_core = &i2c_cores[core_idx];

        int ret = i2c_master_core_init(i2c_core,
                                       &men->register_interface,
                                       &men->i2c_lock,
                                       i2c_core_decl->address_register,
                                       i2c_core_decl->write_register,
                                       i2c_core_decl->read_register,
                                       i2c_core_decl->firmware_clock_frequency,
                                       i2c_core_decl->num_required_safety_writes);

        if (ret != 0) {
            dev_err(&men->dev, "Init of i2c core %u failed\n", core_idx);
            DBG_TRACE_END_FCT;
            return -EPERM;
        }

        for (uint8_t bus_idx = 0; bus_idx < i2c_core_decl->bus_count; ++bus_idx) {
            struct men_i2c_bus_declaration* bus_decl = &i2c_core_decl->bus_declarations[bus_idx];
            struct men_i2c_bus* bus = &i2c_busses[bus_idx];
            struct i2c_bus_controller* bus_controller = &bus->controller;

            bus->id = bus_decl->id;

            i2c_master_core_configure_bus(i2c_core, bus_idx,
                                          bus_decl->bank_activation_bitmask,
                                          bus_decl->write_enable_bitmask,
                                          bus_decl->bus_frequency);

            i2c_bus_controller_init(bus_controller, i2c_core,
                                    bus_decl->bank_number);

#ifndef NO_SYSTEM_I2C

            bus_adapter->owner = THIS_MODULE;
            bus_adapter->class = I2C_CLASS_HWMON;
            bus_adapter->algo = &men_i2c_algorithm;
            bus_adapter->dev.parent = &men->dev;
            strlcpy(bus_adapter->name, bus_decl->name, ARRAY_SIZE(bus_adapter->name));

            dev_dbg(&men->dev, "try to add i2c adapter\n");
            ret = i2c_add_adapter(bus_adapter);
            if (ret != 0) {
                dev_err(&men->dev, "Addding i2c adapter failed.\n");
                DBG_TRACE_END_FCT;
                return -EPERM;
            }

            dev_info(&men->dev, "i2c adapter available at nr %d\n", bus_adapter->nr);

#endif
        }
    }

    DBG_TRACE_END_FCT;

    return 0;
}

