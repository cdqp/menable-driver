/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "menable6.h"
#include "lib/boards/me6_defines.h"
#include "lib/boards/me6_impulse.h"

#include "men_i2c_bus.h"
#include "menable_ioctl.h"
#include "sisoboards.h"

#include <lib/boards/boards.h>
#include <lib/boards/peripheral_declaration.h>
#include <lib/boards/basler_ic.h>
#include <lib/boards/me6_impulse.h>
#include <lib/helpers/dbg.h>
#include <lib/helpers/error_handling.h>
#include <lib/frontends/camera_frontend.h>
#include <lib/dma/messaging_dma_controller.h>

#include <linux/kernel.h>
#include <linux/dma-mapping.h>

static int
me6_impulse_init_camera_frontend(struct siso_menable * men) {

    men->camera_frontend = camera_frontend_factory(men->pci_device_id, men->pcie_dsn_low, men->pcie_dsn_high, &men->register_interface);

    if (men->camera_frontend == NULL) {
        /* For the me6, a camera frontend is mandatory */
        dev_err(&men->dev, "No Camera Frontend available.\n");
        return -EFAULT;
    }

    return 0;
}

static int
me6_impulse_init_messaging_dma_mem(struct me6_data * me6) {

    struct siso_menable * men = me6->men;

    /* Declared on function scope to be available in error handling section */
    int status = STATUS_ERROR;
    int cleanup_idx = -1;
    uint64_t * buffer_dma_addresses = NULL;

    men_get_messaging_dma_declaration(men->pci_device_id, men->pcie_dsn_low, &men->messaging_dma_declaration);

    if (men->messaging_dma_declaration) {
        /* Messaging DMA is supported */

        /* Allocate the array for the addresses */
        me6->messaging_dma_buffers = kmalloc_array(men->messaging_dma_declaration->num_buffers, sizeof(*me6->messaging_dma_buffers), GFP_KERNEL);
        if (me6->messaging_dma_buffers == NULL) {
            dev_err(&men->dev, "Failed to allocate array for messaging DMA buffer addresses.\n");
            status = STATUS_ERR_INSUFFICIENT_MEM;

            goto err_alloc_buffers_array;
        }

        me6->messaging_dma_buffer_bus_addresses = kmalloc_array(men->messaging_dma_declaration->num_buffers, sizeof(dma_addr_t), GFP_KERNEL);
        if (me6->messaging_dma_buffer_bus_addresses == NULL) {
            dev_err(&men->dev, "Failed to allocate array for messaging DMA buffer bus addresses.\n");
            status = STATUS_ERR_INSUFFICIENT_MEM;

            goto err_alloc_dma_addresses_array;
        }

        /* Allocate one page of DMA capable memory per buffer */
        for (int buf_idx = 0; buf_idx < men->messaging_dma_declaration->num_buffers; ++buf_idx) {
            dma_addr_t dma_address;

            void * buffer_address = dma_alloc_coherent(men->dev.parent, PCI_PAGE_SIZE, &dma_address, GFP_ATOMIC);
            if (buffer_address == NULL) {
                dev_err(&men->dev, "Failed to allocate messaging dma buffer number %d.\n", buf_idx);
                cleanup_idx = buf_idx - 1;
                status = STATUS_ERR_INSUFFICIENT_MEM;

                goto err_alloc_buffer;
            }

            me6->messaging_dma_buffers[buf_idx] = (uint32_t*)buffer_address;
            me6->messaging_dma_buffer_bus_addresses[buf_idx] = (uint64_t)dma_address;
        }
        cleanup_idx = men->messaging_dma_declaration->num_buffers;

    } else {
        dev_info(&men->dev, "Messaging DMA not supported.\n");
    }

    return STATUS_OK;

    /* Error Handling Section */

err_alloc_buffer:
    /* Cleanup already allocated buffer pages */
    for (; cleanup_idx >= 0; --cleanup_idx) {
        dma_free_coherent(men->dev.parent, PAGE_SIZE, me6->messaging_dma_buffers[cleanup_idx], (dma_addr_t)buffer_dma_addresses[cleanup_idx]);
    }

    /* Cleanup the array for the bus addresses */
    kfree(me6->messaging_dma_buffer_bus_addresses);
    me6->messaging_dma_buffer_bus_addresses = NULL;

err_alloc_dma_addresses_array:
    /* Cleanup the array for the buffer cpu addresses */
    kfree(me6->messaging_dma_buffers);
    me6->messaging_dma_buffers = NULL;

err_alloc_buffers_array:
    /* Make clear, that we have no messaging dma available */
    men->messaging_dma_declaration = NULL;

    return status;

}

