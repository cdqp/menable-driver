/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "menable.h"

#include <linux/io.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
#include <linux/spinlock_types.h>
#include <asm/cmpxchg.h>
#endif

#include <asm/cpufeature.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/aer.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/vmalloc.h>

#include <lib/helpers/error_handling.h>
#include <lib/helpers/dbg.h>
#include <lib/uiq/uiq_helper.h>

#include "uiq.h"
#include "linux_version.h"
#include "sisoboards.h"

static dev_t devr;

struct kobject *kobj_ref;
int get_menable_info(char *buf)
{
	if(buf != NULL)
		return sprintf(buf, "%-12s:%s\n%-12s:%s\n%-12s:%s\n%-12s:%s %s\n", "VENDOR", DRIVER_VENDOR, "NAME", DRIVER_NAME, "VERSION", DRIVER_VERSION, "BUILD TIME", __TIME__, __DATE__);

	pr_info("VENDOR: %s, NAME: %s, VERSION: %s, BUILD TIME: %s %s\n", DRIVER_VENDOR, DRIVER_NAME, DRIVER_VERSION, __TIME__, __DATE__);
	return 0;
}

static ssize_t sysfs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return get_menable_info(buf);
}

struct kobj_attribute menable_info = __ATTR(info, 0660, sysfs_show, NULL);


/*
 * The DEFINE_SPINLOCK macro fails to compile (at least) with gcc 4.8
 * and RT kernel 4.9.47-rt37. Init happens in menable_init.
 */
static spinlock_t idxlock;

static struct lock_class_key men_idx_lock;
static struct lock_class_key men_board_lock;
static struct lock_class_key men_design_lock;
static struct lock_class_key men_head_lock;
static int maxidx;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
static inline void _menable_get_ts(menable_timespec_t * ts) {
    return ktime_get_ts(ts);
}
#else
static inline void _menable_get_ts(menable_timespec_t * ts) {
    return ktime_get_ts64(ts);
}
#endif

static menable_time_t timespec_tv_sec_offset;

void menable_get_ts(menable_timespec_t *ts)
{
    _menable_get_ts(ts);
    ts->tv_sec -= timespec_tv_sec_offset;
}

static void menable_obj_release(struct device *dev)
{
    struct siso_menable *men = container_of(dev, struct siso_menable, dev);

    spin_lock(&idxlock);
    if (men->idx == maxidx - 1)
        maxidx--;
    spin_unlock(&idxlock);
    kfree(men);
}

static void
men_cleanup_threadgroup(struct siso_menable *men, struct me_threadgroup * tg)
{
    unsigned int i, j;
    unsigned int dmaskip = 0;
    struct menable_dmachan * dma_chan;

#ifdef ENABLE_DEBUG_MSG
    printk(KERN_INFO "[%d]: men_cleanup_threadgroup\n", current->parent->pid);
#endif

    for (j = 0; j < men->active_fpgas; j++) {
        for (i = 0; i < men->dmacnt[j]; i++) {
            if (tg->dmas[j][i] == 1) {
                dma_chan = men_dma_channel(men, dmaskip + i);
                if (dma_chan->state == MEN_DMA_CHAN_STATE_STARTED) {
                    men_stop_dma_locked(dma_chan);
                }
                tg->dmas[j][i] = 0;
            }
        }
        dmaskip += men->dmacnt[j];
    }

    me_free_threadgroup(men, tg);
}

static void
men_cleanup_channels(struct siso_menable *men)
{
    int j;
    unsigned int skipdma = 0;

#ifdef ENABLE_DEBUG_MSG
    printk(KERN_INFO "[%d]: men_cleanup_channels\n", current->parent->pid);
#endif

    for (j = 0; j < men->active_fpgas; j++) {
        int i;
        for (i = 0; i < men->dmacnt[j]; i++) {
            struct menable_dmachan *dma_chan = men_dma_channel(men, skipdma + i);
            if (dma_chan->state == MEN_DMA_CHAN_STATE_STARTED)
                men_stop_dma_locked(dma_chan);
        }
        skipdma += men->dmacnt[j];
    }

    men->stopirq(men);

    for (j = 0; j < men->active_fpgas; j++) {
        int i;
        for (i = 0; i < men->uiqcnt[j]; i++) {
            uiq_base *uiq_base = men->uiqs[j * MEN_MAX_UIQ_PER_FPGA + i];
            if (uiq_base == NULL)
                continue;

            struct menable_uiq * uiq = container_of(uiq_base, struct menable_uiq, base);

            spin_lock(&uiq->lock);
            uiq->base.is_running = false;
            spin_unlock(&uiq->lock);
        }
    }
}

