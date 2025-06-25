/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include <linux/io.h>
#include <linux/pci.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "menable.h"
#include "menable_ioctl.h"
#include "linux_version.h"
#include "sisoboards.h"

struct me_threadgroup *
me_create_threadgroup (struct siso_menable *men, const unsigned int tgid)
{
    struct me_threadgroup *bh;
    unsigned int i, j;

#ifdef ENABLE_DEBUG_MSG
    printk(KERN_INFO "[%d]: Creating Thread Group\n", current->tgid);
#endif

    bh = kzalloc(sizeof(*bh), GFP_KERNEL);
    if (bh == NULL) {
        return NULL;
    }

    INIT_LIST_HEAD(&bh->node);
    bh->id = tgid;
    bh->cnt = 0;
    for (i = 0; i < MAX_FPGAS; i++) {
        for (j = 0; j < MEN_MAX_DMA; j++) {
            bh->dmas[i][j] = 0;
        }
    }

    spin_lock(&men->threadgroups_headlock);
    list_add_tail(&bh->node, &men->threadgroups_heads);
    spin_unlock(&men->threadgroups_headlock);

    return bh;
}

void
me_free_threadgroup(struct siso_menable *men, struct me_threadgroup *bh)
{
#ifdef ENABLE_DEBUG_MSG
    printk(KERN_INFO "[%d]: Removing Thread Group\n", bh->id);
#endif

    spin_lock(&men->threadgroups_headlock);
    __list_del_entry(&bh->node);
    spin_unlock(&men->threadgroups_headlock);
    kfree(bh);
}

struct me_threadgroup *
me_get_threadgroup(struct siso_menable *men, const unsigned int tgid)
{
    struct me_threadgroup *res;

    spin_lock(&men->threadgroups_headlock);
    list_for_each_entry(res, &men->threadgroups_heads, node) {
        if (res->id == tgid) {
            spin_unlock(&men->threadgroups_headlock);
            return res;
        }
    }
    spin_unlock(&men->threadgroups_headlock);

#ifdef ENABLE_DEBUG_MSG
    printk(KERN_INFO "[%d]: Thread Group not found !\n", tgid);
#endif

    return NULL;
}

/**
* men_dma_channel - get DMA channel on the given board
* @men: board to query
* @dma: dma channel index
*
* This will return the DMA channel with the given index if
* it exists, otherwise NULL.
*/
struct menable_dmachan *
men_dma_channel(struct siso_menable *men, const unsigned int index)
{
    unsigned int i;
    unsigned int j = 0; /* the highest valid DMA index +1 */

    for (i = 0; i < MAX_FPGAS; i++) {
        j += men->dmacnt[i];
    }

    WARN_ON(j > MEN_MAX_DMA);

    if (index < j)
        return men->dmachannels[index];
    else
        return NULL;
}

/**
* men_get_dmaimg - print current picture number to sysfs
* @dev: device to query
* @attr: device attribute of the channel file
* @buf: buffer to print information to
*
* The result will be printed in decimal form into the buffer.
*/
static ssize_t
men_get_dmaimg(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct menable_dmachan *d = container_of(dev,
    struct menable_dmachan, dev);

    return sprintf(buf, "%lli\n", d->goodcnt);
}

/**
* men_get_dmalost - print current number of lost pictures to sysfs
* @dev: device to query
* @attr: device attribute of the channel file
* @buf: buffer to print information to
*
* The result will be printed in decimal form into the buffer.
*/
static ssize_t
men_get_dmalost(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct menable_dmachan *d = container_of(dev, struct menable_dmachan, dev);

    return sprintf(buf, "%i\n", d->lost_count);
}

struct device_attribute men_dma_attributes[3] = {
    __ATTR(lost, 0440, men_get_dmalost, NULL),
    __ATTR(img, 0440, men_get_dmaimg, NULL),
    __ATTR_NULL
};

struct class *menable_dma_class = NULL;

static struct lock_class_key men_dmacpl_lock;

