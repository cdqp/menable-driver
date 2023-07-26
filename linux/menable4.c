/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "menable.h"

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "menable4.h"
#include "menable_ioctl.h"
#include "uiq.h"

#include "lib/controllers/spi_defines.h"

static DEVICE_ATTR(design_crc, 0660, men_get_des_val, men_set_des_val);

static ssize_t
me4_get_boardinfo(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct siso_menable *men = container_of(dev, struct siso_menable, dev);
	ssize_t ret = 0;
	int i;

	for (i = 0; i < 4; i++) {
		u32 tmp = men->register_interface.read(&men->register_interface, i);
		ssize_t r = sprintf(buf + ret, "0x%08x\n", tmp);

		if (r < 0)
			return r;

		ret += r;
	}

	return ret;
}

static DEVICE_ATTR(board_info, 0440, me4_get_boardinfo, NULL);

static struct lock_class_key me4_irqmask_lock;

static struct attribute *me4_attributes[3] = {
	&dev_attr_design_crc.attr,
	&dev_attr_board_info.attr,
	NULL
};

static struct attribute_group me4_attribute_group = {
        .attrs = me4_attributes
};

static const struct attribute_group *me4_attribute_groups[2] = {
	&me4_attribute_group,
	NULL
};

const struct attribute_group ** me4_init_attribute_groups(struct siso_menable *men)
{
	return me4_attribute_groups;
}

/**
 * me4_add_uiqs - scan for new UIQs in one FPGA
 * @men: board to scan
 * @fpga: fpga index to scan
 * @count: how many UIQs to add
 * @uiqoffs: register offset of the first UIQ
 *
 * This will keep all UIQs from lower FPGAs.
 */
static int
me4_add_uiqs(struct siso_menable *men, const unsigned int fpga,
		const unsigned int count, const unsigned int uiqoffs)
{
	uiq_base ** nuiqs;
	unsigned int fpga_idx;
	int ret;
	uint32_t uiqtype = men->register_interface.read(&men->register_interface, (fpga * ME4_FULLOFFSET) + ME4_IRQTYPE);

	WARN_ON(!men->design_changing);
	for (fpga_idx = fpga; fpga_idx < MAX_FPGAS; fpga_idx++)
		WARN_ON(men->uiqcnt[fpga_idx] != 0);

	nuiqs = kcalloc(fpga * MEN_MAX_UIQ_PER_FPGA + count,
			sizeof(*men->uiqs), GFP_KERNEL);

	if (nuiqs == NULL)
		return -ENOMEM;

	/* copy the uiq pointers of the FPGAs with a lower index then the target one */
	for (fpga_idx = 0; fpga_idx < fpga; fpga_idx++) {
		unsigned int j;
		for (j = 0; j < men->uiqcnt[fpga_idx]; j++) {
			nuiqs[fpga_idx * MEN_MAX_UIQ_PER_FPGA + j] =
					men->uiqs[fpga_idx * MEN_MAX_UIQ_PER_FPGA + j];
		}
	}

	/* Create the UIQs for the target FPGA */
	for (fpga_idx = 0; fpga_idx < count; fpga_idx++) {
		int chan = fpga * MEN_MAX_UIQ_PER_FPGA + fpga_idx;
		int t = ((uiqtype & (1 << (fpga_idx + ME4_IRQQ_LOW))) != 0) ? UIQ_TYPE_WRITE_LEGACY : UIQ_TYPE_READ;

		uiq_base * uiq_base = men_uiq_init(men, chan, 0, t, (uiqoffs / 4) + 2 * fpga_idx, 16);

		if (IS_ERR(uiq_base)) {
			int j;
			ret = PTR_ERR(uiq_base);

			for (j = fpga * MEN_MAX_UIQ_PER_FPGA; j < chan; j++)
				men_uiq_remove(nuiqs[j]);

			kfree(nuiqs);
			return ret;
		}

		struct menable_uiq *uiq;
		uiq = container_of(uiq_base, struct menable_uiq, base);

		uiq->irqack_offs = (fpga * ME4_FULLOFFSET) + ME4_IRQACK;
		uiq->ackbit = fpga_idx + ME4_IRQQ_LOW;

		nuiqs[chan] = uiq_base;
	}

	kfree(men->uiqs);
	men->uiqs = nuiqs;
	men->uiqcnt[fpga] = count;

	men->num_active_uiqs += count;

	return 0;
}

