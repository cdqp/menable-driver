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
* @file mcap_lib.c
* MCAP Interface Library functions
*
******************************************************************************/

#include "../os/byteorder.h"
#include "../os/print.h"
#include "../os/time.h"
#include "../os/errno.h"
#include "../pci/pci_config_interface.h"
#include "../helpers/dbg.h"

#include "mcap_lib.h"

/* Library Specific Definitions */

#define MCAP_LOOP_COUNT	1000000

#define MCAP_SYNC_DWORD	0xFFFFFFFF
#define MCAP_SYNC_BYTE0 ((MCAP_SYNC_DWORD & 0xFF000000) >> 24)
#define MCAP_SYNC_BYTE1 ((MCAP_SYNC_DWORD & 0x00FF0000) >> 16)
#define MCAP_SYNC_BYTE2 ((MCAP_SYNC_DWORD & 0x0000FF00) >> 8)
#define MCAP_SYNC_BYTE3 ((MCAP_SYNC_DWORD & 0x000000FF) >> 0)

int MCapRegWrite(struct mcap_dev * mdev, int offset, uint32_t value)
{
	int result = mdev->ci->write32(mdev->ci, mdev->reg_base + offset, value);
	if (result != 0) {
		pr_err("Failed to write MCAP register %d (error %d)\n", offset, result);
	}
	return result;
}

int MCapRegRead(struct mcap_dev * mdev, int offset, uint32_t * value)
{
	int result = mdev->ci->read32(mdev->ci, mdev->reg_base + offset, value);
	if (result != 0) {
		pr_err("Failed to read MCAP register %d (error %d)\n", offset, result);
	}
	return result;
}

int IsResetSet(struct mcap_dev *mdev)
{
    uint32_t v = 0;
	int result = MCapRegRead(mdev, MCAP_CONTROL, &v);
	return result == 0 ? ((v & MCAP_CTRL_RESET_MASK) != 0 ? 1 : 0) : 0;
}

int IsModuleResetSet(struct mcap_dev *mdev)
{
    uint32_t v = 0;
	int result = MCapRegRead(mdev, MCAP_CONTROL, &v);
	return result == 0 ? ((v & MCAP_CTRL_MOD_RESET_MASK) != 0 ? 1 : 0) : 0;
}

int IsConfigureMCapReqSet(struct mcap_dev *mdev)
{
    uint32_t v = 0;
	int result = MCapRegRead(mdev, MCAP_STATUS, &v);
	return result == 0 ? ((v & MCAP_STS_CFG_MCAP_REQ_MASK) != 0 ? 1 : 0) : 0;
}

int IsErrSet(struct mcap_dev *mdev)
{
    uint32_t v = 0;
	int result = MCapRegRead(mdev, MCAP_STATUS, &v);
	return result == 0 ? ((v & MCAP_STS_ERR_MASK) != 0 ? 1 : 0) : 0;
}

int IsRegReadComplete(struct mcap_dev *mdev)
{
    uint32_t v = 0;
	int result = MCapRegRead(mdev, MCAP_STATUS, &v);
	return result == 0 ? ((v & MCAP_STS_REG_READ_CMP_MASK) != 0 ? 1 : 0) : 0;
}

int IsFifoOverflow(struct mcap_dev *mdev)
{
    uint32_t v = 0;
	int result = MCapRegRead(mdev, MCAP_STATUS, &v);
	return result == 0 ? ((v & MCAP_STS_FIFO_OVERFLOW_MASK) != 0 ? 1 : 0) : 0;
}

int GetRegReadCount(struct mcap_dev *mdev)
{
    uint32_t v = 0;
	int result = MCapRegRead(mdev, MCAP_STATUS, &v);
	return result == 0 ? (v & MCAP_STS_REG_READ_COUNT_MASK) >> 5 : 0;
}

static int MCapClearRequestByConfigure(struct mcap_dev *mdev, uint32_t *restore)
{
    uint32_t set;
	int loop = MCAP_LOOP_COUNT;

	MCapRegRead(mdev, MCAP_CONTROL, restore);
	set = *restore;

	if (IsConfigureMCapReqSet(mdev)) {
		/* Set 'Mode' and 'In Use by PCIe' bits */
		set |= (MCAP_CTRL_MODE_MASK | MCAP_CTRL_IN_USE_MASK);
		MCapRegWrite(mdev, MCAP_CONTROL, set);

		do {
			if (!(IsConfigureMCapReqSet(mdev)))
				break;
		} while (loop--);

		if (!loop) {
			pr_err("Failed to clear MCAP Request by config bit\n");
			MCapRegWrite(mdev, MCAP_CONTROL, *restore);
			return -EMCAPREQ;
		}
	}

	DBG_STMT(pr_debug("Request by Configure bit cleared!!\n"));

	return 0;
}

