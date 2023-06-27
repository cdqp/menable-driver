/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef MENABLE_H
#define MENABLE_H

/* Disable dynamic debug for this module.
 * Make sure to include menable.h before any other includes
 * to get the desired effect
 */
#undef CONFIG_DYNAMIC_DEBUG

#define DRIVER_NAME "menable"
#define DRIVER_DESCRIPTION "microEnable 4/5/6 driver"
#define DRIVER_VENDOR "Basler AG"
#define DRIVER_VERSION "5.1.0"

//#define ENABLE_DEBUG_MSG 1
//#define DEBUG_SGL 1
#define NO_SYSTEM_I2C 1

#include <linux/version.h>
#include <lib/frontends/cxp_frontend.h>
#include <lib/boards/peripheral_declaration.h>
// TODO: [RKN] It's ugly (and not intended) that the driver needs to include a
//             header from inside the 'os' directory. See if there is a good way to avoid this.
#include <lib/os/linux/pci_config_interface_linux.h>
#include "menable_uapi.h"
#include "menable_ioctl.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
#error This driver requires at least kernel 2.6.23
#endif

/* Maximum number of devices the driver can handle. Only
* effect is the size of the device number region. Increase
* it to any size you need. */
#define MEN_MAX_NUM     8

#define PCI_VENDOR_PLX  0x10b5
#define PCI_VENDOR_SISO 0x1ae8

#ifndef PCI_PAGE_SIZE
#define PCI_PAGE_SIZE 4096
#endif

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/mutex.h>

#include "lib/fpga/menable_register_interface.h"
#include "lib/uiq/uiq_transfer_state.h"

#include "sisoboards.h"

#define FREE_LIST 0
#define GRABBED_LIST 1
#define HOT_LIST 2
#define NO_LIST 3
#define READY_LIST 4

#define ASCII_STR_TO_INT(x) ((u32)((x[0] << 24) | (x[1] << 16) | (x[2] << 8) | (x[3])))
#define ASCII_CHAR4_TO_INT(d, c, b, a) ((u32)((d << 24) | (c << 16) | (b << 8) | (a)))
#define ASCII_CHAR2_TO_INT(b, a) ((u32)((b << 8) | (a)))
#define ASCII_CHAR_FROM_INT(x, n) ((char)(((x) >> (n * 8)) & 0xFF))

/* no device currently supports more than 8 DMA channels */
#define MEN_MAX_DMA 8
#define MEN_MAX_UIQ_PER_FPGA 16

/* currently at most 2 FPGAs implement IRQs */
#define MAX_FPGAS 2

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
typedef time_t menable_time_t;
typedef struct timespec menable_timespec_t;
#else
typedef time64_t menable_time_t;
typedef struct timespec64 menable_timespec_t;
#endif

void menable_get_ts(menable_timespec_t *ts);

struct men_dma_chain {
    union {
        struct me4_sgl *pcie4;
        struct me6_sgl *pcie6;
    };
    struct men_dma_chain *next;
};

struct menable_dmabuf {
    struct men_dma_chain * dma_chain; /* dma descriptor list of buffer */
    dma_addr_t dma;                   /* dma address of dmat */
    struct scatterlist * sg;          /* sg-list of DMA buffer */
    int num_used_sg_entries;          /* number of used entries in sg */
    int num_sg_entries;               /* number of real entries in dmasg */
    struct list_head node;            /* entry in dmaheads *_list */
    unsigned int listname;            /* which list are we currently in? */
    uint64_t dma_length;              /* length of valid data */
    uint64_t buf_length;              /* length of buffer */
    uint32_t dma_tag;                 /* last tag sent by DMA channel */
    uint64_t frame_number;            /* the frame number of the current frame in the buffer */
    long index;                       /* index in DMA channels bufs[] */
    menable_timespec_t timestamp;        /* time when "grabbed" irq was handled */
};

struct menable_dmachan;

struct menable_dmahead {
    struct list_head node;          /* entry in parents heads list*/
    unsigned int id;                /* id number to be used by userspace */
    struct menable_dmabuf **bufs;   /* array of buffers */
    long num_sb;                    /* length of bufs - TODO: This should be an unsigned type, shouldn't it? */
	struct menable_dmabuf dummybuf; /* dummy buffer for landing dma when no buffers are free */
    struct menable_dmachan *chan;   /* channel this head is currently linked to */
};

struct completion;
struct siso_menable;

struct menable_dma_wait {
    struct list_head node;          /* entry in the parents wait_list */
    struct completion cpl;          /* used to wait for specific imgcnt */
    uint64_t frame;                 /* image number to wait for, 0 if none */
};

