/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "menable.h"

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <stdbool.h>
#include "menable5.h"
#include "menable4.h"
#include "menable_ioctl.h"
#include "sisoboards.h"
#include "uiq.h"

#include "linux_version.h"

#include <lib/helpers/type_hierarchy.h>
#include <lib/helpers/bits.h>
#include <lib/uiq/uiq_helper.h>

#include "men_i2c_bus.h"

static DEVICE_ATTR(design_crc, 0660, men_get_des_val, men_set_des_val);

static ssize_t
me5_get_boardinfo(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct siso_menable *men = container_of(dev, struct siso_menable, dev);
    ssize_t ret = 0;
    int i;

	dev_dbg(dev, "%s\n", __FUNCTION__);
    for (i = 0; i < 4; i++) {
        u32 tmp = men->register_interface.read(&men->register_interface, i);
        ssize_t r = sprintf(buf + ret, "0x%08x\n", tmp);

        if (r < 0) {
            return r;
        }

        ret += r;
    }

    return ret;
}

static DEVICE_ATTR(board_info, 0440, me5_get_boardinfo, NULL);


static struct lock_class_key me5_irqmask_lock;

static struct attribute *me5_attributes[3] = {
    &dev_attr_design_crc.attr,
    &dev_attr_board_info.attr,
    NULL
};

struct attribute_group me5_attribute_group = { .attrs = me5_attributes };

static const struct attribute_group *me5_attribute_groups[2] = {
    &me5_attribute_group,
    NULL
};

const struct attribute_group ** me5_init_attribute_groups(struct siso_menable *men)
{
    if (SisoBoardIsAbacus(men->pci_device_id)) {
        return me5_abacus_init_attribute_groups(men);
    }
    return me5_attribute_groups;
}

static unsigned int getBitMask(unsigned int n)
{
    unsigned int mask = 0;
    while (n--) {
        mask = (mask << 1) | 0x1;
    }
    return mask;
}

/**
 * me5_add_uiqs - scan for new UIQs in one FPGA
 * @men: board to scan
 * @fpga: fpga index to scan
 * @count: how many UIQs to add
 * @uiqoffs: register offset of the first UIQ
 *
 * This will keep all UIQs from lower FPGAs.
 */
static int
me5_add_uiqs(struct siso_menable *men, const unsigned int fpga,
        const unsigned int count, const unsigned int uiq_regaddrs_offset, const unsigned int uiqMask)
{
    uiq_base ** nuiqs;
    unsigned int i;
    int ret;
    uint32_t uiq_type_flags = men->register_interface.read(&men->register_interface, ME5_IRQTYPE);

#ifdef ENABLE_DEBUG_MSG
     printk(KERN_INFO "[%d]: me5_add_uiqs (count=%d. uiqoffs=0x%X, uiqMask=0x%X)\n", current->pid, count, uiq_regaddrs_offset, uiqMask);
#endif

    WARN_ON(!men->design_changing);
    for (i = fpga; i < MAX_FPGAS; i++) {
        WARN_ON(men->uiqcnt[i] != 0);
    }

    nuiqs = kcalloc(fpga * MEN_MAX_UIQ_PER_FPGA + count, sizeof(*men->uiqs), GFP_KERNEL);
    if (nuiqs == NULL) {
        return -ENOMEM;
    }

    for (i = 0; i < count; i++) {
        uiq_base * uiq_base;
        int chan = fpga * MEN_MAX_UIQ_PER_FPGA + i;
        uiq_type uiq_type = IS_BIT_SET(uiq_type_flags, (i + ME5_IRQQ_LOW))
                                       ? UIQ_TYPE_WRITE_LEGACY
                                       : UIQ_TYPE_READ;

        if (IS_BIT_SET(uiqMask, i)) {
            uiq_base = men_uiq_init(men, chan, 0, uiq_type, (uiq_regaddrs_offset / 4) + 2 * i, 16);

            if (IS_ERR(uiq_base)) {
                int j;
                ret = PTR_ERR(uiq_base);

                for (j = fpga * MEN_MAX_UIQ_PER_FPGA; j < chan; j++) {
                    men_uiq_remove(nuiqs[j]);
                }
                kfree(nuiqs);
                return ret;
            }

            struct menable_uiq * uiq = container_of(uiq_base, struct menable_uiq, base);
            uiq->irqack_offs = ME5_IRQACK;
            uiq->ackbit = i + ME5_IRQQ_LOW;
        } else {
            uiq_base = NULL;
        }

        nuiqs[chan] = uiq_base;
    }

    kfree(men->uiqs);
    men->uiqs = nuiqs;
    men->uiqcnt[fpga] = count;
    men->num_active_uiqs += count;

    return 0;
}

static struct me_notification_handler *
me5_create_notify_handler(struct siso_menable *men)
{
    struct me_notification_handler *bh;
    unsigned long flags = 0;

#ifdef ENABLE_DEBUG_MSG
    printk(KERN_INFO "[%d %d]: Creating Notify Handler\n", current->parent->pid, current->pid);
#endif

    bh = kzalloc(sizeof(*bh), GFP_USER);
    if (bh == NULL) {
        return NULL;
    }

    INIT_LIST_HEAD(&bh->node);

    bh->pid = current->pid;
    bh->ppid = current->parent->pid;
    bh->quit_requested = false;
    init_completion(&bh->notification_available);

    spin_lock_irqsave(&men->d5->notification_data_lock, flags);
    {
        bh->notification_time_stamp = men->d5->notification_time_stamp;
    }
    spin_unlock_irqrestore(&men->d5->notification_data_lock, flags);

    spin_lock(&men->d5->notification_handler_headlock);
    {
        list_add_tail(&bh->node, &men->d5->notification_handler_heads);
    }
    spin_unlock(&men->d5->notification_handler_headlock);

    return bh;
}

static void
me5_free_notify_handler(struct siso_menable *men,
        struct me_notification_handler *bh)
{
    BUG_ON(in_interrupt());

#ifdef ENABLE_DEBUG_MSG
    printk(KERN_INFO "[%d %d]: Removing Notify Handler\n", bh->ppid, bh->pid);
#endif

    spin_lock(&men->d5->notification_handler_headlock);
    {
        __list_del_entry(&bh->node);
    }
    spin_unlock(&men->d5->notification_handler_headlock);

    kfree(bh);
}

