/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#if defined(DEBUG_IRQS) && !defined(DEBUG)
#define DEBUG
#endif

#include "menable.h"

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/vmalloc.h>

#include <lib/helpers/helper.h>
#include <lib/boards/peripheral_declaration.h>
#include <lib/helpers/dbg.h>
#include <lib/helpers/error_handling.h>
#include <lib/uiq/uiq_base.h>
#include <lib/uiq/uiq_transfer_state.h>

#include "menable6.h"
#include "menable_ioctl.h"
#include "sisoboards.h"
#include "uiq.h"

#include "debugging_macros.h"

#define ME6_MAX_UIQS 64

static DEVICE_ATTR(design_crc, 0660, men_get_des_val, men_set_des_val);

static struct attribute *me6_attributes[2] = {
    &dev_attr_design_crc.attr,
    NULL
};

struct attribute_group me6_attribute_group = { .attrs = me6_attributes };

static const struct attribute_group *me6_attribute_groups[2] = {
    &me6_attribute_group,
    NULL
};

const struct attribute_group ** me6_init_attribute_groups(struct siso_menable *men)
{
    if (SisoBoardIsAbacus(men->pci_device_id)) {
        return me6_abacus_init_attribute_groups(men);
    }
    return me6_attribute_groups;
}

int me6_update_board_status(struct siso_menable *men)
{
    int ret = 0;
    u32 ctrl;
    ret = MCapRegRead(&men->d6->mdev, MCAP_CONTROL, &ctrl);
    if (ret == 0) {
        if ((ctrl & MCAP_CTRL_DESIGN_SWITCH_MASK) != 0) {
            // We probe the register interface here, so force activate it
            men->register_interface.activate(&men->register_interface);
            men->config_ex = men->register_interface.read(&men->register_interface, ME6_REG_BOARD_ID);
            dev_dbg(&men->dev, "PCIe interface is active, Board Status reads as %08x\n", men->config_ex);
        } else {
            men->config = men->config_ex = (unsigned int) -1;
            dev_dbg(&men->dev, "PCIe interface is inactive\n");
        }
    } else {
        men->config = men->config_ex = (unsigned int) -1;
        dev_err(&men->dev, "failed to read MCAP control register\n");
    }

    if (men->config_ex == (unsigned int) -1 || men->config_ex == 0) {
        men->register_interface.deactivate(&men->register_interface);
        enum men_board_state current_board_state = men_get_state(men);
        if (current_board_state > BOARD_STATE_UNINITIALISED) {
            dev_warn(&men->dev, "device appears to be dead\n");
            ret = men_set_state(men, BOARD_STATE_DEAD);
        } else if (current_board_state != BOARD_STATE_UNINITIALISED){
            dev_info(&men->dev, "device is not configured\n");
            ret = men_set_state(men, BOARD_STATE_UNINITIALISED);
        }
    } else {
        if (men_get_state(men) < BOARD_STATE_READY) {
            dev_info(&men->dev, "device is configured\n");
            ret = men_set_state(men, BOARD_STATE_READY);
        }

        // Poll for FPGA DNA ready
        unsigned int i;
        for (i = 0, men->config = (unsigned int)-1; (i < ME6_BOARD_STATUS_DNA_MAX_RETRIES) && ((men->config & ME6_BOARD_STATUS_DNA_NOT_READY) != 0); ++i) {
            men->config = men->register_interface.read(&men->register_interface, ME6_REG_BOARD_STATUS);
        }
        if ((men->config & ME6_BOARD_STATUS_DNA_NOT_READY) == 0) {
            dev_dbg(&men->dev, "device FPGA DNA is available after %u cycle(s)\n", i);
        } else {
            dev_err(&men->dev, "device FPGA DNA is not available; timed out\n");
        }

        men->fpga_dna[0] = men->register_interface.read(&men->register_interface, ME6_REG_FPGA_DNA_0);
        men->fpga_dna[1] = men->register_interface.read(&men->register_interface, ME6_REG_FPGA_DNA_1);
        men->fpga_dna[2] = men->register_interface.read(&men->register_interface, ME6_REG_FPGA_DNA_2);
    }

    return ret;
}

static struct me_notification_handler *
me6_create_notify_handler(struct siso_menable *men)
{
    struct me_notification_handler *bh;
    unsigned long flags = 0;

#ifdef ENABLE_DEBUG_MSG
    printk(KERN_INFO "[%d %d]: creating Notify Handler\n", current->parent->pid, current->pid);
#endif

    bh = kzalloc(sizeof(*bh), GFP_KERNEL);
    if (bh == NULL) {
        return NULL;
    }

    INIT_LIST_HEAD(&bh->node);

    bh->pid = current->pid;
    bh->ppid = current->parent->pid;
    bh->quit_requested = false;
    init_completion(&bh->notification_available);

    spin_lock_irqsave(&men->d6->notification_data_lock, flags);
    {
        bh->notification_time_stamp = men->d6->notification_time_stamp;
    }
    spin_unlock_irqrestore(&men->d6->notification_data_lock, flags);

    spin_lock(&men->d6->notification_handler_headlock);
    {
        list_add_tail(&bh->node, &men->d6->notification_handler_heads);
    }
    spin_unlock(&men->d6->notification_handler_headlock);

    return bh;
}

static void
me6_free_notify_handler(struct siso_menable *men,
        struct me_notification_handler *bh)
{
    BUG_ON(in_interrupt());

#ifdef ENABLE_DEBUG_MSG
    printk(KERN_INFO "[%d %d]: removing Notify Handler\n", bh->ppid, bh->pid);
#endif

    spin_lock(&men->d6->notification_handler_headlock);
    {
        __list_del_entry(&bh->node);
    }
    spin_unlock(&men->d6->notification_handler_headlock);

    kfree(bh);
}

static struct me_notification_handler *
me6_get_notify_handler(struct siso_menable *men, const unsigned int ppid,
        const unsigned int pid)
{
    struct me_notification_handler *res;
    bool found = false;

    //printk(KERN_INFO "[%d %d]: getting notify_handler\n", ppid, pid);

    spin_lock(&men->d6->notification_handler_headlock);
    {
        list_for_each_entry(res, &men->d6->notification_handler_heads, node) {
            if ((res->pid == pid) && (res->ppid == ppid)) {
                found = true;
                break;
            }
        }
    }
    spin_unlock(&men->d6->notification_handler_headlock);

    if (found) {
        return res;
    } else {
#ifdef ENABLE_DEBUG_MSG
        printk(KERN_INFO "[%d %d]: no notify_handler found\n", ppid, pid);
#endif
        return NULL;
    }
}

static void
me6_ack_alarms(struct siso_menable *men, unsigned long alarms)
{
    unsigned long flags = 0;

    spin_lock_irqsave(&men->d6->notification_data_lock, flags);
    {
        men->d6->alarms_status &= ~alarms;
        if (men->d6->alarms_status == 0) {
            men->d6->notifications &= ~NOTIFICATION_DEVICE_ALARM;
        }
        men->d6->alarms_enabled |= alarms;
        men->register_interface.write(&men->register_interface, ME6_REG_IRQ_ALARMS_ENABLE, men->d6->alarms_enabled);
    }
    spin_unlock_irqrestore(&men->d6->notification_data_lock, flags);
}

