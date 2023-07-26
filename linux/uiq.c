/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

// setup debugging before any include!
#if defined(DBG_UIQ) && !defined(DBG_UIQ_OFF)
    #undef DEBUG
    #define DEBUG
#endif

#include "menable.h"

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#include <lib/boards/peripheral_declaration.h>
#include <lib/helpers/crc32.h>
#include <lib/uiq/uiq_base.h>
#include <lib/uiq/uiq_helper.h>

#include <linux/stddef.h>

#include "uiq.h"
#include "debugging_macros.h"

#include <linux/sched/signal.h>

uint32_t
men_fetch_next_incoming_uiq_word(uiq_transfer_state * transfer_state, messaging_dma_declaration * msg_dma_decl, register_interface * register_interface, uint32_t data_register_address) {

    /*
     * TODO: [RKN] It is quite an overhead, to check the tranfer mechanism for every word.
     *             This quick & dirty implementation should be changed so that the
     *             check is only performed once.
     *             Suggestion: create a class "UiqDataReceiverBase" (or similar) and derive two
     *                         subclasses.
     */

    if (msg_dma_decl != NULL) {
        /* We have a messaging DMA engine. Get the word from the DMA buffer. */
        /* Unchecked Precondition: Messaging DMA engine is up and running.   */

        uint32_t word = * transfer_state->current_dma_transmission.read_ptr;
        transfer_state->current_dma_transmission.read_ptr += 1;

        return word;

    } else {

        /* Get the word from the data register */
        return register_interface->read(register_interface, data_register_address);

    }

}

bool
men_uiq_pop(uiq_base * uiq_base, uiq_timestamp *ts, bool *have_ts)
{
    struct menable_uiq * uiq = container_of(uiq_base, struct menable_uiq, base);

    struct siso_menable * men = uiq->parent;

    uint32_t data_word = men_fetch_next_incoming_uiq_word(&men->uiq_transfer, men->messaging_dma_declaration, &men->register_interface, uiq->base.data_register_offset);
    uiq_base->write_from_grabber(uiq_base, data_word, ts, have_ts);

    /* TODO: [RKN] What's up here with the return value?!?
                   The retval is used by men_pop_all to decide whether or not to pop more words.
                   For RAW data, we do not know, if there is more to pop, but we could pop as much as the uiq buffer can hold.
                   For Legacy (decorated) and Event data, the EOT flag could be used.

                   But whats actually the desired behavior after pop_all?
                   Should the buffer on the board be empty?
                   Should the buffer of the uiq be as full as possible?
    */

    return (uiq_base->read_protocol == UIQ_PROTOCOL_LEGACY) 
        && !UIQ_CONTROL_IS_END_OF_TRANSMISSION(data_word) 
        && !UIQ_CONTROL_IS_INVALID(data_word);
}

static bool
men_uiq_pop_all(struct menable_uiq *uiq, uiq_timestamp * ts, bool *have_ts) {

    /* Note: This function is only used for mE4 and mE5 */

    struct siso_menable * men = uiq->parent;

    if (unlikely(uiq->base.is_full(&uiq->base))) {
        uint32_t val;
        do {
            val = men_fetch_next_incoming_uiq_word(&men->uiq_transfer, men->messaging_dma_declaration, &men->register_interface, uiq->base.data_register_offset);
            if (!(val & UIQ_CONTROL_INVALID))
                uiq->base.lost_words_count++; // TODO: [RKN] What about the last_words_were_lost flag?

        } while (!(val & UIQ_CONTROL_EOT));
        return false;
    }

    bool was_empty = (uiq->base.is_empty(&uiq->base));
    while (men_uiq_pop(&uiq->base, ts, have_ts)) {}

    if (uiq->cpltodo && (uiq->cpltodo <= uiq->base.fill))
        complete(&uiq->cpl);

    uiq->base.irq_count++;

    const bool changed_from_empty_to_filled = was_empty && (!uiq->base.is_empty(&uiq->base));

    return changed_from_empty_to_filled;
}