static struct me_notification_handler *
me5_get_notify_handler(struct siso_menable *men, const unsigned int ppid,
        const unsigned int pid)
{
    struct me_notification_handler *res;
    bool found = false;

    //printk(KERN_INFO "[%d %d]: Getting notify_handler\n", ppid, pid);

    spin_lock(&men->d5->notification_handler_headlock);
    {
        list_for_each_entry(res, &men->d5->notification_handler_heads, node) {
            if ((res->pid == pid) && (res->ppid == ppid)) {
                found = true;
                break;
            }
        }
    }
    spin_unlock(&men->d5->notification_handler_headlock);

    if (found) {
        return res;
    } else {
#ifdef ENABLE_DEBUG_MSG
        printk(KERN_INFO "[%d %d]: No notify_handler found\n", ppid, pid);
#endif
        return NULL;
    }
}

static void
me5_device_removed(struct siso_menable* men)
{
    struct me_notification_handler * handler;
    unsigned long flags;
    int i;

    spin_lock_irqsave(&men->boardlock, flags);
    {
        /* Flag releasing */
        men->releasing = true;

        /* Stop IRQs */
        men->stopirq(men);
    }
    spin_unlock_irqrestore(&men->boardlock, flags);

    /* Flag design_changing */
    spin_lock_irqsave(&men->designlock, flags);
    {
        while(men->design_changing) {
            spin_unlock_irqrestore(&men->designlock, flags);
            schedule();
            spin_lock_irqsave(&men->designlock, flags);
        }
        men->design_changing = true;
    }
    spin_unlock_irqrestore(&men->designlock, flags);

    /* Notify all available Handlers for device close */
    spin_lock_irqsave(&men->d5->notification_data_lock, flags);
    {
        men->d5->notifications |= NOTIFICATION_DEVICE_REMOVED;
        men->d5->notification_time_stamp++;
    }
    spin_unlock_irqrestore(&men->d5->notification_data_lock, flags);
    spin_lock(&men->d5->notification_handler_headlock);
    {
        list_for_each_entry(handler, &men->d5->notification_handler_heads, node) {
            complete(&handler->notification_available);
        }
    }
    spin_unlock(&men->d5->notification_handler_headlock);

    /* Free DMAs */
    spin_lock_bh(&men->buffer_heads_lock);
    {
        i = men_alloc_dma(men, 0);
        // head_lock is unlocked inside men_alloc_dma !
    }
    BUG_ON(i != 0);

    men->d5->cleanup_peripherals(men->d5);
}

static void
me5_device_reconnected(struct siso_menable* men)
{
    unsigned long flags = 0;

    men->d5->init_peripherals(men->d5);

	men->config = men->register_interface.read(&men->register_interface, ME5_CONFIG);
	men->config_ex = men->register_interface.read(&men->register_interface, ME5_CONFIG_EX);
	
    spin_lock_irqsave(&men->boardlock, flags);
    {
        /* Unflag releasing */
        men->releasing = false;

        /* Start IRQs */
        men->startirq(men);
    }
    spin_unlock_irqrestore(&men->boardlock, flags);

    /* Add DMAs */
    men_add_dmas(men);

    /* Unflag design_changing */
    spin_lock_irqsave(&men->boardlock, flags);
    {
        men->design_changing = false;
    }
    spin_unlock_irqrestore(&men->boardlock, flags);
}

/*
 * Acknowledge and re-enable the interrupt
 */
static void
me5_irq_ack(struct siso_menable *men, unsigned long alarm)
{
    unsigned long flags = 0;

    //Acknowledge the interrupt
    men->register_interface.write(&men->register_interface, ME5_IRQACK, alarm);

    // Re-enable the interrupt
    spin_lock_irqsave(&men->d5->irqmask_lock, flags);
    {
        men->d5->irq_wanted |= alarm;
        men->register_interface.write(&men->register_interface, ME5_IRQENABLE, men->d5->irq_wanted);
    }
    spin_unlock_irqrestore(&men->d5->irqmask_lock, flags);

    // Update local alarm status / notifications
    spin_lock_irqsave(&men->d5->notification_data_lock, flags);
    {
        men->d5->irq_alarms_status &= ~alarm;
        if (men->d5->irq_alarms_status == 0) {
            men->d5->notifications &= ~NOTIFICATION_DEVICE_ALARM;
        }
    }
    spin_unlock_irqrestore(&men->d5->notification_data_lock, flags);
}

