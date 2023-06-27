/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef MENABLE4_H
#define MENABLE4_H

#include <linux/spinlock.h>

#include "menable.h"

#include "lib/controllers/spi_bs_single_controller.h"

#define ME4_CONFIG      0x0000
#define ME4_CONFIG_EX   0x0001
#define ME4_NUMDMA      0x0002
#define ME4_IRQSTATUS   0x0100
#define ME4_IRQACK      0x0102
#define ME4_IRQTYPE     0x0104
#define ME4_IRQENABLE   0x0106

#define ME4_DMAOFFS 0x0110
#define ME4_DMASZ   0x0010

#define ME4_DMACTRL     0x0000
#define ME4_DMAADDR     0x0002
#define ME4_DMAACTIVE   0x0004
#define ME4_DMALENGTH   0x0006
#define ME4_DMACOUNT    0x0008
#define ME4_DMAMAXLEN   0x000a
#define ME4_DMATYPE     0x000c
#define ME4_DMATAG      0x000e

#define ME4_IRQQUEUE    0x0080
#define ME4_IRQQ_LOW    16
#define ME4_IRQQ_HIGH   29

#define ME4_FULLOFFSET     0x2000
#define ME4_IFCONTROL      0x0004
#define ME4_UIQCNT         0x0006
#define ME4_FIRSTUIQ       0x0007
#define ME4_FPGACONTROL    0x1010
#define ME4_PCIECONFIG0    0x0010
#define ME4_PCIECONFIGMAX  0x001f

/* set when IRQs from next higher FPGA are set */
#define ME4_CHAIN_MASK 0x40000000

#define ME4_DMA_FIFO_DEPTH	1LL

#define ME4_SPI_PEIPHERAL_ID ASCII_CHAR4_TO_INT('S','P','I','0')
#define ME4_SPI_CONTROL_REG 0x1000

struct menable_uiq;

struct me4_sgl {
	__le64 addr[7];
	__le64 next;
} __attribute__ ((packed));

struct me4_data {
    struct siso_menable * men;

	void *dummypage;
	dma_addr_t dummypage_dma;
	uint32_t irq_wanted[MAX_FPGAS];	/* protected by irqmask_lock */
	uint32_t design_crc;
	char design_name[65];	/* user supplied name of design (for IPC) */
	spinlock_t irqmask_lock;	/* to protect IRQ enable register */
	struct spi_bs_single_controller spi_controller;
};

#endif