static int CheckForCompletion(struct mcap_dev *mdev)
{
	unsigned long retry_count = 0;
	uint32_t sr, i;

	MCapRegRead(mdev, MCAP_STATUS, &sr);
	while (!(sr & MCAP_STS_EOS_MASK)) {

		udelay(2);
		for (i = 0; i < EMCAP_EOS_LOOP_COUNT; i++) {
			MCapRegWrite(mdev, MCAP_DATA, EMCAP_NOOP_VAL);
		}
		MCapRegRead(mdev, MCAP_STATUS, &sr);

		// TODO: Xilinx suggests a timeout of 1 second. Implement a time based break here.
		retry_count++;
		if (retry_count > EMCAP_EOS_RETRY_COUNT) {
			pr_err("The MCAP EOS bit did not assert after programming the bitstream\n");
			return -EMCAPREQ;
		}
	}
	return 0;
}

int MCapWritePartialBitStream(struct mcap_dev *mdev, uint32_t *data, int len, uint8_t bswap)
{
    uint32_t set, restore;
	int err, count = 0, i;

	if (!data || !len) {
		pr_err("Invalid Arguments\n");
		return -EMCAPWRITE;
	}

	err = MCapClearRequestByConfigure(mdev, &restore);
	if (err)
		return err;

	if (IsErrSet(mdev) || IsRegReadComplete(mdev) ||
		IsFifoOverflow(mdev)) {
		pr_err("Failed to initialize configuring FPGA\n");
		MCapRegWrite(mdev, MCAP_CONTROL, restore);
		return -EMCAPWRITE;
	}

	/* Set 'Mode', 'In Use by PCIe' and 'Data Reg Protect' bits */
	MCapRegRead(mdev, MCAP_CONTROL, &set);
	set |= MCAP_CTRL_MODE_MASK | MCAP_CTRL_IN_USE_MASK |
		MCAP_CTRL_DATA_REG_PROT_MASK;

	/* Clear 'Reset', 'Module Reset' and 'Register Read' bits */
	set &= ~(MCAP_CTRL_RESET_MASK | MCAP_CTRL_MOD_RESET_MASK |
		 MCAP_CTRL_REG_READ_MASK | MCAP_CTRL_DESIGN_SWITCH_MASK);

	MCapRegWrite(mdev, MCAP_CONTROL, set);

	/* Write Data */
	if (!bswap) {
		for (count = 0; count < len; count++)
			MCapRegWrite(mdev, MCAP_DATA, data[count]);
	} else {
		for (count = 0; count < len; count++)
			MCapRegWrite(mdev, MCAP_DATA, swab32(data[count]));
	}

	for (i = 0 ; i < EMCAP_EOS_LOOP_COUNT; i++) {
		MCapRegWrite(mdev, MCAP_DATA, EMCAP_NOOP_VAL);
	}

	if (IsErrSet(mdev) || IsFifoOverflow(mdev)) {
		pr_err("Failed to write bitstream\n");
		MCapRegWrite(mdev, MCAP_CONTROL, restore);
		MCapFullReset(mdev);
		return -EMCAPWRITE;
	}

	if (!mdev->is_multiplebit) {
		pr_info("A partial reconfiguration clear file was loaded without a partial reconfiguration file.\n");
		pr_info("As result the MCAP Control register was not restored to its original value.\n");
	}

	return 0;
}