static int
me5_ioctl(struct siso_menable *men, const unsigned int cmd,
        const unsigned int size, unsigned long arg)
{
    unsigned long flags = 0;

#ifdef ENABLE_DEBUG_MSG
     printk(KERN_INFO "[%d]: me5_ioctl (cmd = 0x%X, arg = 0x%X)\n", current->pid, cmd, arg);
#endif
    switch (cmd) {
    case IOCTL_BOARD_INFO:
        {
            unsigned int a[4];
            int i;

            if (size != sizeof(a)) {
                warn_wrong_iosize(men, cmd, sizeof(a));
                return -EINVAL;
            }

            for (i = 0; i < ARRAY_SIZE(a); i++) {
                a[i] = men->register_interface.read(&men->register_interface, i);
            }
            if (copy_to_user((void __user *) arg,
                    a, sizeof(a))) {
                return -EFAULT;
            }
            return 0;
        }
    case IOCTL_PP_CONTROL:
        {
            int ret = 0;
            unsigned long flags = 0;

            if (size != 0) {
                warn_wrong_iosize(men, cmd, 0);
                return -EINVAL;
            }

            spin_lock_irqsave(&men->designlock, flags);
            {
                if (men->design_changing) {
                    spin_unlock_irqrestore(&men->designlock, flags);
                    return -EBUSY;
                }

                men->design_changing = true;
            }
            spin_unlock_irqrestore(&men->designlock, flags);

            /* TODO: FPGA Master Management */
            switch (arg) {
            case 0:
            case 1:
                men->active_fpgas = 1;
                break;
            default:
                ret = -EINVAL;
            }

            spin_lock_irqsave(&men->designlock, flags);
            {
                men->design_changing = false;
            }
            spin_unlock_irqrestore(&men->designlock, flags);
            return ret;
        }
    case IOCTL_RESSOURCE_CONTROL:
    case IOCTL_GET_EEPROM_DATA:
    case IOCTL_DESIGN_SETTINGS:
        return -EINVAL;

    case IOCTL_EX_DEVICE_CONTROL:
        {
            struct men_device_control_i ctrl;
            struct men_device_control_o_v2 reply;
            unsigned long leds;
            struct me_notification_handler * handler;
            int result = 0;
            long wakeup_time = msecs_to_jiffies(250);

            if (size < sizeof(ctrl)) {
                warn_wrong_iosize(men, cmd, sizeof(ctrl));
                return -EINVAL;
            }

            if (copy_from_user(&ctrl, (void __user *) arg, sizeof(ctrl))) {
                return -EFAULT;
            }

            if (ctrl._size > size) {
                warn_wrong_iosize(men, cmd, sizeof(ctrl));
                return -EINVAL;
            }

            switch (ctrl.command) {
            case DEVCTRL_RECONFIGURE_FPGA:
                /* Check Magic word */
                if (ctrl.args.reconfigure_fpga.magic
                        != DEVCTRL_RECONFIGURE_MAGIC) {
                    return -EACCES;
                }

                /* Handle device removal */
                me5_device_removed(men);

                /* Save Config Space */
                result = pci_save_state(men->pdev);

                printk(KERN_INFO "%s: Reconfiguring board %d\n", DRIVER_NAME, men->idx);

                /* Ask FPGA to reconfigure itself */
                men->register_interface.write(&men->register_interface, ME5_RECONFIGURE_CONTROL, RECONFIGURE_FLAG);

                // Max. reconfiguration time must be less than 100 ms.
                // We will wait a bit more.
                msleep(200);

                /* Restore Config Space */
                pci_restore_state(men->pdev);

                /* Re-enable the device */
                result = pci_enable_device(men->pdev);

                /* Handle device re-attaching */
                me5_device_reconnected(men);

                break;

            case DEVCTRL_SET_LEDS:
                men->register_interface.write(&men->register_interface, ME5_LED_CONTROL, ctrl.args.set_leds.led_status);
                break;

            case DEVCTRL_GET_LEDS:
                if (size < sizeof(reply)) {
                    warn_wrong_iosize(men, cmd, sizeof(reply));
                    return -EINVAL;
                }

                leds = men->register_interface.read(&men->register_interface, ME5_LED_CONTROL);
                reply.args.get_leds.led_present = leds & 0xFFFF;
                reply.args.get_leds.led_status = (leds >> 16) & 0xFFFF;

                if (copy_to_user((void __user *) arg, &reply, sizeof(reply))) {
                    return -EFAULT;
                }

                break;

            case DEVCTRL_GET_ASYNC_NOTIFY:
                {
                    unsigned long notifications;
                    unsigned long notification_ts;
                    unsigned long alarms;

                    spin_lock_irqsave(&men->d5->notification_data_lock, flags);
                    {
                        notifications = men->d5->notifications;
                        notification_ts = men->d5->notification_time_stamp;
                    }
                    spin_unlock_irqrestore(&men->d5->notification_data_lock, flags);

                    if (size < sizeof(reply)) {
                        warn_wrong_iosize(men, cmd, sizeof(reply));
                        return -EINVAL;
                    }

                    handler = me5_get_notify_handler(men, current->parent->pid, current->pid);
                    if (handler == NULL) {
                        handler = me5_create_notify_handler(men);
                        if (handler == NULL) {
                            return -ENOMEM;
                        }
                    }

                    if ((!handler->quit_requested)
                            && ((handler->notification_time_stamp == notification_ts) || (!notifications))) {
                        // Wait until a notification is received
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)
                        wakeup_time = wait_for_completion_killable_timeout(&handler->notification_available, msecs_to_jiffies(250));
#else
                        wakeup_time = wait_for_completion_interruptible_timeout(&handler->notification_available, msecs_to_jiffies(250));
#endif
                    }

                    spin_lock_irqsave(&men->d5->notification_data_lock, flags);
                    {
                        notifications = men->d5->notifications;
                        notification_ts = men->d5->notification_time_stamp;
                        alarms = men->d5->irq_alarms_status;
                    }
                    spin_unlock_irqrestore(&men->d5->notification_data_lock, flags);

                    reply.args.get_async_event.pl = 0;
                    reply.args.get_async_event.ph = 0;

                    if (handler->quit_requested) {
                        reply.args.get_async_event.event = DEVCTRL_ASYNC_NOTIFY_DRIVER_CLOSED;
                        if (copy_to_user((void __user *) arg, &reply, sizeof(reply))) {
                            return -EFAULT;
                        }
                        me5_free_notify_handler(men, handler);
                        handler = NULL;
                    } else if (handler->notification_time_stamp != notification_ts) {

                        handler->notification_time_stamp = notification_ts;
                        // We might have received 10000 completions and just started
                        // to handle them now, so that we must reset the completion struct now.
                        // Otherwise, we will handle the same interrupt 10000 times.
                        reinit_completion(&handler->notification_available);

                        if (notifications) {
                            if (notifications & NOTIFICATION_DEVICE_REMOVED) {
                                me5_free_notify_handler(men, handler);
                                handler = NULL;
                                reply.args.get_async_event.event = DEVCTRL_ASYNC_NOTIFY_DEVICE_REMOVED;
                            } else if (notifications & NOTIFICATION_DEVICE_ALARM) {
                                reply.args.get_async_event.event = DEVCTRL_ASYNC_NOTIFY_DEVICE_ALARM;
                                reply.args.get_async_event.pl |= (alarms & INT_MASK_TEMPERATURE_ALARM) ? DEVCTRL_DEVICE_ALARM_TEMPERATURE : 0x0;
                                reply.args.get_async_event.pl |= (alarms & INT_MASK_ACTION_CMD_LOST) >> 4;
                                reply.args.get_async_event.pl |= (alarms & INT_MASK_PHY_MANAGEMENT) ? DEVCTRL_DEVICE_ALARM_PHY : 0x0;
                                reply.args.get_async_event.pl |= (alarms & INT_MASK_POE) ? DEVCTRL_DEVICE_ALARM_POE : 0x0;
                            } else if (notifications & NOTIFICATION_DRIVER_CLOSED) {
                                reply.args.get_async_event.event = DEVCTRL_ASYNC_NOTIFY_DRIVER_CLOSED;
                            } else if (notifications & NOTIFICATION_DEVICE_ADDED) {
                                reply.args.get_async_event.event = DEVCTRL_ASYNC_NOTIFY_DEVICE_ADDED;
                            }
                        } else { // No notification happened, but only timestamp updated.
                            // No notification is availble, so that it's better to return timeout
                            result = -ETIMEDOUT;
                        }
                    } else if (wakeup_time == 0) { // Timeout
                        result = -ETIMEDOUT;
                    } else if (wakeup_time > 0) { // someone sent a complete, but we don't know who ! (Should never happen)
                        result = -ETIMEDOUT;
                    } else { // (wakeup_time < 0) // Sleeping process is interrupted !
                        result = -EFAULT;
                    }

                    if (result == 0) {
                        if (copy_to_user((void __user *) arg, &reply, sizeof(reply))) {
                            return -EFAULT;
                        }
                    }

                    break;
                }

            case DEVCTRL_RESET_ASYNC_NOTIFY:
                if (ctrl.args.reset_async_event.event == DEVCTRL_ASYNC_NOTIFY_DRIVER_CLOSED) {
                    if (ctrl.args.reset_async_event.ph == DEVCTRL_CLOSE_DRIVER_MAGIC) {
                        struct me_notification_handler * notify_handler;

                        spin_lock(&men->d5->notification_handler_headlock);
                        {
                            list_for_each_entry(notify_handler, &men->d5->notification_handler_heads, node) {
                                if (current->parent->pid == notify_handler->ppid) {
                                    notify_handler->quit_requested = true;
                                    complete( &notify_handler->notification_available);
                                }
                            }
                        }
                        spin_unlock(&men->d5->notification_handler_headlock);
                    }
                } else if (ctrl.args.reset_async_event.event
                        == DEVCTRL_ASYNC_NOTIFY_DEVICE_ALARM) {
                    if (ctrl.args.reset_async_event.pl & DEVCTRL_DEVICE_ALARM_TEMPERATURE) {
                        if (ctrl.args.reset_async_event.ph != 0 &&
                                ctrl.args.reset_async_event.ph < LONG_MAX) {
                            men->d5->temperature_alarm_period = ctrl.args.reset_async_event.ph;
                            if (cancel_delayed_work(&men->d5->temperature_alarm_work)) {
                                schedule_delayed_work(&men->d5->temperature_alarm_work, msecs_to_jiffies(men->d5->temperature_alarm_period));
                            }
                        }
                    }

                    if (ctrl.args.reset_async_event.pl & DEVCTRL_DEVICE_ALARM_PHY) {
                        me5_irq_ack(men, INT_MASK_PHY_MANAGEMENT);
                    }
                    if (ctrl.args.reset_async_event.pl & DEVCTRL_DEVICE_ALARM_ACL_MASK) {
                        me5_irq_ack(men, ((ctrl.args.reset_async_event.pl & DEVCTRL_DEVICE_ALARM_ACL_MASK) << 4) & INT_MASK_ACTION_CMD_LOST);
                    }
                    if (ctrl.args.reset_async_event.pl & DEVCTRL_DEVICE_ALARM_POE) {
                        me5_irq_ack(men, INT_MASK_POE);
                    }
                }

                break;

            default:
                return -ENOSYS;
            }
            return result;
        }

    default:
        return -ENOIOCTLCMD;
    }
}

