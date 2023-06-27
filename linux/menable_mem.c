/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include "menable.h"
#include "menable_ioctl.h"
#include "linux_version.h"

#include "debugging_macros.h"

/*
 * The functions `mmap_write_lock` and `mmap_write_unlock` exist in the following kernel versions:
 *    - 5.4.x with x >= 208
 *    - 5.8 and above
 *
 * For all other versions we define them ourselves.
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,4,208)) \
    || (LINUX_VERSION_CODE >= KERNEL_VERSION(5,5,0) && LINUX_VERSION_CODE < KERNEL_VERSION(5,8,0))

static inline void mmap_write_lock(struct mm_struct *mm)
{
	down_write(&mm->mmap_sem);
}

static inline void mmap_write_unlock(struct mm_struct *mm)
{
	up_write(&mm->mmap_sem);
}
#endif

/**
* get_page_addresses - get addresses of user pages
* @addr: start address
* @end: end address
* @pg: list of pages will be allocated here
* @len: number of pages in list
*
* This is more or less a copy of mm/memory.c::make_pages_present
*/
static int
get_page_addresses(unsigned long addr, const size_t length, struct page ***pages)
{
    int ret = 0, write = 0, len = 0;
    size_t runlen = 0;
    struct vm_area_struct *vma = NULL;

    mmap_write_lock(current->mm);

    vma = find_vma(current->mm, addr);
    if (!vma) {
        printk(KERN_ERR "No VMA found for buffer");
        ret = -EFAULT;
        goto fail;
    }

    write = (vma->vm_flags & VM_WRITE) ? 1 : 0;
    len = DIV_ROUND_UP(length + (addr % PAGE_SIZE), PAGE_SIZE);

    *pages = vmalloc(len * sizeof(*pages));
    if (!*pages) {
        printk(KERN_ERR "Not enough memory for page table");
        ret = -ENOMEM;
        goto fail;
    }

    /*
     * VM_DONTCOPY prevents the buffer from being copied when the process
     * is forked.
     */
    vm_flags_set(vma, VM_DONTCOPY);
    runlen = vma->vm_end - vma->vm_start;
    while (runlen < length) {
        vma = find_vma(current->mm, addr + runlen);
        if (!vma) {
            printk(KERN_ERR "Next VMA not found for buffer");
            ret = -EFAULT;
            goto fail_mem;

        }

        vm_flags_set(vma, VM_DONTCOPY);
        runlen += vma->vm_end - vma->vm_start;
    }

    /* pin the user pages in memory and get the physical addresses */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    /* Kernel 5.6 introduces a new function specifically for pinning memory */
    ret = pin_user_pages(addr,
        len, FOLL_LONGTERM | ((write == 1) ? FOLL_WRITE : 0), *pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
    /* Kernel 5.2 introduces FOLL_LONGTERM, allowing memory defragmentation before pinning */
    ret = get_user_pages(addr,
        len, FOLL_LONGTERM | ((write == 1) ? FOLL_WRITE : 0), *pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    /* Kernel 4.9 replaces the force flag with the more genearal gup_flags */
    ret = get_user_pages(addr,
        len, (write == 1) ? FOLL_WRITE : 0, *pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
    /* Kernel 4.6 introduces a new signature to get_user_pages and uses current, current->mm implicitly */
    ret = get_user_pages(addr,
        len, 0, *pages, NULL);
#else /* LINUX < 4.6.0 */
    ret = get_user_pages(current, current->mm, addr,
        len, write, 0, *pages, NULL);
#endif
    if (ret < 0) {
        goto fail_mem;
    }

    mmap_write_unlock(current->mm);
    return len;

fail_mem:
    vfree(*pages);
fail:
    mmap_write_unlock(current->mm);
    return ret;
}

/**
* men_create_userbuf - do generic initialisation of user buffer
* @men: device to use this buffer
* @range: user address range
*
* Context: User context
*
* returns: 0 on success, negative error code otherwise
*/
int
men_create_userbuf(struct siso_menable *men, struct men_io_range *range)
{
    struct page **pages;
    struct menable_dmabuf *dma_buf, *dummybuf;
    int ret = -ENOMEM, num_pages, i;
    uint64_t len;
    struct menable_dmahead *buf_head;
    struct menable_dmachan *dma_chan;

#if BITS_PER_LONG > 32
    if (range->length > 16UL * 1024UL * 1024UL * 1024UL)
        return -EINVAL;
#endif

    /* force 32 bit alignment of DMA memory */
    if ((range->start & 0x3) != 0)
        return -EINVAL;

    buf_head = me_get_buf_head(men, range->headnr);
    if (buf_head == NULL)
        return -EINVAL;

    ret = 0;
    if (range->subnr >= buf_head->num_sb) {
        ret = -EINVAL;
    } else if (buf_head->bufs[range->subnr]) {
        ret = -EBUSY;
    }

    /* This is racy if the user does something really stupid like deleting
     * the head from another thread while registering a buffer */
    dummybuf = &buf_head->dummybuf;
    spin_unlock_bh(&men->buffer_heads_lock);
    if (ret != 0)
        return ret;

    ret = -ENOMEM;

    num_pages = get_page_addresses(range->start, range->length, &pages);
    if (num_pages < 0)
        return num_pages;

    dma_buf = kzalloc(sizeof(*dma_buf), GFP_KERNEL);
    if (!dma_buf)
        goto fail_dma_buf;

    /* create a normal linux SG list */
    dma_buf->sg = kzalloc(num_pages * sizeof(*dma_buf->sg), GFP_KERNEL);
    if (!dma_buf->sg)
        goto fail_sg;
    sg_init_table(dma_buf->sg, num_pages);

    dma_buf->dma_chain = kzalloc(sizeof(*dma_buf->dma_chain), GFP_KERNEL);
    if (!dma_buf->dma_chain)
        goto fail_dmat;

    dma_buf->buf_length = len = range->length;
    sg_set_page(dma_buf->sg, pages[0],
        min((PAGE_SIZE - (range->start % PAGE_SIZE)), range->length),
        range->start % PAGE_SIZE);
    len -= dma_buf->sg[0].length;

    for (i = 1; i < num_pages; i++) {
        sg_set_page(dma_buf->sg + i, pages[i],
            min((uint64_t)PAGE_SIZE, len), 0);
        len -= dma_buf->sg[i].length;
    }

    if (len != 0) {
        pr_err("Remaining length %llu is not zero after creating SGL\n", len);
    }

    vfree(pages);
    pages = NULL;

    dma_buf->num_sg_entries = num_pages;
    dma_buf->num_used_sg_entries =
        dma_map_sg(&men->pdev->dev, dma_buf->sg, dma_buf->num_sg_entries, DMA_FROM_DEVICE);
    if (dma_buf->num_used_sg_entries == 0)
        goto fail_map;

    dma_buf->index = range->subnr;
    /* this will also free dma_buf->dma_chain on error */
    ret = men->create_buf(men, dma_buf, dummybuf);
    if (ret)
        goto fail_create;

    // TODO: Should the list name be set after the buffer was added to the list? Could be initialized here to NO_LIST.
    dma_buf->listname = FREE_LIST;
    INIT_LIST_HEAD(&dma_buf->node);
    ret = -EINVAL;

    buf_head = me_get_buf_head(men, range->headnr);
    if (buf_head == NULL)
        goto fail_bh;

    buf_head->bufs[range->subnr] = dma_buf;

    /* now everything is fine. Go and add this buffer to the free list */
    dma_chan = buf_head->chan;
    if (dma_chan != NULL) {
        unsigned long flags;

        spin_lock_irqsave(&dma_chan->listlock, flags);
        list_add_tail(&dma_buf->node, &dma_chan->free_list);
        dma_chan->free_count++;
        spin_unlock_irqrestore(&dma_chan->listlock, flags);
    }
    spin_unlock_bh(&men->buffer_heads_lock);

    return 0;

fail_bh:
    men_destroy_sb(men, dma_buf);
    return ret;

fail_create:
    dma_unmap_sg(&men->pdev->dev, dma_buf->sg, dma_buf->num_sg_entries, DMA_FROM_DEVICE);
fail_map:
    for (i = dma_buf->num_sg_entries - 1; i >= 0; i--) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
        unpin_user_page(sg_page(dma_buf->sg + i));
#else
        put_page(sg_page(dma_buf->sg + i));
#endif
    }
fail_dmat:
    kfree(dma_buf->sg);
fail_sg:
    kfree(dma_buf);
fail_dma_buf:
    if (pages) {
        for (i = num_pages - 1; i >= 0; i--) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
            unpin_user_page(pages[i]);
#else
            put_page(pages[i]);
#endif
        }
        vfree(pages);
    }
    return ret;
}

void
men_destroy_sb(struct siso_menable *men, struct menable_dmabuf *sb)
{
    int i;

    BUG_ON(in_interrupt());

    men->free_buf(men, sb);

    dma_unmap_sg(&men->pdev->dev, sb->sg, sb->num_sg_entries, DMA_FROM_DEVICE);
    for (i = sb->num_sg_entries - 1; i >= 0; i--) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
        unpin_user_page(sg_page(sb->sg + i));
#else
        put_page(sg_page(sb->sg + i));
#endif
    }
    kfree(sb->sg);
    kfree(sb);
}