static int
me6_alloc_va_event_uiqs(struct siso_menable *men, uint32_t num_events) {

    int result = STATUS_OK;

    struct me6_data * me6 = men->d6;

    uint32_t first_va_event_uiq = me6->num_uiq_declarations;
    uint32_t new_uiq_count = first_va_event_uiq + num_events;

    if (new_uiq_count > ME6_MAX_UIQS) {
        result = STATUS_ERR_INVALID_ARGUMENT;
        dev_err(&men->dev, "[UIQ] Cannot allocate %u event UIQs. Exceeding maximum of %d UIQs.\n", new_uiq_count, ME6_MAX_UIQS);
        goto err_exit;
    }

    /* TODO: [RKN] Use a lock and spin_lock_irqsave()? */
    disable_irq(me6->vectors[ME6_IRQ_EVENT_INDEX]);

    if (men->uiqcnt[0] < new_uiq_count) {
        /* We have to increase the UIQ array */

        DEV_DBG_UIQ(&men->dev, "Allocate %u VA events\n", num_events);

        /* keep a local copy of the old uiqs array, in case krealloc fails */
        struct uiq_base ** old_uiqs = men->uiqs;
        men->uiqs = krealloc(men->uiqs, new_uiq_count * sizeof(*men->uiqs), GFP_KERNEL);
        if (men->uiqs == NULL) {
            dev_err(&men->dev, "[UIQ] Failed to increase number of UIQs.\n");
            men->uiqs = old_uiqs;

            result = STATUS_ERR_INSUFFICIENT_MEM;
            goto err_enable_irqs;
        }

        /* Init new UIQs */
        const uint32_t current_number_of_events = men->uiqcnt[0];
        const uint32_t new_number_of_events = first_va_event_uiq + num_events;
        for (uint32_t event_idx = current_number_of_events; event_idx < new_number_of_events; ++event_idx) {
            const uint32_t event_id = event_idx - first_va_event_uiq;
            uiq_base * new_event_uiq = men_uiq_init(men, event_idx, event_id, UIQ_TYPE_READ, ME6_REG_IRQ_EVENT_DATA, 0);
            if (IS_ERR(new_event_uiq)) {
                /* TODO: [RKN] Error Handling! Continue with what we've got? Enter an error state? */

                /* We keep what we got so far and let the caller decide how to deal with it. */
                result = PTR_ERR(new_event_uiq);
                goto err_enable_irqs;
            }
            men->uiqs[event_idx] = new_event_uiq;

            /* this is used to free the uiqs later */
            men->uiqcnt[0] += 1;

            /* this is used to inform the user about the number of available uiqs */
            men->num_active_uiqs += 1;
        }

    } else if (men->num_active_uiqs > first_va_event_uiq + num_events) {
        /* The number of VA event UIQs is being reduced */

        DEV_DBG_UIQ(&men->dev, "Scale number of VA events down to %u\n", num_events);

        /*
         * TODO: [RKN] What do we need to do here? Complete the UIQs completion? Reset its internal state?
         */
        for (unsigned int n = first_va_event_uiq + num_events; n < men->num_active_uiqs; n++) {
            if (men->uiqs[n] != NULL) {
                struct menable_uiq * uiq = container_of(men->uiqs[n], struct menable_uiq, base);
                if (!completion_done(&uiq->cpl)) {
                    /* TODO: [RKN] Use complete_all() in case we have multiple waiting threads? */
                    complete(&uiq->cpl);
                }
            }
        }
        men->num_active_uiqs = first_va_event_uiq + num_events;
    }

err_enable_irqs:
    enable_irq(me6->vectors[ME6_IRQ_EVENT_INDEX]);

err_exit:
    return result;
}

