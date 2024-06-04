/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef MENABLE5_H
#define MENABLE5_H

#include "menable.h"

#include <linux/sched.h>
#include <linux/i2c.h>

#include "men_i2c_bus.h"

#include <lib/controllers/spi_v2_controller.h>
#include <lib/controllers/spi_dual_controller.h>

#define ME5_CONFIG     0x0000   /**< Board status */
#define GETFIRMWAREVERSION(x) (((x) & 0xff00) | (((x) >> 16) & 0xff))
#define ME5_CONFIG_EX  0x0001   /**< Board ID */
#define GETBOARDTYPE(x) (((x) >> 16) & 0xffff)

#define ME5_NUMDMA          0x0002
#define ME5_IRQSTATUS       0x0100
#define ME5_IRQACK          0x0102
#define ME5_IRQTYPE         0x0104
#define ME5_IRQENABLE       0x0106

#define ME5_DMAOFFS         0x0110
#define ME5_DMASZ           0x0010

#define ME5_DMACTRL         0x0000
#define ME5_DMAADDR         0x0002
#define ME5_DMAACTIVE       0x0004
#define ME5_DMALENGTH       0x0006
#define ME5_DMACOUNT        0x0008
#define ME5_DMAMAXLEN       0x000a
#define ME5_DMATYPE         0x000c
#define ME5_DMATAG          0x000e

#define ME5_IRQQUEUE        0x0080
#define ME5_IRQQ_LOW        16
#define ME5_IRQQ_HIGH       29

#define ME5_FULLOFFSET     0x2000
#define ME5_UIQCNT         0x0006
#define ME5_FIRSTUIQ       0x0007
#define ME5_FPGACONTROL    0x1010
#define ME5_PCIECONFIG0    0x0010
#define ME5_PCIECONFIGMAX  0x001f

#define ME5_LED_CONTROL    0x0012
#define LED_W_CONTROL_ENABLE_MASK       0x1
#define LED_W_VALUE_MASK                0x1E
#define LED_R_REGISTER_VALID            0x1
#define LED_R_PRESENT_MASK              0x1E
#define LED_R_CONTROL_ENABLED_MASK      0x10000
#define LED_R_VALUE_MASK                0x1E0000

#define ME5_RECONFIGURE_CONTROL 0x0013
#define RECONFIGURE_FLAG    0x1

#define ME5_DMA_FIFO_DEPTH  1LL

#define ME5_CLCAMERASTATUS      0x0a00  /**< CL status bits */
#define ME5_CLCHIPGROUPA_SHIFT  24      /**< Shift CL status to get chip group a configuration */
#define ME5_CLCHIPGROUPA_MASK   0x7     /**< Mask for CL status after shift to get chip group a configuration */
#define ME5_CLCHIPGROUPNOTUSED  0       /**< Chip group not used */
#define ME5_CLSINGLEBASE0       1       /**< Chip 0 used for single base configuration */
#define ME5_CLSINGLEBASE1       2       /**< Chip 1 used for single base configuration */
#define ME5_CLDUALBASE          3       /**< Chips 0 and 1 used for dual base configuration */
#define ME5_CLMEDIUM            4       /**< Chips 0 and 1 used for medium configuration */
#define ME5_CLFULL              5       /**< Chips 0..2 used for full configuration */

/* IRQ BIT MAPPING (FPGA IRQ Status/Acknowledgement/Enable registers) */
//DMA IRQ
#define INT_MASK_DMA_CHANNEL_IRQ            0x000000FF
//Alarms IRQ
#define INT_MASK_ALARMS                     0x0000BF00
// (Detailed Alarms IRQ)
#define INT_MASK_ACTION_CMD_LOST_CHAN_0     0x00000100
#define INT_MASK_ACTION_CMD_LOST_CHAN_1     0x00000200
#define INT_MASK_ACTION_CMD_LOST_CHAN_2     0x00000400
#define INT_MASK_ACTION_CMD_LOST_CHAN_3     0x00000800
#define INT_MASK_ACTION_CMD_LOST            0x00000F00
#define INT_MASK_PHY_MANAGEMENT             0x00001000
#define INT_MASK_POE                        0x00002000
#define INT_MASK_TEMPERATURE_ALARM          0x00008000
//User IRQ
#define INT_MASK_USER_QUEUE_IRQ             0x3FFF0000