#define IS_32_BIT_FRAMEINDEX (sizeof(long) == 4)
#define GET_31_BIT_WINDOW(value) (value & ~INT32_MAX)
#define THRESHOLD_31_BIT_WRAPAROUND (INT32_MAX / 2)
#define MOVE_INTO_SAME_31_BIT_WINDOW(value, targetWindow) (value |= GET_31_BIT_WINDOW(targetWindow))
#define MOVE_INTO_NEXT_31_BIT_WINDOW(value) (value += ((uint64_t)1 << 31));

/**
* men_wait_dmaimg - wait until the given image is grabbed
* @d: the DMA channel to watch
* @img: the image number to wait for
* @timeout: wait limit
*
* This function blocks until the current image number is at least the one
* requested in @buf.
*
* Returns: current picture number on success, error code on failure
*/
int
men_wait_dmaimg(struct menable_dmachan *dma_chan, int64_t img,
                int timeout_msecs, uint64_t *foundframe, bool is32bitProcOn64bitKernel)
{
    uint64_t latestImage;
    uint64_t waitimg;
    struct device *dv;
    unsigned long flags;
    struct menable_dma_wait waitstr;
    unsigned long timeout_jiffies;

    dv = get_device(&dma_chan->dev);
    init_completion(&waitstr.cpl);
    lockdep_set_class(&waitstr.cpl.wait.lock, &men_dmacpl_lock);
    INIT_LIST_HEAD(&waitstr.node);

    spin_lock_irqsave(&dma_chan->listlock, flags);

    latestImage = dma_chan->goodcnt;
    if (img <= 0) {

        /* For negative image numbers and zero, we wait for current pic number + |img| */
        waitimg = latestImage - img;

        /* after a wrap around, we add 1 since image numbers start with 1 */
        if (waitimg < latestImage) ++waitimg;

    } else {

        waitimg = img;
        /* img originates from a `frameindex_t` value from the runtime, which is a
         * typedef for `long`. So when `frameindex_t` is a 32bit integer, we need to
         * deal with overflows.
         */
        if (is32bitProcOn64bitKernel || IS_32_BIT_FRAMEINDEX) {
            /* Deal with overflows on 32 bit machines.
               The value of img is usually originates from a frameindex_t value,
               which is a long with 32 bit on these machines. */
            MOVE_INTO_SAME_31_BIT_WINDOW(waitimg, latestImage);
            if (waitimg < latestImage
                    && (latestImage - waitimg) > THRESHOLD_31_BIT_WRAPAROUND) {
                MOVE_INTO_NEXT_31_BIT_WINDOW(waitimg)
            }
        }
    }

    if (latestImage >= waitimg) {
        spin_unlock_irqrestore(&dma_chan->listlock, flags);
        put_device(dv);
        *foundframe = dma_chan->goodcnt;
        return 0;
    }
    if (unlikely(dma_chan->state != MEN_DMA_CHAN_STATE_STARTED)) {
        spin_unlock_irqrestore(&dma_chan->listlock, flags);
        put_device(dv);
        return -ETIMEDOUT;
    }
    waitstr.frame = waitimg;
    list_add_tail(&waitstr.node, &dma_chan->wait_list);
    spin_unlock_irqrestore(&dma_chan->listlock, flags);
	
	timeout_jiffies = msecs_to_jiffies(timeout_msecs);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)
    wait_for_completion_killable_timeout(&waitstr.cpl, timeout_jiffies);
#else
    wait_for_completion_interruptible_timeout(&waitstr.cpl, timeout_jiffies);
#endif

    spin_lock_irqsave(&dma_chan->listlock, flags);
    list_del(&waitstr.node);

    /* goodcnt may have changed in the meantime */
    latestImage = dma_chan->goodcnt;
    spin_unlock_irqrestore(&dma_chan->listlock, flags);

    put_device(dv);
    if (latestImage < waitimg)
        return -ETIMEDOUT;

    *foundframe = dma_chan->goodcnt;
    return 0;
}

