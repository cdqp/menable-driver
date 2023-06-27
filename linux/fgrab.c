/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include <linux/uaccess.h>
#include <linux/delay.h>
#include "menable.h"
#include "menable_ioctl.h"
#include "debugging_macros.h"

int
fg_start_transfer(struct siso_menable *men, struct fg_ctrl *fgr, const size_t tsize)
{
	struct menable_dmachan *dma_chan;
	struct menable_dmahead *buf_head;
	struct menable_dmabuf *buf;
	unsigned long flags;
	int ret = -EINVAL;

#ifdef ENABLE_DEBUG_MSG
        printk(KERN_INFO "[%d]: fg_start_transfer\n", current->parent->pid);
#endif

	if (fgr->transfer_todo == 0) {
		dev_warn(&men->dev, "Attempt to start acquisition of zero frames.");
		return ret;
	}

	buf_head = me_get_buf_head(men, fgr->head);
	if (buf_head == NULL) {
		dev_err(&men->dev, "No buffer head at index %d", fgr->head);
		return ret;
	}

	if ((fgr->start_buf >= buf_head->num_sb) || (fgr->start_buf < 0))
		goto out_err;

	buf = buf_head->bufs[fgr->start_buf];
	if (buf == NULL) {
		dev_err(&men->dev, "No buffer at startbuffer index %ld", fgr->start_buf);
		goto out_err;
	}

	if ((tsize != 0) && (buf->buf_length < tsize))
		goto out_err;

	dma_chan = men_dma_channel(men, fgr->chan);

	if (dma_chan == NULL) {
		ret = -ECHRNG;
		goto out_err;
	}

	if (fgr->dma_dir > 1) {
		dev_err(&men->dev, "Wrong DMA direction to start transfer.");
		goto out_err;
	}

	/* The channel is already running. Don't touch
	 * any of it's state variables */
	if (dma_chan->state == MEN_DMA_CHAN_STATE_ACTIVE) {
		spin_unlock_bh(&men->buffer_heads_lock);
		return -EBUSY;
	}

	if (fgr->mode == DMA_HANDSHAKEMODE
	        && fgr->dma_dir == MEN_DMA_DIR_CPU_TO_DEVICE) {
		ret = -EINVAL;
		goto out_err;
	}

	if (dma_chan->state == MEN_DMA_CHAN_STATE_IN_SHUTDOWN) {
		int dma_done_was_cancelled;
		struct menable_dmahead *new_buf_head;

		/* TODO The locking here looks suspicious.
		 *      - different locks are used (buffer_heads_lock + chanlock)
		 *      - two ways of locking are used (_irqsave + _bh)
		 *      - unlocks happen different functions than the corresponding locks
		 *      - there are checks that seem to aim for race condition handling
		 *        in between release of one lock and acquisition of another one
		 *
		 *      See if this can be simplified by using fewer (and better) locks
		 *      and making some checks obsolete.
		 */
		/* channel is waiting for shutdown */
		spin_unlock_bh(&men->buffer_heads_lock);
		dma_done_was_cancelled = cancel_work_sync(&dma_chan->dwork);

		new_buf_head = me_get_buf_head(men, fgr->head);

		if (new_buf_head == NULL) {
			WARN_ON(dma_done_was_cancelled);
			return -EBUSY;
		}

		spin_lock_irqsave(&dma_chan->chanlock, flags);
		if (dma_done_was_cancelled && (dma_chan->state == MEN_DMA_CHAN_STATE_IN_SHUTDOWN)) {
			men_dma_clean_sync(dma_chan);
			if (dma_chan->active) {
				dma_chan->active->chan = NULL;
				dma_chan->active = NULL;
			}
		}

		if ((dma_chan->state != MEN_DMA_CHAN_STATE_FINISHED)
		        && (dma_chan->state != MEN_DMA_CHAN_STATE_STOPPED)) {
			spin_unlock_irqrestore(&dma_chan->chanlock, flags);
			spin_unlock_bh(&men->buffer_heads_lock);
			return -EBUSY;
		}

		/*
		 * Someone has changed the buffer while we were waiting
		 * TODO: [RKN] This smells like dirty locking. Analyze and improve!
		 */
		if (new_buf_head != buf_head) {
			spin_unlock_irqrestore(&dma_chan->chanlock, flags);
			spin_unlock_bh(&men->buffer_heads_lock);
			return -EBUSY;
		}
	} else {
		spin_lock_irqsave(&dma_chan->chanlock, flags);
	}

	if (fgr->transfer_todo == -1)
		dma_chan->transfer_todo = LLONG_MAX;
	else
		dma_chan->transfer_todo = fgr->transfer_todo;

	dma_chan->timeout = (fgr->timeout < INT_MAX - 1) ? fgr->timeout : 0;

	dma_chan->direction = (fgr->dma_dir == MEN_DMA_DIR_DEVICE_TO_CPU)
	                        ? PCI_DMA_FROMDEVICE
	                        : PCI_DMA_TODEVICE;

	INIT_WORK(&dma_chan->dwork, men_dma_done_work);
	dma_chan->mode = fgr->mode;

	dma_chan->latest_frame_number = 0;

	DEV_DBG_ACQ(&men->dev, "Starting DMA.\n");
	ret = men_start_dma(dma_chan, buf_head, fgr->start_buf);

	spin_unlock_irqrestore(&dma_chan->chanlock, flags);
	spin_unlock_bh(&men->buffer_heads_lock);

	DEV_DBG_ACQ(&men->dev, "Started acquisition, mode: %s, frames: %lld, timeout: %lu\n",
	            get_acqmode_name(dma_chan->mode), dma_chan->transfer_todo, dma_chan->timeout);

	return ret;
out_err:
	spin_unlock_bh(&men->buffer_heads_lock);
	return ret;
}

void
men_stop_dma_locked(struct menable_dmachan *dma_chan)
{
	spin_lock(&dma_chan->listlock);
	if (dma_chan->transfer_todo > 0) {
		dma_chan->transfer_todo = 0;
		spin_unlock(&dma_chan->listlock);
		dma_chan->state = MEN_DMA_CHAN_STATE_IN_SHUTDOWN;
		dma_chan->parent->stopdma(dma_chan->parent, dma_chan);
		schedule_work(&dma_chan->dwork);
	} else {
		spin_unlock(&dma_chan->listlock);
	}
	/* Delete it here: abort has finished, the device will _not_ touch
	 * the buffer anymore. Make sure that there are no more references
	 * on it so we can free it. */
	if (dma_chan->active)
		dma_chan->active->chan = NULL;
	dma_chan->active = NULL;
}

void
men_stop_dma(struct menable_dmachan *dc)
{
	DEV_DBG_ACQ(&dc->parent->dev, "Stopping acquisition on channel %u.\n", dc->number);

	unsigned long flags;

	spin_lock_bh(&dc->parent->buffer_heads_lock);
	spin_lock_irqsave(&dc->chanlock, flags);
	if (dc->state == MEN_DMA_CHAN_STATE_ACTIVE)
		men_stop_dma_locked(dc);
	spin_unlock_irqrestore(&dc->chanlock, flags);
	spin_unlock_bh(&dc->parent->buffer_heads_lock);
}