/**
* men_free_userbuf - remove the DMA buffer from it's head
* @men: board buffer belongs to
* @db: DMA head buffer belongs to
* @index: buffer index
* returns: 0 on success, error code otherwise
*
* The caller must get and release &men->headlock if neccessary.
*/
int
men_free_userbuf(struct siso_menable *men, struct menable_dmahead *db,
                 long index)
{
    struct menable_dmabuf *sb;
    struct menable_dmachan *dc;
    unsigned long flags;

    if ((index < 0) || (index > db->num_sb))
        return -EINVAL;

    sb = db->bufs[index];
    if (sb == NULL)
        return -EINVAL;

    dc = db->chan;
    if (dc == NULL) {
        /* The channel is not active: nobody but us knows about the
        * buffer. Just kill it. */
        db->bufs[index] = NULL;
        return 0;
    }

    spin_lock_irqsave(&dc->chanlock, flags);
    spin_lock(&dc->listlock);
    if ((dc->state != MEN_DMA_CHAN_STATE_STOPPED) && (dc->state != MEN_DMA_CHAN_STATE_FINISHED) &&
        (sb->listname == HOT_LIST)) {
            /* The buffer is active, that means we would have to wait
            * until the board is finished with it. Users problem. */
            spin_unlock(&dc->listlock);
            spin_unlock_irqrestore(&dc->chanlock, flags);
            return -EBUSY;
    }

    if (sb->listname != NO_LIST)
        list_del(&sb->node);

    switch (sb->listname) {
        case FREE_LIST:
            dc->free_count--;
            break;
        case READY_LIST:
            dc->ready_count--;
            break;
        case GRABBED_LIST:
            dc->grabbed_count--;
            break;
        case NO_LIST:
            dc->locked_count--;
            break;
        case HOT_LIST:
            dc->hot_count--;
            break;
        default:
            BUG();
    }
    db->bufs[index] = NULL;
    spin_unlock(&dc->listlock);
    spin_unlock_irqrestore(&dc->chanlock, flags);

    return 0;
}