/**
 * me4_reset_vlink - reset link between bridge FPGA and upper FPGA
 * @men: board to reset
 * @upper: reset upper FPGA part or not
 * returns: 0 on success, error code else
 */
static int
me4_reset_vlink(struct siso_menable *men, const bool upper)
{
	uint32_t cplid;
	int i;
	//void __iomem *ME4_IFCONTROL = me4_fpga_control(men, 0) + ME4_IFCONTROL;
	//void __iomem *ME4_FULLOFFS + ME4_IFCONTROL = me4_fpga_control(men, 1) + ME4_IFCONTROL;

	/* multiple register accesses are here to
	 * ensure the value passes the register
	 * pipeline */

	men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 2);
	men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 0);
	men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 0);

	if (!upper)
		return 0;

	for (i = 0; i < 5; i++)
		men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 1);

	cplid = men->register_interface.read(&men->register_interface, ME4_FPGACONTROL) & 0xffff0000;
	men->register_interface.write(&men->register_interface, ME4_FULLOFFSET + ME4_IFCONTROL, cplid | 0x2);
	for (i = 0; i < 4; i++)
		men->register_interface.write(&men->register_interface, ME4_FULLOFFSET + ME4_IFCONTROL, cplid);

	for (i = ME4_PCIECONFIG0; i < ME4_PCIECONFIGMAX; i += 1) {
		uint32_t v = men->register_interface.read(&men->register_interface, i);
		men->register_interface.write(&men->register_interface, ME4_FULLOFFSET + i, v);
	}

	men->register_interface.write(&men->register_interface, ME4_FULLOFFSET + ME4_IFCONTROL, cplid | 0x1);

	return 0;
}

static int
me4_ioctl(struct siso_menable *men, const unsigned int cmd,
		const unsigned int size, unsigned long arg)
{
	switch (cmd) {
	case IOCTL_BOARD_INFO: {
		unsigned int a[4];
		int i;

		if (size != sizeof(a)) {
			warn_wrong_iosize(men, cmd, sizeof(a));
			return -EINVAL;
		}

		for (i = 0; i < ARRAY_SIZE(a); i++)
			a[i] = men->register_interface.read(&men->register_interface, i);
		if (copy_to_user((void __user *) arg,
				a, sizeof(a)))
			return -EFAULT;
		return 0;
	}
	case IOCTL_PP_CONTROL: {
		int ret;
		unsigned long flags;

		if (size != 0) {
			warn_wrong_iosize(men, cmd, 0);
			return -EINVAL;
		}

		spin_lock_irqsave(&men->designlock, flags);
		if (men->design_changing) {
			spin_unlock_irqrestore(&men->designlock, flags);
			return -EBUSY;
		}

		men->design_changing = true;
		spin_unlock_irqrestore(&men->designlock, flags);

        /* TODO: FPGA Master Management */
		switch (arg) {
		case 0:
			spin_lock_bh(&men->buffer_heads_lock);
			if (men->active_fpgas > 1) {
				men->register_interface.write(&men->register_interface, ME4_FULLOFFSET + ME4_IFCONTROL, 0);
			}
			men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 0);
			men_del_uiqs(men, 1);
			men->active_fpgas = 1;

			ret = men_alloc_dma(men, men->dmacnt[0]);
			break;
		case 1:
			/* DCM reset */
			men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 0x10);
			udelay(10);
			men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 0);
			men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 0);
			msleep(5);
			if (men->register_interface.read(&men->register_interface, ME4_IFCONTROL) & 0x10)
				return -EBUSY;

			ret = me4_reset_vlink(men, true);
			if (ret == 0) {
				men->active_fpgas = 2;

				ret = men_add_dmas(men);
				if (ret == 0) {
					unsigned int cnt = men->register_interface.read(&men->register_interface, ME4_FULLOFFSET + ME4_UIQCNT);
					unsigned int offs = men->register_interface.read(&men->register_interface, ME4_FULLOFFSET + ME4_FIRSTUIQ);
					ret = me4_add_uiqs(men, 1, cnt, offs);
				}
			}
			break;
		default:
			ret = -EINVAL;
		}

		spin_lock_irqsave(&men->designlock, flags);
		men->design_changing = false;

		spin_unlock_irqrestore(&men->designlock, flags);
		return ret;
	}
	case IOCTL_RESSOURCE_CONTROL:
	case IOCTL_GET_EEPROM_DATA:
	case IOCTL_DESIGN_SETTINGS:
		return -EINVAL;
	default:
		return -ENOIOCTLCMD;
	}
}