static void
me5_free_sgl(struct siso_menable *men, struct menable_dmabuf *sb)
{
    struct men_dma_chain *res = sb->dma_chain;
    dma_addr_t dma = sb->dma;

    while (res) {
        struct men_dma_chain *n;
        dma_addr_t ndma;

        n = res->next;
        ndma = (dma_addr_t) (le64_to_cpu(res->pcie4->next) & ~(3ULL));
        if (dma == ndma) {
            break;
        }
        dma_pool_free(men->sgl_dma_pool, res->pcie4, dma);
        kfree(res);
        res = n;
        dma = ndma;
    }
}

static void
me5_queue_sb(struct menable_dmachan *db, struct menable_dmabuf *sb)
{
    w64(db->parent, db->iobase + ME5_DMAMAXLEN, sb->buf_length / 4);
    w64(db->parent, db->iobase + ME5_DMAADDR, sb->dma);
}

static void
me5_temperature_alarm_work(struct work_struct *work)
{
    struct me5_data *me5;

    me5 = container_of(to_delayed_work(work), struct me5_data, temperature_alarm_work);

    me5_irq_ack(me5->men, INT_MASK_TEMPERATURE_ALARM);
}

static void
me5_irq_notification_work(struct work_struct *work)
{
    struct me5_data *me5;
    uint32_t alarms_status = 0;
    unsigned long flags = 0;
    struct me_notification_handler *notification_handler;

    me5 = container_of(work, struct me5_data, irq_notification_work);

    /* Notify all Notification Handlers */
    spin_lock(&me5->notification_handler_headlock);
    {
        list_for_each_entry(notification_handler, &me5->notification_handler_heads, node) {
            complete(&notification_handler->notification_available);
        }
    }
    spin_unlock(&me5->notification_handler_headlock);

    spin_lock_irqsave(&me5->notification_data_lock, flags);
    {
        alarms_status = me5->irq_alarms_status;
    }
    spin_unlock_irqrestore(&me5->notification_data_lock, flags);

    /* Handle Temperature alarm */
    if (alarms_status & INT_MASK_TEMPERATURE_ALARM) {
        // Acknowledge & re-enable Temperature alarm after 1 second
        // (it might be already handled within this time)
        schedule_delayed_work(&me5->temperature_alarm_work, msecs_to_jiffies(me5->temperature_alarm_period));
    }
}