int
men_create_buf_head(struct siso_menable *men, const size_t maxsize,
                    const long subbufs)
{
    struct menable_dmahead *dma_head, *heads_entry;
    int next_id, ret;

    if (subbufs <= 0)
        return -EINVAL;

    ret = -ENOMEM;

    dma_head = kzalloc(sizeof(*dma_head), GFP_KERNEL);
    if (dma_head == NULL)
        goto err_bh_alloc;

    dma_head->bufs = kcalloc(subbufs, sizeof(*dma_head->bufs), GFP_KERNEL);
    if (!dma_head->bufs)
    	goto err_sb_alloc;

    dma_head->num_sb = subbufs;

    ret = men->create_dummybuf(men, &dma_head->dummybuf);
    if (ret)
        goto err_dummybuf;

    next_id = 0;
    INIT_LIST_HEAD(&dma_head->node);
    spin_lock_bh(&men->buffer_heads_lock);

    /* set next_id to 1 + highest id */
    list_for_each_entry(heads_entry, &men->buffer_heads_list, node) {
        if (heads_entry->id >= next_id)
            next_id = heads_entry->id + 1;
    }

    men->num_buffer_heads++;
    dma_head->id = next_id;
    list_add_tail(&dma_head->node, &men->buffer_heads_list);
    spin_unlock_bh(&men->buffer_heads_lock);

    return next_id;

err_dummybuf:
    kfree(dma_head->bufs);
err_sb_alloc:
    kfree(dma_head);
err_bh_alloc:
    return ret;
}

int
men_release_buf_head(struct siso_menable *men, struct menable_dmahead *bh)
{
    int r;

    if (bh->chan != NULL) {
        unsigned long flags;

        spin_lock_irqsave(&bh->chan->chanlock, flags);
        if (bh->chan->state == MEN_DMA_CHAN_STATE_ACTIVE) {
            r = -EBUSY;
        } else {
            r = 0;
            bh->chan->active = NULL;
        }
        spin_unlock_irqrestore(&bh->chan->chanlock, flags);
        if (r)
            return r;
    }
    list_del(&bh->node);
    men->num_buffer_heads--;
    return 0;
}

void
men_free_buf_head(struct siso_menable *men, struct menable_dmahead *bh)
{
    long i;

    BUG_ON(in_interrupt());

    for (i = 0; i < bh->num_sb; i++) {
        if (bh->bufs[i])
            men_destroy_sb(men, bh->bufs[i]);
    }

    men->free_dummybuf(men, &bh->dummybuf);
    kfree(bh->bufs);
    kfree(bh);
}