static void uiq_write_to_device_raw(struct menable_uiq * uiq, unsigned int write_count) {
    // all but the last entries
    struct siso_menable * men = uiq->parent;
    for (unsigned int i = 0; i < write_count; i++) {
        const unsigned int pos = (uiq->base.read_index + i) % uiq->base.capacity;
        men->register_interface.write(&men->register_interface, uiq->base.data_register_offset, uiq->base.data[pos] & 0xffff);
    }
}

static void uiq_write_to_device_legacy(struct menable_uiq * uiq, unsigned int write_count) {
    struct siso_menable * men = uiq->parent;

    if (write_count == 0) {
        return;
    }

    // all but the last entries
    for (unsigned int i = 0; i < write_count - 1; i++) {
        const unsigned int pos = (uiq->base.read_index + i) % uiq->base.capacity;
        men->register_interface.write(&men->register_interface, uiq->base.data_register_offset, uiq->base.data[pos] & 0xffff);
    }

    // last entry + EOT flag
    const unsigned int pos = (uiq->base.read_index + write_count - 1) % uiq->base.capacity;
    men->register_interface.write(&men->register_interface, uiq->base.data_register_offset, (uiq->base.data[pos] & 0xffff) | UIQ_CONTROL_EOT);
}

static void uiq_write_to_device_cxp(struct menable_uiq * uiq, unsigned int write_count) {

    struct siso_menable * men = uiq->parent;
    uint32_t packet_length = UIQ_CXP_HEADER_PACKET_LENGTH(uiq->base.data[uiq->base.read_index]);

    // TODO: [RKN] Should this be a while loop? Can there be more than one packet in the uiq?
    // TODO: [RKN] Can it happen, that the current rindex does not point to a packet boundary?
    if (write_count > packet_length) {
        write_count = packet_length + 1; // We only take one packet out of the queue at a time

        dev_dbg(&men->dev, "Writing %u words to CXP uiq 0x%x\n", write_count, uiq->base.id);

        /* Send K-word and packet type before packet data, CRC and K-word after packet data
         * Make sure the FIFO depth is given so that these additional words can always be appended
         */
        men->register_interface.write(&men->register_interface, uiq->base.data_register_offset + 1, 1); // send K-Word
        men->register_interface.write(&men->register_interface, uiq->base.data_register_offset, UIQ_CXP_SOP_K_WORD);

        men->register_interface.write(&men->register_interface, uiq->base.data_register_offset + 1, 0); // send data

        // send packet type first
        uint32_t packet_type = ((uiq->base.data[uiq->base.read_index] & UIQ_CXP_HEADER_TAGGED_PACKET_FLAG) != 0)
                                ? UIQ_CXP_TAGGED_COMMAND_PACKET
                                : UIQ_CXP_COMMAND_PACKET;
        men->register_interface.write(&men->register_interface, uiq->base.data_register_offset, packet_type);

        // send actual data
        uint32_t crc = 0xffffffff;
        for (unsigned int i = 0; i < packet_length; i++) {
            uint32_t data_word = uiq->base.data[(uiq->base.read_index + i + 1) % uiq->base.capacity];
            men->register_interface.write(&men->register_interface, uiq->base.data_register_offset, data_word);

            crc = crc32(&data_word, sizeof(data_word), crc);
        }

        // send data crc
        men->register_interface.write(&men->register_interface, uiq->base.data_register_offset, crc);

        // Send End-of-Packet K-Word
        men->register_interface.write(&men->register_interface, uiq->base.data_register_offset + 1, 1);
        men->register_interface.write(&men->register_interface, uiq->base.data_register_offset, UIQ_CXP_EOP_K_WORD);

    } else {
        // TODO: [RKN] Is discarding everything a good idea? Is it possible that
        //             there is garbage data in the uiq followed by a packet so
        //             that discarding only some data might be sensible?

        // If theres not enough data for one packet, something is terribly wrong, so just discard all data
        dev_err(&men->dev, ": Not enought data for packet. Discarding %u bytes of data for uiq 0x%x.\n", write_count, uiq->base.id);
    }
}