static void
men_cleanup_mem(struct siso_menable *men)
{
    struct menable_dmahead *dh, *tmp;
    struct list_head heads;

    INIT_LIST_HEAD(&heads);
    list_for_each_entry_safe(dh, tmp, &men->buffer_heads_list, node) {
        int r = men_release_buf_head(men, dh);
        WARN_ON(r != 0);
        INIT_LIST_HEAD(&dh->node);
        list_add_tail(&dh->node, &heads);
    }

    WARN_ON(men->num_buffer_heads != 0);
    spin_unlock_bh(&men->buffer_heads_lock);

    list_for_each_entry_safe(dh, tmp, &heads, node) {
        men_free_buf_head(men, dh);
    }
}

//static int men_init_camera_frontend(struct siso_menable * men) {
//
//    unsigned int num_ports = SisoBoardNumberOfPhysicalPorts(men->pci_device_id);
//    mutex_init(&men->camera_frontend_lock);
//
//    /*
//     * Here we rely on that the boardspecific meX_probe function has
//     * initialized men->camera_frontend_type properly.
//     */
//    switch (men->camera_frontend_type) {
//    case CAMERA_FRONTEND_CXP:
//    {
//        struct cxp_frontend * frontend = kzalloc(sizeof(*men->camera_frontend.cxp), GFP_KERNEL);
//        if (frontend == NULL)
//            return -ENOMEM;
//
//        int ret = cxp_frontend_init(men->camera_frontend.cxp, &men->register_interface, &men->camera_frontend_lock, num_ports);
//        if (ret != STATUS_OK)
//            return -EFAULT;
//    }
//
//    }
//
//    return 0;
//}
//
//static int men_cleanup_camera_frontend(struct siso_menable * men) {
//    switch(men->camera_frontend_type) {
//    case CAMERA_FRONTEND_CXP:
//        mutex_lock(&men->camera_frontend_lock);
//        if (men->camera_frontend.cxp != NULL) {
//            men->camera_frontend.cxp->cleanup(men->camera_frontend.cxp);
//        }
//        mutex_unlock(&men->camera_frontend_lock);
//        break;
//    }
//
//    men->camera_frontend_type = CAMERA_FRONTEND_UNKNOWN;
//
//    return 0;
//}

static int menable_open(struct inode *inode, struct file *file)
{
    struct siso_menable *men = container_of(inode->i_cdev, struct siso_menable, cdev);
    int ret;
    unsigned long flags;
    struct me_threadgroup *tg;

    file->private_data = men;

    if (men->open) {
        ret = men->open(men, file);
        if (ret)
            return ret;
    }

    spin_lock_irqsave(&men->boardlock, flags);
    if (men->use == 0)
        men->startirq(men);
    men->use++;
    spin_unlock_irqrestore(&men->boardlock, flags);

    tg = me_get_threadgroup(men, current->tgid);
    if (tg == NULL) {
        tg = me_create_threadgroup(men, current->tgid);
    }
    if (tg != NULL) {
        tg->cnt++;
    }

    return 0;
}

static int menable_release(struct inode *inode, struct file *file)
{
    struct siso_menable *men = file->private_data;
    unsigned long flags;
    struct me_threadgroup *tg;

    spin_lock_bh(&men->buffer_heads_lock);
    spin_lock_irqsave(&men->boardlock, flags);
    tg = me_get_threadgroup(men, current->tgid);
    if (tg != NULL) {
        tg->cnt--;
        // Cleanup per process
        if (tg->cnt == 0) {
            //Cleanup DMAs used by this thread group
            men_cleanup_threadgroup(men, tg);
        }
    }

    if (--men->use == 0) {
        men_cleanup_channels(men);
        spin_unlock_irqrestore(&men->boardlock, flags);
        men_cleanup_mem(men);

        if (men->cleanup != NULL)
            men->cleanup(men);
    } else {
        spin_unlock_irqrestore(&men->boardlock, flags);
        spin_unlock_bh(&men->buffer_heads_lock);
    }

    return 0;
}

static const struct file_operations menable_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = menable_ioctl,
    .compat_ioctl = menable_compat_ioctl,
    .open = menable_open,
    .release = menable_release,
};