static enum hrtimer_restart
men_dma_timeout(struct hrtimer * arg)
{
    unsigned long flags;
    struct menable_dmachan *dc = container_of(arg, struct menable_dmachan, timer);
    struct menable_dma_wait *waitstr;

    spin_lock(&dc->parent->buffer_heads_lock);
    spin_lock_irqsave(&dc->chanlock, flags);
    if (!spin_trylock(&dc->timerlock)) {
        /* If this fails someone else tries to modify this timer.
        * This means the timer will either get restarted or deleted.
        * In both cases we don't need to do anything here as the
        * other function will take care of everything. */
        spin_unlock_irqrestore(&dc->chanlock, flags);
        spin_unlock(&dc->parent->buffer_heads_lock);
        return HRTIMER_NORESTART;
    }

    dc->parent->abortdma(dc->parent, dc);
    dc->state = MEN_DMA_CHAN_STATE_STOPPED;
    spin_lock(&dc->listlock);
    list_for_each_entry(waitstr, &dc->wait_list, node) {
        complete(&waitstr->cpl);
    }
    spin_unlock(&dc->listlock);

    spin_unlock(&dc->timerlock);
    spin_unlock_irqrestore(&dc->chanlock, flags);
    spin_unlock(&dc->parent->buffer_heads_lock);

    return HRTIMER_NORESTART;
}

static void
menable_chan_release(struct device *dev)
{
    struct menable_dmachan *d = container_of(dev, struct menable_dmachan, dev);

    kfree(d);
}

static struct lock_class_key men_dma_lock;
static struct lock_class_key men_dmachan_lock;
static struct lock_class_key men_dmatimer_lock;

/**
* dma_clean_sync - put DMA channel in stopped state
* @db: channel to work
*
* This resets everything in the DMA channel to stopped state, e.g. clearing
* association of a memory buffer.
*
* context: IRQ (headlock and chanlock must be locked and released from caller)
*          the channel must be in state MEN_DMA_CHAN_STATE_STOPPING
*/
void
men_dma_clean_sync(struct menable_dmachan *dma_chan)
{
    struct menable_dma_wait *waitstr;

    BUG_ON(dma_chan->state != MEN_DMA_CHAN_STATE_STOPPING);
    dma_chan->parent->stopdma(dma_chan->parent, dma_chan);
    hrtimer_cancel(&dma_chan->timer);
    dma_chan->state = MEN_DMA_CHAN_STATE_STOPPED;
    dma_chan->transfer_todo = 0;
    spin_lock(&dma_chan->listlock);
    list_for_each_entry(waitstr, &dma_chan->wait_list, node) {
        complete(&waitstr->cpl);
    }
    spin_unlock(&dma_chan->listlock);
}

void
men_dma_done_work(struct work_struct *work)
{
    struct menable_dmachan *dma_chan =
            container_of(work, struct menable_dmachan, dwork);

    struct siso_menable *men = dma_chan->parent;
    unsigned long flags;

    spin_lock_bh(&men->buffer_heads_lock);
    spin_lock_irqsave(&dma_chan->chanlock, flags);
    /* the timer might have cancelled everything in the mean time */
    if (dma_chan->state == MEN_DMA_CHAN_STATE_STOPPING)
        men_dma_clean_sync(dma_chan);
    spin_unlock_irqrestore(&dma_chan->chanlock, flags);
    spin_unlock_bh(&men->buffer_heads_lock);
}