enum men_dma_chan_state {
    MEN_DMA_CHAN_STATE_STOPPED,
    MEN_DMA_CHAN_STATE_ACTIVE,
    MEN_DMA_CHAN_STATE_FINISHED,
    MEN_DMA_CHAN_STATE_IN_SHUTDOWN
};

struct menable_dmachan {
    struct device dev;
    struct siso_menable *parent;

    spinlock_t chanlock;            /* lock to protect administrative changes */
    struct menable_dmahead *active; /* active dma_head */
    unsigned char number;           /* number of DMA channel on device */
    unsigned char fpga;             /* FPGA index this channel belongs to */
    unsigned int mode:6;            /* streaming or controlled */
    unsigned int direction:2;       /* DMA_{TO,FROM]_DEVICE */
    unsigned int state:2;           /* 0: stopped, 1: active, 2: finished, 3: in shutdown */
    unsigned int ackbit:5;          /* bit in irqack */
    unsigned int enablebit:5;       /* bit in irqenable */
    unsigned int iobase;            /* base address for register space */
    unsigned int irqack;            /* IRQ ACK register */
    unsigned int irqenable;         /* IRQ enable register */

    spinlock_t listlock;            /* lock to protect list changes */
    uint64_t imgcnt;                /* absolute image number of this DMA */
    uint64_t goodcnt;               /* number of transfers to real buffers */
    uint64_t latest_frame_number;   /* number of acquired images during current acquisition */
    unsigned int free_count;        /* number of entries in free_list */
    unsigned int ready_count;       /* number of entries in ready_list */
    unsigned int grabbed_count;     /* number of entries in grabbed_list */
    unsigned int hot_count;         /* number of entries in hot_list */
    unsigned int lost_count;        /* lost_count pictures */
    unsigned int locked_count;      /* locked pictures, TODO: Add a locked list for more consistent buffer handling */
    struct list_head free_list;     /* list of free buffers */
    struct list_head ready_list;    /* buffers that are queud and ready to be pushed to the grabber */
    struct list_head grabbed_list;  /* list of filled buffers */
    struct list_head hot_list;      /* entries currently "in hardware" */
    struct list_head wait_list;     /* completions waiting for a frame */
    long long transfer_todo;        /* number of image still to transfer */

    spinlock_t timerlock;           /* lock to protect timer */
    struct hrtimer timer;           /* DMA timeouts */
    unsigned long timeout;          /* delay for timer restarts (in seconds) */

    struct work_struct dwork;       /* called when all pictures grabbed */
};

struct menable_uiq;


struct siso_menable {
    /* kernel stuff */
    struct device dev;
    struct module *owner;
    struct pci_dev *pdev;
    spinlock_t boardlock;
    struct dma_pool *sgl_dma_pool;
    struct cdev cdev;

    int idx;                                /* driver-internal number of board */
    enum men_board_state _state;             /* board state */
    unsigned char active_fpgas;             /* how many FPGAs are active */
    unsigned char dmacnt[MAX_FPGAS];        /* number of active DMA channels (per FPGA) */

    messaging_dma_declaration * messaging_dma_declaration; /* Declaration of the Messaging DMA Engine. If NULL, no Messaging DMA is supported. */

    /**
     * The number of allocated uiqs.
     *
     * mE4: constant for FPGA 0 and vary for FPGA 1 depending on the design.
     * mE5: constant, only FPGA 0 is valid
     * mE6: grows when VA events are initialized, only FPGA0 is valid
     */
    unsigned char uiqcnt[MAX_FPGAS];        /* number of UIQs (per FPGA) */
    struct uiq_base **uiqs;                 /* UIQ control structs */
    uiq_transfer_state uiq_transfer;        /* State of the Current UIQ transfer */

    /**
     * The number of UIQs that are currently in use.
     *
     * For mE6, this numer is not necessarily equal to the number of allocated uiqs.
     * When the number of event UIQs is decreased, this number is also decreased
     * but the UIQs remain allocated.
     */
    unsigned int num_active_uiqs;

    struct menable_dmachan **dmachannels;   /* array of DMA channels */
    bool dma_stop_bugfix_present;           /* DMA stop bug fix present */
    int use;                                /* number of open fds on this board */
    spinlock_t designlock;
    char *desname;                          /* design name */
    size_t deslen;                          /* length of desname buffer */
    uint32_t desval;                        /* design clock on meIII, design CRC on meIV */
    bool releasing;                         /* board is about to be destroyed */
    bool design_changing;                   /* the device is reconfigured and changes it's properties */
    union {
        struct me4_data *d4;
        struct me5_data *d5;
        struct me6_data *d6;
    };