int MCapWriteBitStream(struct mcap_dev *mdev, uint32_t *data, int len, uint8_t bswap)
{
    uint32_t set, restore;
	int err, count = 0;

	if (!data || !len) {
		pr_err("Invalid arguments\n");
		return -EMCAPWRITE;
	}

	err = MCapClearRequestByConfigure(mdev, &restore);
	if (err)
		return err;

	if (IsErrSet(mdev) || IsRegReadComplete(mdev) ||
		IsFifoOverflow(mdev)) {
		pr_err("Failed to initialize configuring FPGA\n");
		MCapRegWrite(mdev, MCAP_CONTROL, restore);
		return -EMCAPWRITE;
	}

	if (!mdev->is_multiplebit) {
		/* Set 'Mode', 'In Use by PCIe' and 'Data Reg Protect' bits */
		MCapRegRead(mdev, MCAP_CONTROL, &set);
		set |= MCAP_CTRL_MODE_MASK | MCAP_CTRL_IN_USE_MASK |
			MCAP_CTRL_DATA_REG_PROT_MASK;

		/* Clear 'Reset', 'Module Reset' and 'Register Read' bits */
		set &= ~(MCAP_CTRL_RESET_MASK | MCAP_CTRL_MOD_RESET_MASK |
			 MCAP_CTRL_REG_READ_MASK | MCAP_CTRL_DESIGN_SWITCH_MASK);

		MCapRegWrite(mdev, MCAP_CONTROL, set);
	}

	/* Write Data */
	if (!bswap) {
		for (count = 0; count < len; count++) { 
			MCapRegWrite(mdev, MCAP_DATA, data[count]);
		}	
	} else {
		for (count = 0; count < len; count++){ 
			MCapRegWrite(mdev, MCAP_DATA, swab32(data[count]));
		}	
	}

	/* Check for Completion */
	err = CheckForCompletion(mdev);
	if (err)
		return -EMCAPCFG;

	if (IsErrSet(mdev) || IsFifoOverflow(mdev)) {
		pr_err("Failed to write bitstream\n");
		MCapRegWrite(mdev, MCAP_CONTROL, restore);
		MCapFullReset(mdev);
		return -EMCAPWRITE;
	}

	/* Enable PCIe BAR reads/writes in the PCIe hardblock */
	restore |= MCAP_CTRL_DESIGN_SWITCH_MASK;

	MCapRegWrite(mdev, MCAP_CONTROL, restore);

	return 0;
}

int MCapLibInit(struct mcap_dev *mdev, struct pci_config_interface *ci)
{
	mdev->ci = ci;
	mdev->is_multiplebit = 0;

	/* Get the MCAP Register base */
	mdev->reg_base = mdev->ci->find_ext_cap_address(mdev->ci, MEN_PCIE_CAP_ID_VENDOR_SPECIFIC);
	/* TODO: Verify the VSEC ID of the vendor specific capability? */
	if (mdev->reg_base == 0) {
		pr_err("MCAP Extended Capability not found\n");
		return -EIO;
	}

	DBG_STMT(pr_debug("MCAP Extended Capability found at offset 0x%x\n", mdev->reg_base));

	return 0;
}

int MCapReset(struct mcap_dev *mdev)
{
	uint32_t set, restore;
	int err;

	err = MCapClearRequestByConfigure(mdev, &restore);
	if (err)
		return err;

	/* Set 'Mode', 'In Use by PCIe' and 'Reset' bits */
	MCapRegRead(mdev, MCAP_CONTROL, &set);
	set |= MCAP_CTRL_MODE_MASK | MCAP_CTRL_IN_USE_MASK |
		MCAP_CTRL_RESET_MASK;
	MCapRegWrite(mdev, MCAP_CONTROL, set);

	if (IsErrSet(mdev) || !(IsResetSet(mdev))) {
		pr_err("Failed to Reset\n");
		MCapRegWrite(mdev, MCAP_CONTROL, restore);
		return -EMCAPRESET;
	}

	MCapRegWrite(mdev, MCAP_CONTROL, restore);
	pr_info("Reset Done!!\n");

	return 0;
}

int MCapModuleReset(struct mcap_dev *mdev)
{
	uint32_t set, restore;
	int err;

	err = MCapClearRequestByConfigure(mdev, &restore);
	if (err)
		return err;

	/* Set 'Mode', 'In Use by PCIe' and 'Module Reset' bits */
	MCapRegRead(mdev, MCAP_CONTROL, &set);
	set |= MCAP_CTRL_MODE_MASK | MCAP_CTRL_IN_USE_MASK |
		MCAP_CTRL_MOD_RESET_MASK;
	MCapRegWrite(mdev, MCAP_CONTROL, set);

	if (IsErrSet(mdev) || !(IsModuleResetSet(mdev))) {
		pr_err("Failed to Reset Module\n");
		MCapRegWrite(mdev, MCAP_CONTROL, restore);
		return -EMCAPMODRESET;
	}

	MCapRegWrite(mdev, MCAP_CONTROL, restore);
	DBG_STMT(pr_debug("Module Reset Done!!\n"));

	return 0;
}

int MCapFullReset(struct mcap_dev *mdev)
{
	uint32_t set, restore;
	int err;

	err = MCapClearRequestByConfigure(mdev, &restore);
	if (err)
		return err;

	/* Set 'Mode', 'In Use by PCIe' and 'Module Reset' bits */
	MCapRegRead(mdev, MCAP_CONTROL, &set);
	set |= MCAP_CTRL_MODE_MASK | MCAP_CTRL_IN_USE_MASK |
		MCAP_CTRL_RESET_MASK | MCAP_CTRL_MOD_RESET_MASK;
	MCapRegWrite(mdev, MCAP_CONTROL, set);

	if (IsErrSet(mdev) || !(IsModuleResetSet(mdev)) ||
		!(IsResetSet(mdev))) {
		pr_err("Failed to Full Reset\n");
		MCapRegWrite(mdev, MCAP_CONTROL, restore);
		return -EMCAPFULLRESET;
	}

	MCapRegWrite(mdev, MCAP_CONTROL, restore);
	DBG_STMT(pr_debug("Full Reset Done!!\n"));

	return 0;
}