static void
me4_free_sgl(struct siso_menable *men, struct menable_dmabuf *sb)
{
	struct men_dma_chain *res = sb->dma_chain;
	dma_addr_t dma = sb->dma;

	while (res) {
		struct men_dma_chain *n;
		dma_addr_t ndma;

		n = res->next;
		ndma = (dma_addr_t) (le64_to_cpu(res->pcie4->next) & ~(3ULL));
		if (dma == ndma)
			break;
		dma_pool_free(men->sgl_dma_pool, res->pcie4, dma);
		kfree(res);
		res = n;
		dma = ndma;
	}
}

static void
me4_queue_sb(struct menable_dmachan *db, struct menable_dmabuf *sb)
{
    w64(db->parent, db->iobase + ME4_DMAMAXLEN, sb->buf_length / 4);
    w64(db->parent, db->iobase + ME4_DMAADDR, sb->dma);
}

static irqreturn_t
me4_irq(int irq, void *dev_id)
{
	uint32_t sr;
	struct siso_menable *men = dev_id;
	unsigned int fpga;
	unsigned char skipdma = 0;	/* DMA channels in lower FPGAs */
	unsigned char skipuiq = 0;	/* UIQs in lower FPGAs */
	menable_timespec_t ts;
	ktime_t timeout;
	bool have_ts = false;

	if (pci_channel_offline(men->pdev))
		return IRQ_HANDLED;

	sr = men->register_interface.read(&men->register_interface, ME4_IRQSTATUS);

	if (unlikely(sr == 0))
		return IRQ_NONE;

	for (fpga = 0; fpga < men->active_fpgas; fpga++) {
		int dma;
		uint32_t badmask = 0;
		uint32_t st;	/* tmp status */

		if (unlikely(sr == 0xffffffff)) {
			dev_warn(&men->dev, "IRQ status register %i read returned -1\n", fpga);
			men->register_interface.write(&men->register_interface, (fpga * ME4_FULLOFFSET) + ME4_IRQENABLE, 0);
			men->register_interface.write(&men->register_interface, (fpga * ME4_FULLOFFSET) + ME4_IRQACK, 0xffffffff);
			return IRQ_HANDLED;
		}

		spin_lock(&men->d4->irqmask_lock);
		badmask = sr & ~men->d4->irq_wanted[fpga];
		if (unlikely(badmask != 0)) {
			men->register_interface.write(&men->register_interface, (fpga * ME4_FULLOFFSET) + ME4_IRQENABLE, men->d4->irq_wanted[fpga]);
			men->register_interface.write(&men->register_interface, (fpga * ME4_FULLOFFSET) + ME4_IRQACK, badmask);
			sr &= men->d4->irq_wanted[fpga];
		}
		spin_unlock(&men->d4->irqmask_lock);

		for (dma = 0; dma < men->dmacnt[fpga]; dma++) {
			struct menable_dmachan *db;
			struct menable_dma_wait *waitstr;
			uint32_t dma_count;

			if ((sr & (0x1 << dma)) == 0)
				continue;

			if (!have_ts) {
				have_ts = true;
				menable_get_ts(&ts);
			}

			db = men_dma_channel(men, dma + skipdma);
			BUG_ON(db == NULL);

			spin_lock(&db->chanlock);
			men->register_interface.write(&men->register_interface, db->irqack, 1 << db->ackbit);
			dma_count = men->register_interface.read(&men->register_interface, db->iobase + ME4_DMACOUNT);
			spin_lock(&db->listlock);
			if (unlikely(db->active == NULL)) {
				for (int i = dma_count - db->imgcnt; i > 0; i--) {
					uint32_t tmp = men->register_interface.read(&men->register_interface, db->iobase + ME4_DMALENGTH);
					tmp = men->register_interface.read(&men->register_interface, db->iobase + ME4_DMATAG);
					db->lost_count++;
				}
				spin_unlock(&db->listlock);
				spin_unlock(&db->chanlock);
				continue;
			}

			uint32_t delta = dma_count - db->imgcnt;
			for (int i = 0; i < delta; ++i) {
				struct menable_dmabuf *sb = men_move_hot(db, &ts);
				uint32_t len = men->register_interface.read(&men->register_interface, db->iobase + ME4_DMALENGTH);
				uint32_t tag = men->register_interface.read(&men->register_interface, db->iobase + ME4_DMATAG);

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
			spin_unlock(&db->chanlock);
		}

		st = (sr & 0x3fff0000);
		if (st != 0) {
			uint32_t bit;
			uiq_timestamp uiq_ts;
			bool have_uiq_ts = false;

			for (bit = ME4_IRQQ_LOW; (bit <= ME4_IRQQ_HIGH) && st; bit++) {
				if (st & (1 << bit)) {
					uiq_irq(men->uiqs[bit - ME4_IRQQ_LOW + skipuiq], &uiq_ts, &have_uiq_ts);
					st ^= (1 << bit);
				}
			}
		}

		if (sr & ME4_CHAIN_MASK) {
			WARN_ON(fpga == men->active_fpgas - 1);

			if (likely(fpga < men->active_fpgas - 1)) {
				sr = men->register_interface.read(&men->register_interface, ((fpga + 1) * ME4_FULLOFFSET) + ME4_IRQSTATUS);
			} else {
				sr = 0;
			}

			men->register_interface.write(&men->register_interface, (fpga * ME4_FULLOFFSET) + ME4_IRQACK, ME4_CHAIN_MASK);

			skipdma += men->dmacnt[fpga];
			skipuiq += men->uiqcnt[fpga];
		} else {
			break;
		}
	}

	return IRQ_HANDLED;
}

static void
me4_abortdma(struct siso_menable *men, struct menable_dmachan *dc)
{
	men->register_interface.write(&men->register_interface, dc->iobase + ME4_DMACTRL, 2);
	(void) men->register_interface.read(&men->register_interface, dc->iobase + ME4_DMACTRL);
	men->register_interface.write(&men->register_interface, dc->iobase + ME4_DMACTRL, 0);
	(void) men->register_interface.read(&men->register_interface, dc->iobase + ME4_DMACTRL);
}

static void
me4_stopdma(struct siso_menable *men, struct menable_dmachan *dc)
{
	uint32_t irqreg;
	unsigned long flags;
	unsigned char fpga;

	irqreg = men->register_interface.read(&men->register_interface, dc->irqenable);
	irqreg &= ~(1 << dc->enablebit);
	men->register_interface.write(&men->register_interface, dc->irqenable, irqreg);

	men->register_interface.write(&men->register_interface, dc->iobase + ME4_DMACTRL, 0);

	spin_lock_irqsave(&men->d4->irqmask_lock, flags);
	fpga = dc->fpga;
	men->d4->irq_wanted[fpga] &= ~(1 << dc->enablebit);
	while ((fpga > 0) && (men->d4->irq_wanted[fpga] == 0)) {
		fpga--;
		men->d4->irq_wanted[fpga] &= ~ME4_CHAIN_MASK;
	}
	spin_unlock_irqrestore(&men->d4->irqmask_lock, flags);
}

static int
me4_create_userbuf(struct siso_menable *men, struct menable_dmabuf *db, struct menable_dmabuf *dummybuf)
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

		if ((idx == ARRAY_SIZE(cur->pcie4->addr) - 1) &&
						(i + 1 < db->num_used_sg_entries)) {
			dma_addr_t next;

			cur->next = kzalloc(sizeof(*cur->next), GFP_USER);
			if (!cur->next)
				goto fail;

			cur->next->pcie4 = dma_pool_alloc(men->sgl_dma_pool,
					GFP_USER, &next);
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
	me4_free_sgl(men, db);
	return -ENOMEM;
fail_pcie:
	kfree(db->dma_chain);
	return -ENOMEM;
}

static int
me4_create_dummybuf(struct siso_menable *men, struct menable_dmabuf *db)
{
	struct men_dma_chain *cur;
	int i;

	db->index = -1;
	db->dma_chain = kzalloc(sizeof(*db->dma_chain), GFP_KERNEL);
	if (!db->dma_chain)
		goto fail_dmat;

	db->dma_chain->pcie4 = dma_pool_alloc(men->sgl_dma_pool, GFP_USER, &db->dma);
	if (!db->dma_chain->pcie4)
		goto fail_pcie;

	memset(db->dma_chain->pcie4, 0, sizeof(*db->dma_chain->pcie4));

	cur = db->dma_chain;

	for (i = 0; i < ARRAY_SIZE(cur->pcie4->addr); i++)
		cur->pcie4->addr[i] = cpu_to_le64(men->d4->dummypage_dma + 0x1);

	cur->pcie4->next = cpu_to_le64(db->dma + 0x2);

	db->buf_length = -1;

	return 0;

fail_pcie:
	kfree(db->dma_chain);
fail_dmat:
	return -ENOMEM;
}

static void
me4_destroy_dummybuf(struct siso_menable *men, struct menable_dmabuf *db)
{
	dma_pool_free(men->sgl_dma_pool, db->dma_chain->pcie4, db->dma);
	kfree(db->dma_chain);
}

static void
me4_exit(struct siso_menable *men)
{
    free_irq(men->pdev->irq, men);
    dmam_pool_destroy(men->sgl_dma_pool);
	dma_free_coherent(&men->pdev->dev, PCI_PAGE_SIZE, men->d4->dummypage, men->d4->dummypage_dma);
	kfree(men->uiqs);
	kfree(men->d4);
}

static unsigned int
me4_query_dma(struct siso_menable *men, const unsigned int fpga)
{
	uint32_t u;

	BUG_ON(fpga >= men->active_fpgas);

	u = men->register_interface.read(&men->register_interface, (fpga * ME4_FULLOFFSET) + ME4_NUMDMA);
	if (unlikely(u == 0xffffffff)) {
		dev_warn(&men->dev,
			"Reading DMACNT from FPGA %i failed\n", fpga);
		u = 0;
	} else {
		dev_dbg(&men->dev, "%i DMA channels detected in FPGA %i\n",
				u, fpga);
	}

	return u;
}

static int
me4_startdma(struct siso_menable *men, struct menable_dmachan *dmac)
{
	uint32_t tmp, dir;
	unsigned long flags;
	unsigned char fpga;

	me4_abortdma(men, dmac);

	dir = (dmac->direction == DMA_TO_DEVICE) ? 2 : 1;

	tmp = men->register_interface.read(&men->register_interface, dmac->iobase + ME4_DMATYPE);
	if (!(tmp & dir))
		return -EACCES;
	men->register_interface.write(&men->register_interface, dmac->iobase + ME4_DMATYPE, dir);

	/* clear IRQ */
	men->register_interface.write(&men->register_interface, dmac->irqack, 1 << dmac->ackbit);

	dmac->imgcnt = men->register_interface.read(&men->register_interface, dmac->iobase + ME4_DMACOUNT);

	men_dma_queue_max(dmac);

	spin_lock_irqsave(&men->d4->irqmask_lock, flags);
	men->d4->irq_wanted[dmac->fpga] |= (1 << dmac->enablebit);
	men->register_interface.write(&men->register_interface, dmac->irqenable, men->d4->irq_wanted[dmac->fpga]);
	for (fpga = 0; fpga < dmac->fpga; fpga++) {
		if ((men->d4->irq_wanted[fpga] & ME4_CHAIN_MASK) == 0) {
			men->d4->irq_wanted[fpga] |= ME4_CHAIN_MASK;
			men->register_interface.write(&men->register_interface, (fpga * ME4_FULLOFFSET) + ME4_IRQENABLE, men->d4->irq_wanted[fpga]);
		}
	}
	spin_unlock_irqrestore(&men->d4->irqmask_lock, flags);

	men->register_interface.write(&men->register_interface, dmac->iobase + ME4_DMAACTIVE, 1);
	men->register_interface.read(&men->register_interface, dmac->iobase + ME4_DMAACTIVE);
	men->register_interface.write(&men->register_interface, dmac->iobase + ME4_DMAACTIVE, 0);
	men->register_interface.read(&men->register_interface, dmac->iobase + ME4_DMAACTIVE);

	men->register_interface.write(&men->register_interface, dmac->iobase + ME4_DMACTRL, 1);
	men->register_interface.read(&men->register_interface, dmac->iobase + ME4_DMACTRL);

	return 0;
}

static void
me4_dmabase(struct siso_menable *men, struct menable_dmachan *dc)
{
	unsigned int relative_number = dc->number;
	unsigned int i;

	for (i = 0; i < dc->fpga; i++)
		relative_number -= men->dmacnt[i];

	dc->iobase = (dc->fpga * ME4_FULLOFFSET) + ME4_DMAOFFS + (ME4_DMASZ * relative_number);
	dc->irqack = (dc->fpga * ME4_FULLOFFSET) + ME4_IRQACK;
    dc->ackbit = relative_number;
	dc->irqenable = (dc->fpga * ME4_FULLOFFSET) + ME4_IRQENABLE;
	dc->enablebit = dc->ackbit;
}

static void
me4_stopirq(struct siso_menable *men)
{
	unsigned int i;

	for (i = men->active_fpgas; i > 0; i--) {
		unsigned int skipuiq = MEN_MAX_UIQ_PER_FPGA * (i - 1);
		int j;

		for (j = 0; j < men->uiqcnt[i]; j++) {
			if (men->uiqs[j + skipuiq] == NULL)
				continue;
			men->uiqs[j + skipuiq]->is_running = false;
		}

		men->register_interface.write(&men->register_interface, ((i - 1) * ME4_FULLOFFSET) + ME4_IRQENABLE, 0);
		men->register_interface.write(&men->register_interface, ((i - 1) * ME4_FULLOFFSET) + ME4_IRQACK, 0xffffffff);
		men->register_interface.write(&men->register_interface, ((i - 1) * ME4_FULLOFFSET) + ME4_IFCONTROL, 0);
	}
	men->active_fpgas = 1;

	for (i = 0; i < men->uiqcnt[0]; i++) {
		if (men->uiqs[i] == NULL)
			continue;
		men->uiqs[i]->is_running = false;
	}
}

static void
me4_startirq(struct siso_menable *men)
{
	uint32_t mask = ((1 << men->uiqcnt[0]) - 1) << ME4_IRQQ_LOW;
	unsigned int i;

	for (i = 1; i < MAX_FPGAS; i++)
		men->d4->irq_wanted[i] = 0;
	men->d4->irq_wanted[0] = mask;
	men->register_interface.write(&men->register_interface, ME4_IRQACK, 0xffffffff);
	men->register_interface.write(&men->register_interface, ME4_IRQENABLE, mask);
}

/**
 * me4_reset_core - reset state machines near the PCIe core
 * @men: board to reset
 *
 * This will reset the state machines and logic directly connected to the
 * PCIe core.
 */
static void
me4_reset_core(struct siso_menable *men)
{
	int i;

	men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 0xa);
	men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 0xa);
	for (i = 0; i < 4; i++)
		men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 0x8);
	for (i = 0; i < 6; i++)
		men->register_interface.write(&men->register_interface, ME4_IFCONTROL, 0);

	me4_reset_vlink(men, false);
}