    uint32_t config;
    uint32_t config_ex;
    uint32_t pcie_dsn_high;
    uint32_t pcie_dsn_low;
    uint16_t pci_vendor_id;
    uint16_t pci_device_id;
    uint16_t pci_subsys_vendor_id;
    uint16_t pci_subsys_device_id;
    uint32_t pcie_device_caps;
    uint32_t pcie_link_caps;
    uint16_t pcie_device_ctrl;
    uint16_t pcie_link_stat;
    uint32_t fpga_dna[3];

    /**
     * The index of the first UIQ for VA events.
     */
    uint32_t first_va_event_uiq_idx;

    spinlock_t buffer_heads_lock;
    unsigned int num_buffer_heads;
    struct list_head buffer_heads_list;

    unsigned int dma_fifo_length;

    struct list_head threadgroups_heads;
    spinlock_t threadgroups_headlock;

    int (*open)(struct siso_menable *, struct file *);
    int (*release)(struct siso_menable *, struct file *);
    int (*create_dummybuf)(struct siso_menable *, struct menable_dmabuf *);
    void (*free_dummybuf)(struct siso_menable *, struct menable_dmabuf *);
    int (*create_buf)(struct siso_menable *, struct menable_dmabuf *, struct menable_dmabuf *);
    void (*free_buf)(struct siso_menable *, struct menable_dmabuf *);
    int (*startdma)(struct siso_menable *, struct menable_dmachan *);
    void (*abortdma)(struct siso_menable *, struct menable_dmachan *);
    void (*stopdma)(struct siso_menable *, struct menable_dmachan *);
    int (*ioctl)(struct siso_menable *, const unsigned int, const unsigned int, unsigned long);
    long (*compat_ioctl)(struct siso_menable *, const unsigned int, const unsigned int, unsigned long);
    void (*exit)(struct siso_menable *);
    void (*cleanup)(struct siso_menable *); /* garbage collection if last handle is closed */
    unsigned int (*query_dma)(struct siso_menable *, const unsigned int);
    void (*dmabase)(struct siso_menable *, struct menable_dmachan *);
    void (*stopirq)(struct siso_menable *);
    void (*startirq)(struct siso_menable *);
    void (*queue_sb)(struct menable_dmachan *, struct menable_dmabuf *);
    struct controller_base * (*get_controller)(struct siso_menable * self, uint32_t peripheral);

    struct register_interface register_interface;
    struct pci_config_interface_linux config_interface;
    struct mutex i2c_lock;
    struct mutex flash_lock;

    struct camera_frontend * camera_frontend;
    struct mutex camera_frontend_lock;
};

struct me_threadgroup {
    struct list_head node;
    pid_t id;
    unsigned int cnt;
    int dmas[MAX_FPGAS][MEN_MAX_DMA];
};

struct me_notification_handler {
    struct list_head node;                      /* entry in notification_handler heads *_list */
    pid_t pid;                                  /* handler process id */
    pid_t ppid;                                 /* handler parent process id */
    unsigned long notification_time_stamp;      /* Time stamp of last handled notification */
    struct completion notification_available;   /* Notification Available Event: this is triggered whenever a new notification is available */
    bool quit_requested;                        /* true when quit is requested, false otherwise. */
};

struct men_io_range;
struct fg_ctrl;

struct me_threadgroup * me_create_threadgroup (struct siso_menable *men, const unsigned int tgid);
void me_free_threadgroup(struct siso_menable *men, struct me_threadgroup *bh);
struct me_threadgroup * me_get_threadgroup(struct siso_menable *men, const unsigned int tgid);