static struct class *menable_class;

static DEFINE_PCI_DEVICE_TABLE(menable_pci_table) = {
    /* LightBridge/Marathon VCL */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL),
      .driver_data = PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL },
    /* Marathon AF2 */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_MARATHON_AF2_DP),
      .driver_data = PN_MICROENABLE5_MARATHON_AF2_DP },
    /* Marathon ACX QP */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_MARATHON_ACX_QP),
      .driver_data = PN_MICROENABLE5_MARATHON_ACX_QP },
    /* LightBridge/Marathon ACL */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL),
      .driver_data = PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL },
    /* Marathon ACX SP */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_MARATHON_ACX_SP),
      .driver_data = PN_MICROENABLE5_MARATHON_ACX_SP },
    /* Marathon ACX DP */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_MARATHON_ACX_DP),
      .driver_data = PN_MICROENABLE5_MARATHON_ACX_DP },
    /* Marathon VCX QP */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_MARATHON_VCX_QP),
      .driver_data = PN_MICROENABLE5_MARATHON_VCX_QP },
    /* Marathon VF2 */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_MARATHON_VF2_DP),
      .driver_data = PN_MICROENABLE5_MARATHON_VF2_DP },
    /* Marathon VCLx */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx),
      .driver_data = PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx },
    /* Abacus 4G */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_ABACUS_4G),
      .driver_data = PN_MICROENABLE5_ABACUS_4G },
    /* Abacus 4G Base */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_ABACUS_4G_BASE),
      .driver_data = PN_MICROENABLE5_ABACUS_4G_BASE },
    /* Abacus 4G Base II */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE5_ABACUS_4G_BASE_II),
      .driver_data = PN_MICROENABLE5_ABACUS_4G_BASE_II },
    /* imaWorx CXP-12 Quad */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE6_IMAWORX_CXP12_QUAD),
      .driver_data = PN_MICROENABLE6_IMAWORX_CXP12_QUAD },
    /* imaFlex CXP-12 Quad */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE6_IMAFLEX_CXP12_QUAD),
      .driver_data = PN_MICROENABLE6_IMAFLEX_CXP12_QUAD },
    /* imaFlex CXP-12 Penta */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE6_IMAFLEX_CXP12_PENTA),
      .driver_data = PN_MICROENABLE6_IMAFLEX_CXP12_PENTA },
    /* CXP12-IC-1C */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE6_CXP12_IC_1C),
      .driver_data = PN_MICROENABLE6_CXP12_IC_1C },
    /* CXP12-IC-2C */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE6_CXP12_IC_2C),
      .driver_data = PN_MICROENABLE6_CXP12_IC_2C },
    /* CXP12-IC-4C */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE6_CXP12_IC_4C),
      .driver_data = PN_MICROENABLE6_CXP12_IC_4C },
    /* CXP12-LB-2C */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE6_CXP12_LB_2C),
      .driver_data = PN_MICROENABLE6_CXP12_LB_2C },
    /* Abacus 4TG */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE6_ABACUS_4TG),
      .driver_data = PN_MICROENABLE6_ABACUS_4TG },
#ifdef WITH_PROTOTYPE_FRAMEGRABBERS
    /* KCU116 */
    { PCI_DEVICE(PCI_VENDOR_SISO, PN_MICROENABLE6_IMPULSE_KCU116),
      .driver_data = PN_MICROENABLE6_IMPULSE_KCU116 },
#endif
    /* EOT */
    { 0 },
};

MODULE_DEVICE_TABLE(pci, menable_pci_table);

