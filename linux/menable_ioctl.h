/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef MENABLE_IOCTL_H
#define MENABLE_IOCTL_H

#include <lib/os/types.h>
#include <linux/time.h>

#define MEN_IOCTL(name, x) \
    IOCTL_##name = (x)

#define MEN_IOCTL_EX(name, x) \
    IOCTL_EX_##name = ((x) + 64)

#if BITS_PER_LONG > 32

#define MEN_IOCTL64(name, x) \
    IOCTL_##name##32 = (x), \
    MEN_IOCTL(name, ((x) + 128))

#else /* BITS_PER_LONG > 32 */

#define MEN_IOCTL64(name, x) \
    IOCTL_##name##32 = (x), \
    MEN_IOCTL(name, x)

#endif /* BITS_PER_LONG > 32 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
typedef struct timespec menable_ioctl_timespec_t;
#else
typedef struct __kernel_old_timespec menable_ioctl_timespec_t;
#endif

struct men_io_range32 {
    uint32_t start;
    uint32_t length;
    int subnr;
    unsigned int headnr;
} __attribute__ ((packed));

struct men_io_range {
    unsigned long start;
    unsigned long length;
    long subnr;
    unsigned int headnr;
} __attribute__ ((packed));

struct men_io_bufidx32 {
    unsigned int headnr;
    int index;
};

struct men_io_bufidx {
    unsigned int headnr;
    long index;
};

struct men_io_bufwait32 {
    int index;
    unsigned int dmachan;
    unsigned int timeout;
};

struct men_io_bufwait {
    long index;
    unsigned int dmachan;
    unsigned int timeout;
};

struct mm_create_s32 {
    uint64_t maxsize;
    unsigned int subbufs;
};

struct mm_create_s {
    uint64_t maxsize;
    long subbufs;
};

struct fg_ctrl_s {
    unsigned int cmd;

    union {
        struct fg_start_s {
            int start_param;
            int param;
            unsigned int timeout;
            unsigned int chan;
            unsigned int act_size;
            int mode;
            unsigned int head;
            unsigned int start_buf;
            int dma_mode;
            int transfer_todo;
            unsigned int dma_dir;
        } fg_start;
    } u;
};

enum men_dma_dir {
    MEN_DMA_DIR_DEVICE_TO_CPU,
    MEN_DMA_DIR_CPU_TO_DEVICE
};

struct fg_ctrl {
    int mode;
    int timeout;
    long transfer_todo;
    unsigned int chan;
    unsigned int head;
    long start_buf;
    int dma_dir;
};

struct dma_timestamp32 {
    unsigned int head;
    int buf;
    menable_ioctl_timespec_t stamp;
};

struct dma_timestamp {
    unsigned int head;
    long buf;
    menable_ioctl_timespec_t stamp;
};

struct handshake_frame {
    union {
        struct {
            unsigned int head;
            unsigned int mode;
        };
        long long frame;
    };
};

struct bufstatus {
    union {
        struct {
            unsigned int head;
            long long index;
        } idx;
        struct {
            long long free;
            long long grabbed;
            long long locked;
            long long lost;
            unsigned int is_locked;
        } status;
    };
};

struct men_get_device_status_o_v6 { /* Version 1..5 has never been implemented on Linux */
    unsigned int _size;
    unsigned int _version;
    unsigned int status;
    unsigned int config;
    unsigned int config_ext;
    unsigned int dma_count;
    unsigned int uiq_count;
    unsigned int node_number;
    unsigned int group_affinity;
    unsigned int affinity_high;
    unsigned int affinity_low;
    unsigned int pcie_dsn_high;
    unsigned int pcie_dsn_low;
    unsigned int pci_vendor_device_id;
    unsigned int pci_subsys_vendor_device_id;
    unsigned int pcie_payload_mode;
    unsigned int pcie_link_training;
    /* Version 6 */
};

struct men_get_device_status_o_v7 {
    unsigned int _size;
    unsigned int _version;
    unsigned int status;
    unsigned int config;
    unsigned int config_ext;
    unsigned int dma_count;
    unsigned int uiq_count;
    unsigned int node_number;
    unsigned int group_affinity;
    unsigned int affinity_high;
    unsigned int affinity_low;
    unsigned int pcie_dsn_high;
    unsigned int pcie_dsn_low;
    unsigned int pci_vendor_device_id;
    unsigned int pci_subsys_vendor_device_id;
    unsigned int pcie_payload_mode;
    unsigned int pcie_link_training;
    /* Version 6 */
    unsigned int fpga_dna[3];
    /* Version 7 */
};

struct men_get_device_status_o_v8 {
    unsigned int _size;
    unsigned int _version;
    unsigned int status;
    unsigned int config;
    unsigned int config_ext;
    unsigned int dma_count;
    unsigned int uiq_count;
    unsigned int node_number;
    unsigned int group_affinity;
    unsigned int affinity_high;
    unsigned int affinity_low;
    unsigned int pcie_dsn_high;
    unsigned int pcie_dsn_low;
    unsigned int pci_vendor_device_id;
    unsigned int pci_subsys_vendor_device_id;
    unsigned int pcie_payload_mode;
    unsigned int pcie_link_training;
    /* Version 6 */
    unsigned int fpga_dna[3];
    /* Version 7 */
    unsigned int va_event_offset;
    /* Version 8 */
};