/**
 * Takes data out of the local UIQ buffer and writes it to the device.
 * Only one burst is written. Data that remains in the local buffer will
 * be written on the next call to when men_uiq_push.
 *
 * @param uiq The UIQ
 * @return true if the local buffer is empty afterwards, otherwise false.
 */
bool
men_uiq_push(uiq_base * uiq_base)
{
    struct menable_uiq * uiq = container_of(uiq_base, struct menable_uiq, base);

    dev_dbg(&uiq->dev, "%s uiq.fill: %u bytes, uiq id=0x%x, uiq type=%u\n",
            __FUNCTION__, uiq->base.fill, uiq->base.id, uiq->base.type);

    unsigned int write_count = min((unsigned int)uiq->base.fpga_fifo_depth, uiq->base.fill);

    switch (uiq->base.type) {
    case UIQ_TYPE_WRITE_RAW:
        uiq_write_to_device_raw(uiq, write_count);
        break;

    case UIQ_TYPE_WRITE_LEGACY:
        uiq_write_to_device_legacy(uiq, write_count);
        break;

    case UIQ_TYPE_WRITE_CXP:
        uiq_write_to_device_cxp(uiq, write_count);
        break;
    }

    uiq->base.read_index = (uiq->base.read_index + write_count) % uiq->base.capacity;
    uiq->base.irq_count++;
    uiq->base.fill -= write_count;
    if (uiq->cpltodo && (uiq->cpltodo <= uiq->base.capacity - uiq->base.fill))
        complete(&uiq->cpl);

    if (uiq->base.fill == 0) {
        /* avoid wraparound */
        uiq->base.read_index = 0;
        return true;
    } else {
        return false;
    }
}

static ssize_t
uiq_read(
struct file *filp,
struct kobject *kobj, struct bin_attribute *attr, char *buf,
    loff_t off, size_t buffer_size)
{
    unsigned long flags;
    struct device *dev = container_of(kobj, struct device, kobj);
    struct menable_uiq *uiq = container_of(dev, struct menable_uiq, dev);
    unsigned int num_words_requested = buffer_size / sizeof(*uiq->base.data);

    spin_lock_irqsave(&uiq->lock, flags);
    if (unlikely(uiq->cpltodo)) {
        spin_unlock_irqrestore(&uiq->lock, flags);
        return -EBUSY;
    }

    if (uiq->base.fill < num_words_requested) {
        unsigned long jiffies_until_timeout = uiq->cpltimeout;
        
        uiq->cpltodo = num_words_requested;

        bool some_data_received = false;
        bool requested_words_received = false;

        do {
            spin_unlock_irqrestore(&uiq->lock, flags);
            long completion_status = wait_for_completion_killable_timeout(&uiq->cpl, jiffies_until_timeout);
            some_data_received = (completion_status > 0);

            spin_lock_irqsave(&uiq->lock, flags);
            requested_words_received = (uiq->base.fill >= num_words_requested);

        } while (some_data_received && !requested_words_received && !fatal_signal_pending(current));
        uiq->cpltodo = 0;
    }

    ssize_t num_bytes_read = 0;
    if (uiq->base.fill > 0) {
        // We received at least some data -> pass it to the user.
        int num_words_read = uiq->base.pop_front(&uiq->base, (uint32_t*)buf, num_words_requested);
        num_bytes_read = num_words_read * sizeof(*uiq->base.data);
    } else {
        // Nothing received at all -> timeout!
        num_bytes_read = -ETIMEDOUT;
    }
    spin_unlock_irqrestore(&uiq->lock, flags);

    return num_bytes_read;
}