static int
me6_impulse_init_peripherals(struct me6_data * me6)
{
    struct me6_impulse * self = downcast(me6, struct me6_impulse);
    struct siso_menable * men = me6->men;

    men_init_i2c_peripherals(men, &me6_impulse_i2c_declaration, 1,
                             &self->i2c_controller, self->i2c_busses);

    int ret = spi_v2_controller_init(&self->spi_controller,
                                     &men->register_interface,
                                     &men->flash_lock,
                                     me6_impulse_spi_declaration.control_register);

    if (ret != 0) {
        dev_warn(&men->dev, "Failed to init SPI.\n");
        return ret;
    }

    /* Init Camera Frontend */
    if (men->camera_frontend == NULL) {
        ret = me6_impulse_init_camera_frontend(men);
        if (ret != 0) {
            dev_warn(&men->dev, "Failed to init camera frontend.\n");

            /* TODO: [RKN] Should we return here?!? */
        }
    }

    if (men->camera_frontend != NULL) {
        men->camera_frontend->reset(men->camera_frontend);
    }

#if defined(DEBUG)
    { /* TODO: Debugging code, remove later; read SPI id */
        struct controller_base * cb = upcast(&self->spi_controller);

        u8 command_data = '\x9e';
        u8 reply_data[3];
        struct burst_header command = { BURST_TYPE_WRITE, SPI_POST_BURST_FLAG_LEAVE_CS_ASSERTED, 1, (u64)&command_data };
        struct burst_header reply = { BURST_TYPE_READ, SPI_BURST_FLAG_NONE, 1, (u64)&reply_data[2]};

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

    /* Init Messaging DMA */
    if (men->messaging_dma_declaration != NULL) {
        int result = messaging_dma_controller_init(&me6->messaging_dma_controller, &men->register_interface,
                                                   men->messaging_dma_declaration->control_register, men->messaging_dma_declaration->init_register,
                                                   men->messaging_dma_declaration->num_buffers,
                                                   me6->messaging_dma_buffers,
                                                   me6->messaging_dma_buffer_bus_addresses);

        if (MEN_IS_ERROR(result)) {
        	men->messaging_dma_declaration = NULL;
            dev_warn(&men->dev, "Failed to init messaging DMA controller. Error code: %d.\n", result);
            return result;
        }

        me6->messaging_dma_controller.start(&me6->messaging_dma_controller);
    }

    /* Init alarms */
    men->register_interface.write(&men->register_interface, ME6_REG_IRQ_SOFTWARE_TRIGGER, 0);
    me6->alarms_enabled = ME6_ALARM_SOFTWARE;
    men->register_interface.write(&men->register_interface, ME6_REG_IRQ_ALARMS_ENABLE, me6->alarms_enabled);

    /* Init UIQ transfer state */
    men->uiq_transfer.current_header = 0;
    men->uiq_transfer.remaining_packet_words = 0;

    return 0;
}

static void
me6_impulse_start_peripherals(struct me6_data * me6)
{
}

static void
me6_impulse_stop_peripherals(struct me6_data * me6)
{
}

static int
me6_impulse_cleanup_peripherals(struct me6_data * me6)
{
    struct me6_impulse * self = downcast(me6, struct me6_impulse);
    struct siso_menable * men = me6->men;

    if (me6->men->camera_frontend != NULL) {
        me6->men->camera_frontend->destroy(me6->men->camera_frontend);
        me6->men->camera_frontend = NULL;
    }

    men_i2c_cleanup_busses(self->i2c_busses, ARRAY_SIZE(self->i2c_busses));

    struct controller_base * spi_base = upcast(&self->spi_controller);
    if (spi_base->destroy != NULL) {
        spi_base->destroy(spi_base);
    }

    if (men->messaging_dma_declaration != NULL) {
        messaging_dma_controller_destroy(&me6->messaging_dma_controller);
    }

    return 0;
}

static struct controller_base *
me6_impulse_get_controller(struct siso_menable * men, uint32_t peripheral)
{
    struct me6_impulse * self = downcast(men->d6, struct me6_impulse);
    for (int i = 0; i < ARRAY_SIZE(self->i2c_busses); ++i) {
        if (self->i2c_busses[i].id == peripheral)
            return upcast(&self->i2c_busses[i].controller);
    }

    if (peripheral == me6_impulse_spi_declaration.id)
        return upcast(&self->spi_controller);

    return NULL;
}

static unsigned long
me6_impulse_alarm_to_event_pl(unsigned long alarms)
{
    unsigned long pl = 0;
    pl |= (alarms & ME6_ALARM_OVERTEMP) ? DEVCTRL_DEVICE_ALARM_TEMPERATURE : 0x0;
    pl |= (alarms & ME6_ALARM_SOFTWARE) ? DEVCTRL_DEVICE_ALARM_SOFTWARE : 0x0;
    return pl;
}

static unsigned long
me6_impulse_event_pl_to_alarm(unsigned long pl)
{
    unsigned long alarms = 0;
    alarms |= (pl & DEVCTRL_DEVICE_ALARM_TEMPERATURE) ? ME6_ALARM_OVERTEMP : 0x0;
    alarms |= (pl & DEVCTRL_DEVICE_ALARM_SOFTWARE) ? ME6_ALARM_SOFTWARE : 0x0;
    return alarms;
}

static int
me6_impulse_init_uiqs(struct siso_menable * men) {

    struct me6_data * me6 = men->d6;

    me6->num_uiq_declarations = men_get_uiq_declaration(men->pci_device_id, &me6->uiq_declarations);

    if (me6->uiq_declarations != NULL) {
        return me6_add_uiqs(men, me6->uiq_declarations, me6->num_uiq_declarations);
    } else {
        dev_err(&men->dev, "[UIQ] No UIQ declarations found for board type 0x%04x\n", men->pci_device_id);
        return -EINVAL;
    }
}

static int
me6_impulse_exit(struct me6_data * self) {
    struct siso_menable * men = self->men;

    if (men->messaging_dma_declaration != NULL) {
        /*
         * Cleanup the buffers
         */
        for (int buf_idx = 0; buf_idx < men->messaging_dma_declaration->num_buffers; ++buf_idx) {
            dma_free_coherent(men->dev.parent, PAGE_SIZE, self->messaging_dma_buffers[buf_idx],
                              (dma_addr_t)self->messaging_dma_buffer_bus_addresses[buf_idx]);
        }

        /* Cleanup the array for buffers' addresses */
        kfree(self->messaging_dma_buffers);
        self->messaging_dma_buffers = NULL;

        /* Cleanup the array for the buffers' bus addresses */
        kfree(self->messaging_dma_buffer_bus_addresses);
        self->messaging_dma_buffer_bus_addresses = NULL;

        /* Missing declaration object indicates that the function is not available */
        men->messaging_dma_declaration = NULL;
    }

    return STATUS_OK;
}

int
me6_impulse_init(struct siso_menable * men)
{
    struct me6_data * me6 = men->d6;

    pr_debug(KBUILD_MODNAME " %s\n", __FUNCTION__);

    men->get_controller = me6_impulse_get_controller;
    me6->init_peripherals = me6_impulse_init_peripherals;
    me6->cleanup_peripherals = me6_impulse_cleanup_peripherals;
	me6->start_peripherals = me6_impulse_start_peripherals;
	me6->stop_peripherals = me6_impulse_stop_peripherals;
    me6->alarms_to_event_pl = me6_impulse_alarm_to_event_pl;
    me6->event_pl_to_alarms = me6_impulse_event_pl_to_alarm;
    me6->exit = me6_impulse_exit;

    me6->men = men;

    /* Init Messaging DMA memory */
    int result = me6_impulse_init_messaging_dma_mem(me6);
    if (MEN_IS_ERROR(result)) {
        dev_warn(&me6->men->dev, "Failed to initialize Messaging DMA memory.\n");
        return result;
    }

    /* Initialise UIQs */
    return me6_impulse_init_uiqs(men);
}