struct men_device_control_i {
    unsigned int _size;
    unsigned int _version;
    unsigned int command;
    union {
        struct {
            unsigned short led_status;
        } set_leds;
        struct {
            unsigned int chan;
            unsigned int param;
        } get_dma_param;
        struct {
            unsigned int chan;
            unsigned int param;
            unsigned long long value;
        } set_dma_param;
        /* Version 1 */
        struct {
            unsigned long event;
            unsigned long pl;
            unsigned long ph;
        } reset_async_event;
        /* Version 1.1 */
        struct {
            unsigned long magic;
        } reconfigure_fpga;
        /* Version 1.2 */
        struct {
            unsigned long event;
            unsigned long pl;
            unsigned long ph;
        } set_async_event;
        /* Version 1.3 */
        struct {
            unsigned int num_events;
        } alloc_va_events;
        /* Version 1.4 */
    } args;
};

struct men_device_control_o_v2 { /* Version 1 has never been implemented on Linux */
    unsigned int _size;
    unsigned int _version;
    union {
        struct {
            unsigned short led_present;
            unsigned short led_status;
        } get_leds;
        struct {
            unsigned long long value;
        } get_dma_param;
        /* Version 1 */
        struct {
            unsigned long status;
        } get_status;
        /* Version 1.1 */
        struct {
            unsigned long event;
            unsigned long pl;
            unsigned long ph;
        } get_async_event;
        /* Version 2 */
    } args;
};

enum {
    DEVCTRL_DMA_PARAM_STOP_TIMEOUT,
    DEVCTRL_DMA_PARAM_CURRENT_FRAME_NUMBER,
};

enum {
    DEVCTRL_GET_LEDS,
    DEVCTRL_SET_LEDS,
    DEVCTRL_GET_DMA_PARAM,
    DEVCTRL_SET_DMA_PARAM,
    DEVCTRL_GET_ASYNC_NOTIFY,
    DEVCTRL_RESET_ASYNC_NOTIFY,
    DEVCTRL_GET_STATUS,
    DEVCTRL_RECONFIGURE_FPGA,
    DEVCTRL_SET_ASYNC_NOTIFY,
    DEVCTRL_ALLOC_VA_EVENTS,
    DEVCTRL_RECONFIGURE_FPGA_FROM_SPI
};

enum {
    DEVCTRL_ASYNC_NOTIFY_DRIVER_CLOSED,
    DEVCTRL_ASYNC_NOTIFY_DEVICE_REMOVED,
    DEVCTRL_ASYNC_NOTIFY_DEVICE_ADDED, /* Not raised in driver */
    DEVCTRL_ASYNC_NOTIFY_DEVICE_ALARM,
};

#define DEVCTRL_DEVICE_ALARM_TEMPERATURE    0x00000001ul   // Temperature alarm
#define DEVCTRL_DEVICE_ALARM_PHY            0x00000002ul   // PHY management, shared
#define DEVCTRL_DEVICE_ALARM_POE            0x00000004ul   // POE Chip Fault Interrupt
#define DEVCTRL_DEVICE_ALARM_ACL_0          0x00000010ul   // Action command lost on channel [0]
#define DEVCTRL_DEVICE_ALARM_ACL_1          0x00000020ul   // Action command lost on channel [1]
#define DEVCTRL_DEVICE_ALARM_ACL_2          0x00000040ul   // Action command lost on channel [2]
#define DEVCTRL_DEVICE_ALARM_ACL_3          0x00000080ul   // Action command lost on channel [3]
#define DEVCTRL_DEVICE_ALARM_ACL_MASK       0x000000F0ul   // Action command lost mask
#define DEVCTRL_DEVICE_ALARM_PHY_0          0x00000100ul   // PHY Chip 0 management interrupt
#define DEVCTRL_DEVICE_ALARM_PHY_1          0x00000200ul   // PHY Chip 0 management interrupt
#define DEVCTRL_DEVICE_ALARM_PHY_2          0x00000400ul   // PHY Chip 0 management interrupt
#define DEVCTRL_DEVICE_ALARM_PHY_3          0x00000800ul   // PHY Chip 0 management interrupt
#define DEVCTRL_DEVICE_ALARM_PHY_MASK       0x00000F02ul   // PHY management interrupt mask
#define DEVCTRL_DEVICE_ALARM_SOFTWARE       0x80000000ul   // Software triggered test interrupt

#define DEVCTRL_STATUS_CONFIGURED       0x00000001ul
#define DEVCTRL_STATUS_LOCKED           0x00000002ul
#define DEVCTRL_STATUS_OVERTEMP         0x40000000ul
#define DEVCTRL_STATUS_DEAD             0x80000000ul

#define DEVCTRL_RECONFIGURE_MAGIC       0xa144b556ul
#define DEVCTRL_CLOSE_DRIVER_MAGIC      0x6e7745f1ul



struct men_configure_fpga {
    unsigned int _size; /* Size of this header */
    unsigned int _version; /* Version of this header */
	unsigned int flags; /* Flags (platform specific; see below) */
	unsigned int length; /* Length of the data */
    uint64_t buffer_address; /* Data buffer */
	/* Version 1 */
};

#define CONFIGURE_ME6_BITSTREAM_TYPE_MASK 0x00000001ul
#define CONFIGURE_ME6_PARTIAL_BITSTREAM   0x00000000ul
#define CONFIGURE_ME6_FULL_BITSTREAM      0x00000001ul
#define CONFIGURE_ME6_DESIGN_SWITCH       0x00000002ul
#define CONFIGURE_ME6_SWAP_BYTES          0x00000004ul

#include "men_ioctl_codes.h"

struct siso_menable;

extern void warn_wrong_iosize(struct siso_menable *men, unsigned int cmd,
        const size_t expsize);

#endif