static ssize_t
uiq_write(
struct file *filp,
struct kobject *kobj, struct bin_attribute *attr, char *buf,
    loff_t off, size_t buffer_size)
{
    unsigned long flags;
    struct device *dev = container_of(kobj, struct device, kobj);
    struct menable_uiq *uiq = container_of(dev, struct menable_uiq, dev);
    const unsigned int num_words_requested = buffer_size / sizeof(*uiq->base.data);
    //unsigned int wrap;
    //unsigned int windex;
    bool notify = false;

    if (uiq->base.type == UIQ_TYPE_WRITE_CXP) {
        dev_dbg(&uiq->parent->dev, "[UIQ] Writing %zu bytes to CXP UIQ 0x%x.\n", buffer_size, uiq->base.id);
    }

    if (unlikely(buffer_size == 0))
        return 0;

    if (unlikely(buffer_size % sizeof(*uiq->base.data) != 0))
        return -EINVAL;

    spin_lock_irqsave(&uiq->lock, flags);
    if (unlikely((uiq->base.capacity == 0) || (uiq->base.capacity < num_words_requested))) {
        spin_unlock_irqrestore(&uiq->lock, flags);
        return -ENOSPC;
    }
    if (unlikely(uiq->cpltodo)) {
        spin_unlock_irqrestore(&uiq->lock, flags);
        return -EBUSY;
    }
    if (uiq->base.capacity - uiq->base.fill < num_words_requested) {
        unsigned long jiffies_until_timeout = uiq->cpltimeout;
        uiq->cpltodo = num_words_requested;
        do {
            spin_unlock_irqrestore(&uiq->lock, flags);
            dev_dbg(&uiq->dev, "%s - Wait for completion\n", __FUNCTION__);
            jiffies_until_timeout = wait_for_completion_killable_timeout(&uiq->cpl, jiffies_until_timeout);
            spin_lock_irqsave(&uiq->lock, flags);
            if (uiq->cpltodo == -1) {
                dev_dbg(&uiq->dev, "%s - cpldtodo unexpectedly set to -1\n", __FUNCTION__);
                /* someone send a clear request while we were
                * waiting. We act as if the data of this request
                * would already have been queued: we silently
                * drop it. */
                uiq->cpltodo = 0;
                spin_unlock_irqrestore(&uiq->lock, flags);
                return buffer_size;
            }
            if (uiq->base.capacity - uiq->base.fill >= num_words_requested)
                break;
        } while ((jiffies_until_timeout > 0) && !fatal_signal_pending(current));
        uiq->cpltodo = 0;
    }
    if (uiq->base.capacity - uiq->base.fill < num_words_requested) {
        spin_unlock_irqrestore(&uiq->lock, flags);
        dev_dbg(&uiq->dev, "%s - (uiq->base.capacity - uiq->base.fill < cnt) == true, length=%u, fill=%u, cnt=%u\n",
                __FUNCTION__, uiq->base.capacity, uiq->base.fill, num_words_requested);
        return -EBUSY;
    }

    uiq->base.push_back(&uiq->base, (uint32_t *) buf, num_words_requested);

    if (!uiq->base.is_running) {
        notify = men_uiq_push(&uiq->base);
        uiq->base.is_running = true;
    }

    spin_unlock_irqrestore(&uiq->lock, flags);

#if 0
    if (notify)
        sysfs_notify(&uiq->dev.kobj, NULL, "data");
#endif

    return buffer_size;
}

static ssize_t
men_uiq_readsize(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct menable_uiq *uiq = container_of(dev, struct menable_uiq, dev);

    /* This is not synchronized as this may change anyway
    * until the user can use the information */
    return sprintf(buf, "%zi\n", uiq->base.capacity * sizeof(*uiq->base.data));
}