static void __devexit menable_pci_remove(struct pci_dev *pdev)
{
    struct siso_menable *men = pci_get_drvdata(pdev);
    int i;
    unsigned long flags;

    dev_info(&men->dev, "removing device");

    get_device(&men->dev);

    sysfs_remove_link(&men->dev.kobj, "pci_dev");

    spin_lock_bh(&men->buffer_heads_lock);
    
    /* If the last reconfiguration failed, `men->design_changing` may still be true.
     * In that case, men->releasing is also true, so we check for this flag here
     * to avoid a deadklock. */
    if (!men->releasing) {
        spin_lock_irqsave(&men->designlock, flags);
        while (men->design_changing) {
            spin_unlock_irqrestore(&men->designlock, flags);
            spin_unlock_bh(&men->buffer_heads_lock);
            schedule();
            spin_lock_bh(&men->buffer_heads_lock);
            spin_lock_irqsave(&men->designlock, flags);
        }
        men->design_changing = true;
        spin_unlock_irqrestore(&men->designlock, flags);

        spin_lock_irqsave(&men->boardlock, flags);
        men->stopirq(men);
        men->releasing = true;
        spin_unlock_irqrestore(&men->boardlock, flags);

        i = men_alloc_dma(men, 0);
        BUG_ON(i != 0);
    }

    men_del_uiqs(men, 0);

    men->exit(men);

    device_unregister(&men->dev);

    /*
     * TODO: [RKN] device_unregister causes a lot of implicit cleanup so here
     *             we should be cautious with further manual cleansing.
     *
     *             Among others, a call to menable_obj_release is issued which
     *             currently frees the `men` struct, which makes any further usage
     *             of men->dev or men->cdev illegal.
     *
     *             It feels like device_unregister should be the last call in the
     *             cleanup routine. But this nees further research.
     */

    cdev_del(&men->cdev);

    pci_set_drvdata(pdev, NULL);
    put_device(&men->dev);
}

static int __devinit menable_pci_probe(struct pci_dev *,
                                       const struct pci_device_id *);

static struct pci_driver menable_pci_driver = {
    .name = DRIVER_NAME,
    .id_table = menable_pci_table,
    .probe = menable_pci_probe,
    .remove = __devexit_p(menable_pci_remove),
};

static void menable_read_config_space(struct siso_menable *men)
{
    int ret;
    struct pci_config_interface * pci_cfg = upcast(&men->config_interface);

    struct men_pci_header pci_header;
    ret = pci_cfg->get_pci_header(pci_cfg, &pci_header);
    if (MEN_IS_SUCCESS(ret)) {
        men->pci_vendor_id = pci_header.vendor_id;
        men->pci_device_id = pci_header.device_id;
        men->pci_subsys_vendor_id = pci_header.subsys_vendor_id;
        men->pci_subsys_device_id = pci_header.subsys_id;
        dev_dbg(&men->dev, "PCI Vendor %04x, Device %04x (Subsystem Vendor %04x, Device %04x)\n",
                        men->pci_vendor_id, men->pci_device_id,
                        men->pci_subsys_vendor_id, men->pci_subsys_device_id);
    } else {
        dev_warn(&men->dev, "failed to read PCI header\n");
    }

    struct pci_capability pci_cap;
    ret = pci_cfg->find_capability(pci_cfg, MEN_PCI_CAPABILITY_ID_PCI_EXPRESS, &pci_cap);
    if (MEN_IS_SUCCESS(ret)) {
        men->pcie_device_caps = pci_cap.pci_express.device_capabilities;
        men->pcie_device_ctrl = pci_cap.pci_express.device_control;
        men->pcie_link_caps = pci_cap.pci_express.link_capabilities;
        men->pcie_link_stat = pci_cap.pci_express.link_status;
        DBG_STMT(dev_dbg(&men->dev, "PCIe Capability found; DEVCAP %08x, DEVCTL %08x, LNKCAP %08x, LNKSTA %08x\n",
                            men->pcie_device_caps, men->pcie_device_ctrl,
                            men->pcie_link_caps, men->pcie_link_stat));
    } else {
        dev_warn(&men->dev, "PCIe Capability not found\n");
    }

    struct pci_express_capability pcie_cap;
    ret = pci_cfg->find_ext_capability(pci_cfg, MEN_PCIE_CAP_ID_DEVICE_SERIAL_NUMBER, &pcie_cap);
    if (MEN_IS_SUCCESS(ret)) {
        men->pcie_dsn_low = pcie_cap.device_serial_number.sn_low;
        men->pcie_dsn_high = pcie_cap.device_serial_number.sn_hi;
        DBG_STMT(dev_dbg(&men->dev, "PCIe DSN Extended Capability found; DSN is %08x:%08x\n",
                 men->pcie_dsn_high, men->pcie_dsn_low));
    } else {
        dev_warn(&men->dev, "PCIe DSN Extended Capability not found\n");
    }
}

enum men_board_state men_get_state(struct siso_menable * men)
{
    return men->_state;
}

