/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef UIQ_H
#define UIQ_H

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include <lib/boards/peripheral_declaration.h>
#include <lib/uiq/uiq_defines.h>
#include <lib/uiq/uiq_base.h>
#include <lib/uiq/uiq_transfer_state.h>

struct siso_menable;
struct register_interface;

struct menable_uiq {
	/* Platform independent UIQ data */
	uiq_base base;

	struct device dev;
	struct siso_menable *parent;

	unsigned int cpltodo;  /* number of entries cpl still waits for */
	unsigned int irqack_offs; /* irq acknowledge register offset */
	unsigned char ackbit;  /* bit in irqack */
	unsigned long cpltimeout;  /* timeout for read */
	struct completion cpl;     /* wait for timeout */
	spinlock_t lock;
};

extern struct uiq_base * men_uiq_init(struct siso_menable *parent, int chan, unsigned int id,
		unsigned int type, unsigned int reg_offs, unsigned char burst);
extern int men_scale_uiq(uiq_base * uiq_base, const unsigned int len);
extern void men_uiq_remove(uiq_base * uiq_base);
extern void uiq_irq(uiq_base *uiq_base, uiq_timestamp *ts, bool *have_ts);
extern void men_del_uiqs(struct siso_menable *men, const unsigned int fpga);

extern uint32_t men_fetch_next_incoming_uiq_word(uiq_transfer_state * transfer_state, messaging_dma_declaration * msg_dma_decl, register_interface * register_interface, uint32_t data_register_address);
extern bool men_uiq_pop(uiq_base * uiq_base, uiq_timestamp *ts, bool *have_ts);
extern bool men_uiq_push(uiq_base * uiq_base);

extern struct class *menable_uiq_class;
extern struct device_attribute men_uiq_attributes[6];

#endif /* UIQ_H */