static void
me4_cleanup(struct siso_menable *men)
{
	unsigned long flags;

	spin_lock_irqsave(&men->designlock, flags);
	men->design_changing = true;
	spin_unlock_irqrestore(&men->designlock, flags);

	men_del_uiqs(men, 1);
	memset(men->desname, 0, men->deslen);

	spin_lock_irqsave(&men->designlock, flags);
	men->design_changing = false;
	spin_unlock_irqrestore(&men->designlock, flags);
}

static struct controller_base *
me4_get_controller(struct siso_menable * men, uint32_t peripheral) {
	if (peripheral == ME4_SPI_PEIPHERAL_ID)
		return upcast(&men->d4->spi_controller);

    return NULL;
}

int
me4_probe(struct siso_menable *men)
{
	int ret = -ENOMEM;
	unsigned int uiqoffs;
	unsigned int uiqcnt;

	men->d4 = kzalloc(sizeof(*men->d4), GFP_KERNEL);
	if (men->d4 == NULL)
		goto fail;

    men->register_interface.activate(&men->register_interface);

    men->d4->men = men;

	spin_lock_init(&men->d4->irqmask_lock);
	lockdep_set_class(&men->d4->irqmask_lock, &me4_irqmask_lock);

	men->active_fpgas = 1;

	me4_reset_core(men);
	me4_stopirq(men);

	if (dma_set_mask(&men->pdev->dev, DMA_BIT_MASK(64))) {
		dev_err(&men->dev, "Failed to set DMA mask\n");
		goto fail_mask;
	}
	dma_set_coherent_mask(&men->pdev->dev, DMA_BIT_MASK(64));
	men->sgl_dma_pool = dmam_pool_create("me4_sgl", &men->pdev->dev,
			sizeof(struct me4_sgl), 128, PCI_PAGE_SIZE);
	if (!men->sgl_dma_pool) {
		dev_err(&men->dev, "Failed to allocate DMA pool\n");
		goto fail_pool;
	}

	ret = -ENOMEM;
	men->d4->dummypage = dma_alloc_coherent(&men->pdev->dev, PCI_PAGE_SIZE,
	&men->d4->dummypage_dma, GFP_ATOMIC);
	if (men->d4->dummypage == NULL) {
    	dev_err(&men->dev, "Failed to allocate dummy page\n");
		goto fail_dummy;
	}

	men->dma_fifo_length = ME4_DMA_FIFO_DEPTH;

	men->create_dummybuf = me4_create_dummybuf;
	men->free_dummybuf = me4_destroy_dummybuf;
	men->create_buf = me4_create_userbuf;
	men->free_buf = me4_free_sgl;
	men->startdma = me4_startdma;
	men->abortdma = me4_abortdma;
	men->stopdma = me4_stopdma;
	men->stopirq = me4_stopirq;
	men->startirq = me4_startirq;
	men->ioctl = me4_ioctl;
	men->exit = me4_exit;
	men->cleanup = me4_cleanup;
	men->query_dma = me4_query_dma;
	men->dmabase = me4_dmabase;
	men->queue_sb = me4_queue_sb;
	men->get_controller = me4_get_controller;

	men->config = men->register_interface.read(&men->register_interface, ME4_CONFIG);
	men->config_ex = men->register_interface.read(&men->register_interface, ME4_CONFIG_EX);

	uiqcnt = men->register_interface.read(&men->register_interface, ME4_UIQCNT);
	uiqoffs = men->register_interface.read(&men->register_interface, ME4_FIRSTUIQ);

	if ((uiqcnt == 0) && (uiqoffs == 0)) {
		/* old firmware versions did not provide this */
		uiqcnt = ME4_IRQQ_HIGH - ME4_IRQQ_LOW + 1;
		uiqoffs = ME4_IRQQUEUE;
	}

	if (uiqcnt != 0) {
		ret = me4_add_uiqs(men, 0, uiqcnt, uiqoffs);
		if (ret != 0)
			goto fail_uiqs;
	}

	/* No VA event UIQs on mE4 -> set to invalid value */
	men->first_va_event_uiq_idx = UINT32_MAX;

	men->desname = men->d4->design_name;
	men->deslen = sizeof(men->d4->design_name);

	ret = request_irq(men->pdev->irq, me4_irq, IRQF_SHARED, DRIVER_NAME, men);
	if (ret) {
		dev_err(&men->dev, "Failed to request interrupt\n");
		goto fail_irq;
	}

	spi_bs_single_data_transport_init(&men->d4->spi_controller,
	                                  &men->register_interface,
									  &men->flash_lock,
									  ME4_SPI_CONTROL_REG);

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
    dma_free_coherent(&men->pdev->dev, PCI_PAGE_SIZE, men->d4->dummypage, men->d4->dummypage_dma);
fail_dummy:
    dmam_pool_destroy(men->sgl_dma_pool);
fail_pool:
fail_mask:
	kfree(men->d4);
fail:
	return ret;
}