int men_set_state(struct siso_menable * men, enum men_board_state state)
{
    static const char * states[] = {
        "UNKNOWN",
        "UNINITIALISED",
        "DEAD",
        "SLEEPING",
        "RECONFIGURING",
        "READY",
        "CONFIGURED",
        "ACTIVE"
    };

    int ret = 0;

    if (state < BOARD_STATE_UNINITIALISED || state > BOARD_STATE_ACTIVE) {
        dev_err(&men->dev, "request to change to invalid state\n");
        return -EINVAL;
    }

    if (state == men->_state) {
        /* No state change */
        return 0;
    }

    dev_info(&men->dev, "request to change board state from %s to %s\n", states[men->_state], states[state]);

    if (men->_state < BOARD_STATE_READY && state >= BOARD_STATE_READY) {
        /* Bring board up */

        BUG_ON(men->active_fpgas == 0);
        BUG_ON(men->active_fpgas > MAX_FPGAS);

        ret = men_add_dmas(men);
    } else if (men->_state >= BOARD_STATE_READY && state < BOARD_STATE_READY) {
        /* TODO: Take board down */
    }

    if (ret == 0) {
        men->_state = state;
        dev_info(&men->dev, "board state changed to %s\n", states[men->_state]);
    }

    return ret;
}

static int __devinit menable_pci_probe(struct pci_dev *pdev,
                                       const struct pci_device_id *id)
{
    struct siso_menable *men;
    int ret;
    unsigned long flags;

    printk(KERN_INFO "%s: probing device %i", DRIVER_NAME, maxidx);

    ret = pcim_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to enable/initialize device\n");
        return ret;
    }

    men = kzalloc(sizeof(*men), GFP_KERNEL);
    if (men == NULL) {
        dev_err(&pdev->dev, "failed to alloc mem for device struct.\n");
        return -ENOMEM;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
    men->owner = THIS_MODULE;
#else
    men->owner = menable_class->owner;
#endif
    men->dev.parent = &pdev->dev;
    men->pdev = pdev;
    men->dev.release = menable_obj_release;
    men->dev.class = menable_class;
    men->dev.driver = &menable_pci_driver.driver;
    spin_lock(&idxlock);
    men->idx = maxidx++;
    spin_unlock(&idxlock);
    dev_set_name(&men->dev, "menable%i", men->idx);

    men->releasing = false;
    men->design_changing = true;

    pci_set_master(pdev);
    pci_set_drvdata(pdev, men);
    spin_lock_init(&men->boardlock);
    lockdep_set_class(&men->boardlock, &men_board_lock);
    spin_lock_init(&men->designlock);
    lockdep_set_class(&men->designlock, &men_design_lock);
    spin_lock_init(&men->buffer_heads_lock);
    lockdep_set_class(&men->buffer_heads_lock, &men_head_lock);
    INIT_LIST_HEAD(&men->buffer_heads_list);
    spin_lock_init(&men->threadgroups_headlock);
    INIT_LIST_HEAD(&men->threadgroups_heads);

    /*
     * Create a character device to be able to provide an IOCTL interface
     * for the device.
     */
    cdev_init(&men->cdev, &menable_fops);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
    men->cdev.owner = THIS_MODULE;
#else
    men->cdev.owner = men->dev.class->owner;
#endif
    kobject_set_name(&men->cdev.kobj, "menablec%i", men->idx);

    ret = cdev_add(&men->cdev, devr + men->idx, 1);
    if (ret) {
        dev_err(&men->dev, "Failed to add character device / IOCTL interface.\n");
        goto err_release;
    }

    men->dev.devt = men->cdev.dev;

    /* Query things from config space */
    pci_config_interface_linux_init(&men->config_interface, pdev);
    menable_read_config_space(men);

    if (is_me5(men)) {
        men->dev.groups = me5_init_attribute_groups(men);
    } else if (is_me6(men)) {
        men->dev.groups = me6_init_attribute_groups(men);
    } else {
        dev_err(&men->dev, "Device %03x is an unknown board.\n", men->pci_device_id);
        goto err_cdev;
    }

    /*
     * TODO: [RKN] Can we do this earlier to make it easier to clean up in
     *             case of an error?
     */
    ret = device_register(&men->dev);
    if (ret) {
        dev_err(&men->dev, "Failed to register device\n");
        goto err_cdev;
    }

    dev_info(&men->dev, "device is a %s\n", GetBoardName(id->device));

    ret = pci_request_selected_regions(pdev, 0x1, DRIVER_NAME);
    if (ret) {
        dev_err(&men->dev, "Failed to reserve PCI resources.\n");
        goto err_dev;
    }

    void __iomem * runtime_base = pcim_iomap(pdev, 0, 0);
    if (runtime_base == NULL) {
        dev_err(&men->dev, "Failed to map BAR 0.\n");
        goto err_dev;
    }

    menable_register_interface_init(&men->register_interface, runtime_base);
    
    mutex_init(&men->i2c_lock);
    mutex_init(&men->flash_lock);

    mutex_init(&men->camera_frontend_lock);

    if (is_me5(men)) {
        ret = me5_probe(men);
    } else if (is_me6(men)) {
        ret = me6_probe(men);
    } else {
        dev_err(&men->dev, "Unsupported board type. Could not initialize.\n");
        ret = ENODEV;
    }

    if (ret) {
        goto err_board_dev;
    }

    ret = sysfs_create_link(&men->dev.kobj, &men->pdev->dev.kobj, "pci_dev");
    if (ret) {
        dev_err(&men->dev, "Failed to create sysfs link 'pci_dev' for device.\n");
        goto err_sysfs_link;
    }

    spin_lock_irqsave(&men->boardlock, flags);
    men->design_changing = false;
    spin_unlock_irqrestore(&men->boardlock, flags);

    dev_info(&men->dev, "Initialization completed\n");
    return 0;

err_sysfs_link:
    pr_debug(KBUILD_MODNAME "err_sysfs_link\n");
    men_del_uiqs(men, 0);
    men->exit(men);

err_board_dev:
err_dev:
    pr_debug(KBUILD_MODNAME "err_dev\n");
    /* TODO: [RKN] Does the following make sense?
     *             Let's say, maxidx == 1 and men->idx == 0.
     *             Then maxidx will remain at 1 and the device will be closed.
     *             When device 1 is closed, maxidx will be decremented to 0
     *             which makes no sense since device 0 is already gone.
     *
     *             Since it might not be guaranteed, that devices are closed
     *             in the reverse order of the opening order, a more sophisticated
     *             approach would be required. We could for example keep an array
     *             of device pointers with all open devices to figure out a suitable
     *             index.
     */
    spin_lock(&idxlock);
    if (men->idx == maxidx - 1) {
        maxidx--;
    }
    spin_unlock(&idxlock);

    cdev_del(&men->cdev);
    device_unregister(&men->dev); /* calls kfree(men) indirectly via menable_obj_release. */
    pci_set_drvdata(pdev, NULL);

    /* Cleansing is done. Return here. */
    return ret;

err_cdev:
    pr_debug(KBUILD_MODNAME "err_cdev\n");
    cdev_del(&men->cdev);

err_release:
    pr_debug(KBUILD_MODNAME "err_release\n");
    pci_set_drvdata(pdev, NULL);
    spin_lock(&idxlock);
    if (men->idx == maxidx - 1)
        maxidx--;
    spin_unlock(&idxlock);
    kfree(men);
    return ret;
}

