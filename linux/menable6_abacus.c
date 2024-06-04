/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "menable6.h"
#include "lib/boards/me6_defines.h"
#include "lib/boards/me6_abacus.h"

#include "men_i2c_bus.h"
#include "menable_ioctl.h"

#include <lib/boards/peripheral_declaration.h>

#include <linux/kernel.h>

ssize_t
me6_abacus_get_cpci_slot_id(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct siso_menable *men = container_of(dev, struct siso_menable, dev);

    if (men_get_state(men) >= BOARD_STATE_READY) {
        int slot_id = (int) men->register_interface.read(&men->register_interface, ME6_ABACUS_REG_SLOT_ID);
        return sprintf(buf, "%i\n", slot_id);
    } else {
        return sprintf(buf, "-1\n");
    }
}

static DEVICE_ATTR(cpci_slot_id, 0440, me6_abacus_get_cpci_slot_id, NULL);

ssize_t
me6_abacus_set_spi_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct siso_menable *men = container_of(dev, struct siso_menable, dev);
    uint32_t mode;
    int ret = buf_get_uint(buf, count, &mode);
    if (ret)
        return ret;

    if (men_get_state(men) >= BOARD_STATE_READY) {
        if (mode < 4) {
            men->register_interface.write(&men->register_interface, ME6_ABACUS_REG_SPI_MODE, mode);
        } else {
            return -EINVAL;
        }
    } else {
        return -EIO;
    }

    return count;
}

static DEVICE_ATTR(spi_mode, 0220, NULL, me6_abacus_set_spi_mode);

static struct attribute *me6_abacus_attributes[3] = {
    &dev_attr_cpci_slot_id.attr,
	&dev_attr_spi_mode.attr,
    NULL
};

static struct attribute_group me6_abacus_attribute_group = { .attrs = me6_abacus_attributes };

static const struct attribute_group *me6_abacus_attribute_groups[3] = {
    &me6_attribute_group,
	&me6_abacus_attribute_group,
    NULL
};

const struct attribute_group **
me6_abacus_init_attribute_groups(struct siso_menable *men)
{
    return me6_abacus_attribute_groups;
}

static int
me6_abacus_init_peripherals(struct me6_data * me6)
{
    struct me6_abacus * self = downcast(me6, struct me6_abacus);
    struct siso_menable * men = me6->men;

    men_init_i2c_peripherals(men, &me6_abacus_i2c_declaration, 1,
                             &self->i2c_controller, self->i2c_busses);

    int ret = spi_v2_controller_init(&self->spi_controller,
                                      &men->register_interface,
                                      &men->flash_lock,
                                      me6_abacus_spi_declaration.control_register);

    if (ret != 0) {
        dev_warn(&men->dev, "Failed to init SPI.\n");
        return ret;
    }

    int slot_id = (int) men->register_interface.read(&men->register_interface, ME6_ABACUS_REG_SLOT_ID);
    dev_info(&men->dev, "device is in slot %d\n", slot_id);

    men->register_interface.write(&men->register_interface, ME6_REG_IRQ_SOFTWARE_TRIGGER, 0);
    me6->alarms_enabled = ME6_ALARM_SOFTWARE;
    men->register_interface.write(&men->register_interface, ME6_REG_IRQ_ALARMS_ENABLE, me6->alarms_enabled);

    men->uiq_transfer.current_header = 0;
    men->uiq_transfer.remaining_packet_words = 0;

    return 0;
}

static void
me6_abacus_start_peripherals(struct me6_data * me6)
{
    struct siso_menable * men = me6->men;
    men->register_interface.write(&men->register_interface, ME6_ABACUS_REG_SPI_DISABLE_PIC_INTERFACE, ME6_ABACUS_ENABLE_PIC_INTERFACE);
}

static void
me6_abacus_stop_peripherals(struct me6_data * me6)
{
    struct siso_menable * men = me6->men;
    men->register_interface.write(&men->register_interface, ME6_ABACUS_REG_SPI_DISABLE_PIC_INTERFACE, ME6_ABACUS_DISABLE_PIC_INTERFACE);
}

static int
me6_abacus_cleanup_peripherals(struct me6_data * me6)
{
    struct me6_abacus * self = downcast(me6, struct me6_abacus);

    men_i2c_cleanup_busses(self->i2c_busses, ARRAY_SIZE(self->i2c_busses));

    struct controller_base * spi_base = upcast(&self->spi_controller);
    if (spi_base->destroy != 0) {
        spi_base->destroy(spi_base);
    }
    
    return 0;
}

