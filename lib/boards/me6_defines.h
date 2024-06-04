/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_BOARDS_ME6_DEFINES_H
#define LIB_BOARDS_ME6_DEFINES_H

#include <multichar.h>
#include "../helpers/bits.h"

#define ME6_REG_BOARD_STATUS 0x0000 /**< Board status register */
#define GETFIRMWAREVERSION(x) (((x) & 0xff00) | (((x) >> 16) & 0xff))

#define ME6_REG_BOARD_ID 0x0001 /**< Board ID register */

#define ME6_REG_IRQ_STATUS 0x0003 /**< IRQ status register (only used in single-MSIX mode) */
#define ME6_IRQ_VEC_ALARMS 0x01 /**< Level-triggered interrupt sources */
#define ME6_IRQ_VEC_EVENT  0x02 /**< Events/UIQs/Messaging Channel */
#define ME6_IRQ_VEC_DMA_0  0x04 /**< DMA 0 */
#define ME6_IRQ_VEC_DMA_1  0x08 /**< DMA 1 */
#define ME6_IRQ_VEC_DMA_2  0x10 /**< DMA 2 */
#define ME6_IRQ_VEC_DMA_3  0x20 /**< DMA 3 */
#define ME6_IRQ_VEC_DMA_4  0x40 /**< DMA 4 */
#define ME6_IRQ_VEC_DMAFROMPC 0x80 /**< DMA from PC */
#define ME6_IRQ_VALID_MASK    0xff /**< Valid IRQ sources */

#define ME6_REG_IRQ_ALARMS_ENABLE 0x0004 /**< W: Level-triggered interrupt sources enable register */
#define ME6_REG_IRQ_ALARMS_STATUS 0x0004 /**< R: Level-triggered interrupt sources status register */
#define ME6_ALARM_OVERTEMP  0x1
#define ME6_ALARM_SOFTWARE  0x2
#define ME6_ABACUS_ALARM_PHY_0_MGM 0x4
#define ME6_ABACUS_ALARM_PHY_1_MGM 0x8
#define ME6_ABACUS_ALARM_PHY_2_MGM 0x10
#define ME6_ABACUS_ALARM_PHY_3_MGM 0x20
#define ME6_ABACUS_ALARM_POE_FAULT 0x40

#define ME6_REG_IRQ_SOFTWARE_TRIGGER 0x000D /**< W: Write '1' to trigger software interrupt */

#define ME6_REG_IRQ_EVENT_COUNT 0x0005 /**< Number of events */
#define ME6_IRQ_EVENT_OVERFLOW 0x1 /**< Overflow of event counter */
#define ME6_IRQ_EVENT_GET_COUNT(x) (((x) >> 1) & 0xffff)

#define ME6_REG_IRQ_DMA_0_COUNT 0x0007 /**< Number of pending DMA 0 interrupts */
#define ME6_REG_IRQ_DMA_1_COUNT 0x0008 /**< Number of pending DMA 1 interrupts */
#define ME6_REG_IRQ_DMA_2_COUNT 0x0009 /**< Number of pending DMA 2 interrupts */
#define ME6_REG_IRQ_DMA_3_COUNT 0x000A /**< Number of pending DMA 3 interrupts */
#define ME6_REG_IRQ_DMA_4_COUNT 0x000B /**< Number of pending DMA 4 interrupts */
#define ME6_REG_IRQ_DMA_5_COUNT 0x000C /**< Number of pending DMA 5 interrupts */
#define ME6_REG_IRQ_DMA_COUNT_FOR_CHANNEL_IDX(chan) (ME6_REG_IRQ_DMA_0_COUNT + (chan))
#define ME6_IRQ_DMA_OVERFLOW 0x1 /**< Overflow of DMA interrupt counter */
#define ME6_IRQ_DMA_GET_COUNT(x) (((x) >> 1) & 0xff)

#define ME6_REG_LED_CONTROL 0x0012

#define ME6_MAX_NUM_DMAS          6      /**< Maximum number of DMA channels */
#define ME6_REG_NUM_DMA_CHANNELS  0x0002 /**< Number of DMA channels */
#define ME6_REG_DMA_BASE          0x0110 /**< DMA engine base address */
#define ME6_DMA_CHAN_OFFSET       0x000A /**< Add n*offs to DMA_BASE for base of channel n*/

/* The following addresses are relative to channel base */
#define ME6_REG_DMA_CONTROL       0x0000 /**< W: DMA engine control register */
#define ME6_REG_DMA_STATUS        0x0000 /**< R: DMA engine status register  */
#define ME6_REG_DMA_SGL_ADDR_LOW  0x0001 /**< W: SGL FIFO low address */
#define ME6_REG_DMA_SGL_ADDR_HIGH 0x0002 /**< W: SGL FIFO high address */
#define ME6_REG_DMA_TYPE          0x0003 /**< R: DMA type */
#define ME6_REG_DMA_LENGTH        0x0004 /**< R: DMA transfer length FIFO */
#define ME6_REG_DMA_TAG           0x0005 /**< R: DMA tag FIFO */