static ssize_t
men_uiq_writesize(struct device *dev, struct device_attribute *attr,
                  const char *buf, size_t buffer_size)
{
    struct menable_uiq *uiq = container_of(dev, struct menable_uiq, dev);
    unsigned int sz;
    int completion_status;

    completion_status = buf_get_uint(buf, buffer_size, &sz);
    if (completion_status)
        return completion_status;

    completion_status = men_scale_uiq(&uiq->base, sz);

    return completion_status ? completion_status : buffer_size;
}

static ssize_t
men_uiq_readlost(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct menable_uiq *uiq = container_of(dev, struct menable_uiq, dev);

    /* This is not synchronized as this may change anyway
    * until the user can use the information */
    return sprintf(buf, "%i\n", uiq->base.lost_words_count);
}

static ssize_t
men_uiq_readfill(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct menable_uiq *uiq = container_of(dev, struct menable_uiq, dev);

    /* This is not synchronized as this may change anyway
    * until the user can use the information */
    return sprintf(buf, "%i\n", uiq->base.fill);
}

static ssize_t
men_uiq_writefill(struct device *dev, struct device_attribute *attr,
                  const char *buf, size_t buffer_size)
{
    struct menable_uiq *uiq = container_of(dev, struct menable_uiq, dev);
    unsigned long flags;

    // 0 is the only valid input value!
    if (strcmp(buf, "0") != 0)
        return -EINVAL;

    spin_lock_irqsave(&uiq->lock, flags);
    uiq->base.reset(&uiq->base);
    if (uiq->cpltodo) {
        uiq->cpltodo = -1;
        complete(&uiq->cpl);
    }
    spin_unlock_irqrestore(&uiq->lock, flags);

    return buffer_size;
}

static ssize_t
men_uiq_readirqcnt(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct menable_uiq *uiq = container_of(dev, struct menable_uiq, dev);

    return sprintf(buf, "%i\n", uiq->base.irq_count);
}

static ssize_t
men_uiq_writetimeout(struct device *dev, struct device_attribute *attr,
                     const char *buf, size_t buffer_size)
{
    struct menable_uiq *uiq = container_of(dev, struct menable_uiq, dev);
    int completion_status;
    unsigned long flags;
    unsigned int timout_msecs;
    unsigned long timout_jiffies;

    // get timeout in msecs
    completion_status = buf_get_uint(buf, buffer_size, &timout_msecs);
    if (completion_status)
        return completion_status;

    timout_jiffies = msecs_to_jiffies(timout_msecs);

    spin_lock_irqsave(&uiq->lock, flags);
    uiq->cpltimeout = timout_jiffies;
    spin_unlock_irqrestore(&uiq->lock, flags);

    return buffer_size;
}

static struct bin_attribute bin_attr_uiq_data_r = {
    .attr = {
        .name = "data",
        .mode = S_IFREG | S_IRUSR | S_IRGRP
    },
    .size = 4096,
    .read = uiq_read
};

static struct bin_attribute bin_attr_uiq_data_w = {
    .attr = {
        .name = "data",
        .mode = S_IFREG | S_IWUSR | S_IWGRP
    },
    .size = 4096,
    .write = uiq_write
};

struct device_attribute men_uiq_attributes[6] = {
    __ATTR(irqcnt, 0440, men_uiq_readirqcnt, NULL),
    __ATTR(fill, 0660, men_uiq_readfill, men_uiq_writefill),
    __ATTR(lost, 0440, men_uiq_readlost, NULL),
    __ATTR(size, 0660, men_uiq_readsize, men_uiq_writesize),
    __ATTR(timeout, 0220, NULL, men_uiq_writetimeout),
    __ATTR_NULL
};

static void menable_uiq_release(struct device *dev)
{
    struct menable_uiq *uiq = container_of(dev, struct menable_uiq, dev);

    kfree(uiq->base.data);
    kfree(uiq);
}