static int
me6_ioctl(struct siso_menable *men, const unsigned int cmd,
        const unsigned int size, unsigned long arg)
{
    int ret = 0;
    unsigned long flags = 0;

#ifdef ENABLE_DEBUG_MSG
     printk(KERN_INFO "[%d]: me6_ioctl (cmd = 0x%X, arg = 0x%X)\n", current->pid, cmd, arg);
#endif
    switch (cmd) {
    case IOCTL_PP_CONTROL:
        {
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
                break;
            case 1:
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

    case IOCTL_EX_CONFIGURE_FPGA:
        {
            struct men_configure_fpga conf;
            uint32_t * data = NULL;
            int len = 0;

            if (size < sizeof(conf)) {
                warn_wrong_iosize(men, cmd, 0);
                return -EINVAL;
            }

            if (copy_from_user(&conf, (const void __user *) arg, sizeof(conf))) {
                dev_err(&men->dev, "failed to copy ioctl data from user");
                return -EFAULT;
            }

            if (conf._size < sizeof(conf) || conf._version < 1 || conf.length == 0) {
                dev_err(&men->dev, "invalid parameters in ioctl data");
                return -EINVAL;
            }

            len = conf.length / sizeof(uint32_t);
            data = vmalloc(conf.length);
            if (!data) {
                dev_err(&men->dev, "failed to allocate memory");
                return -ENOMEM;
            }

            if (copy_from_user(data, ((const u8 __user *) conf.buffer_address), conf.length)) {
                vfree(data);
                dev_err(&men->dev, "failed to copy configuration data from user");
                return -EFAULT;
            }

            if (men_get_state(men) > BOARD_STATE_UNINITIALISED) {
                men->d6->stop_peripherals(men->d6);
                men->stopirq(men);
                men->d6->cleanup_peripherals(men->d6);
                men_set_state(men, BOARD_STATE_UNINITIALISED);
            }

            if ((conf.flags & CONFIGURE_ME6_BITSTREAM_TYPE_MASK) == CONFIGURE_ME6_PARTIAL_BITSTREAM) {
                dev_dbg(&men->dev, "partial reconfiguration, data length %d bytes\n", len);
                ret = MCapWritePartialBitStream(&men->d6->mdev, data, len,
                                                (conf.flags & CONFIGURE_ME6_SWAP_BYTES) != 0 ? 1 : 0);
            } else {
                dev_dbg(&men->dev, "full reconfiguration, data length %d bytes\n", len);
                ret = MCapWriteBitStream(&men->d6->mdev, data, len,
                                         (conf.flags & CONFIGURE_ME6_SWAP_BYTES) != 0 ? 1 : 0);
            }
            vfree(data);

            if (ret == 0 && (conf.flags & CONFIGURE_ME6_DESIGN_SWITCH) != 0) {
                dev_dbg(&men->dev, "activating design switch\n");
                u32 ctrl = 0;
                ret = MCapRegRead(&men->d6->mdev, MCAP_CONTROL, &ctrl);
                if (ret == 0) {
                    ret = MCapRegWrite(&men->d6->mdev, MCAP_CONTROL, ctrl | MCAP_CTRL_DESIGN_SWITCH_MASK);
                }
            }

            if (ret == 0) {
                ret = me6_update_board_status(men);
            }

            if (ret == 0 && men_get_state(men) >= BOARD_STATE_READY) {
                ret = men->d6->init_peripherals(men->d6);
                if (ret == 0) {
                    men->startirq(men);
                    men->d6->start_peripherals(men->d6);
                } else {
                    /* ret is a driver lib error code. Map it to an errno value. */
                    /* TODO: [RKN] EFAULT refers to memory issues, but I did not find a suitable
                     *             errno code for "device initialization failed". Possible candiates
                     *             are ENOTRECOVERABLE and EOWNERDEAD.
                     */
                    ret = -EFAULT;
                }
            }

            return ret;
        }

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
            case DEVCTRL_SET_LEDS:
                men->register_interface.write(&men->register_interface, ME6_REG_LED_CONTROL, ctrl.args.set_leds.led_status);
                break;

            case DEVCTRL_GET_LEDS:
                if (size < sizeof(reply)) {
                    warn_wrong_iosize(men, cmd, sizeof(reply));
                    return -EINVAL;
                }

                leds = men->register_interface.read(&men->register_interface, ME6_REG_LED_CONTROL);
                reply.args.get_leds.led_present = leds & 0xFFFF;
                reply.args.get_leds.led_status = (leds >> 16) & 0xFFFF;

                if (copy_to_user((void __user *) arg, &reply, sizeof(reply))) {
                    return -EFAULT;
                }

                break;

            case DEVCTRL_GET_ASYNC_NOTIFY:
                {
                    unsigned long notifications;
                    unsigned long notifications_ts;
                    unsigned long alarms_status;

                    spin_lock_irqsave(&men->d6->notification_data_lock, flags);
                    {
                        notifications = men->d6->notifications;
                        notifications_ts = men->d6->notification_time_stamp;
                    }
                    spin_unlock_irqrestore(&men->d6->notification_data_lock, flags);

                    if (size < sizeof(reply)) {
                        warn_wrong_iosize(men, cmd, sizeof(reply));
                        return -EINVAL;
                    }

                    handler = me6_get_notify_handler(men, current->parent->pid, current->pid);
                    if (handler == NULL) {
                        handler = me6_create_notify_handler(men);
                        if (handler == NULL) {
                            return -ENOMEM;
                        }
                    }

                    if ((!handler->quit_requested)
                            && ((handler->notification_time_stamp == notifications_ts) || (!notifications))) {
                        // Wait until a notification is received
                        wakeup_time = wait_for_completion_killable_timeout(&handler->notification_available, msecs_to_jiffies(250));
                    }

                    spin_lock_irqsave(&men->d6->notification_data_lock, flags);
                    {
                        notifications = men->d6->notifications;
                        notifications_ts = men->d6->notification_time_stamp;
                        alarms_status = men->d6->alarms_status;
                    }
                    spin_unlock_irqrestore(&men->d6->notification_data_lock, flags);

                    reply.args.get_async_event.pl = 0;
                    reply.args.get_async_event.ph = 0;

                    if (handler->quit_requested) {
                        DBG_STMT(pr_debug("Quit requested\n"));

                        reply.args.get_async_event.event = DEVCTRL_ASYNC_NOTIFY_DRIVER_CLOSED;
                        if (copy_to_user((void __user *) arg, &reply, sizeof(reply))) {
                            return -EFAULT;
                        }
                        me6_free_notify_handler(men, handler);
                        handler = NULL;
                    } else if (handler->notification_time_stamp != notifications_ts) {

                        handler->notification_time_stamp = notifications_ts;
                        // We might have received 10000 completions and just started
                        // to handle them now, so that we must reset the completion struct now.
                        // Otherwise, we will handle the same interrupt 10000 times.
                        reinit_completion(&handler->notification_available);

                        if (notifications) {
                            if (notifications & NOTIFICATION_DEVICE_REMOVED) {
                                me6_free_notify_handler(men, handler);
                                handler = NULL;
                                reply.args.get_async_event.event = DEVCTRL_ASYNC_NOTIFY_DEVICE_REMOVED;
                            } else if (notifications & NOTIFICATION_DEVICE_ALARM) {
                                reply.args.get_async_event.event = DEVCTRL_ASYNC_NOTIFY_DEVICE_ALARM;
                                reply.args.get_async_event.pl = men->d6->alarms_to_event_pl(alarms_status);
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
                }

                break;

            case DEVCTRL_RESET_ASYNC_NOTIFY:
                if (ctrl.args.reset_async_event.event == DEVCTRL_ASYNC_NOTIFY_DRIVER_CLOSED) {
                    if (ctrl.args.reset_async_event.ph == DEVCTRL_CLOSE_DRIVER_MAGIC) {
                        struct me_notification_handler * notify_handler;

                        spin_lock(&men->d6->notification_handler_headlock);
                        {
                            list_for_each_entry(notify_handler, &men->d6->notification_handler_heads, node) {
                                if (current->parent->pid == notify_handler->ppid) {
                                    notify_handler->quit_requested = true;
                                    complete( &notify_handler->notification_available);
                                }
                            }
                        }
                        spin_unlock(&men->d6->notification_handler_headlock);
                    }
                } else if (ctrl.args.reset_async_event.event == DEVCTRL_ASYNC_NOTIFY_DEVICE_ALARM) {
                    unsigned long alarms_to_ack = 0;

                    if (ctrl.args.reset_async_event.pl & DEVCTRL_DEVICE_ALARM_TEMPERATURE) {
                        if (ctrl.args.reset_async_event.ph != 0 &&
                                ctrl.args.reset_async_event.ph < LONG_MAX) {
                            men->d6->temperature_alarm_period = ctrl.args.reset_async_event.ph;
                            if (cancel_delayed_work(&men->d6->temperature_alarm_work)) {
                                schedule_delayed_work(&men->d6->temperature_alarm_work, msecs_to_jiffies(men->d6->temperature_alarm_period));
                            }
                        }

                        ctrl.args.reset_async_event.pl &= ~DEVCTRL_DEVICE_ALARM_TEMPERATURE;
                    }

                    alarms_to_ack = men->d6->event_pl_to_alarms(ctrl.args.reset_async_event.pl);

                    if (alarms_to_ack != 0) {
                        me6_ack_alarms(men, alarms_to_ack);
                    }
                }

                break;

            case DEVCTRL_SET_ASYNC_NOTIFY:
                if (ctrl.args.set_async_event.event == DEVCTRL_ASYNC_NOTIFY_DEVICE_ALARM) {

                    if (ctrl.args.reset_async_event.pl == DEVCTRL_DEVICE_ALARM_SOFTWARE) {
                        men->register_interface.write(&men->register_interface, ME6_REG_IRQ_SOFTWARE_TRIGGER, ctrl.args.reset_async_event.ph);
                    }
                }

                break;

            case DEVCTRL_ALLOC_VA_EVENTS:
                dev_dbg(&men->dev, "[IOCTL] Allocating %d VA Event UIQs\n", ctrl.args.alloc_va_events.num_events);
                result = me6_alloc_va_event_uiqs(men, ctrl.args.alloc_va_events.num_events);

                break;

            default:
                dev_err(&men->dev, "[IOCTL] Invalid Device Control command.\n");
                return -ENOSYS;
            }

            return result;
        }

    default:
        return -ENOIOCTLCMD;
    }
}

static void
me6_temperature_alarm_work(struct work_struct *work)
{
    struct me6_data *me6;

    me6 = container_of(to_delayed_work(work), struct me6_data, temperature_alarm_work);
    me6_ack_alarms(me6->men, me6->event_pl_to_alarms(DEVCTRL_DEVICE_ALARM_TEMPERATURE));
}

static void
me6_irq_notification_work(struct work_struct *work)
{
    struct me6_data *me6 = container_of(work, struct me6_data, irq_notification_work);
    uint32_t alarms_status = 0;
    unsigned long flags = 0;

    /* Notify all Notification Handlers */
    spin_lock(&me6->notification_handler_headlock);
    {
        struct me_notification_handler *notification_handler;
        list_for_each_entry(notification_handler, &me6->notification_handler_heads, node) {
            complete(&notification_handler->notification_available);
        }
    }
    spin_unlock(&me6->notification_handler_headlock);

    spin_lock_irqsave(&me6->notification_data_lock, flags);
    {
        alarms_status = me6->alarms_status;
    }
    spin_unlock_irqrestore(&me6->notification_data_lock, flags);

    /* Handle Temperature alarm */
    if (alarms_status & me6->event_pl_to_alarms(DEVCTRL_DEVICE_ALARM_TEMPERATURE)) {
        // Acknowledge & re-enable Temperature alarm after 1 second
        // (it might be already handled within this time)
        schedule_delayed_work(&me6->temperature_alarm_work, msecs_to_jiffies(me6->temperature_alarm_period));
    }
}

static void
me6_level_irq(struct siso_menable *men)
{
    unsigned long flags = 0;
    uint32_t status = men->register_interface.read(&men->register_interface, ME6_REG_IRQ_ALARMS_STATUS);

#if defined(DEBUG_IRQS)
    dev_info(&men->dev, "level interrupt sources: %08x\n", status);
#endif

    if (status != 0) {
        if (status & ME6_ALARM_SOFTWARE) {
            men->register_interface.write(&men->register_interface, ME6_REG_IRQ_SOFTWARE_TRIGGER, 0);
        }

        spin_lock_irqsave(&men->d6->notification_data_lock, flags);
        {
            men->d6->alarms_enabled &= ~status;
            men->d6->alarms_status |= status;
            men->d6->notifications |= NOTIFICATION_DEVICE_ALARM;
            men->d6->notification_time_stamp++;
            schedule_work(&men->d6->irq_notification_work);
        }
        spin_unlock_irqrestore(&men->d6->notification_data_lock, flags);
    }
}

static void
process_uiq_data(struct siso_menable * men, uint32_t num_words, register_interface * register_interface, uiq_transfer_state * uiq_transfer, uiq_timestamp *ts, bool *have_ts, int device_number) {

    while (num_words > 0) {

        if (uiq_transfer->remaining_packet_words == 0) {

            /*
             * Get the next header
             */
            uiq_transfer->current_header = men_fetch_next_incoming_uiq_word(&men->uiq_transfer, men->messaging_dma_declaration, &men->register_interface, ME6_REG_IRQ_EVENT_DATA);
            --num_words;

            if (unlikely((uiq_transfer->current_header == 0) || (uiq_transfer->current_header == 0xffffffff))) {
                dev_warn(&men->dev, "invalid event interrupt header received.\n");
                uiq_transfer->current_header = 0;
                uiq_transfer->remaining_packet_words = 0;
            } else {
                uiq_transfer->remaining_packet_words = ME6_IRQ_EVENT_GET_PACKET_LENGTH(uiq_transfer->current_header) - 1;

                /* Get the target UIQ for the Interrupt */
                uiq_transfer->current_uiq = NULL;
                const uint32_t target_uiq_id = ME6_IRQ_EVENT_GET_PACKET_ID(uiq_transfer->current_header);

                DEV_DBG_UIQ(&men->dev, "Received new header. Packet length = %u words, target UIQ is 0x%03x.\n",
                            uiq_transfer->remaining_packet_words, target_uiq_id);

                /* TODO: [RKN] How can we determine the target uiq in a platform independent way?
                 *             Consider introducing a comman base class for the menable struct. */
                for (int i = 0; i < men->num_active_uiqs; ++i) {
                     if (men->uiqs[i] && men->uiqs[i]->id == target_uiq_id) {
                         uiq_transfer->current_uiq = men->uiqs[i];
                         break;
                     }
                }
                if (uiq_transfer->current_uiq == NULL) {
                    dev_warn(&men->dev, "[UIQ] Received data for invalid source id 0x%x; discarding %u words\n",
                             target_uiq_id, uiq_transfer->remaining_packet_words);
                }
            }
        } else {

            /*
             * Process payload
             */
            if (uiq_transfer->current_uiq != NULL) {

                uiq_base * uiq_base = uiq_transfer->current_uiq;
                struct menable_uiq * uiq = container_of(uiq_base, struct menable_uiq, base);

                /* TODO: [RKN] Make the locking platform independent. (Platform abstraction for locks or dependency injection) */
                spin_lock(&uiq->lock);
                if (UIQ_TYPE_IS_WRITE(uiq_base->type)) {
                    /* Receiving data on the write queue means that the queue is now empty. */

                    DEV_DBG_IRQ(&men->dev, "Received data for write queue, source id 0x%x; discarding %u words\n",
                                uiq_base->id, uiq_transfer->remaining_packet_words);

                    while (num_words > 0 && uiq_transfer->remaining_packet_words > 0) {
                        (void)men_fetch_next_incoming_uiq_word(&men->uiq_transfer, men->messaging_dma_declaration, &men->register_interface, ME6_REG_IRQ_EVENT_DATA);
                        --num_words;
                        --uiq_transfer->remaining_packet_words;
                    }

                    /* After receiving this packet, we may push more data to the device */
                    if (uiq_transfer->remaining_packet_words == 0) {
                        if (uiq_base->fill == 0) {
                            uiq_base->is_running = false;
                            uiq_base->read_index = 0;
                        } else {
                            (void)men_uiq_push(uiq_base);
                        }
                    }
                } else {

                    /* Pop as much data from the device into local buffer as we have. */

                    DEV_DBG_IRQ(&men->dev, "Received data for read queue, source id 0x%x; reading %u words\n",
                                uiq_base->id, uiq_transfer->remaining_packet_words);

                    while (num_words > 0 && uiq_transfer->remaining_packet_words > 0) {
                        (void)men_uiq_pop(uiq_base, ts, have_ts);
                        --num_words;
                        --uiq_transfer->remaining_packet_words;
                    }

                    /* TODO: [RKN] Make cpldtodo platform independent. Move to uiq_base if it has a correspondence on windows. */
                    if (uiq->cpltodo && (uiq->cpltodo <= uiq_base->fill)) {
                        /* TODO: [RKN] Try to move the completion out of this function for platform independency. */
                        complete(&uiq->cpl);
                    }
                    ++uiq->base.irq_count;
                }

                spin_unlock(&uiq->lock);
            } else {
               /* invalid UIQ, discard data */
               while (num_words > 0 && uiq_transfer->remaining_packet_words > 0) {
                    (void)men_fetch_next_incoming_uiq_word(&men->uiq_transfer, men->messaging_dma_declaration, &men->register_interface, ME6_REG_IRQ_EVENT_DATA);
                    --num_words;
                    --uiq_transfer->remaining_packet_words;
                }
            }
        }
    }
}

static void
me6_messaging_irq(struct siso_menable * men /* TODO: [RKN] Try to remove this argument or replace it with something OS independent */,
                  struct register_interface * register_interface,
                  struct messaging_dma_controller * messaging_dma_ctrl,
                  struct messaging_dma_transmission_info * transmission_info,
                  uiq_timestamp *ts, bool *have_ts, int device_number) {

    /* get the number of transmissions */
    const uint32_t pending_reg_val = register_interface->read(register_interface, ME6_REG_IRQ_EVENT_COUNT);

    if (ME6_MESSAGING_COUNT_HAS_OVERFLOW(pending_reg_val)) {
        dev_warn(&men->dev, "[WARNING][MSG DMA] Pending transmissions overflow!\n");
    } else {
        uint32_t num_transmissions = ME6_MESSAGING_GET_COUNT(pending_reg_val);

        DEV_DBG_MESSAGING_DMA(&men->dev, "Received %u new messaging transmissions\n", num_transmissions);

        if (num_transmissions == 0) {
            dev_warn(&men->dev, "[WARNING][MSG DMA] Unexpectedly received interrupt for zero transmission!\n");
        }

        for (uint32_t i = 0; i < num_transmissions; ++i) {

            /* set the transmission info to the UIQ transfer state */
            int ret = messaging_dma_ctrl->get_next_transmission(messaging_dma_ctrl, transmission_info);
            if (MEN_IS_ERROR(ret)) {
                /* TODO: [RKN] At this point, we cannot rely on messaging dma anymore. What should we do? Reset? Enter a failed state? */
                dev_warn(&men->dev, "[WARNING][MSG DMA] Cannot fetch data from transmission %u.\n", i);
            } else {

#if defined(DBG_MESSAGING_DMA) && !defined(DBG_MESSAGING_DMA_OFF)

                uint32_t num_words = transmission_info->num_words;
                uint32_t * dma_read_ptr = transmission_info->read_ptr;

                /* Print details for the transmission */
                DEV_DBG_MESSAGING_DMA(&men->dev, "transmission[%u]: num_words=%u, read_ptr=0x%016llx\n",
                                      i, num_words, (uint64_t)dma_read_ptr);

                uint32_t rdIdx = 0;
                for (rdIdx = 0; rdIdx + 8 < num_words; rdIdx += 8) {
                    uint32_t * data = &(dma_read_ptr[rdIdx]);
                    DEV_DBG_MESSAGING_DMA(&men->dev, "data[%u] | 0x%08x | 0x%08x | 0x%08x | 0x%08x | 0x%08x | 0x%08x | 0x%08x | 0x%08x |\n",
                                                          rdIdx, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
                }

                if (rdIdx < num_words) {
                    uint32_t len = num_words - rdIdx;
                    uint32_t data[8] = {0};

                    for (int i = 0; i < ARRAY_SIZE(data); ++i) {
                        data[i] = (len > i) ? dma_read_ptr[rdIdx + i] : 0xaffed00f;
                    }

                    DEV_DBG_MESSAGING_DMA(&men->dev, "data[%u] | 0x%08x | 0x%08x | 0x%08x | 0x%08x | 0x%08x | 0x%08x | 0x%08x | 0x%08x |\n",
                                                          rdIdx, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
                }

                pr_debug(DRIVER_NAME "[%u]: [MSG DMA] Handling UIQ Data. %u words at read ptr  0x%016llx\n", device_number, transmission_info->num_words, (uint64_t)dma_read_ptr);
#endif

                process_uiq_data(men, transmission_info->num_words, register_interface, &men->uiq_transfer, ts, have_ts, device_number);
            }
        }

    }

    /* tell the board, that we have processed all transmitted data */
    register_interface->write(register_interface, ME6_REG_IRQ_MESSAGING_PROCESSED, 1);

    DEV_DBG_MESSAGING_DMA(&men->dev, "Acknowledged processing of all transmissions.\n");
}

static void
me6_event_irq(struct siso_menable *men, uiq_timestamp *ts, bool *have_ts)
{
    const uint32_t reg_content = men->register_interface.read(&men->register_interface, ME6_REG_IRQ_EVENT_COUNT);

    if ((reg_content & ME6_IRQ_EVENT_OVERFLOW) == 0) {
        uint32_t pending_irq_words = ME6_IRQ_EVENT_GET_COUNT(reg_content);
        process_uiq_data(men, pending_irq_words, &men->register_interface, &men->uiq_transfer, ts, have_ts, men->idx);
    } else {
        dev_err(&men->dev, "overflow on event data fifo\n");
    }
}

static void
me6_dma_irq(struct siso_menable *men, int dma_idx, menable_timespec_t *ts, bool *have_ts)
{
    struct menable_dma_wait *waiting;
    ktime_t timeout;

    struct menable_dmachan *dc = men_dma_channel(men, dma_idx);

    BUG_ON(dc == NULL);
    uint32_t pending = men->register_interface.read(&men->register_interface, ME6_REG_IRQ_DMA_COUNT_FOR_CHANNEL_IDX(dma_idx));
    if ((pending & ME6_IRQ_DMA_OVERFLOW) == 0) {
        uint32_t new_frames_count = ME6_IRQ_DMA_GET_COUNT(pending);

        if (!*have_ts) {
            *have_ts = true;
            menable_get_ts(ts);
        }

        spin_lock(&dc->chanlock);
        if (dc->active != NULL) {
            for (int i = 0; i < new_frames_count; ++i) {
                /* get latest buffer from hot list and move it to grabbed list */
                spin_lock(&dc->listlock);
                struct menable_dmabuf *sb = men_move_hot(dc, ts);

                uint32_t len = men->register_interface.read(&men->register_interface, dc->iobase + ME6_REG_DMA_LENGTH);
                uint32_t tag = men->register_interface.read(&men->register_interface, dc->iobase + ME6_REG_DMA_TAG);

                if (likely(sb != NULL)) {
                    if (sb->index == -1) {
                        // dummy buffer
                        DEV_DBG_ACQ(&men->dev, "Received frame number %llu in dummy buffer.", dc->latest_frame_number);
                    } else {
                        sb->dma_length = len;
                        sb->dma_tag = tag;
                        sb->frame_number = dc->latest_frame_number;

                        DEV_DBG_ACQ(&men->dev, "Received frame number %llu.", sb->frame_number);
                    }
                }

                /* At this point the buffer we just received is ready to use by the user application.
                 * We release the lock so the user has a chance to pick up the frame and/or unlock
                 * a buffer (if in blocking mode) before we continue. */
                spin_unlock(&dc->listlock);
            }

            spin_lock(&dc->listlock);
            list_for_each_entry(waiting, &dc->wait_list, node) {
                /* TODO In a Unit Test, waiting can be NULL. Check this here or change the Test? */
                if (waiting->frame <= dc->goodcnt)
                    complete(&waiting->cpl);
            }

            /* TODO: [RKN] We could release the listlock here for a moment to allow
             *             unlocking of a buffer to reduce the risk of losing a frame
             *             in the dummy buffer. Should we do this? Or should we keep the
             *             irq handler as fast as possible? */

            DEV_DBG_ACQ(&men->dev, "Frames remaining: %llu.", dc->transfer_todo);
            if (likely(dc->transfer_todo > 0)) {
                /* TODO: [RKN] What is the intention of the following condition?
                 *             Is it to only call queue_max if we have processed any
                 *             buffers? If so, should we include the condition that
                 *             hot_count is zero?
                 */
                if (new_frames_count) {
                    men_dma_queue_max(dc);
                }

                spin_unlock(&dc->listlock);

                if (dc->timeout) {
                    timeout = ktime_set(dc->timeout, 0);
                    spin_lock(&dc->timerlock);
                    hrtimer_cancel(&dc->timer);
                    hrtimer_start(&dc->timer, timeout, HRTIMER_MODE_REL);
                    spin_unlock(&dc->timerlock);
                }
            } else {
                spin_unlock(&dc->listlock);
                dc->state = MEN_DMA_CHAN_STATE_IN_SHUTDOWN;
                schedule_work(&dc->dwork);
            }

        } else {
            for (int i = 0; i < new_frames_count; ++i) {
                uint32_t tmp = men->register_interface.read(&men->register_interface, dc->iobase + ME6_REG_DMA_LENGTH);
                tmp = men->register_interface.read(&men->register_interface, dc->iobase + ME6_REG_DMA_TAG);
                dc->lost_count++;
            }
        }

        spin_unlock(&dc->chanlock);
    } else {
        dev_err(&men->dev, "overflow on DMA channel %d\n", dc->number);
        dc->lost_count++;
    }
}

static void
me6_irq_dispatch(struct siso_menable *men, enum me6_irq_index irq_idx)
{
    switch (irq_idx) {
    case ME6_IRQ_LEVEL_INDEX:
        {
            DEV_DBG_IRQ(&men->dev, "level-triggered interrupt\n");
            me6_level_irq(men);
        }
        break;

    case ME6_IRQ_EVENT_INDEX:
        {
            DEV_DBG_IRQ(&men->dev, "event interrupt\n");
            BUG_ON(men->uiqs == NULL);

            uiq_timestamp uiq_ts;
            bool have_uiq_ts = false;

            struct me6_data * me6 = men->d6;
            /* TODO: [RKN] Is the existance of the messaging_dma_declaration still
             *             a valid indicator for a working controller? */
            if (men->messaging_dma_declaration != NULL) {
                /* Messaging DMA available -> this is a Messaging DMA IRQ */
                me6_messaging_irq(men, &men->register_interface, &me6->messaging_dma_controller,
                                  &men->uiq_transfer.current_dma_transmission,
                                  &uiq_ts, &have_uiq_ts, men->idx);
            } else {
                me6_event_irq(men, &uiq_ts, &have_uiq_ts);
            }

        }
        break;

    case ME6_IRQ_DMA_0_INDEX:
    case ME6_IRQ_DMA_1_INDEX:
    case ME6_IRQ_DMA_2_INDEX:
    case ME6_IRQ_DMA_3_INDEX:
        {
            bool have_ts = false;
            menable_timespec_t ts;

            int dma_idx = irq_idx - ME6_IRQ_DMA_0_INDEX;
            DEV_DBG_IRQ(&men->dev, "DMA %d interrupt\n", dma_idx);

            me6_dma_irq(men, dma_idx, &ts, &have_ts);
        }
        break;
    default:
        dev_warn(&men->dev, "[IRQ] unknown interrupt source %d\n", (int) irq_idx);
    }
}

static irqreturn_t
me6_irq(int irq, void *dev_id)
{
    struct siso_menable *men = (struct siso_menable *) dev_id;
    int i, found;

    DEV_DBG_IRQ(&men->dev, "received interrupt %d\n", irq);

    if (pci_channel_offline(men->pdev)) {
        return IRQ_HANDLED;
    }

    if (men->active_fpgas == 0) {
        return IRQ_HANDLED;
    }

    if (men->d6->num_vectors > 1) {
        found = 0;
        for (i = 0; i < ME6_NUM_IRQS; ++i) {
            if (men->d6->vectors[i] == irq) {
                me6_irq_dispatch(men, i);
                found = 1;
                break;
            }
        }
    } else {
        uint32_t status = men->register_interface.read(&men->register_interface, ME6_REG_IRQ_STATUS);
        if (unlikely(status == (uint32_t) -1)) {
            dev_err(&men->dev, "failed to read interrupt status register\n");
            men_set_state(men, BOARD_STATE_DEAD);
            return IRQ_HANDLED;
        }

        found = 0;
        for (i = 0; i < ME6_NUM_IRQS; ++i) {
            if ((status & (1 << i)) != 0) {
                me6_irq_dispatch(men, i);
                found = 1;
                /* don't break here; handle all pending interrupts ... */
            }
        }
    }

    if (!found) {
        dev_err(&men->dev, "invalid interrupt source\n");
    }

    return IRQ_HANDLED;
}

static void
me6_set_sgl_entry(struct me6_sgl *sgl, int entry_idx, dma_addr_t addr,
                  u32 payload_size, u8 last)
{
    u64 temp;

    switch (entry_idx) {
    case 0:
        // d0[63:19] = addr[46:2], d0[18] = last, d[17:0] = rsvd
        temp = (last ? BIT_ULL(18) : 0) | msl64(addr, 46, 2, (19 - 2));
        sgl->data[0] = cpu_to_le64(temp);

        // d1[39:17] = size[22:0], d1[16:0] = addr[63:47]
        temp = le64_to_cpu(sgl->data[1]);
        temp = rmsr64(temp, addr, 63, 47, 47);
        temp = rmsl64(temp, payload_size, 22, 0, 17);
        sgl->data[1] = cpu_to_le64(temp);
        break;

    case 1:
        // d1[63:41] = addr[24:2], d1[40] = last
        temp = le64_to_cpu(sgl->data[1]);
        if (last) temp |= BIT_ULL(40); else temp &= ~BIT_ULL(40);
        temp = rmsl64(temp, addr, 24, 2, (41 - 2));
        sgl->data[1] = cpu_to_le64(temp);

        // d2[61:39] = size[22:0], d2[38:0] = addr[63:25]
        temp = le64_to_cpu(sgl->data[2]);
        temp = rmsr64(temp, addr, 63, 25, 25);
        temp = rmsl64(temp, payload_size, 22, 0, 39);
        sgl->data[2] = cpu_to_le64(temp);
        break;

    case 2:
        // d2[63] = addr[2], d2[62] = last
        temp = le64_to_cpu(sgl->data[2]);
        if (last) temp |= BIT_ULL(62); else temp &= ~BIT_ULL(62);
        temp = rmsl64(temp, addr, 2, 2, (63 - 2));
        sgl->data[2] = cpu_to_le64(temp);

        // d3[63:61] = size[2:0], d3[60:0] = addr[63:3]
        temp = msr64(addr, 63, 3, 3) | msl64(payload_size, 2, 0, 61);
        sgl->data[3] = cpu_to_le64(temp);

        // d4[19:0] = size[22:3]
        temp = le64_to_cpu(sgl->data[4]);
        temp = rmsr64(temp, payload_size, 22, 3, 3);
        sgl->data[4] = cpu_to_le64(temp);
        break;

    case 3:
        // d4[63:21] = addr[44:2], d4[20] = last
        temp = le64_to_cpu(sgl->data[4]);
        if (last) temp |= BIT_ULL(20); else temp &= ~BIT_ULL(20);
        temp = rmsl64(temp, addr, 44, 2, (21 - 2));
        sgl->data[4] = cpu_to_le64(temp);

        // d5[41:19] = size[22:0], d5[18:0] = addr[63:45]
        temp = le64_to_cpu(sgl->data[5]);
        temp = rmsr64(temp, addr, 63, 45, 45);
        temp = rmsl64(temp, payload_size, 22, 0, 19);
        sgl->data[5] = cpu_to_le64(temp);
        break;

    case 4:
        // d5[63:43] = addr[22:2], d5[42] = last
        temp = le64_to_cpu(sgl->data[5]);
        if (last) temp |= BIT_ULL(42); else temp &= ~BIT_ULL(42);
        temp = rmsl64(temp, addr, 22, 2, (43 - 2));
        sgl->data[5] = cpu_to_le64(temp);

        // d6[63:41] = size[22:0], d6[40:0] = addr[63:23]
        temp = msr64(addr, 63, 23, 23) | msl64(payload_size, 22, 0, 41);
        sgl->data[6] = cpu_to_le64(temp);
        break;

    default:
        BUG();
    }
}

#if defined(DEBUG_SGL)
static void
dumpsgl(struct me6_sgl *sgl)
{
    pr_info("dumping SGL block");
    pr_info("0: %016llx\n", sgl->next);
    for (unsigned int i = 0; i < 7; ++i) {
        pr_info("%d: %016llx\n", i+1, sgl->data[i]);
    }
}
#endif

static u32
me6_sgl_size(const u16 offset, const u32 size, const u32 payload)
{
    const u32 first_transfer_size = (offset % payload) == 0 ? payload : (payload - (offset % payload));

#if defined(DEBUG_SGL)
    pr_info("offset %04x, size %08x, payload %08x\n", (u32)offset, size, payload);
#endif

    u32 num_transfers;
    u32 last_transfer_size;

    /* See DmaSystemSpecification for mE6, pages 9 and 10 */
    if (size <= first_transfer_size) {
        num_transfers = 1;
        last_transfer_size = size;
    } else {
        const u32 aligned_transfer_size = size - first_transfer_size;

        num_transfers = 1 + (aligned_transfer_size / payload);
        last_transfer_size = aligned_transfer_size % payload;
        if (last_transfer_size == 0) {
            last_transfer_size = payload;
        } else {
            num_transfers += 1;
        }
    }

    /* Convert to number of 32bit words for the SGL */
    last_transfer_size /= 4;

#if defined(DEBUG_SGL)
    pr_info("num_transfers %08x, last_transfer_size %08x\n", num_transfers, last_transfer_size);
#endif

    /* Yes, a value of BIT(15) for num_transfers, or BIT(8) for last_transfer, is allowed */
    /* It will be translated into 0, which means largest value (not 0!) to the firmware */
    BUG_ON(num_transfers > BIT(15) || last_transfer_size > BIT(8));

    return (num_transfers & GENMASK(14, 0)) | ((last_transfer_size & GENMASK(7, 0)) << (15));
}

static void
me6_free_sgl(struct siso_menable *men, struct menable_dmabuf *sb)
{
    struct men_dma_chain *res = sb->dma_chain;
    dma_addr_t dma = sb->dma;

#if defined(DEBUG_SGL)
    pr_info("destroying user buffer %ld\n", sb->index);
#endif

    while (res) {
        struct men_dma_chain *n;
        dma_addr_t ndma;

        n = res->next;
        ndma = (dma_addr_t) ((le64_to_cpu(res->pcie6->next) << 1) & ~(3ULL));
        if (dma == ndma) {
            break;
        }
        dma_pool_free(men->sgl_dma_pool, res->pcie6, dma);

        kfree(res);
        res = n;
        dma = ndma;
    }
}

static void
me6_queue_sb(struct menable_dmachan *db, struct menable_dmabuf *sb)
{
    w64(db->parent, db->iobase + ME6_REG_DMA_SGL_ADDR_LOW, (sb->dma >> 2));
    wmb();
}

static int
me6_create_userbuf(struct siso_menable * men, struct menable_dmabuf * dma_buf,
                   struct menable_dmabuf * dummybuf)
{
    const u32 payload_size = 128 << ((men->pcie_device_ctrl & GENMASK(7, 5)) >> 5);
    u64 remaining_length = dma_buf->buf_length;
    struct men_dma_chain *chain_node;
    int block_idx, block_entry_idx;

    dma_buf->dma_chain->pcie6 = dma_pool_alloc(men->sgl_dma_pool, GFP_KERNEL, &dma_buf->dma);
    if (!dma_buf->dma_chain->pcie6)
        goto fail_pcie;

    memset(dma_buf->dma_chain->pcie6, 0, sizeof(*dma_buf->dma_chain->pcie6));

    chain_node = dma_buf->dma_chain;

#if defined(DEBUG_SGL)
    pr_info("creating user buffer %ld\n", dma_buf->index);
#endif

    block_idx = 0;
    block_entry_idx = 0;
    while (block_idx < dma_buf->num_used_sg_entries) {
        const dma_addr_t addr = sg_dma_address(dma_buf->sg + block_idx);
        unsigned int len = sg_dma_len(dma_buf->sg + block_idx);
        if (len > remaining_length) {
            len = (u32)remaining_length;
        }

#if defined(DEBUG_SGL)
        pr_info("block %04d: address %016llx, length %08x, span %d\n", block_idx, (u64) addr, len, block_idx);
#endif

        ++block_idx;
        u8 is_last = block_idx < dma_buf->num_used_sg_entries ? 0 : 1;
        remaining_length -= len;

        me6_set_sgl_entry(chain_node->pcie6, block_entry_idx, addr, me6_sgl_size(addr & 0xfff, len, payload_size), is_last);

        ++block_entry_idx;
        if ((block_entry_idx == ME6_SGL_ENTRIES) && !is_last) {
            /* block full, create next block and start over with entry 0 */
            block_entry_idx = 0;

            chain_node->next = kzalloc(sizeof(*chain_node->next), GFP_KERNEL);
            if (!chain_node->next)
                goto fail;

            dma_addr_t next_block_address;
            chain_node->next->pcie6 = dma_pool_alloc(men->sgl_dma_pool, GFP_KERNEL, &next_block_address);
            if (!chain_node->next->pcie6) {
                kfree(chain_node->next);
                chain_node->next = NULL;
                goto fail;
            }
            chain_node->pcie6->next = cpu_to_le64((next_block_address >> 1) | 0x1);
            chain_node = chain_node->next;
            memset(chain_node->pcie6, 0, sizeof(*chain_node->pcie6));
        }
    }

#if defined(DEBUG_SGL)
    pr_info("SGL start address %016llx\n", dma_buf->dma);
    chain_node = dma_buf->dma_chain;
    while (chain_node != NULL) {
        dumpsgl(chain_node->pcie6);
        chain_node = chain_node->next;
    }
#endif

    return 0;

fail:
    me6_free_sgl(men, dma_buf);
    return -ENOMEM;
fail_pcie:
    kfree(dma_buf->dma_chain);
    return -ENOMEM;
}

static int
me6_create_dummybuf(struct siso_menable *men, struct menable_dmabuf *dma_buf)
{
    struct men_dma_chain *dma_chain;
    int i;

    dma_buf->index = -1;
    dma_buf->dma_chain = kzalloc(sizeof(*dma_buf->dma_chain), GFP_KERNEL);
    if (!dma_buf->dma_chain) {
        goto fail_dma_chain;
    }

    dma_buf->dma_chain->pcie6 = dma_pool_alloc(men->sgl_dma_pool, GFP_KERNEL, &dma_buf->dma);
    if (!dma_buf->dma_chain->pcie6) {
        goto fail_pcie;
    }

    memset(dma_buf->dma_chain->pcie6, 0, sizeof(*dma_buf->dma_chain->pcie6));

    dma_chain = dma_buf->dma_chain;

    for (i = 0; i < ME6_SGL_ENTRIES; i++) {
        me6_set_sgl_entry(dma_chain->pcie6, i, men->d6->dummypage_dma,
                          me6_sgl_size(0, PCI_PAGE_SIZE, men->d6->payload_size), 0);
    }

    dma_chain->pcie6->next = cpu_to_le64(dma_buf->dma >> 1 | 0x1);

    dma_buf->buf_length = -1;

    return 0;

fail_pcie:
    kfree(dma_buf->dma_chain);
fail_dma_chain:
    return -ENOMEM;
}

static void
me6_destroy_dummybuf(struct siso_menable *men, struct menable_dmabuf *db)
{
    dma_pool_free(men->sgl_dma_pool, db->dma_chain->pcie6, db->dma);
    kfree(db->dma_chain);
}

static void
me6_exit(struct siso_menable *men)
{
    /* Tear down interrupts first to avoid race conditions */
    for (int k = 0; k < men->d6->num_vectors; ++k) {
        devm_free_irq(&men->pdev->dev, men->d6->vectors[k], men);
        men->d6->vectors[k] = 0;
    }
    men->d6->num_vectors = 0;
    pci_free_irq_vectors(men->pdev);

    men->d6->stop_peripherals(men->d6);
    men->d6->cleanup_peripherals(men->d6);

    /*
     * Perform device type specific cleanup if required.
     */
    if (men->d6->exit != NULL)
        men->d6->exit(men->d6);

    dma_free_coherent(&men->pdev->dev, PCI_PAGE_SIZE, men->d6->dummypage, men->d6->dummypage_dma);
    dmam_pool_destroy(men->sgl_dma_pool);

    kfree(men->uiqs);
    kfree(men->d6);
}

static unsigned int
me6_query_dma(struct siso_menable *men, const unsigned int arg)
{
    uint32_t num_dmas;

    BUG_ON(men->active_fpgas <= 0);

    num_dmas = men->register_interface.read(&men->register_interface, ME6_REG_NUM_DMA_CHANNELS);
    if (unlikely(num_dmas == (uint32_t) -1)) {
        dev_err(&men->dev, "failed to get number of DMAs\n");
        men_set_state(men, BOARD_STATE_DEAD);
        num_dmas = 0;
    }

    return num_dmas;
}

static void
me6_dmabase(struct siso_menable *men, struct menable_dmachan *dc)
{
    dc->iobase = ME6_REG_DMA_BASE + ME6_DMA_CHAN_OFFSET * dc->number;
    dc->irqack = 0;
    dc->ackbit = 0;
    dc->irqenable = 0;
    dc->enablebit = 0;
}

static void
me6_stopdma(struct siso_menable *men, struct menable_dmachan *dc)
{
    unsigned long timeout = ME6_DMA_STAT_TIMEOUT;
    uint32_t status;

    status = men->register_interface.read(&men->register_interface, dc->iobase + ME6_REG_DMA_STATUS);
    dev_dbg(&men->dev, "%s - dma status = 0x%02x (enabled = %d, reset = %d, running = %d)\n",
            __FUNCTION__, status, status & 1, (status & 2) >> 1, (status & 4) >> 2);

    if (status & ME6_DMA_STAT_RUNNING) {
        men->register_interface.write(&men->register_interface, dc->iobase + ME6_REG_DMA_CONTROL, 0);
        do {
            status = men->register_interface.read(&men->register_interface, dc->iobase + ME6_REG_DMA_STATUS);
            if (unlikely(status == (uint32_t) -1)) {
                dev_err(&men->dev, "failed to read DMA %d status register\n", dc->number);
                men_set_state(men, BOARD_STATE_DEAD);
                break;
            }
        } while ((status & ME6_DMA_STAT_RUNNING) != 0 && --timeout);

        if (timeout == 0) {
            dev_warn(&men->dev, "timed out while waiting for DMA %d to stop\n", dc->number);
            dev_dbg(&men->dev, "%s - dma status = 0x%02x (enabled = %d, reset = %d, running = %d)\n",
                    __FUNCTION__, status, status & 1, (status & 2) >> 1, (status & 4) >> 2);
        }
    }

}

static void
me6_abortdma(struct siso_menable *men, struct menable_dmachan *dc)
{
    me6_stopdma(men, dc);

    men->register_interface.write(&men->register_interface, dc->iobase + ME6_REG_DMA_CONTROL, ME6_DMA_CTRL_RESET);
    (void) men->register_interface.read(&men->register_interface, dc->iobase + ME6_REG_DMA_STATUS);

    men->register_interface.write(&men->register_interface, dc->iobase + ME6_REG_DMA_CONTROL, 0);
    (void) men->register_interface.read(&men->register_interface, dc->iobase + ME6_REG_DMA_STATUS);
}

static int
me6_startdma(struct siso_menable *men, struct menable_dmachan *dc)
{
    unsigned long timeout = ME6_DMA_STAT_TIMEOUT;
    uint32_t status;

    me6_abortdma(men, dc);
    men_dma_queue_max(dc);

    /* set channel to 'running' */
    men->register_interface.write(&men->register_interface, dc->iobase + ME6_REG_DMA_CONTROL, ME6_DMA_CTRL_ENABLE);

    /* check if status change has worked */
    do {
        status = men->register_interface.read(&men->register_interface, dc->iobase + ME6_REG_DMA_STATUS);
        if (unlikely(status == (uint32_t) -1)) {
            dev_err(&men->dev, "failed to read DMA channel %u status\n", (unsigned int) dc->number);
            men_set_state(men, BOARD_STATE_DEAD);
            return -EFAULT;
        }
    } while ((status & ME6_DMA_STAT_RUNNING) == 0 && --timeout);

    if (timeout == 0) {
        dev_err(&men->dev, "timed out while waiting for DMA %d to start\n", dc->number);
        return -ETIMEDOUT;
    }

    return 0;
}

static void
me6_stopirq(struct siso_menable *men)
{
    unsigned long flags;

    // Make sure that temperature alarm handling is not running
    if (!cancel_delayed_work(&men->d6->temperature_alarm_work)) {
        flush_delayed_work(&men->d6->temperature_alarm_work);
    }

    // Disable IRQ0 sources
    spin_lock_irqsave(&men->d6->notification_data_lock, flags);
    {
        men->d6->alarms_enabled = 0;
        men->register_interface.write(&men->register_interface, ME6_REG_IRQ_ALARMS_ENABLE, 0);
    }
    spin_unlock_irqrestore(&men->d6->notification_data_lock, flags);

    // We might have scheduled another round of temperature alarm work in the meantime, so cancel again
    cancel_delayed_work(&men->d6->temperature_alarm_work);
}

static void
me6_startirq(struct siso_menable *men)
{
    unsigned long flags;

    if (men_get_state(men) >= BOARD_STATE_READY) {
        men->register_interface.write(&men->register_interface, ME6_REG_IRQ_SOFTWARE_TRIGGER, 0);

        // Enable IRQ0 sources
        spin_lock_irqsave(&men->d6->notification_data_lock, flags);
        {
            men->d6->alarms_enabled = men->d6->event_pl_to_alarms(0xffffffff);
            men->register_interface.write(&men->register_interface, ME6_REG_IRQ_ALARMS_ENABLE, men->d6->alarms_enabled);
        }
        spin_unlock_irqrestore(&men->d6->notification_data_lock, flags);
    }
}

static void
me6_cleanup(struct siso_menable *men)
{
    unsigned long flags;

    spin_lock_irqsave(&men->designlock, flags);
    {
        men->design_changing = true;
    }
    spin_unlock_irqrestore(&men->designlock, flags);

    memset(men->desname, 0, men->deslen);

    spin_lock_irqsave(&men->designlock, flags);
    {
        men->design_changing = false;
    }
    spin_unlock_irqrestore(&men->designlock, flags);
}

int
me6_add_uiqs(struct siso_menable * men, struct uiq_declaration * decls, unsigned int elems)
{
    int i, k;

    men->uiqs = kcalloc(elems, sizeof(*men->uiqs), GFP_KERNEL);
    if (men->uiqs == NULL) {
        return -ENOMEM;
    }

    for (i = 0; i < elems; ++i) {
        DEV_DBG_UIQ(&men->dev, "Initializing UIQ 0x%03x for '%s'\n", decls[i].id, decls[i].name);
        men->uiqs[i] = men_uiq_init(men, i, decls[i].id, decls[i].type, decls[i].reg_offs, decls[i].write_burst);

        if (IS_ERR(men->uiqs[i])) {
            int result = PTR_ERR(men->uiqs[i]);
            pr_err("[UIQ] failed to initialize UIQ %s (%d; error %d)\n", decls[i].name, decls[i].id, result);

            for (k = 0; k < i; ++k) {
                men_uiq_remove(men->uiqs[k]);
            }
            kfree(men->uiqs);
            men->uiqs = NULL;

            return result;
        }

        men_scale_uiq(men->uiqs[i], 100 * sizeof(*(men->uiqs[i]->data)));
    }

    men->uiqcnt[0] = elems;
    men->num_active_uiqs = elems;

    return 0;
}

int
me6_probe(struct siso_menable *men)
{
    int ret = 0, i, k;

    /* Basic Setup */

    ret = dma_set_mask_and_coherent(men->dev.parent, DMA_BIT_MASK(64));
    if (ret != 0) {
        dev_err(&men->dev, "failed to set DMA mask\n");
        goto fail_mask;
    }

    void * me6 = NULL; /* generic pointer to me6 data for cleanup during error handling */

    if (SisoBoardIsAbacus(men->pci_device_id)) {
        me6 = kzalloc(sizeof(struct me6_abacus), GFP_KERNEL);
        if (me6 == NULL) goto fail;

        struct me6_abacus * me6_abacus = me6;
        ret = me6_abacus_init(men, me6_abacus);
        if (ret != 0) goto fail_init;

        men->d6 = upcast(me6_abacus);

    } else if (SisoBoardIsImpulse(men->pci_device_id)) {
        me6 = kzalloc(sizeof(struct me6_impulse), GFP_KERNEL);
        if (me6 == NULL) goto fail;

        /* set the pointer in `men` so it can be used during initialization.
         * The initialization routine must take care of not using uinitializes
         * parts. */
        struct me6_impulse * me6_impulse = me6;
        men->d6 = upcast(me6_impulse);

        ret = me6_impulse_init(men);
        if (ret != 0) goto fail_init;

    } else {
        dev_err(&men->dev, "failed to detect board variant.");
        goto fail;
    }

    /* The VA event UIQs follow directly after the UIQs from the declarations */
    men->first_va_event_uiq_idx = men->d6->num_uiq_declarations;

    men->active_fpgas = 1;

    men->sgl_dma_pool = dmam_pool_create("me6_sgl", &men->pdev->dev,
            sizeof(struct me6_sgl), 128, PCI_PAGE_SIZE);

    if (!men->sgl_dma_pool) {
        dev_err(&men->dev, "failed to allocate DMA pool\n");
        goto fail_pool;
    }

    men->d6->dummypage = dma_alloc_coherent(&men->pdev->dev, PCI_PAGE_SIZE, &men->d6->dummypage_dma, GFP_KERNEL);
    if (men->d6->dummypage == NULL) {
        dev_err(&men->dev, "failed to allocate dummy page\n");
        goto fail_dummy;
    }

    men->d6->payload_size = 128 << ((men->pcie_device_ctrl & GENMASK(7, 5)) >> 5);

    men->dma_fifo_length = ME6_DMA_SGL_ADDR_FIFO_DEPTH;

    /* Init member functions */
    men->create_dummybuf = me6_create_dummybuf;
    men->free_dummybuf = me6_destroy_dummybuf;
    men->create_buf = me6_create_userbuf;
    men->free_buf = me6_free_sgl;
    men->startdma = me6_startdma;
    men->abortdma = me6_abortdma;
    men->stopdma = me6_stopdma;
    men->stopirq = me6_stopirq;
    men->startirq = me6_startirq;
    men->ioctl = me6_ioctl;
    men->exit = me6_exit;
    men->cleanup = me6_cleanup;
    men->query_dma = me6_query_dma;
    men->dmabase = me6_dmabase;
    men->queue_sb = me6_queue_sb;

    men->d6->temperature_alarm_period = 1000;

    INIT_LIST_HEAD(&men->d6->notification_handler_heads);
    INIT_DELAYED_WORK(&men->d6->temperature_alarm_work, me6_temperature_alarm_work);
    INIT_WORK(&men->d6->irq_notification_work, me6_irq_notification_work);

    men->desname = men->d6->design_name;
    men->deslen = sizeof(men->d6->design_name);

    me6_stopirq(men);

    /* It's either all MSI-X vectors, or one single */
    ret = pci_alloc_irq_vectors(men->pdev, ME6_NUM_IRQS, ME6_NUM_IRQS, PCI_IRQ_MSIX);
    if (ret == ME6_NUM_IRQS) {
        men->d6->num_vectors = ME6_NUM_IRQS;
        dev_info(&men->dev, "allocated %d interrupt vectors\n", men->d6->num_vectors);
    } else {
        dev_info(&men->dev, "failed to allocate %d interrupt vectors; falling back to one\n", ME6_NUM_IRQS);
        ret = pci_alloc_irq_vectors(men->pdev, 1, 1, PCI_IRQ_MSIX);
        if (ret == 1) {
            men->d6->num_vectors = 1;
        } else {
            dev_err(&men->dev, "failed to allocate any interrupt vectors\n");
            dev_info(&men->dev, "this might be due to MSI-X not enabled in kernel config, or too many MSI-X resources in use");
            goto fail_irq_alloc;
        }
    }

    /* Setup IRQs */
    for (i = 0; i < men->d6->num_vectors; ++i) {
        ret = pci_irq_vector(men->pdev, i);
        if (ret >= 0) {
            men->d6->vectors[i] = ret;
            ret = devm_request_irq(&men->pdev->dev, men->d6->vectors[i], me6_irq, 0, DRIVER_NAME, men);
        }

        if (ret != 0) {
            dev_err(&men->dev, "failed to request interrupt vector %d\n", i);
            for (k = 0; k < i; ++k) {
                devm_free_irq(&men->pdev->dev, men->d6->vectors[k], men);
                men->d6->vectors[k] = 0;
            }
            men->d6->vectors[i] = 0;
            goto fail_irq_request;
        }
    }

    ret = MCapLibInit(&men->d6->mdev, upcast(&men->config_interface));
    if (ret != 0) {
        goto fail_state;
    }

    ret = MCapFullReset(&men->d6->mdev);
    if (ret != 0) {
        goto fail_state;
    }

    ret = me6_update_board_status(men);
    if (ret) {
        goto fail_state;
    }

    if (men_get_state(men) >= BOARD_STATE_READY) {
        ret = men->d6->init_peripherals(men->d6);
        if (MEN_IS_ERROR(ret)) {
            dev_warn(&men->dev, "Failed to initialize board peripherals.\n");
            goto fail_state;
        }

        // Reset the camera frontend to get into a defined state.
        mutex_lock(&men->camera_frontend_lock);
        if (men->camera_frontend != NULL) {
            men->camera_frontend->reset(men->camera_frontend);
        }
        mutex_unlock(&men->camera_frontend_lock);

        if (ret == 0) {
            men->startirq(men);
            men->d6->start_peripherals(men->d6);
        }
    }

    return 0;

fail_state:
    for (k = 0; k < men->d6->num_vectors; ++k) {
        devm_free_irq(&men->pdev->dev, men->d6->vectors[k], men);
        men->d6->vectors[k] = 0;
    }
    men->d6->num_vectors = 0;
fail_irq_request:
    pci_free_irq_vectors(men->pdev);

fail_irq_alloc:
    dma_free_coherent(&men->pdev->dev, PCI_PAGE_SIZE, men->d6->dummypage, men->d6->dummypage_dma);

fail_dummy:
    dmam_pool_destroy(men->sgl_dma_pool);

fail_pool:
    men_del_uiqs(men, 0);
    kfree(men->uiqs);

fail_init:
    kfree(me6);

fail_mask:
fail:
    return ret;
}