static struct menable_dmachan *
men_create_dmachan(struct siso_menable *parent, const unsigned char index, const unsigned char fpga)
{
    char symlinkname[16];
    struct menable_dmachan *res = kzalloc(sizeof(*res), GFP_KERNEL);
    int r = 0;

    if (!res)
        return ERR_PTR(ENOMEM);

    /* our device */
    res->dev.parent = &parent->dev;
    res->dev.release = menable_chan_release;
    res->dev.driver = parent->dev.driver;
    res->dev.class = menable_dma_class;
    res->dev.groups = NULL;
    res->number = index;
    res->fpga = fpga;
    parent->dmabase(parent, res);
    dev_set_name(&res->dev, "%s_dma%i", dev_name(&parent->dev), index);

    /* misc setup */
    res->parent = parent;
    spin_lock_init(&res->listlock);
    spin_lock_init(&res->chanlock);
    spin_lock_init(&res->timerlock);
    lockdep_set_class(&res->listlock, &men_dma_lock);
    lockdep_set_class(&res->chanlock, &men_dmachan_lock);
    lockdep_set_class(&res->timerlock, &men_dmatimer_lock);
    INIT_LIST_HEAD(&res->free_list);
    INIT_LIST_HEAD(&res->ready_list);
    INIT_LIST_HEAD(&res->grabbed_list);
    INIT_LIST_HEAD(&res->hot_list);
    INIT_LIST_HEAD(&res->wait_list);
    hrtimer_setup(&res->timer, men_dma_timeout, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    res->timer.function = men_dma_timeout;
    INIT_WORK(&res->dwork, men_dma_done_work);

    r = device_register(&res->dev);
    if (r != 0)
        goto err;

    snprintf(symlinkname, sizeof(symlinkname), "dma%i", index);
    r = sysfs_create_link(&parent->dev.kobj, &res->dev.kobj, symlinkname);
    if (r)
        goto err_data;

    return res;
err_data:
    device_unregister(&res->dev);
err:
    kfree(res);
    return ERR_PTR(r);
}

static void
men_dma_remove(struct menable_dmachan *d)
{
    char symlinkname[16];

    hrtimer_cancel(&d->timer);
    flush_scheduled_work();

    snprintf(symlinkname, sizeof(symlinkname), "dma%i", d->number);
    sysfs_remove_link(&d->parent->dev.kobj, symlinkname);

    device_unregister(&d->dev);
}

/**
* men_add_dmas - add more DMA channels
* @men: board to pimp
*/
int
men_add_dmas(struct siso_menable *men)
{
    unsigned int i;
    struct menable_dmachan **old, **nc;
    unsigned long flags;
    unsigned int countOFNewRequestedDMA = 0;
    unsigned int nrOfAllExistingDMA = 0;
    unsigned int nrOfExistingDMAPerFPGA[MAX_FPGAS];
    unsigned int countOfNewRequestedDMAPerFPGA[MAX_FPGAS];
    unsigned int newindex;

    for (i = 0; i < MAX_FPGAS; i++) {
        nrOfAllExistingDMA += men->dmacnt[i];
        nrOfExistingDMAPerFPGA[i] = men->dmacnt[i];
    }

    /* For pre me6 boards, the DMA count may only change if that FPGA previously had none. */
    const bool warnOnChange = !SisoBoardIsMe6(men->pci_device_id);
    
    for (i = 0; i < men->active_fpgas; i++) {
        unsigned int tmp = men->query_dma(men, i);
        if (warnOnChange && (tmp > nrOfExistingDMAPerFPGA[i]) && (nrOfExistingDMAPerFPGA[i] != 0)) {
            dev_warn(&men->dev, "menable DMA count changed from %u to %u\n", nrOfExistingDMAPerFPGA[i], tmp);
            WARN_ON((tmp > nrOfExistingDMAPerFPGA[i]) && (nrOfExistingDMAPerFPGA[i] != 0));
        }
        countOFNewRequestedDMA += tmp;
        countOfNewRequestedDMAPerFPGA[i] = tmp;
    }

    /* For pre me6 boards, the DMA count is not supposed to decrease. */
    WARN_ON(warnOnChange && countOFNewRequestedDMA < nrOfAllExistingDMA);
    if (unlikely(countOFNewRequestedDMA <= nrOfAllExistingDMA))
        return 0;

    nc = kcalloc(countOFNewRequestedDMA, sizeof(*nc), GFP_KERNEL);

    if (unlikely(nc == NULL))
        return -ENOMEM;

    newindex = nrOfAllExistingDMA;
    for (i = 0; i < men->active_fpgas; i++) {
        unsigned int j;

        if (nrOfExistingDMAPerFPGA[i] == countOfNewRequestedDMAPerFPGA[i])
            continue;

		if ((countOfNewRequestedDMAPerFPGA[i] - nrOfExistingDMAPerFPGA[i]) > 0) {
			/* allocate control structs for all new channels */
			for (j = 0; j < (countOfNewRequestedDMAPerFPGA[i] - nrOfExistingDMAPerFPGA[i]); j++) {
				nc[newindex] = men_create_dmachan(men, newindex, i);

				if (unlikely(IS_ERR(nc[newindex]))) {
					int ret = PTR_ERR(nc[newindex]);
					unsigned int j;

					for (j = nrOfAllExistingDMA; j < newindex; j++)
						men_dma_remove(nc[j]);

					kfree(nc);
					return ret;
				}
				newindex++;
			}
		}
    }

    spin_lock_bh(&men->buffer_heads_lock);
    spin_lock_irqsave(&men->boardlock, flags);
    /* copy old array contents to new one */
    old = men->dmachannels;
    for (i = 0; i < nrOfAllExistingDMA; i++)
        nc[i] = old[i];

    men->dmachannels = nc;
    for (i = 0; i < men->active_fpgas; i++)
        men->dmacnt[i] = men->query_dma(men, i);
    spin_unlock_irqrestore(&men->boardlock, flags);
    spin_unlock_bh(&men->buffer_heads_lock);

    kfree(old);

    return 0;
}

/**
* men_alloc_dma - change number of DMA channels of this board
* @men: board
* @count: new channel count
*
* count must not be greater than the current DMA count. Use men_add_dmas()
* instead.
*/
int
men_alloc_dma(struct siso_menable *men, unsigned int count)
{
    struct menable_dmachan *old[MEN_MAX_DMA];
    struct menable_dmachan **delold = NULL;
    int i;
    unsigned int oldcnt = 0;
    unsigned long flags;

    spin_lock_irqsave(&men->boardlock, flags);

    if (unlikely(men->releasing))
        count = 0;

    for (i = 0; i < MAX_FPGAS; i++)
        oldcnt += men->dmacnt[i];

    BUG_ON(oldcnt > MEN_MAX_DMA);

    if (count == oldcnt) {
        spin_unlock_irqrestore(&men->boardlock, flags);
        spin_unlock_bh(&men->buffer_heads_lock);
        return 0;
    } else if (count > oldcnt) {
        spin_unlock_irqrestore(&men->boardlock, flags);
        spin_unlock_bh(&men->buffer_heads_lock);
        WARN_ON(count > oldcnt);
        return 0;
    }

    for (i = count; i < oldcnt; i++) {
        struct menable_dmachan *dc = men_dma_channel(men, i);
        spin_lock(&dc->chanlock);
        men_stop_dma_locked(dc);
        spin_unlock(&dc->chanlock);
    }

    memset(old, 0, sizeof(old));

    /* copy those we will free */
    for (i = count; i < oldcnt; i++) {
        old[i] = men->dmachannels[i];
        men->dmachannels[i] = NULL;
    }
    /* don't bother shrinking the array here, it's
     * not worth the effort */

    for (i = 0; i < MAX_FPGAS; i++) {
        unsigned int oldcnt = men->dmacnt[i];

        men->dmacnt[i] = min(count, oldcnt);
        if (count > 0)
            count -= oldcnt;
    }
    if (count == 0) {
        delold = men->dmachannels;
        men->dmachannels = NULL;
    }
    spin_unlock_irqrestore(&men->boardlock, flags);
    spin_unlock_bh(&men->buffer_heads_lock);

    kfree(delold);

    for (i = oldcnt - 1; i >= 0; --i) {
        if (old[i] == NULL)
            continue;

        men_dma_remove(old[i]);
    }

    return 0;
}

ssize_t
men_get_dmas(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct siso_menable *men = container_of(dev, struct siso_menable, dev);
    unsigned int i;
    unsigned int cnt = 0;

    for (i = 0; i < men->active_fpgas; i++)
        cnt += men->dmacnt[i];

    return sprintf(buf, "%i\n", cnt);
}

/*
 * Initializes all buffer queues for a DMA channel.
 * The channels's mode determines how this is performed exactly:
 *
 *  - SELECTIVEMODE: hot -> ready, grabbed -> ready, free unchanged
 *  - other: all buffers -> ready
 */
static void
men_clean_bh(struct menable_dmachan *dma_chan, const unsigned int startbuf)
{
    struct menable_dmahead * active_dma_head = dma_chan->active;

    /* clear all lists, so we can add re-add all buffers as required later */
    dma_chan->free_count = 0;
    INIT_LIST_HEAD(&dma_chan->free_list);

    dma_chan->ready_count = 0;
    INIT_LIST_HEAD(&dma_chan->ready_list);

    dma_chan->hot_count = 0;
    INIT_LIST_HEAD(&dma_chan->hot_list);

    dma_chan->grabbed_count = 0;
    INIT_LIST_HEAD(&dma_chan->grabbed_list);

    /* clear counters */
    dma_chan->locked_count = 0;
    dma_chan->lost_count = 0;
    dma_chan->goodcnt = 0;

    for (long i = startbuf; i < (startbuf + active_dma_head->num_sb); ++i) {
        long buf_idx = i % active_dma_head->num_sb;
        struct menable_dmabuf * buf = active_dma_head->bufs[buf_idx];

        /* In SELECTIVEMODE, the entries in FREE are not moved. */
        if (buf != NULL) {
            if (dma_chan->mode != DMA_SELECTIVEMODE) {
                /* All buffer become ready */
                list_add_tail(&active_dma_head->bufs[buf_idx]->node, &dma_chan->ready_list);
                buf->listname = READY_LIST;
                dma_chan->ready_count++;
            } else if (buf->listname == READY_LIST){
                /* All buffers that are marked READY become ready, since they have
                 * been queued by the user. All others are moved to FREE and must
                 * be queued explicitly by the usere before they are used.
                 *
                 * TODO: Running multiple consecutive acquisistions in different modes is likely to
                 *       cause inconsistencies (i.e. buffers in ready that have never
                 *       been queued explicitly). Should we deal with that?
                 */
                list_add_tail(&active_dma_head->bufs[buf_idx]->node, &dma_chan->ready_list);
                buf->listname = READY_LIST;
                dma_chan->ready_count++;
            } else {
                /* Non-READY buffer in selective mode goes to FREE */
                list_add_tail(&active_dma_head->bufs[buf_idx]->node, &dma_chan->free_list);
                buf->listname = FREE_LIST;
                dma_chan->free_count++;
            }

        }
    }

    active_dma_head->chan = dma_chan;
}

int
men_start_dma(struct menable_dmachan *dma_chan, struct menable_dmahead *dma_head,
              const unsigned int startbuf)
{
    int ret = 0;
    struct me_threadgroup * tg;
    ktime_t timeout;

    if ((dma_head->chan != NULL) &&
        ((dma_chan->state != MEN_DMA_CHAN_STATE_STOPPED) ||  (dma_head->chan->state != MEN_DMA_CHAN_STATE_STOPPED))) {
            return -EBUSY;
    }

    if ((dma_chan->active != NULL) && (dma_chan->active != dma_head))
        dma_chan->active->chan = NULL;

    dma_chan->active = dma_head;
    dma_chan->state = MEN_DMA_CHAN_STATE_STARTING;

    spin_lock(&dma_chan->listlock);
    men_clean_bh(dma_chan, startbuf);

    // In all modes except selective mode, buffers must be ready before starting
    // the acquisition
    if (dma_chan->ready_count == 0 && dma_chan->mode != DMA_SELECTIVEMODE)
        ret = -ENODEV;

    spin_unlock(&dma_chan->listlock);
    if (ret) {
        dma_chan->state = MEN_DMA_CHAN_STATE_STOPPING;
        return ret;
    }

    ret = dma_chan->parent->startdma(dma_chan->parent, dma_chan);
    if (ret) {
    	dma_chan->state = MEN_DMA_CHAN_STATE_STOPPING;
        return ret;
    }

    tg = me_get_threadgroup(dma_chan->parent, current->tgid);
    if (likely(tg != NULL)) {
        tg->dmas[dma_chan->fpga][dma_chan->number] = 1;
    }

    if (ret == 0 && dma_chan->timeout) {
        timeout = ktime_set(dma_chan->timeout, 0);
        spin_lock(&dma_chan->timerlock);
        hrtimer_start(&dma_chan->timer, timeout, HRTIMER_MODE_REL);
        spin_unlock(&dma_chan->timerlock);
    }

    dma_chan->state = MEN_DMA_CHAN_STATE_STARTED;

    return ret;
}