/* Notification sent from the driver (Software) */
#define NOTIFICATION_DRIVER_CLOSED  0x01    // Fired when driver closed the process
#define NOTIFICATION_DEVICE_REMOVED 0x02    // Fired when an existing device is removed (E.g. Thunderbolt device)
#define NOTIFICATION_DEVICE_ADDED   0x04    // Fired when a new device is added
#define NOTIFICATION_DEVICE_ALARM   0x08    // Fired when a notification IRQ is received

#define BPI_ADDRESS_REGISTER    0x1003
#define BPI_DATA_WRITE_REGISTER 0x1004

/* The Marathon boards exist in two variants with different flash memory chips */
typedef enum
{
    FLASH_TYPE_BPI,
    FLASH_TYPE_SPI
} FlashMemoryType;

/*
 * These defines are used for reconfigurating a board with SPI flash memory
 * over the BPI Firmware interface.
 */
#define SPI_RECONFIGURATION_ADDRESS_WORD BIT(21)
#define SPI_RECONFIGURATION_DATA_WORD    0x1E0000

struct menable_uiq;

extern struct class *menable_notify_class;

struct me5_sgl {
	__le64 addr[7];
	__le64 next;
} __attribute__ ((packed));

struct me5_data {
    struct siso_menable * men;

    void *dummypage;
    dma_addr_t dummypage_dma;

    uint32_t design_crc;
    char design_name[65];                           /* user supplied name of design (for IPC) */

    // IRQ
    spinlock_t irqmask_lock;                        /* Protects IRQ Enable register (ME5_IRQENABLE) */
    uint32_t irq_wanted;                            /* 32-bits mapped to the IRQs that are of interest, protected by irqmask_lock */

    spinlock_t notification_data_lock;              /* Protects irq_wanted_alarms_status, notification/time_stamp variables */
    uint32_t irq_alarms_status;                     /* Alarms of interest status (0 = off, 1 = fired) */

    // Notifications
    unsigned long notifications;                    /* Current available notifications */
    unsigned long notification_time_stamp;          /* Current notifications time stamp */

    // Notifications Handlers
    spinlock_t notification_handler_headlock;       /* Protects notification_handler_heads list */
    struct list_head notification_handler_heads;    /* All buffer heads */

    // IRQ Work
    struct work_struct irq_notification_work;       /* Called directly after me5_irq is executed, to complete IRQ handling */

    // Temperature Alarm
    /*
     * Unlike other notifications, device over temperature requires long time to be fixed.
     * During this period, receiving repetitive interrupts are not required, therefore the alarm is
     * set off and reset every temperatureAlarmPeriod (default: 1000 ms).  temperature_alarm_work
     * is responsible for resetting the alarm.
     */
    struct delayed_work temperature_alarm_work;     /* called after temperature alarm is set (delayed for temperatureAlarmPeriod milliseconds) */
    unsigned int temperature_alarm_period;            /* Minimum period between two consecutive temperature alarms (milliseconds) */

    int (*init_peripherals)(struct me5_data * self);
    int (*cleanup_peripherals)(struct me5_data * self);
};

extern struct attribute_group me5_attribute_group;

struct me5_marathon {
    DERIVE_FROM(me5_data);
    struct i2c_master_core i2c_cores[1];
    struct men_i2c_bus i2c_busses[2];
};

int me5_marathon_init(struct siso_menable * men, struct me5_marathon * me5);

struct me5_ironman {
    DERIVE_FROM(me5_data);
    struct i2c_master_core i2c_cores[1];
    struct men_i2c_bus i2c_busses[1];
    struct spi_dual_controller spi_controller;
};

int me5_ironman_init(struct siso_menable * men, struct me5_ironman * me5);

struct me5_tdi {
    DERIVE_FROM(me5_data);
    struct i2c_master_core i2c_cores[1];
    struct men_i2c_bus i2c_busses[5];
    struct spi_v2_controller spi_controller;
};
/* TODO implement in menable5_tdi.c */
static inline int me5_tdi_init(struct siso_menable * men, struct me5_tdi * me5) { return 0; }

struct me5_abacus {
    DERIVE_FROM(me5_data);
    struct i2c_master_core i2c_cores[1];
    struct men_i2c_bus i2c_busses[2];
    struct spi_v2_controller spi_controller;
};

int me5_abacus_init(struct siso_menable * men, struct me5_abacus * me5);

const struct attribute_group ** me5_abacus_init_attribute_groups(struct siso_menable *men);

#endif
