/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef JTAG_CONTROLLER_H_
#define JTAG_CONTROLLER_H_

#include "../controllers/controller_base.h"
#include "../fpga/register_interface.h"
#include "../helpers/type_hierarchy.h"


#ifdef __cplusplus
extern "C" {
#endif

    enum JtagControllerBits {
        JTAG_DEFAULT_VALUE = 0x03c00000,
        JTAG_DATA_MASK = 0x0000ffff,
        JTAG_TMS_nTDO = 0x00010000,
        JTAG_ENABLE = 0x00020000,
        JTAG_BITCOUNT_MASK = 0x003c0000,
        JTAG_BITCOUNT_SHIFT = 18,
        JTAG_PRESCALER_MASK = 0x03c00000,
        JTAG_PRESCALER_SHIFT = 22,
        JTAG_PRESCALER_BASE_FREQUENCY = 125000000,
        JTAG_PRESCALER_MAX_VALUE = 0xf,
        JTAG_DONE_TOGGLE   = 0x00020000,
        JTAG_CTRL_ACTIVATE = 0x04000000,
    };

    enum ErrorCodes {
        JTAG_ERROR = -1,  /**< An internal error occurred */
        JTAG_ERROR_INVALID_POINTER = -2,  /**< A pointer passed as parameter was either NULL, or memory could not be accessed */
        JTAG_ERROR_INVALID_SIZE = -3,  /**< The size of memory passed by pointer was invalid */
        JTAG_ERROR_INVALID_RWFLAGS = -4,  /**< The flags passed were invalid for the type of port the handle refers to, or not allowed in the combination given, or plain invalid */
        JTAG_ERROR_IO_FAILURE = -5,  /**< A general input/output error occurred */
        JTAG_ERROR_TIMEOUT = -6,  /**< The operation could not be completed within the time specified */
        JTAG_ERROR_NO_DATA = -7,  /**< No data is available */
        JTAG_ERROR_FREQUENCY_TOO_LOW = -8,  /**< The requested frequency was too low */
        JTAG_ERROR_FREQUENCY_TOO_HIGH = -9,  /**< The requested frequency was too high */
        JTAG_ERROR_DATA_LENGTH_MISMATCH = -10,  /**< TMS data length does not match TDI data length on RAW send */
    };

    enum JtagTransmitionFlags {
        LENGTH_IN_BYTES         = 0x1, /**< transfer length is number of bytes to read or write */
        LENGTH_IN_BITS          = 0x2, /**< transfer length is number of bits to read or write */
        WAIT_ON_READ            = 0x0, /**< Wait on read if no write has been issued, or not enough data is available */
        SEND_ZEROES_ON_READ     = 0x20, /**< Send 0 bits on read if not enough data is available */
        SEND_ONES_ON_READ       = 0x40, /**< Send 1 bits on read if not enough data is available */
        READ_MODE_MASK          = 0x60, /**< Read mode mask */
        RAW_MODE                = 0x80, /**< Write data to JTAG shift registers */
        STATE_IDLE              = 0x100, /**< Transmit in IDLE state */
        STATE_SHIFT_DR          = 0x200, /**< Transmit in SHIFT-DR state */
        STATE_SHIFT_IR          = 0x300, /**< Transmit in SHIFT_IR state */
        STATE_RESET             = 0x400, /**< Transmit in STATE_RESET state */
        STATE_EXIT              = 0x800, /**< Transmit in STATE_EXIT state */
        STATE_INVALID           = 0x1000, /**< Transmit in STATE_INVALID state */
        STATE_MASK              = 0x1F00, /**< Transmit state mask */
        LSB_FIRST               = 0x2000, /**< Send and receive data starting with least significant bit */
        WRITE_TMS_DATA          = 0x4000, /**< Write data to the TMS shift register */
        SET_DATA_LENGTH         = 0x8000,
        SET_FREQUENCY           = 0x10000,
    };

    struct jtag_controller {
        DERIVE_FROM(controller_base);

        int (*write)(struct jtag_controller* ctrl, const void* data, unsigned int length, unsigned int flags);
        int (*read)(struct jtag_controller* ctrl, void* data, unsigned int length, unsigned int flags);
        int (*set_state)(struct jtag_controller* ctrl, enum JtagTransmitionFlags state);
        int (*set_frequency)(struct jtag_controller* ctrl);
        int (*get_frequency)(struct jtag_controller* ctrl, unsigned int* freq);

        uint32_t jtag_control_register;
        uint32_t flags;
        uint32_t frequency;
        uint32_t devices_counts;
        uint32_t prescaler_value;
        enum JtagTransmitionFlags state;
        char *tms_buffer;
        size_t tms_buffer_valid_bits;
        char *read_buffer;
        size_t read_buffer_valid_bits;
        uint32_t lengths_in_bits;
        bool use_bits_length;
    };

    int jtag_controller_init(struct jtag_controller* jtag_ctr,
        struct register_interface* ri,
        user_mode_lock * lock,
        uint32_t jtag_register,
        uint32_t devCounts);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* JTAG_CONTROLLER_H_ */