struct menable_dmabuf *
men_move_hot(struct menable_dmachan *dma_chan, const menable_timespec_t *ts)
{
    struct menable_dmabuf *sb;

    if (dma_chan->active == NULL) {
        /* Something in the IRQ reset did not block this one.
        * Flush them out. */
        dma_chan->lost_count++;
        dma_chan->transfer_todo = 0;
        return NULL;
    }

    /* TODO: [RKN] What might be the reason of hot_count being 0?
     *       Doesn't that mean that no buffers are queued and we should not
     *       get an interrupt at all? */
    if (dma_chan->hot_count != 0) {
        sb = list_first_entry(&dma_chan->hot_list, typeof(*sb), node);
        if (sb->index == -1) {
            /* this is the dummy buffer */
            list_del(&sb->node);
            dma_chan->lost_count++;
        } else {

            /* select list according to acquisition mode */
            unsigned int list_name;
            struct list_head * list;
            unsigned int * list_size;

            switch (dma_chan->mode) {

            case DMA_HANDSHAKEMODE: {
                /* buffer must be unlocked explicitly */
                list_name = GRABBED_LIST;
                list = &dma_chan->grabbed_list;
                list_size = &dma_chan->grabbed_count;
                break;
            }

            case DMA_SELECTIVEMODE: {
                /* explicit re-queuing required */
                list_name = FREE_LIST;
                list = &dma_chan->free_list;
                list_size = &dma_chan->free_count;
                break;
            }

            default: {
                /* buffer will be reuesed directly */
                list_name = READY_LIST;
                list = &dma_chan->ready_list;
                list_size = &dma_chan->ready_count;
                break;
            }

            } // end of switch

            /* move the buffer from HOT to selected list */
            list_move_tail(&sb->node, list);
            *list_size += 1;
            sb->listname = list_name;

            dma_chan->goodcnt++;
        }
        dma_chan->transfer_todo--;
        dma_chan->hot_count--;
        dma_chan->imgcnt++;
        dma_chan->latest_frame_number++;

        sb->timestamp = *ts;
        dma_sync_sg_for_cpu(&dma_chan->parent->pdev->dev, sb->sg, sb->num_sg_entries,
            dma_chan->direction);
    } else {
        WARN_ON(dma_chan->hot_count == 0);
        dev_err(&dma_chan->parent->dev, "[ERROR][ACQ] Received interrupt but there is no buffer in hot queue.\n");
        sb = NULL;
        dma_chan->lost_count++;
    }

    return sb;
}

struct menable_dmahead *
me_get_buf_head(struct siso_menable *men, const unsigned int num)
{
    struct menable_dmahead *res;

    // TODO: [RKN] Modify to either not lock or always lock + eventually add __acquires annotation for sparse
    spin_lock_bh(&men->buffer_heads_lock);
    list_for_each_entry(res, &men->buffer_heads_list, node) {
        if (res->id == num) {
            /* return without unlocking */
            return res;
        }
    }

    spin_unlock_bh(&men->buffer_heads_lock);
    return NULL;
}

struct menable_dmabuf *
me_get_sub_buf_by_head(struct menable_dmahead *head, const long bufidx)
{
    if (head == NULL)
        return NULL;

    if ((bufidx < 0) || (bufidx >= head->num_sb))
        return NULL;

    return head->bufs[bufidx];

}

struct menable_dmabuf *
    me_get_sub_buf(struct siso_menable *men, const unsigned int headnum,
    const long bufidx)
{
    struct menable_dmahead *head = me_get_buf_head(men, headnum);
    struct menable_dmabuf *res;

    if (!head)
        return NULL;

    res = me_get_sub_buf_by_head(head, bufidx);

    if (res == NULL)
        spin_unlock_bh(&men->buffer_heads_lock);

    return res;
}

