/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef _LIB_UIQ_UIQ_DEFINES_H_
#define _LIB_UIQ_UIQ_DEFINES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <lib/helpers/bits.h>

typedef enum uiq_protocol
{
    UIQ_PROTOCOL_RAW,
    UIQ_PROTOCOL_LEGACY,
    UIQ_PROTOCOL_VA_EVENT
} uiq_protocol;

/**
 * TODO: [RKN] Read and write are ambiguous terms for the UIQs, since the driver
 *             reads from the device and writes to the user or vice versa. So
 *             maybe we should rename the types to sth. like "BOARD_TO_USER" or "USER_TO_BOARD".
 */
typedef enum uiq_type {
    UIQ_TYPE_READ,         /**< Read queue (further protocol selection is done according to the packet type: 0x00 -> event protocol, others -> raw data) *//**< UIQ_TYPE_READ */
    UIQ_TYPE_WRITE_RAW,    /**< Write Queue without driver level protocol handling (raw data) */                                                           /**< UIQ_TYPE_WRITE_RAW */
    UIQ_TYPE_WRITE_LEGACY, /**< Write Queue with driver handling for legacy serial data pipeline (CLSerial, Abacus Slave SPI, etc.) */                     /**< UIQ_TYPE_WRITE_LEGACY */
    UIQ_TYPE_WRITE_CXP,    /**< Write Queue with driver handling for CXP packet pipeline */                                                                /**< UIQ_TYPE_WRITE_CXP */
} uiq_type;

#define UIQ_TYPE_IS_READ(type) (type == UIQ_TYPE_READ)
#define UIQ_TYPE_IS_WRITE(type) (type != UIQ_TYPE_READ)

#define UIQ_CONTROL_EOT         0x00010000  /* the FIFO will be empty after this value */
#define UIQ_CONTROL_DATALOSS    0x00020000  /* data was lost before this value */
#define UIQ_CONTROL_INSERT_TS   0x00040000  /* insert timestamp after this word */
#define UIQ_CONTROL_INVALID     0x80000000  /* the current value is invalid and must be discarded */

#define UIQ_CONTROL_IS_END_OF_TRANSMISSION(word) ARE_BITS_SET(word, UIQ_CONTROL_EOT)
#define UIQ_CONTROL_IS_INVALID(word)             ARE_BITS_SET(word, UIQ_CONTROL_INVALID)
#define UIQ_CONTROL_SHALL_INSERT_TIMESTAMP(word) ARE_BITS_SET(word, UIQ_CONTROL_INSERT_TS)

#define UIQ_NUM_PAYLOAD_BITS 16 /* The number of payload bits in decorated words (legacy and VA event data) */

#define UIQ_CXP_HEADER_PACKET_LENGTH(x)     ((x) & 0xffff) // extracts the length information from a packet header
#define UIQ_CXP_HEADER_CRC_ERROR_FLAG            0x100000
#define UIQ_CXP_HEADER_TAGGED_PACKET_FLAG        0x200000
#define UIQ_CXP_HEADER_RX_BUFFER_OVERFLOW_FLAG   0x400000
#define UIQ_CXP_HEADER_RX_PKT_FIFO_OVERFLOW_FLAG 0x800000

#define UIQ_CXP_SOP_K_WORD            0xfbfbfbfb
#define UIQ_CXP_COMMAND_PACKET        0x02020202
#define UIQ_CXP_TAGGED_COMMAND_PACKET 0x05050505
#define UIQ_CXP_EOP_K_WORD            0xfdfdfdfd

/**
 * Struct to hold timestamp data in a format that can
 * easily be pushed into a UIQ. This is not necessarily 
 * a monotonically increasing value.
 * 
 * The decoding of the format happens in the Runtime.
 */
typedef struct uiq_timestamp {
    uint32_t data[4];
} uiq_timestamp;

#ifdef __cplusplus
}
#endif

#endif /* Include Guard */
