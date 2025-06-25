/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_BOARDS_ME5_DEFINES_H
#define LIB_BOARDS_ME5_DEFINES_H

#include <multichar.h>

/* SPI controller */
#define ME5_IRONMAN_SPI_CONTROL_REG 0x1000
#define ME5_IRONMAN_SPI_SELECT_REG  0x1001
#define ME5_ABACUS_SPI_CONTROL_REG  0x1002
#define ME5_SPI0_PEIPHERAL_ID MULTICHAR32('S', 'P', 'I', '0')

/* I2C controller */
#define ME5_I2C_ADDRESS_REG        0x1005
#define ME5_I2C_WRITE_REG          0x1006
#define ME5_IRONMAN_I2C_READ_REG   0x1007
#define ME5_MARATHON_I2C_READ_REG  0x1006
#define ME5_ABACUS_I2C_READ_REG    0x1006
#define ME5_FW_CLOCK_FREQ 125000000 /* 125 MHz */
#define ME5_I2C_FREQ         400000 /* 400 KHz */
#define ME5_I2C_FREQ_POE      10000 /*  10 KHz (POE/PSE on Abacus) */
#define ME5_I2C_FREQ_EXT     100000 /* 100 KHz */
#define ME5_I2C_NUM_SAFETY_WRITES 1
#define ME5_I2C0_PERIPHERAL_ID MULTICHAR32('I', '2', 'C', '0')
#define ME5_I2C1_PERIPHERAL_ID MULTICHAR32('I', '2', 'C', '1')

/* BoardStatus DNA not ready */
#define ME5_BOARD_STATUS_DNA_MAX_RETRIES 100
#define ME5_BOARD_STATUS_DNA_NOT_READY 0x10000

/* DNA registers */
#define ME5_REG_FPGA_DNA_0 0x1012
#define ME5_REG_FPGA_DNA_1 0x1013
#define ME5_REG_FPGA_DNA_2 0x1014

/* BPI registers  */
#define ME5_MARATHON_BPI_ADDRESS_REG    0x1003
#define ME5_MARATHON_BPI_WRITE_READ_REG 0x1004
#define ME5_MARATHON_BPI_BANK_SELECTION_STATUS_REG  0x100D
#define ME5_BPI_PEIPHERAL_ID MULTICHAR32('B', 'P', 'I', '0')

/* JTAG registers  */
#define ME5_JTAG_CTR_REG   0x100B
#define ME5_JTAG_DEVICES_COUNTS   0x2
#define ME5_JTAG_PEIPHERAL_ID MULTICHAR32('J', 'T', 'A', 'G')
#endif /* LIB_BOARDS_ME5_DEFINES_H */
