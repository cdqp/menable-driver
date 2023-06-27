/******************************************************************************
* Copyright (C) 2014-2015 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* XILINX CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/
/*****************************************************************************/
/**
*
* @file mcap_lib.h
*  MCAP Interface Library support header file
*
******************************************************************************/

#ifndef LIB_MCAP_MCAP_LIB_H
#define LIB_MCAP_MCAP_LIB_H

#include "../os/types.h"

/* Register Definitions */
#define MCAP_EXT_CAP_HEADER	0x00
#define MCAP_VEND_SPEC_HEADER	0x04
#define MCAP_FPGA_JTAG_ID	0x08
#define MCAP_FPGA_BIT_VERSION	0x0C
#define MCAP_STATUS		0x10
#define MCAP_CONTROL		0x14
#define MCAP_DATA		0x18
#define MCAP_READ_DATA_0	0x1C
#define MCAP_READ_DATA_1	0x20
#define MCAP_READ_DATA_2	0x24
#define MCAP_READ_DATA_3	0x28

#define MCAP_CTRL_MODE_MASK		(1 << 0)
#define MCAP_CTRL_REG_READ_MASK		(1 << 1)
#define MCAP_CTRL_RESET_MASK		(1 << 4)
#define MCAP_CTRL_MOD_RESET_MASK	(1 << 5)
#define MCAP_CTRL_IN_USE_MASK		(1 << 8)
#define MCAP_CTRL_DESIGN_SWITCH_MASK	(1 << 12)
#define MCAP_CTRL_DATA_REG_PROT_MASK	(1 << 16)

#define MCAP_STS_ERR_MASK		(1 << 0)
#define MCAP_STS_EOS_MASK		(1 << 1)
#define MCAP_STS_REG_READ_CMP_MASK	(1 << 4)
#define MCAP_STS_REG_READ_COUNT_MASK	(7 << 5)
#define MCAP_STS_FIFO_OVERFLOW_MASK	(1 << 8)
#define MCAP_STS_FIFO_OCCUPANCY_MASK	(15 << 12)
#define MCAP_STS_CFG_MCAP_REQ_MASK	(1 << 24)

/* Maximum FIFO Depth */
#define MCAP_FIFO_DEPTH		16

/* PCIe Extended Capability Id */
#define MCAP_EXT_CAP_ID		0xB

/* Error Values */
#define EMCAPREQ	120
#define EMCAPRESET	121
#define EMCAPMODRESET	122
#define EMCAPFULLRESET	123
#define EMCAPWRITE	124
#define EMCAPREAD	125
#define EMCAPCFG	126
#define EMCAPBUSWALK	127
#define EMCAPCFGACC	128

#define EMCAP_EOS_RETRY_COUNT 10
#define EMCAP_EOS_LOOP_COUNT 100
#define EMCAP_NOOP_VAL	0x2000000

/* Bitfile Type */
#define EMCAP_CONFIG_FILE	 0
#define EMCAP_PARTIALCONFIG_FILE 1

struct pci_config_interface;

/* MCAP Device Information */
struct mcap_dev {
	struct pci_config_interface *ci;
	unsigned int reg_base;
	uint32_t is_multiplebit;
};

/* Function Prototypes */
int MCapLibInit(struct mcap_dev *mdev, struct pci_config_interface *ci);
void MCapDumpRegs(struct mcap_dev *mdev);
void MCapDumpReadRegs(struct mcap_dev *mdev);
int MCapReset(struct mcap_dev *mdev);
int MCapModuleReset(struct mcap_dev *mdev);
int MCapFullReset(struct mcap_dev *mdev);
int MCapReadRegisters(struct mcap_dev *mdev, uint32_t *data);
int MCapRegWrite(struct mcap_dev *mdev, int offset, uint32_t value);
int MCapRegRead(struct mcap_dev *mdev, int offset, uint32_t *value);
int MCapWritePartialBitStream(struct mcap_dev *mdev, uint32_t *data, int len, uint8_t bswap);
int MCapWriteBitStream(struct mcap_dev *mdev, uint32_t *data, int len, uint8_t bswap);

int IsResetSet(struct mcap_dev *mdev);
int IsModuleResetSet(struct mcap_dev *mdev);
int IsConfigureMCapReqSet(struct mcap_dev *mdev);
int IsErrSet(struct mcap_dev *mdev);
int IsRegReadComplete(struct mcap_dev *mdev);
int IsFifoOverflow(struct mcap_dev *mdev);
int GetRegReadCount(struct mcap_dev *mdev);

#endif