static irqreturn_t
me5_irq(int irq, void *dev_id)
{
    uint32_t sr; /* Status Register */
    struct siso_menable *men = dev_id;
    menable_timespec_t timeStamp;
    ktime_t timeout;
    bool haveTimeStamp = false;

    int dma;
    uint32_t badmask = 0;
    uint32_t st; /* Masked status */

    if (pci_channel_offline(men->pdev)) {
        return IRQ_HANDLED;
    }

    if (men->active_fpgas == 0) {
        return IRQ_HANDLED;
    }

    spin_lock(&men->d5->irqmask_lock);
    {
        // Get IRQ sources
        sr = men->register_interface.read(&men->register_interface, ME5_IRQSTATUS);
        if (unlikely(sr == 0)) {
            spin_unlock(&men->d5->irqmask_lock);
            return IRQ_NONE;
        }

        // Check if FPGA is dead
        if (unlikely(sr == 0xffffffff)) {
            dev_warn(&men->dev, "IRQ status register %i read returned -1\n", 0);
            men->d5->irq_wanted = 0;
            men->register_interface.write(&men->register_interface, ME5_IRQENABLE, men->d5->irq_wanted);
            men->register_interface.write(&men->register_interface, ME5_IRQACK, 0xffffffff);
            spin_unlock(&men->d5->irqmask_lock);
            return IRQ_HANDLED;
        }

        // Check for unwanted IRQs
        badmask = sr & ~men->d5->irq_wanted;
        if (unlikely(badmask != 0)) {
            men->register_interface.write(&men->register_interface, ME5_IRQENABLE, men->d5->irq_wanted);
            men->register_interface.write(&men->register_interface, ME5_IRQACK, badmask);
            sr &= men->d5->irq_wanted;
        }
    }
    spin_unlock(&men->d5->irqmask_lock);

    /* Handle DMA Interrupts */
    st = (sr & INT_MASK_DMA_CHANNEL_IRQ);
    if (st) {
        for (dma = 0; dma < men->dmacnt[0]; dma++) {
            struct menable_dmachan *db;
            struct menable_dma_wait *waitstr;

            if ((st & (0x1 << dma)) == 0) {
                continue;
            }

            if (!haveTimeStamp) {
                haveTimeStamp = true;
                menable_get_ts(&timeStamp);
            }

            db = men_dma_channel(men, dma);
            BUG_ON(db == NULL);

            spin_lock(&db->chanlock);
            {
                men->register_interface.write(&men->register_interface, db->irqack, 1 << db->ackbit);
                uint32_t dma_count = men->register_interface.read(&men->register_interface, db->iobase + ME5_DMACOUNT);
                spin_lock(&db->listlock);
                {
                    if (unlikely(db->active == NULL)) {
                        for (int i = dma_count - db->imgcnt; i > 0; i--) {
                            uint32_t tmp = men->register_interface.read(&men->register_interface, db->iobase + ME5_DMALENGTH);
                            tmp = men->register_interface.read(&men->register_interface, db->iobase + ME5_DMATAG);
                            db->lost_count++;
                        }
                        spin_unlock(&db->listlock);
                        spin_unlock(&db->chanlock);
                        continue;
                    }

                    uint32_t delta = dma_count - db->imgcnt;
                    for (int i = 0; i < delta; ++i) {
                        struct menable_dmabuf *sb = men_move_hot(db, &timeStamp);
                        uint32_t len = men->register_interface.read(&men->register_interface, db->iobase + ME5_DMALENGTH);
                        uint32_t tag = men->register_interface.read(&men->register_interface, db->iobase + ME5_DMATAG);

                        if (unlikely(sb != NULL)) {
                            sb->dma_length = len;
                            sb->dma_tag = tag;
                            sb->frame_number = db->latest_frame_number;
                        }
                    }

                    list_for_each_entry(waitstr, &db->wait_list, node) {
                        if (waitstr->frame <= db->goodcnt)
                            complete(&waitstr->cpl);
                    }

                    if (likely(db->transfer_todo > 0)) {
                        if (delta)
                            men_dma_queue_max(db);

                        spin_unlock(&db->listlock);

                        if (db->timeout) {
                            timeout = ktime_set(db->timeout, 0);
                            spin_lock(&db->timerlock);
                            hrtimer_cancel(&db->timer);
                            hrtimer_start(&db->timer, timeout, HRTIMER_MODE_REL);
                            spin_unlock(&db->timerlock);
                        }
                    } else {
                        spin_unlock(&db->listlock);
                        db->state = MEN_DMA_CHAN_STATE_IN_SHUTDOWN;
                        schedule_work(&db->dwork);
                    }
                }
            }
            spin_unlock(&db->chanlock);
        }
    }

    /* Handle User Queue IRQ */
    st = (sr & INT_MASK_USER_QUEUE_IRQ);
    if (st) {
        uint32_t bit;
        uiq_timestamp uiq_ts;
        bool have_uiq_ts = false;

        for (bit = ME5_IRQQ_LOW; (bit <= ME5_IRQQ_HIGH) && st; bit++) {
            if (men->uiqs[bit - ME5_IRQQ_LOW] == NULL) {
                continue;
            }

            if (st & (1 << bit)) {
                uiq_irq(men->uiqs[bit - ME5_IRQQ_LOW], &uiq_ts,
                        &have_uiq_ts);
                st ^= (1 << bit);
            }
        }
    }

    /* Handle Alarms IRQ */
    st = (sr & INT_MASK_ALARMS);
    if (st) {
        uint32_t tmp = 0;
        spin_lock(&men->d5->notification_data_lock);
        {
            men->d5->irq_alarms_status |= st & men->d5->irq_wanted;
            if (likely(men->d5->irq_alarms_status != 0)) {
                // Update available notifications
                men->d5->notifications |= NOTIFICATION_DEVICE_ALARM;
                men->d5->notification_time_stamp++;
                schedule_work(&men->d5->irq_notification_work);
            }
            tmp = men->d5->irq_alarms_status;
        }
        spin_unlock(&men->d5->notification_data_lock);

        spin_lock(&men->d5->irqmask_lock);
        {
            // Disable all alarms until they are handled
            men->d5->irq_wanted &= ~tmp;
            men->register_interface.write(&men->register_interface, ME5_IRQENABLE, men->d5->irq_wanted);
        }
        spin_unlock(&men->d5->irqmask_lock);
    }

    return IRQ_HANDLED;
}

static void
me5_abortdma(struct siso_menable *men, struct menable_dmachan *dc)
{
    uint32_t dmastat;
    unsigned long retries;

    /* Reset the DMA engine and wait for Abort bit to go high */
    men->register_interface.write(&men->register_interface, dc->iobase + ME5_DMACTRL, 2);
    for (retries = 0; retries < 100; ++retries) {
        dmastat = men->register_interface.read(&men->register_interface, dc->iobase + ME5_DMACTRL);
        if ((dmastat & 4) != 0) break;
    }
        
    /* Get DMA engine out of reset and wait for Abort bit to go low */
    men->register_interface.write(&men->register_interface, dc->iobase + ME5_DMACTRL, 0);
    for (retries = 0; retries < 100; ++retries) {
        dmastat = men->register_interface.read(&men->register_interface, dc->iobase + ME5_DMACTRL);
        if ((dmastat & 4) == 0) break;
    }
}

static void
me5_stopdma(struct siso_menable *men, struct menable_dmachan *dc)
{
    uint32_t irqreg, dmastat;
    unsigned long flags, retries;

    spin_lock_irqsave(&men->d5->irqmask_lock, flags);
    {
        /* Disable the DMA engine */
        men->register_interface.write(&men->register_interface, dc->iobase + ME5_DMACTRL, 0);
        dmastat = men->register_interface.read(&men->register_interface, dc->iobase + ME5_DMACTRL);
        
        /* Disable DMA interrupt */
        irqreg = men->register_interface.read(&men->register_interface, dc->irqenable);
        irqreg &= ~(1 << dc->enablebit);
        men->register_interface.write(&men->register_interface, dc->irqenable, irqreg);

        /* Acknowledge any interrupt request that might have been generated */
        men->register_interface.write(&men->register_interface, dc->irqack, (1 << dc->ackbit));

        men->d5->irq_wanted &= ~(1 << dc->enablebit);
    }
    spin_unlock_irqrestore(&men->d5->irqmask_lock, flags);

    if (men->dma_stop_bugfix_present) {
        /* Reset the DMA engine and wait for Abort bit to go high */
        men->register_interface.write(&men->register_interface, dc->iobase + ME5_DMACTRL, 2);
        for (retries = 0; retries < 100; ++retries) {
            dmastat = men->register_interface.read(&men->register_interface, dc->iobase + ME5_DMACTRL);
            if ((dmastat & 4) != 0) break;
        }
        
        /* Get DMA engine out of reset and wait for Abort bit to go low */
        men->register_interface.write(&men->register_interface, dc->iobase + ME5_DMACTRL, 0);
        for (retries = 0; retries < 100; ++retries) {
            dmastat = men->register_interface.read(&men->register_interface, dc->iobase + ME5_DMACTRL);
            if ((dmastat & 4) == 0) break;
        }
    }
}