static struct controller_base *
me6_abacus_get_controller(struct siso_menable * men, uint32_t peripheral)
{
    struct me6_abacus * self = downcast(men->d6, struct me6_abacus);
    for (int i = 0; i < ARRAY_SIZE(self->i2c_busses); ++i) {
        if (self->i2c_busses[i].id == peripheral)
            return upcast(&self->i2c_busses[i].controller);
    }

    if (peripheral == me6_abacus_spi_declaration.id)
        return upcast(&self->spi_controller);

    return NULL;
}

static unsigned long
me6_abacus_alarm_to_event_pl(unsigned long alarms)
{
    unsigned long pl = 0;
    pl |= (alarms & ME6_ALARM_OVERTEMP) ? DEVCTRL_DEVICE_ALARM_TEMPERATURE : 0x0;
    pl |= (alarms & ME6_ALARM_SOFTWARE) ? DEVCTRL_DEVICE_ALARM_SOFTWARE : 0x0;
    pl |= (alarms & ME6_ABACUS_ALARM_PHY_0_MGM) ? DEVCTRL_DEVICE_ALARM_PHY_0 : 0x0;
    pl |= (alarms & ME6_ABACUS_ALARM_PHY_1_MGM) ? DEVCTRL_DEVICE_ALARM_PHY_1 : 0x0;
    pl |= (alarms & ME6_ABACUS_ALARM_PHY_2_MGM) ? DEVCTRL_DEVICE_ALARM_PHY_2 : 0x0;
    pl |= (alarms & ME6_ABACUS_ALARM_PHY_3_MGM) ? DEVCTRL_DEVICE_ALARM_PHY_3 : 0x0;
    pl |= (alarms & ME6_ABACUS_ALARM_POE_FAULT) ? DEVCTRL_DEVICE_ALARM_POE : 0x0;
    return pl;
}

static unsigned long
me6_abacus_event_pl_to_alarm(unsigned long pl)
{
    unsigned long alarms = 0;
    alarms |= (pl & DEVCTRL_DEVICE_ALARM_TEMPERATURE) ? ME6_ALARM_OVERTEMP : 0x0;
    alarms |= (pl & DEVCTRL_DEVICE_ALARM_SOFTWARE) ? ME6_ALARM_SOFTWARE : 0x0;
    alarms |= (pl & DEVCTRL_DEVICE_ALARM_PHY) ?
        (ME6_ABACUS_ALARM_PHY_0_MGM | ME6_ABACUS_ALARM_PHY_1_MGM | ME6_ABACUS_ALARM_PHY_2_MGM | ME6_ABACUS_ALARM_PHY_3_MGM) : 0x0;
    alarms |= (pl & DEVCTRL_DEVICE_ALARM_PHY_0) ? ME6_ABACUS_ALARM_PHY_0_MGM : 0x0;
    alarms |= (pl & DEVCTRL_DEVICE_ALARM_PHY_1) ? ME6_ABACUS_ALARM_PHY_1_MGM : 0x0;
    alarms |= (pl & DEVCTRL_DEVICE_ALARM_PHY_2) ? ME6_ABACUS_ALARM_PHY_2_MGM : 0x0;
    alarms |= (pl & DEVCTRL_DEVICE_ALARM_PHY_3) ? ME6_ABACUS_ALARM_PHY_3_MGM : 0x0;
    alarms |= (pl & DEVCTRL_DEVICE_ALARM_POE) ? ME6_ABACUS_ALARM_POE_FAULT : 0x0;
    return alarms;
}

int
me6_abacus_init(struct siso_menable * men, struct me6_abacus * me6_abacus)
{
    unsigned int result;
    struct me6_data * me6 = upcast(me6_abacus);

    pr_debug(KBUILD_MODNAME "%s\n", __FUNCTION__);

    men->get_controller = me6_abacus_get_controller;
    me6->init_peripherals = me6_abacus_init_peripherals;
    me6->cleanup_peripherals = me6_abacus_cleanup_peripherals;
    me6->start_peripherals = me6_abacus_start_peripherals;
    me6->stop_peripherals = me6_abacus_stop_peripherals;
    me6->alarms_to_event_pl = me6_abacus_alarm_to_event_pl;
    me6->event_pl_to_alarms = me6_abacus_event_pl_to_alarm;

    me6->men = men;

    /* Initialise UIQs */
    result = me6_add_uiqs(men, me6_abacus_uiq_declaration, ARRAY_SIZE(me6_abacus_uiq_declaration));

    return result;
}