static int MCapReadDataRegisters(struct mcap_dev *mdev, uint32_t *data)
{
	uint32_t set, restore, read_cnt;
	int err;

	if (!data) {
		pr_err("Invalid Arguments\n");
		return -EMCAPREAD;
	}

	err = MCapClearRequestByConfigure(mdev, &restore);
	if (err)
		return err;

	/* Set 'Mode', 'In Use by PCIe' and 'Data Reg Protect' bits */
	MCapRegRead(mdev, MCAP_CONTROL, &set);
	set |= MCAP_CTRL_MODE_MASK | MCAP_CTRL_IN_USE_MASK |
		MCAP_CTRL_REG_READ_MASK;

	/* Clear 'Reset', 'Module Reset' and 'Register Read' bits */
	set &= ~(MCAP_CTRL_RESET_MASK | MCAP_CTRL_MOD_RESET_MASK);

	MCapRegWrite(mdev, MCAP_CONTROL, set);
	read_cnt = GetRegReadCount(mdev);

	if (!(read_cnt) || !(IsRegReadComplete(mdev))) {
		MCapRegWrite(mdev, MCAP_CONTROL, restore);
		return EMCAPREAD;
	}

	if (IsErrSet(mdev) || IsFifoOverflow(mdev)) {
		pr_err("Read Register Set Configuration Failed\n");
		MCapRegWrite(mdev, MCAP_CONTROL, restore);
		return -EMCAPREAD;
	}

	switch (read_cnt) {
	case 7: case 6: case 5: case 4:
		MCapRegRead(mdev, MCAP_READ_DATA_3, &data[3]);
		/* Fall-through */
	case 3:
		MCapRegRead(mdev, MCAP_READ_DATA_2, &data[2]);
		/* Fall-through */
	case 2:
		MCapRegRead(mdev, MCAP_READ_DATA_1, &data[1]);
		/* Fall-through */
	case 1:
		MCapRegRead(mdev, MCAP_READ_DATA_0, &data[0]);
		break;
	}

	MCapRegWrite(mdev, MCAP_CONTROL, restore);
	DBG_STMT(pr_debug("Read Data Registers Complete!\n"));

	return 0;
}

void MCapDumpReadRegs(struct mcap_dev *mdev)
{
	uint32_t data[4];
	uint32_t status;

	status = MCapReadDataRegisters(mdev, data);
	if (status == EMCAPREAD)
		return;

	if (status) {
		pr_err("Failed Reading Registers.\n");
		pr_err("This may be due to inappropriate FPGA configuration.");
		return;
	}
	pr_info("Register Read Data 0:\t0x%08x\n", data[0]);
	pr_info("Register Read Data 1:\t0x%08x\n", data[1]);
	pr_info("Register Read Data 2:\t0x%08x\n", data[2]);
	pr_info("Register Read Data 3:\t0x%08x\n", data[3]);
}

void MCapDumpRegs(struct mcap_dev *mdev)
{
	uint32_t v = 0;
	MCapRegRead(mdev, MCAP_EXT_CAP_HEADER, &v);
	pr_info("Extended Capability:\t0x%08x\n", v);
	MCapRegRead(mdev, MCAP_VEND_SPEC_HEADER, &v);
	pr_info("Vendor Specific Header:\t0x%08x\n", v);
	MCapRegRead(mdev, MCAP_FPGA_JTAG_ID, &v);
	pr_info("FPGA JTAG ID:\t\t0x%08x\n", v);
	MCapRegRead(mdev, MCAP_FPGA_BIT_VERSION, &v);
	pr_info("FPGA Bit-Stream Version:0x%08x\n", v);
	MCapRegRead(mdev, MCAP_STATUS, &v);
	pr_info("Status:\t\t\t0x%08x\n", v);
	MCapRegRead(mdev, MCAP_CONTROL, &v);
	pr_info("Control:\t\t0x%08x\n", v);
	MCapRegRead(mdev, MCAP_DATA, &v);
	pr_info("Data:\t\t\t0x%08x\n", v);

	MCapDumpReadRegs(mdev);
}