static int
me5_create_userbuf(struct siso_menable *men, struct menable_dmabuf *db, struct menable_dmabuf *dummybuf)
{
	struct men_dma_chain *cur;
	int i;

	db->dma_chain->pcie4 = dma_pool_alloc(men->sgl_dma_pool, GFP_USER, &db->dma);
	if (!db->dma_chain->pcie4)
		goto fail_pcie;
	memset(db->dma_chain->pcie4, 0, sizeof(*db->dma_chain->pcie4));

	cur = db->dma_chain;

	for (i = 0; i < db->num_used_sg_entries; i++) {
		int idx = i % ARRAY_SIZE(cur->pcie4->addr);

		cur->pcie4->addr[idx] =
				cpu_to_le64(sg_dma_address(db->sg + i) + 0x1);

		if ((idx == ARRAY_SIZE(cur->pcie4->addr) - 1) && (i + 1 < db->num_used_sg_entries)) {
			dma_addr_t next;

			cur->next = kzalloc(sizeof(*cur->next), GFP_USER);
			if (!cur->next)
				goto fail;

			cur->next->pcie4 = dma_pool_alloc(men->sgl_dma_pool, GFP_USER, &next);
			if (!cur->next->pcie4) {
				kfree(cur->next);
				cur->next = NULL;
				goto fail;
			}
			cur->pcie4->next = cpu_to_le64(next + 0x2);
			cur = cur->next;
			memset(cur->pcie4, 0, sizeof(*cur->pcie4));
		}
	}
	cur->pcie4->next = dummybuf->dma_chain->pcie4->next;

	return 0;
fail:
	me5_free_sgl(men, db);
	return -ENOMEM;
fail_pcie:
	kfree(db->dma_chain);
	return -ENOMEM;
}

static int
me5_create_dummybuf(struct siso_menable *men, struct menable_dmabuf *db)
{
    struct men_dma_chain *cur;
    int i;

    db->index = -1;
    db->dma_chain = kzalloc(sizeof(*db->dma_chain), GFP_KERNEL);
    if (!db->dma_chain) {
        goto fail_dmat;
    }

    db->dma_chain->pcie4 = dma_pool_alloc(men->sgl_dma_pool, GFP_USER, &db->dma);
    if (!db->dma_chain->pcie4) {
        goto fail_pcie;
    }

    memset(db->dma_chain->pcie4, 0, sizeof(*db->dma_chain->pcie4));

    cur = db->dma_chain;

    for (i = 0; i < ARRAY_SIZE(cur->pcie4->addr); i++) {
        cur->pcie4->addr[i] = cpu_to_le64(men->d5->dummypage_dma + 0x1);
    }

    cur->pcie4->next = cpu_to_le64(db->dma + 0x2);

    db->buf_length = -1;

    return 0;

fail_pcie:
    kfree(db->dma_chain);
fail_dmat:
    return -ENOMEM;
}

static void
me5_destroy_dummybuf(struct siso_menable *men, struct menable_dmabuf *db)
{
    dma_pool_free(men->sgl_dma_pool, db->dma_chain->pcie4, db->dma);
    kfree(db->dma_chain);
}

static unsigned int
me5_query_dma(struct siso_menable *men, const unsigned int arg)
{
    uint32_t u;

    BUG_ON(men->active_fpgas <= 0);

    u = men->register_interface.read(&men->register_interface, ME5_NUMDMA);
    if (unlikely(u == 0xffffffff)) {
        dev_warn(&men->dev, "Reading DMACNT from FPGA %i failed\n", 0);
        u = 0;
    } else {
        dev_dbg(&men->dev, "%i DMA channels detected in FPGA %i\n", u, 0);
    }

    return u;
}

static int
me5_startdma(struct siso_menable *men, struct menable_dmachan *dc)
{
    uint32_t tmp, dir;
    unsigned long flags;

    me5_abortdma(men, dc);

    dir = (dc->direction == PCI_DMA_TODEVICE) ? 2 : 1;

    tmp = men->register_interface.read(&men->register_interface, dc->iobase + ME5_DMATYPE);
    if (!(tmp & dir)) {
        return -EACCES;
    }
    men->register_interface.write(&men->register_interface, dc->iobase + ME5_DMATYPE, dir);

    /* clear IRQ */
    men->register_interface.write(&men->register_interface, dc->irqack, 1 << dc->ackbit);

    dc->imgcnt = men->register_interface.read(&men->register_interface, dc->iobase + ME5_DMACOUNT);

    men_dma_queue_max(dc);

    /* Enable DMA IRQ*/
    spin_lock_irqsave(&men->d5->irqmask_lock, flags);
    {
        men->d5->irq_wanted |= (1 << dc->enablebit);
        men->register_interface.write(&men->register_interface, dc->irqenable, men->d5->irq_wanted);
    }
    spin_unlock_irqrestore(&men->d5->irqmask_lock, flags);

    /* TODO: Is this really necessary? This code is from mE4; the register documentation for mE5 listst this as reserved... */
    men->register_interface.write(&men->register_interface, dc->iobase + ME5_DMAACTIVE, 1);
    (void) men->register_interface.read(&men->register_interface, dc->iobase + ME5_DMAACTIVE);
    men->register_interface.write(&men->register_interface, dc->iobase + ME5_DMAACTIVE, 0);
    (void) men->register_interface.read(&men->register_interface, dc->iobase + ME5_DMAACTIVE);

    men->register_interface.write(&men->register_interface, dc->iobase + ME5_DMACTRL, 1);
    (void) men->register_interface.read(&men->register_interface, dc->iobase + ME5_DMACTRL);

    return 0;
}

static void
me5_dmabase(struct siso_menable *men, struct menable_dmachan *dc)
{
    dc->iobase = ME5_DMAOFFS + ME5_DMASZ * dc->number;
    dc->irqack = ME5_IRQACK;
    dc->ackbit = dc->number;
    dc->irqenable = ME5_IRQENABLE;
    dc->enablebit = dc->ackbit;
}