#define ME6_DMA_CTRL_RESET  0x2 /**< Reset DMA engine */
#define ME6_DMA_CTRL_ENABLE 0x1 /**< Enable DMA engine */

#define ME6_DMA_STAT_RUNNING 0x4 /**< DMA engine is running */
#define ME6_DMA_STAT_TIMEOUT 100000

#define ME6_REG_DMA_FROM_PC_IRQ_ID 0x000E /**< Vector ID of DMA from PC */

#define ME6_REG_DMA_FROM_PC_CONTROL 0x012A /**< W: DMA from PC engine control register */
#define ME6_REG_DMA_FROM_PC_STATUS  0x012A /**< R: DMA from PC engine status register */
#define ME6_REG_DMA_FROM_PC_SGL_IF  0x012B /**< DMA from PC SGL interface */

/* Event data register */
#define ME6_REG_IRQ_EVENT_COUNT 0x0005
#define ME6_IRQ_EVENT_COUNT_OVERFLOW_BIT 0x1
#define ME6_IRQ_EVENT_GET_COUNT(x) (((x) >> 1) & 0xffff)
#define ME6_REG_IRQ_EVENT_DATA 0x0006
#define ME6_IRQ_EVENT_GET_PACKET_LENGTH(x) (((x) >> 16) & 0xffff)
#define ME6_IRQ_EVENT_GET_PACKET_TYPE(x) (((x) >> 8) & 0xff)
#define ME6_EVENT_PACKET_TYPE_NATIVE 0x0
#define ME6_EVENT_PACKET_TYPE_RAW_READ 0x1
#define ME6_EVENT_PACKET_TYPE_WRITE_DONE 0x2
#define ME6_IRQ_EVENT_GET_PACKET_SOURCE(x) ((x)&0xff)
#define ME6_IRQ_EVENT_GET_PACKET_ID(x) ((x)&0xffff)

/* Messaging DMA */
#define ME6_REG_IRQ_MESSAGING_COUNT     0x0005
#define ME6_REG_IRQ_MESSAGING_PROCESSED 0x0006
#define ME6_MESSAGING_COUNT_HAS_OVERFLOW(x) ((x & 0x1) == 0x1)
#define ME6_MESSAGING_GET_COUNT(x) (((x) >> 1) & 0x1ffff)

/* SPI controller */
#define ME6_REG_SPI_CONTROL 0x1002
#define ME6_SPI0_PEIPHERAL_ID MULTICHAR32('S', 'P', 'I', '0')

/* I2C controller */
#define ME6_REG_I2C_ADDRESS 0x1005
#define ME6_REG_I2C_WRITE   0x1006
#define ME6_REG_I2C_READ    0x1006

#define ME6_REG_I2C_1_ADDRESS 0x1003
#define ME6_REG_I2C_1_WRITE   0x1004
#define ME6_REG_I2C_1_READ    0x1004
#define ME6_REG_I2C_2_ADDRESS 0x1007
#define ME6_REG_I2C_2_WRITE   0x1008
#define ME6_REG_I2C_2_READ    0x1008
#define ME6_REG_I2C_3_ADDRESS 0x1009
#define ME6_REG_I2C_3_WRITE   0x100A
#define ME6_REG_I2C_3_READ    0x100A

#define ME6_FW_CLOCK_FREQ 300000000 /* 300 MHz */
#define ME6_I2C_FREQ         400000 /* 400 KHz */
#define ME6_I2C_FREQ_POE      10000 /*  10 KHz (POE/PSE on Abacus) */
#define ME6_I2C_FREQ_EXT     100000 /* 100 KHz */
#define ME6_I2C_NUM_SAFETY_WRITES 1
#define ME6_I2C0_PERIPHERAL_ID MULTICHAR32('I', '2', 'C', '0')
#define ME6_I2C1_PERIPHERAL_ID MULTICHAR32('I', '2', 'C', '1')
#define ME6_I2C2_PERIPHERAL_ID MULTICHAR32('I', '2', 'C', '2')
#define ME6_I2C3_PERIPHERAL_ID MULTICHAR32('I', '2', 'C', '3')
#define ME6_I2C4_PERIPHERAL_ID MULTICHAR32('I', '2', 'C', '4')

/* BoardStatus DNA not ready */
#define ME6_BOARD_STATUS_DNA_MAX_RETRIES 100
#define ME6_BOARD_STATUS_DNA_NOT_READY BIT(24)

/* DNA registers */
#define ME6_REG_FPGA_DNA_0 0x1012
#define ME6_REG_FPGA_DNA_1 0x1013
#define ME6_REG_FPGA_DNA_2 0x1014

/* JTAG registers  */
#define ME6_JTAG_CTR_REG   0x100B
#define ME6_JTAG_DEVICES_COUNTS   0x1
#define ME6_JTAG_PEIPHERAL_ID MULTICHAR32('J', 'T', 'A', 'G')

#endif /* LIB_BOARDS_ME6_DEFINES_H */