void
men_uiq_remove(uiq_base * uiq_base)
{
    if (uiq_base == NULL)
    {
    	pr_warn(KBUILD_MODNAME ": Invalid NULL argument in men_uiq_remove\n");
        return;
    }

    struct menable_uiq * uiq = container_of(uiq_base, struct menable_uiq, base);

    struct siso_menable * men = uiq->parent;
    char symlinkname[16];

    snprintf(symlinkname, sizeof(symlinkname), "uiq%i", uiq->base.channel_index);
    sysfs_remove_link(&men->dev.kobj, symlinkname);

    if (UIQ_TYPE_IS_WRITE(uiq->base.type)) {
        device_remove_bin_file(&uiq->dev, &bin_attr_uiq_data_w);
    } else {
        device_remove_bin_file(&uiq->dev, &bin_attr_uiq_data_r);
    }

    device_unregister(&uiq->dev);
}

static struct lock_class_key men_uiq_lock;
static struct lock_class_key men_uiqcpl_lock;

struct class *menable_uiq_class = NULL;

/**
* men_uiq_init - create a UIQ object for the given queue
* @chan UIQ channel number
* @addr address of the queue I/O register
* @parent grabber this UIQ belongs to
* @write if queue is a read or write queue
* @burst FIFO depth for write queues
*
* This will allocate the memory for the UIQ object and return
* the allocated pointer or an ERR_PTR() value on failure.
*/
uiq_base *
men_uiq_init(struct siso_menable *parent, int chan, unsigned int id, unsigned int type, unsigned int data_register_offset, unsigned char burst)
{
    int completion_status;
    struct menable_uiq *uiq;
    char symlinkname[16];

    dev_dbg(&parent->dev, "[UIQ] Initializing UIQ channel %d: id=0x%x, type=%u, burst=%u\n", chan, id, type, burst);

    uiq = kzalloc(sizeof(*uiq), GFP_KERNEL);
    if (uiq == NULL)
        return ERR_PTR(-ENOMEM);

    uiq_protocol read_protocol;
    completion_status = determine_uiq_protocol(id, chan, parent->pci_device_id, &read_protocol);
    if (MEN_IS_ERROR(completion_status)) {
        dev_err(&parent->dev, "[UIQ] Failed to determine protocol for UIQ id 0x%03x on board 0x%0x04x.\n", id, parent->pci_device_id);
        completion_status = -EINVAL;
        goto err;
    }
    dev_dbg(&parent->dev, "[UIQ] Protocol for UIQ Id 0x%03x is %s (%d)\n", id, men_get_uiq_protocol_name(read_protocol), read_protocol);

    uiq_base_init(&uiq->base, data_register_offset, NULL, 0, id, type, read_protocol, burst, chan);

    uiq->parent = parent;
    uiq->dev.driver = parent->dev.driver;
    uiq->dev.class = menable_uiq_class;
    init_completion(&uiq->cpl);
    spin_lock_init(&uiq->lock);
    lockdep_set_class(&uiq->lock, &men_uiq_lock);
    lockdep_set_class(&uiq->cpl.wait.lock, &men_uiqcpl_lock);

    dev_set_name(&uiq->dev, "%s_uiq%i", dev_name(&parent->dev), chan);

    uiq->dev.parent = &parent->dev;
    uiq->dev.release = menable_uiq_release;
    uiq->dev.groups = NULL;

    device_initialize(&uiq->dev);
    dev_set_uevent_suppress(&uiq->dev, 1);
    completion_status = device_add(&uiq->dev);
    if (completion_status)
        goto err;

    if (UIQ_TYPE_IS_WRITE(type)) {
        completion_status = sysfs_create_bin_file(&uiq->dev.kobj, &bin_attr_uiq_data_w);
    } else {
        completion_status = sysfs_create_bin_file(&uiq->dev.kobj, &bin_attr_uiq_data_r);
    }

    if (completion_status)
        goto err_data;

    dev_set_uevent_suppress(&uiq->dev, 0);
    kobject_uevent(&uiq->dev.kobj, KOBJ_ADD);

    snprintf(symlinkname, sizeof(symlinkname), "uiq%i", chan);
    completion_status = sysfs_create_link(&parent->dev.kobj, &uiq->dev.kobj, symlinkname);
    if (completion_status)
        goto err_create_link;

    return &uiq->base;


err_create_link:
	if (UIQ_TYPE_IS_WRITE(type)) {
		sysfs_remove_bin_file(&uiq->dev.kobj, &bin_attr_uiq_data_w);
    } else {
    	sysfs_remove_bin_file(&uiq->dev.kobj, &bin_attr_uiq_data_r);
	}

err_data:
    device_unregister(&uiq->dev);
err:
    kfree(uiq);
    return ERR_PTR(completion_status);
}

