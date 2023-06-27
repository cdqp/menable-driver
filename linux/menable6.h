/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef MENABLE6_H
#define MENABLE6_H

#include "menable.h"

#include <linux/sched.h>
#include <linux/dma-mapping.h>

#include "uiq.h"

#include <lib/boards/me6_defines.h>
#include <lib/boards/peripheral_declaration.h>
#include <lib/fpga/mcap_lib.h>
#include <lib/controllers/spi_v2_controller.h>
#include <lib/dma/messaging_dma_controller.h>
#include <lib/uiq/uiq_defines.h>
#include "men_i2c_bus.h"


enum me6_irq_index {
    ME6_IRQ_LEVEL_INDEX,
    ME6_IRQ_EVENT_INDEX,
    ME6_IRQ_DMA_0_INDEX,
    ME6_IRQ_DMA_1_INDEX,
    ME6_IRQ_DMA_2_INDEX,
    ME6_IRQ_DMA_3_INDEX,
    ME6_NUM_IRQS
};

#define ME6_DMA_SGL_ADDR_FIFO_DEPTH 17LL /**< SGL FIFO depth */

struct mcap_dev;

struct me6_sgl {
    __le64 next;
    __le64 data[7];
} __attribute__ ((packed));

#define ME6_SGL_ENTRIES 5

/* Notification sent from the driver (Software) */
#define NOTIFICATION_DRIVER_CLOSED  0x01    // Fired when driver closed the process
#define NOTIFICATION_DEVICE_REMOVED 0x02    // Fired when an existing device is removed (E.g. Thunderbolt device)
#define NOTIFICATION_DEVICE_ADDED   0x04    // Fired when a new device is added
#define NOTIFICATION_DEVICE_ALARM   0x08    // Fired when a notification IRQ is received

extern struct class *menable_notify_class;

struct me6_data {
    struct siso_menable * men;

    void *dummypage;
    dma_addr_t dummypage_dma;
    uint32_t payload_size;

    char design_name[65];
    uint32_t design_crc;

    int num_vectors, vectors[ME6_NUM_IRQS];

    spinlock_t notification_data_lock;
    unsigned long notifications;
    unsigned long notification_time_stamp;
    unsigned long alarms_status;
    unsigned long alarms_enabled;
    spinlock_t notification_handler_headlock;
    struct list_head notification_handler_heads;
    struct work_struct irq_notification_work;
    struct delayed_work temperature_alarm_work;
    unsigned int temperature_alarm_period;

    struct mcap_dev mdev;

    messaging_dma_controller messaging_dma_controller;
    /*
     * TODO: [RKN] Here (on linux) we do not allocate all the buffer memory in
     *       one go, because there does not seem to an API that allows to allocate
     *       uncontiguous dma memory and contiguous allocation is a risk, since
     *       it is more likely not to be satisfied. Therefore we need an array of buffers.
     *
     *       The array is duplicated (the pointers, not the memory) in the messaging_dma_controller.
     *       Can we avoid this duplication?
     */
    uint32_t ** messaging_dma_buffers; /* size will be the same as the number of buffers in the messaging dma declaration */
    dma_addr_t * messaging_dma_buffer_bus_addresses;

    /* TODO: [RKN] These are only used for impulse boards. Use for others as well? */
    uint32_t num_uiq_declarations;
    uiq_declaration * uiq_declarations;

    int (*init_peripherals)(struct me6_data * self);
    int (*cleanup_peripherals)(struct me6_data * self);
    void (*start_peripherals)(struct me6_data * self);
    void (*stop_peripherals)(struct me6_data * self);
    unsigned long (*alarms_to_event_pl)(unsigned long alarm);
    unsigned long (*event_pl_to_alarms)(unsigned long alarm);
    int (*exit)(struct me6_data * self);
};

extern struct attribute_group me6_attribute_group;

struct me6_abacus {
    DERIVE_FROM(me6_data);
    struct spi_v2_controller spi_controller;
    struct i2c_master_core i2c_controller;
    struct men_i2c_bus i2c_busses[2];
};

int me6_abacus_init(struct siso_menable * men, struct me6_abacus * me6);

const struct attribute_group ** me6_abacus_init_attribute_groups(struct siso_menable *men);

struct me6_impulse {
    DERIVE_FROM(me6_data);
    struct spi_v2_controller spi_controller;
    struct i2c_master_core i2c_controller;
    struct men_i2c_bus i2c_busses[2];
};

/**
 * Initializes the member `d6` within the given menable struct with a
 * me6_impulse instance.
 *
 * @param men The menable struct instance for wich the `d6` member should be initialized.
 * @return Status code. 0: succes, <0: error
 */
int me6_impulse_init(struct siso_menable * men);

int me6_add_uiqs(struct siso_menable * men, struct uiq_declaration * decls, unsigned int elems);

int me6_update_board_status(struct siso_menable * men);

#endif