static void
me5_stopirq(struct siso_menable *men)
{
    unsigned int i;

    // Make sure that temperature alarm handling is not running
    if (!cancel_delayed_work(&men->d5->temperature_alarm_work)) {
        flush_delayed_work(&men->d5->temperature_alarm_work);
    }

    if (men->active_fpgas > 0) {
        unsigned long flags = 0;

        spin_lock_irqsave(&men->d5->irqmask_lock, flags);
        {
            men->register_interface.write(&men->register_interface, ME5_IRQENABLE, 0);
            men->d5->irq_wanted = 0;
        }
        spin_unlock_irqrestore(&men->d5->irqmask_lock, flags);

        men->register_interface.write(&men->register_interface, ME5_IRQACK, 0xffffffff);
    }
    men->active_fpgas = 1;

    for (i = 0; i < men->uiqcnt[0]; i++) {
        if (men->uiqs[i] == NULL) {
            continue;
        }
        men->uiqs[i]->is_running = false;
    }

    // We might have scheduled another round of temperature alarm work in the meantime, so cancel again
    cancel_delayed_work(&men->d5->temperature_alarm_work);
}

static void
me5_startirq(struct siso_menable *men)
{
    unsigned long flags = 0;

    // Enable User IRQs
    uint32_t mask = ((1 << men->uiqcnt[0]) - 1) << ME5_IRQQ_LOW;

    // Enable all Alarms
    mask |= INT_MASK_ALARMS;

    spin_lock_irqsave(&men->d5->irqmask_lock, flags);
    {
        men->d5->irq_wanted = mask;

        men->register_interface.write(&men->register_interface, ME5_IRQACK, 0xffffffff);
        men->register_interface.write(&men->register_interface, ME5_IRQENABLE, mask);
    }
    spin_unlock_irqrestore(&men->d5->irqmask_lock, flags);
}

static void
me5_cleanup(struct siso_menable *men)
{
    unsigned long flags;

    spin_lock_irqsave(&men->designlock, flags);
    {
        men->design_changing = true;
    }
    spin_unlock_irqrestore(&men->designlock, flags);

    men_del_uiqs(men, 1);
    memset(men->desname, 0, men->deslen);

    spin_lock_irqsave(&men->designlock, flags);
    {
        men->design_changing = false;
    }
    spin_unlock_irqrestore(&men->designlock, flags);
}

static void
me5_exit(struct siso_menable *men)
{
    free_irq(men->pdev->irq, men);

    men->d5->cleanup_peripherals(men->d5);

    dmam_pool_destroy(men->sgl_dma_pool);
    pci_free_consistent(men->pdev, PCI_PAGE_SIZE, men->d5->dummypage, men->d5->dummypage_dma);
    kfree(men->uiqs);
    kfree(men->d5);
}

static int
me5_init_uiqs(struct siso_menable *men)
{
    unsigned int boardType;
    unsigned int fpgaUIQs;
    unsigned int uiq_regaddr_offset;
    unsigned int uiqcnt;
    unsigned int uiqmask;

    boardType = GETBOARDTYPE(men->config_ex);

    fpgaUIQs = men->register_interface.read(&men->register_interface, ME5_UIQCNT);
    uiq_regaddr_offset = men->register_interface.read(&men->register_interface, ME5_FIRSTUIQ);

    // Now try to get the UIQ mask
    // This is a workaround for gaps in single CL designs
    switch (boardType) {
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
        {
            unsigned int clConfig = (men->register_interface.read(&men->register_interface, ME5_CLCAMERASTATUS) >> ME5_CLCHIPGROUPA_SHIFT) & ME5_CLCHIPGROUPA_MASK;
            switch (clConfig) {
            case ME5_CLCHIPGROUPNOTUSED:
                uiqmask = getBitMask(fpgaUIQs) << 4;
                uiqcnt = fpgaUIQs + 4;
                break;
            case ME5_CLSINGLEBASE0:
            case ME5_CLSINGLEBASE1:
            case ME5_CLFULL:
                uiqmask = 0x3 | (getBitMask(fpgaUIQs - 2) << 4);
                uiqcnt = fpgaUIQs + 2;
                break;
            case ME5_CLMEDIUM:
                // In the firmware, the number of UIQs is erroneously calculated as 14 on medium configuration; it should be reported as 12
                // (basically, the firmware does not distinguish between medium and dual base)
                uiqmask = 0x3 | (getBitMask(fpgaUIQs - 4) << 4);
                uiqcnt = fpgaUIQs;
                break;
            case ME5_CLDUALBASE:
                uiqmask = getBitMask(fpgaUIQs);
                uiqcnt = fpgaUIQs;
                break;
            default:
                uiqmask = getBitMask(fpgaUIQs);
                uiqcnt = fpgaUIQs;
                break;
            }
        }
        break;

    default:
        uiqmask = getBitMask(fpgaUIQs);
        uiqcnt = fpgaUIQs;
        break;
    }

    if ((uiqcnt == 0) && (uiq_regaddr_offset == 0)) {
        /* old firmware versions did not provide this through a register */
        uiqcnt = ME5_IRQQ_HIGH - ME5_IRQQ_LOW + 1;
        uiq_regaddr_offset = 4 * ME5_IRQQUEUE; /* This is the only place we still multiply the register address by 4, as the number read from the register is a byte address */
    }

    if (uiqcnt != 0) {
        return me5_add_uiqs(men, 0, uiqcnt, uiq_regaddr_offset, uiqmask);
    }

    return 0;
}

static void
me5_check_stop_bugfix(struct siso_menable *men)
{
    unsigned int boardType;
    unsigned short firmwareVersion;

    boardType = GETBOARDTYPE(men->config_ex);

    firmwareVersion = GETFIRMWAREVERSION(men->config);
    switch (boardType) {
    case PN_MICROENABLE5_MARATHON_ACX_SP:
    case PN_MICROENABLE5_MARATHON_ACX_DP:
    case PN_MICROENABLE5_MARATHON_VCX_QP:
        if (firmwareVersion >= 0x101) men->dma_stop_bugfix_present = true;
        break;
    case PN_MICROENABLE5_ABACUS_4G_BASE:
        if (firmwareVersion >= 0x102) men->dma_stop_bugfix_present = true;
        break;
    case PN_MICROENABLE5_ABACUS_4G_BASE_II:
        if (firmwareVersion >= 0x103) men->dma_stop_bugfix_present = true;
        break;
    case PN_MICROENABLE5AD8CL:
    case PN_MICROENABLE5VD8CL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
        if (firmwareVersion >= 0x106) men->dma_stop_bugfix_present = true;
        break;
    case PN_MICROENABLE5A1CLHSF2:
    case PN_MICROENABLE5A2CLHSF2:
    case PN_MICROENABLE5AD8CLHSF2:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
        if (firmwareVersion >= 0x107) men->dma_stop_bugfix_present = true;
        break;
    case PN_MICROENABLE5_MARATHON_VF2_DP:
        if (firmwareVersion >= 0x201) men->dma_stop_bugfix_present = true;
        break;
    case PN_MICROENABLE5_MARATHON_AF2_DP:
        if (firmwareVersion >= 0x202) men->dma_stop_bugfix_present = true;
        break;
    case PN_MICROENABLE5A1CXP4:
    case PN_MICROENABLE5AQ8CXP6B:
    case PN_MICROENABLE5AQ8CXP6D:
    case PN_MICROENABLE5VQ8CXP6B:
    case PN_MICROENABLE5VQ8CXP6D:
        if (firmwareVersion >= 0x20a) men->dma_stop_bugfix_present = true;
        break;
    case PN_TDI:
        if (firmwareVersion >= 0x301) men->dma_stop_bugfix_present = true;
        break;
    case PN_MICROENABLE5_MARATHON_ACX_QP:
        if (firmwareVersion >= 0x302) men->dma_stop_bugfix_present = true;
        break;
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
        men->dma_stop_bugfix_present = true;
        break;
    }

    if (men->dma_stop_bugfix_present) {
        dev_info(&men->dev, "Board has DMA bugfix\n");
    }
}