int
men_scale_uiq(uiq_base * uiq_base, const unsigned int len)
{
    struct menable_uiq * uiq = container_of(uiq_base, struct menable_uiq, base);
    
    const unsigned int new_buffer_size = len / sizeof(*uiq->base.data);

    if (uiq->base.capacity == new_buffer_size)
        return 0;

    uint32_t * new_buffer;
    if (len == 0) {
        new_buffer = NULL;
    } else {
        new_buffer = kmalloc(len, GFP_USER);
        if (unlikely(new_buffer == NULL))
            return -ENOMEM;
    }

    unsigned long flags;
    spin_lock_irqsave(&uiq->lock, flags);

    uint32_t * old;
    uiq->base.replace_buffer(&uiq->base, new_buffer, new_buffer_size, &old);

    spin_unlock_irqrestore(&uiq->lock, flags);
    kfree(old);

    return 0;
}

void
uiq_irq(uiq_base *uiq_base, uiq_timestamp *ts, bool *have_ts)
{
    struct menable_uiq * uiq = container_of(uiq_base, struct menable_uiq, base);

    bool notify = false;
    struct siso_menable * men = uiq->parent;

    BUG_ON(uiq == NULL);

    DEV_DBG_UIQ(&uiq->dev, "Received intterrupt for UIQ channel %d.\n", (int)uiq_base->channel_index);

    spin_lock(&uiq->lock);

    if (UIQ_TYPE_IS_READ(uiq->base.type)) {
        notify = men_uiq_pop_all(uiq, ts, have_ts);
        men->register_interface.write(&men->register_interface, uiq->irqack_offs, 1 << uiq->ackbit);
    } else {
        WARN_ON(!uiq->base.is_running);

        men->register_interface.write(&men->register_interface, uiq->irqack_offs, 1 << uiq->ackbit);
        if (uiq->base.fill == 0) {
            uiq->base.is_running = false;
            uiq->base.read_index = 0;
        } else {
            notify = men_uiq_push(&uiq->base);
        }
    }

    spin_unlock(&uiq->lock);

#if 0
    if (notify)
        sysfs_notify(&uiq->dev.kobj, NULL, "data");
#endif
}

/**
* men_del_uiqs - delete UIQs from given FPGA
* @men: board to use
* @fpga: fpga index to use
*
* This will also remove all UIQs from higher FPGAs.
*/
void
men_del_uiqs(struct siso_menable *men, const unsigned int fpga)
{
    WARN(!men->design_changing, "Deleting UIQs although design is not changing.\n");

    for (unsigned int fpga_idx = fpga; fpga_idx < MAX_FPGAS; fpga_idx++) {
        DEV_DBG_UIQ(&men->dev, "Deleting %d UIQs for FPGA %d\n", men->uiqcnt[fpga_idx], fpga_idx);
        for (unsigned int uiq_idx = 0; uiq_idx < men->uiqcnt[fpga_idx]; uiq_idx++) {
            unsigned int chan = fpga_idx * MEN_MAX_UIQ_PER_FPGA + uiq_idx;
            men_uiq_remove(men->uiqs[chan]);
            men->uiqs[chan] = NULL;
        }
        men->num_active_uiqs -= men->uiqcnt[fpga_idx];
        men->uiqcnt[fpga_idx] = 0;
    }
}
