/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "menable5.h"

#include "lib/boards/me5_defines.h"
#include "lib/boards/me5_abacus.h"

#include "men_i2c_bus.h"

#include <linux/kernel.h>

ssize_t
me5_abacus_get_cpci_slot_id(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct siso_menable *men = container_of(dev, struct siso_menable, dev);

    if (men_get_state(men) >= BOARD_STATE_READY) {
        int slot_id = (int) men->register_interface.read(&men->register_interface, ME5_ABACUS_REG_SLOT_ID);
        return sprintf(buf, "%i\n", slot_id);
    } else {
        return sprintf(buf, "-1\n");
    }
}

static DEVICE_ATTR(cpci_slot_id, 0440, me5_abacus_get_cpci_slot_id, NULL);

ssize_t
me5_abacus_set_spi_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct siso_menable *men = container_of(dev, struct siso_menable, dev);
    uint32_t mode;
    int ret = buf_get_uint(buf, count, &mode);
    if (ret)
        return ret;

    if (men_get_state(men) >= BOARD_STATE_READY) {
        if (mode < 4) {
            men->register_interface.write(&men->register_interface, ME5_ABACUS_REG_SPI_MODE, mode);
        } else {
            return -EINVAL;
        }
    } else {
        return -EIO;
    }

    return count;
}

static DEVICE_ATTR(spi_mode, 0220, NULL, me5_abacus_set_spi_mode);

static struct attribute *me5_abacus_attributes[3] = {
    &dev_attr_cpci_slot_id.attr,
    &dev_attr_spi_mode.attr,
    NULL
};

static struct attribute_group me5_abacus_attribute_group = { .attrs = me5_abacus_attributes };

static const struct attribute_group *me5_abacus_attribute_groups[3] = {
    &me5_attribute_group,
	&me5_abacus_attribute_group,
    NULL
};

const struct attribute_group **
me5_abacus_init_attribute_groups(struct siso_menable *men)
{
    return me5_abacus_attribute_groups;
}

static int
me5_abacus_init_peripherals(struct me5_data * me5) {
    struct me5_abacus * self = downcast(me5, struct me5_abacus);
    struct siso_menable * men = me5->men;

    men_init_i2c_peripherals(men, &me5_abacus_i2c_declaration, 1,
                             self->i2c_cores, self->i2c_busses);

    int ret = spi_v2_controller_init(&self->spi_controller,
                               &men->register_interface,
                               &men->flash_lock,
                               me5_abacus_spi_declaration.control_register);

#if defined(DEBUG)
    {
        struct controller_base * cb = upcast(&self->spi_controller);

        u8 command_data = '\x9f';
        u8 reply_data[3];
        struct burst_header command = { BURST_TYPE_WRITE, SPI_POST_BURST_FLAG_LEAVE_CS_ASSERTED, 1, (u64)&command_data };
        struct burst_header reply = { BURST_TYPE_READ, SPI_BURST_FLAG_NONE, 3, (u64)&reply_data[0]};

        int result = cb->write_burst(cb, &command, &command_data, 1);
        if (result != 0) {
            dev_err(&men->dev, "Failed to write command to spi controller; result %d\n", result);
        } else {
            result = cb->read_burst(cb, &reply, &reply_data[0], 3);
            if (result != 0) {
                dev_err(&men->dev, "Failed to read back data from spi controller; result %d\n", result);
            } else {
                dev_info(&men->dev, "SPI chip id is %02x:%02x:%02x\n", (u32) reply_data[0], (u32) reply_data[1], (u32) reply_data[2]);
            }
        }
    }
#endif

    int slot_id = (int) men->register_interface.read(&men->register_interface, ME5_ABACUS_REG_SLOT_ID);
    dev_info(&men->dev, "device is in slot %d\n", slot_id);

    return ret;
}

static int
me5_abacus_cleanup_peripherals(struct me5_data * me5) {
    struct me5_abacus * self = downcast(me5, struct me5_abacus);
    
    men_i2c_cleanup_busses(self->i2c_busses, ARRAY_SIZE(self->i2c_busses));

    struct controller_base * spi_base = upcast(&self->spi_controller);
    if (spi_base->destroy != 0) {
        spi_base->destroy(spi_base);
    }

    return 0;
}

static struct controller_base *
me5_abacus_get_controller(struct siso_menable * men, uint32_t peripheral) {
    struct me5_abacus * self = downcast(men->d5, struct me5_abacus);
    for (int i = 0; i < ARRAY_SIZE(self->i2c_busses); ++i) {
        if (self->i2c_busses[i].id == peripheral)
            return upcast(&self->i2c_busses[i].controller);
    }

    if (peripheral == me5_abacus_spi_declaration.id)
        return upcast(&self->spi_controller);

    return NULL;
}

int
me5_abacus_init(struct siso_menable * men, struct me5_abacus * me5_abacus) {
    struct me5_data * me5 = upcast(me5_abacus);
    pr_debug("menable %s\n", __FUNCTION__);
    men->get_controller = me5_abacus_get_controller;
    me5->init_peripherals = me5_abacus_init_peripherals;
    me5->cleanup_peripherals = me5_abacus_cleanup_peripherals;

    return 0;
}