int men_create_userbuf(struct siso_menable *, struct men_io_range *);
int men_free_userbuf(struct siso_menable *, struct menable_dmahead *, long index);
int men_alloc_dma(struct siso_menable *men, unsigned int count) __releases(&men->buffer_heads_lock);
int men_add_dmas(struct siso_menable *men);
int buf_get_uint(const char *, size_t, unsigned int *);
int men_start_dma(struct menable_dmachan *dc, struct menable_dmahead *, const unsigned int startbuf);
int men_wait_dmaimg(struct menable_dmachan *d, int64_t img, int timeout_msecs, uint64_t *foundframe, bool is32bitProcOn64bitKernel);
int men_create_buf_head(struct siso_menable *, const size_t maxsize, const long subbufs);
int men_release_buf_head(struct siso_menable *, struct menable_dmahead *);
void men_free_buf_head(struct siso_menable *, struct menable_dmahead *);
struct menable_dmabuf *men_move_hot(struct menable_dmachan *db, const menable_timespec_t *ts);
void men_destroy_sb(struct siso_menable *, struct menable_dmabuf *);
void men_stop_dma(struct menable_dmachan *);
void men_stop_dma_locked(struct menable_dmachan *);
struct menable_dmahead *me_get_buf_head(struct siso_menable *men, const unsigned int);
struct menable_dmabuf *me_get_sub_buf(struct siso_menable *men, const unsigned int headnum, const long bufidx);
struct menable_dmabuf *me_get_sub_buf_by_head(struct menable_dmahead *head, const long bufidx);
int fg_start_transfer(struct siso_menable *, struct fg_ctrl *, const size_t tsize);
void men_dma_queue_max(struct menable_dmachan *);
long menable_ioctl(struct file *, unsigned int, unsigned long);
long menable_compat_ioctl(struct file *, unsigned int, unsigned long);
void men_dma_clean_sync(struct menable_dmachan *db);
void men_dma_done_work(struct work_struct *);
void men_unblock_buffer(struct menable_dmachan *dc, struct menable_dmabuf *sb);
struct menable_dmabuf *men_next_blocked(struct siso_menable *men, struct menable_dmachan *dc);
struct menable_dmabuf *men_last_blocked(struct siso_menable *men, struct menable_dmachan *dc);
struct menable_dmachan *men_dma_channel(struct siso_menable *men, const unsigned int index);

int me4_probe(struct siso_menable *men);
int me5_probe(struct siso_menable *men);
int me6_probe(struct siso_menable *men);

enum men_board_state men_get_state(struct siso_menable * men);
int men_set_state(struct siso_menable * men, enum men_board_state state);

ssize_t men_get_des_val(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t men_set_des_val(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
ssize_t men_get_dmas(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t men_get_des_name(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t men_set_des_name(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

const struct attribute_group ** me6_init_attribute_groups(struct siso_menable *men);
const struct attribute_group ** me5_init_attribute_groups(struct siso_menable *men);
const struct attribute_group ** me4_init_attribute_groups(struct siso_menable *men);

extern struct class *menable_dma_class;
extern struct device_attribute men_dma_attributes[3];

static inline const char* get_acqmode_name(int acqmode) {

    switch(acqmode) {
        case DMA_CONTMODE      : return "standard";
        case DMA_BLOCKINGMODE  : return "blocking";
        case DMA_PULSEMODE     : return "pulse";
        case DMA_SELECTIVEMODE : return "selective";
        default                : return "UNKNWON";
    }
}

static inline int
is_me4(const struct siso_menable *men)
{
    return SisoBoardIsMe4(men->pci_device_id) ? 1 : 0;
}

static inline int
is_me5(const struct siso_menable *men)
{
    return SisoBoardIsMe5(men->pci_device_id) ? 1 : 0;
}

static inline int
is_me6(const struct siso_menable *men)
{
    return SisoBoardIsMe6(men->pci_device_id) ? 1 : 0;
}

static inline void
w64(struct siso_menable *men, uint32_t offs, uint64_t v)
{
    men->register_interface.write(&men->register_interface, offs, (unsigned int)(v & 0xffffffff));
    men->register_interface.write(&men->register_interface, offs + 1, (unsigned int)((v >> 32) & 0xffffffff));
}

/* Mask and Shift Left: value is masked from bit h..l, then shifted left by s */
static inline u64
msl64(const u64 value, const int h, const int l, const int s)
{
    return (value & GENMASK_ULL(h, l)) << s;
}

/* Mask and Shift Right: value is masked from bit h..l, then shifted right by s */
static inline u64
msr64(const u64 value, const int h, const int l, const int s)
{
    return (value & GENMASK_ULL(h, l)) >> s;
}

/* Replace Masked and Shift Left: src is masked from bit h..l, then shifted left by s; the resulting bits are replaced in dest */
static inline u64
rmsl64(const u64 dest, const u64 src, const int h, const int l, const int s)
{
    const u64 m = GENMASK_ULL(h, l);
    return (dest & ~(m << s)) | ((src & m) << s);
}

/* Replace Masked and Shift Right: src is masked from bit h..l, then shifted right by s; the resulting bits are replaced in dest */
static inline u64
rmsr64(const u64 dest, const u64 src, const int h, const int l, const int s)
{
    const u64 m = GENMASK_ULL(h, l);
    return (dest & ~(m >> s)) | ((src & m) >> s);
}

#endif /* MENABLE_H */