static struct device_attribute men_device_attributes[3] = {
    __ATTR(dma_channels, 0444, men_get_dmas, NULL),
    __ATTR(design_name, 0660, men_get_des_name, men_set_des_name),
    __ATTR_NULL,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)

static struct attribute * men_device_attrs[3] = {
    &men_device_attributes[0].attr,
    &men_device_attributes[1].attr,
    NULL
};

ATTRIBUTE_GROUPS(men_device);

static struct attribute * men_dma_attrs[3] = {
    &men_dma_attributes[0].attr,
    &men_dma_attributes[1].attr,
    NULL
};

ATTRIBUTE_GROUPS(men_dma);

static struct attribute * men_uiq_attrs[6] = {
    &men_uiq_attributes[0].attr,
    &men_uiq_attributes[1].attr,
    &men_uiq_attributes[2].attr,
    &men_uiq_attributes[3].attr,
    &men_uiq_attributes[4].attr,
    NULL
};

ATTRIBUTE_GROUPS(men_uiq);

#endif /* LINUX >= 3.12.0 */

static void timespec_diff(menable_timespec_t *start, menable_timespec_t *stop,
                   menable_timespec_t *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
 
    return;
}

static int __init menable_init(void)
{
    menable_timespec_t start, d;
    u64 dns_min = (u64)-1;
    int ret = 0, i = 0;

    // Some compilers don't initialize statics to zero ...
    maxidx = 0;
    printk(KERN_INFO  "%s: loading %s %s Version %s\n", DRIVER_NAME, DRIVER_VENDOR, DRIVER_DESCRIPTION, DRIVER_VERSION);

    spin_lock_init(&idxlock);

    lockdep_set_class(&idxlock, &men_idx_lock);

    ret = alloc_chrdev_region(&devr, 0, MEN_MAX_NUM, DRIVER_NAME);
    if (ret) {
        printk(KERN_ERR "%s: failed to register device numbers\n",
            DRIVER_NAME);
        return ret;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    menable_class = class_create("menable");
#else
    menable_class = class_create(THIS_MODULE, "menable");
#endif
    if (IS_ERR(menable_class)) {
        ret = PTR_ERR(menable_class);
        goto err_class;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    menable_uiq_class = class_create("menable_uiq");
#else
    menable_uiq_class = class_create(THIS_MODULE, "menable_uiq");
#endif
    if (IS_ERR(menable_uiq_class)) {
        ret = PTR_ERR(menable_uiq_class);
        goto err_uiq_class;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    menable_dma_class = class_create("menable_dma");
#else
    menable_dma_class = class_create(THIS_MODULE, "menable_dma");
#endif;
    if (IS_ERR(menable_dma_class)) {
        ret = PTR_ERR(menable_dma_class);
        goto err_dma_class;
    }

	// Since the IOCTLs use the old struct timespec, make sure we use
	// a timestamp relative to the start of the driver;
	// this gives us 2^31 - 1 seconds (~68 years) until overflow
    menable_timespec_t now;
    menable_get_ts(&now);
    timespec_tv_sec_offset = now.tv_sec;
    set_uiq_timestamp_offset(now.tv_sec);

    // Measure the resolution of menable_get_ts.
    // Try a few times to reduce jitter, but not too many to avoid slowing
    // down on systems with low resolution.
    for (i = 0; i < 10; ++i) {
        u64 dns = 0;
        menable_get_ts(&start);
        do {
            menable_get_ts(&now);
            timespec_diff(&start, &now, &d);
            dns = 1000000000ull * d.tv_sec + d.tv_nsec;
        } while (dns == 0);
        if (dns < dns_min) {
            dns_min = dns;
        }
    }
    printk(KERN_INFO "%s: frame timestamp resolution is at least %lluns\n", DRIVER_NAME, dns_min);
    
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
    menable_class->dev_attrs = men_device_attributes;
    menable_uiq_class->dev_attrs = men_uiq_attributes;
    menable_dma_class->dev_attrs = men_dma_attributes;
#else /* LINUX < 3.12.0 */
    menable_class->dev_groups = men_device_groups;
    menable_uiq_class->dev_groups = men_uiq_groups;
    menable_dma_class->dev_groups = men_dma_groups;
#endif /* LINUX < 3.12.0 */

    ret = pci_register_driver(&menable_pci_driver);
    if (ret)
        goto err_reg;

    // Write menable info to dmesg
    get_menable_info(NULL);

    //Creating a directory menable in /sys folder
     kobj_ref = kobject_create_and_add("menable",NULL);

     //Creating sysfs file for info
     if(sysfs_create_file(kobj_ref, &menable_info.attr)){
     	pr_err("Cannot create sysfs file......\n");
     	goto err_sysfs;
        }

    return 0;

err_sysfs:
	kobject_put(kobj_ref);
	sysfs_remove_file(kernel_kobj, &menable_info.attr);
err_reg:
    class_destroy(menable_dma_class);
err_dma_class:
    class_destroy(menable_uiq_class);
err_uiq_class:
    class_destroy(menable_class);
err_class:
    unregister_chrdev_region(devr, MEN_MAX_NUM);
    return ret;
}

static void __exit menable_exit(void)
{
    pci_unregister_driver(&menable_pci_driver);
    class_destroy(menable_dma_class);
    class_destroy(menable_uiq_class);
    class_destroy(menable_class);
    unregister_chrdev_region(devr, MEN_MAX_NUM);
    kobject_put(kobj_ref);
    sysfs_remove_file(kernel_kobj, &menable_info.attr);

    printk(KERN_INFO  "%s: unloading %s %s\n", DRIVER_NAME, DRIVER_VENDOR, DRIVER_DESCRIPTION);
}

module_init(menable_init);
module_exit(menable_exit);

MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_AUTHOR(DRIVER_VENDOR);
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