void
men_dma_queue_max(struct menable_dmachan *dma_chan)
{
    // If we already have enough buffers queued, we do nothing.
    if (dma_chan->transfer_todo > dma_chan->hot_count) {

        struct menable_dmabuf *sb;

        if ((dma_chan->mode == DMA_HANDSHAKEMODE) && (dma_chan->ready_count == 0)) {

            if (dma_chan->hot_count == 0) {
                /* There are no buffers ready and also none active. Queue the dummy
                 * buffer to keep the DMA engine running. */
                WARN_ON(dma_chan->active == NULL);
                if (dma_chan->active) {
                    sb = &dma_chan->active->dummybuf;
                    INIT_LIST_HEAD(&sb->node);
                    dma_chan->parent->queue_sb(dma_chan, sb);
                    list_add(&sb->node, &dma_chan->hot_list);
                    dma_chan->hot_count++;
                }
            }
        } else {
            if ((dma_chan->mode != DMA_SELECTIVEMODE) && (dma_chan->ready_count == 0)) {
                // in all modes except selective mode, having no buffers ready here may be a problem.
                dev_warn(&dma_chan->parent->dev, "No buffers are ready for acquisition.");
            }

            const long long num_free_in_dma_fifo = dma_chan->parent->dma_fifo_length - dma_chan->hot_count;
            const long long num_max_bufs_for_transfer = dma_chan->transfer_todo - dma_chan->hot_count;
            long long num_bufs_to_queue = min3(num_max_bufs_for_transfer, (long long)dma_chan->ready_count, num_free_in_dma_fifo);

            DEV_DBG_BUFS(&dma_chan->parent->dev, "Queuing %lld buffers.\n", num_bufs_to_queue);
            while (num_bufs_to_queue--) {
                sb = list_first_entry(&dma_chan->ready_list, struct menable_dmabuf, node);

                dma_sync_sg_for_device(&dma_chan->parent->pdev->dev, sb->sg, sb->num_sg_entries, dma_chan->direction);

                dma_chan->parent->queue_sb(dma_chan, sb);

                list_move_tail(&sb->node, &dma_chan->hot_list);
                dma_chan->ready_count--;
                dma_chan->hot_count++;
                sb->listname = HOT_LIST;
                sb->frame_number = 0; // reset the frame number to 0
            }
            DEV_DBG_BUFS(&dma_chan->parent->dev, "Buffers in hot queue: %u\n", dma_chan->hot_count);
        }
    }
}

/*
 * Moves the oldest entry in the GRABBED list to NO_LIST (remains blocked)
 * and returns it.
 */
struct menable_dmabuf *
men_next_blocked(struct siso_menable *men, struct menable_dmachan *dc)
{
    struct menable_dmabuf *ret;

    if (dc->grabbed_count == 0) {
        ret = NULL;
    } else {
        ret = list_first_entry(&dc->grabbed_list, typeof(*ret), node);
        list_del(&ret->node);
        dc->grabbed_count--;
        dc->locked_count++;
        ret->listname = NO_LIST;
    }

    return ret;
}

/*
 * Moves all buffers from GRABBED_LIST to FREE_LIST (unblock), except the last
 * one which is moved into NO_LIST (remains blocked) and returned.
 */
struct menable_dmabuf *
men_last_blocked(struct siso_menable *men, struct menable_dmachan *dc)
{
    struct menable_dmabuf *ret;

    if (dc->grabbed_count == 0) {
        ret = NULL;
    } else {

        /* Move all but one buffer from GRABBED to FREE */
        while (dc->grabbed_count > 1) {
            ret = list_first_entry(&dc->grabbed_list,
            struct menable_dmabuf, node);
            list_move_tail(&ret->node, &dc->ready_list);
            dc->ready_count++;
            dc->grabbed_count--;
            ret->listname = READY_LIST;
        }

        /* Move last entry from GRABBED to NO_LIST, delete GRABBED list */
        ret = list_first_entry(&dc->grabbed_list, struct menable_dmabuf, node);
        list_del(&ret->node);
        dc->grabbed_count = 0;
        dc->locked_count++;
        ret->listname = NO_LIST;
    }

    return ret;
}

/*
 * Unlocks a buffer by moving it from GRABBED or NO_LIST to free.
 * If the buffer is in HOT or FREE, nothing happens.
 */
void
men_unblock_buffer(struct menable_dmachan *dc, struct menable_dmabuf *sb)
{
    if (sb == NULL)
        return;

    switch (sb->listname) {
        case HOT_LIST:
        case FREE_LIST:
        case READY_LIST:
            return; // TODO: Really no error handling?
        case GRABBED_LIST:
            sb->listname = READY_LIST;
            list_move_tail(&sb->node, &dc->ready_list);
            dc->grabbed_count--;
            dc->ready_count++;
            break;
        case NO_LIST:
            sb->listname = READY_LIST;
            INIT_LIST_HEAD(&sb->node);
            list_add_tail(&sb->node, &dc->ready_list);
            dc->locked_count--;
            dc->ready_count++;
            break;
        default:
            BUG();
    }

    /* If we are about to run out ouf queued buffers, queue some! */
    if (dc->hot_count <= 1 && dc->ready_count > 0) {
        men_dma_queue_max(dc);
    }
}