int
me5_probe(struct siso_menable *men)
{
    int ret = -ENOMEM;

    if(SisoBoardIsMarathon(men->pci_device_id)) {
        struct me5_marathon * me5 = kzalloc(sizeof(*me5), GFP_KERNEL);
        if (me5 == NULL) goto fail;
        me5_marathon_init(men, me5);
        men->d5 = upcast(me5);
    } else if (SisoBoardIsIronMan(men->pci_device_id)) {
        struct me5_ironman * me5 = kzalloc(sizeof(*me5), GFP_KERNEL);
        if (me5 == NULL) goto fail;
        me5_ironman_init(men, me5);
        men->d5 = upcast(me5);
    } else if (SisoBoardIsAbacus(men->pci_device_id)) {
        struct me5_abacus * me5 = kzalloc(sizeof(*me5), GFP_KERNEL);
        if (me5 == NULL) goto fail;
        me5_abacus_init(men, me5);
        men->d5 = upcast(me5);
    } else {
        dev_err(&men->dev, "Failed to detect board variant.");
        goto fail;
    }

    men->d5->men = men;

    men->active_fpgas = 1;

    men->register_interface.activate(&men->register_interface);

    spin_lock_init(&men->d5->irqmask_lock);
    lockdep_set_class(&men->d5->irqmask_lock, &me5_irqmask_lock);

    spin_lock_init(&men->d5->notification_data_lock);
    spin_lock_init(&men->d5->notification_handler_headlock);

    if (pci_set_dma_mask(men->pdev, DMA_BIT_MASK(64))) {
        dev_err(&men->dev, "Failed to set DMA mask\n");
        goto fail_mask;
    }
    pci_set_consistent_dma_mask(men->pdev, DMA_BIT_MASK(64));
    men->sgl_dma_pool = dmam_pool_create("me5_sgl", &men->pdev->dev,
            sizeof(struct me4_sgl), 128, PCI_PAGE_SIZE);
    if (!men->sgl_dma_pool) {
        dev_err(&men->dev, "Failed to allocate DMA pool\n");
        goto fail_pool;
    }

    men->d5->dummypage = pci_alloc_consistent(men->pdev, PCI_PAGE_SIZE, &men->d5->dummypage_dma);
    if (men->d5->dummypage == NULL) {
        dev_err(&men->dev, "Failed to allocate dummy page\n");
        goto fail_dummy;
    }

    men->dma_fifo_length = ME5_DMA_FIFO_DEPTH;

    men->create_dummybuf = me5_create_dummybuf;
    men->free_dummybuf = me5_destroy_dummybuf;
    men->create_buf = me5_create_userbuf;
    men->free_buf = me5_free_sgl;
    men->startdma = me5_startdma;
    men->abortdma = me5_abortdma;
    men->stopdma = me5_stopdma;
    men->stopirq = me5_stopirq;
    men->startirq = me5_startirq;
    men->ioctl = me5_ioctl;
    men->exit = me5_exit;
    men->cleanup = me5_cleanup;
    men->query_dma = me5_query_dma;
    men->dmabase = me5_dmabase;
    men->queue_sb = me5_queue_sb;

    men->config = men->register_interface.read(&men->register_interface, ME5_CONFIG);
    men->config_ex = men->register_interface.read(&men->register_interface, ME5_CONFIG_EX);

    ret = me5_init_uiqs(men);
    if (ret != 0) {
        goto fail_uiqs;
    }

    men->first_va_event_uiq_idx = get_first_va_event_uiq_idx_me5(men->pci_device_id);
    if (MEN_IS_ERROR(men->first_va_event_uiq_idx)) {
        dev_err(&men->dev, "Failed to determine the first VA event UIQ index. Error Code: %d\n", men->first_va_event_uiq_idx);
        goto fail_uiqs;
    }

    me5_check_stop_bugfix(men);

    men->d5->temperature_alarm_period = 1000;

    INIT_LIST_HEAD(&men->d5->notification_handler_heads);
    INIT_DELAYED_WORK(&men->d5->temperature_alarm_work, me5_temperature_alarm_work);
    INIT_WORK(&men->d5->irq_notification_work, me5_irq_notification_work);

    men->desname = men->d5->design_name;
    men->deslen = sizeof(men->d5->design_name);

    me5_stopirq(men);

    ret = request_irq(men->pdev->irq, me5_irq, IRQF_SHARED, DRIVER_NAME, men);
    if (ret) {
        dev_err(&men->dev, "Failed to request interrupt\n");
        goto fail_irq;
    }

    ret = men->d5->init_peripherals(men->d5);
    if (ret) {
        /* An error during peripheral initialization is not passed on to
         * the caller so that the device will at least be available in
         * the system and analyzing the problem will be possible to some extent.
         */
        dev_warn(&men->dev, "Error during peripheral initialization.\n");
    }

    ret = men_set_state(men, BOARD_STATE_READY);
    if (ret) {
        goto fail_state;
    }

    return 0;

fail_state:
    free_irq(men->pdev->irq, men);
fail_irq:
    men_del_uiqs(men, 0);
    kfree(men->uiqs);
fail_uiqs:
	pci_free_consistent(men->pdev, PCI_PAGE_SIZE, men->d5->dummypage, men->d5->dummypage_dma);
fail_dummy:
    dmam_pool_destroy(men->sgl_dma_pool);
fail_pool:
fail_mask:
    kfree(men->d5);
fail:
    return ret;
}
