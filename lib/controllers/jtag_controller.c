/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

/* debugging first */
#ifdef DBG_JTAG_CTRL
    #undef MEN_DEBUG
    #define MEN_DEBUG
#endif

#ifdef TRACE_JTAG_CTRL
    #undef DBG_TRACE_ON
    #define DBG_TRACE_ON
#endif

#define DBG_NAME "[JTAG]"
#define DBG_PRFX KBUILD_MODNAME DBG_NAME

#include "../helpers/dbg.h"


#include "../controllers/jtag_controller.h"
#include "../os/assert.h"
#include "../helpers/memory.h"
#include "../helpers/error_handling.h"

#include <lib/os/types.h>
#include <multichar.h>


#include "../os/print.h"

#define JTAG_READ_FIFO_LENGTH 1
#define JTAG_BYTES_PER_READ (INT32_MAX / 8)

#define JTAG_WRITE_FIFO_LENGTH 1
#define JTAG_BYTES_PER_WRITE (INT32_MAX / 8)

#define MEMORY_TAG_TMS0 MULTICHAR32('0', 'S', 'M', 'T')
#define MEMORY_TAG_TDO1 MULTICHAR32('1','O','D','T')
#define MEMORY_TAG_RBR1 MULTICHAR32('1','R','B','R')
#define MEMORY_TAG_RED0 MULTICHAR32('0','D','E','R')

static inline void  write_control_register(struct jtag_controller* jtag_ctrl, uint32_t value)
{
    struct register_interface* ri = jtag_ctrl->base_type.register_interface;
    ri->write(ri, jtag_ctrl->jtag_control_register, value);
}

static inline uint32_t read_control_register(struct jtag_controller* jtag_ctrl)
{
    struct register_interface* ri = jtag_ctrl->base_type.register_interface;
    return ri->read(ri, jtag_ctrl->jtag_control_register);
}

static void jtag_b2b_barrier(struct jtag_controller* jtag_ctrl)
{
    struct register_interface* ri = jtag_ctrl->base_type.register_interface;
    ri->reorder_b2b_barrier(ri);
}

static unsigned char reverse_bit_char(unsigned char c)
{
    c = (c & 0xF0) >> 4 | (c & 0x0F) << 4;
    c = (c & 0xCC) >> 2 | (c & 0x33) << 2;
    c = (c & 0xAA) >> 1 | (c & 0x55) << 1;
    return c;
}

static unsigned char reverse_bits_pointer(const unsigned char *p)
{
    unsigned char c = *p;
    c = (c & 0xF0) >> 4 | (c & 0x0F) << 4;
    c = (c & 0xCC) >> 2 | (c & 0x33) << 2;
    c = (c & 0xAA) >> 1 | (c & 0x55) << 1;
    return c;
}

// Calculate CLK/2^(n+1) type prescaler value
unsigned int getExp2n1PrescalerValue(unsigned int clk, unsigned int *freq)
{
    unsigned int prescaler = 0;
    unsigned int f = clk / 2;
    while (f > *freq) {
        prescaler++;
        f /= 2;
    }

    *freq = f;
    return prescaler;
}

// Calculate CLK/2^(n+1) type frequency value
unsigned int getExp2n1FrequencyValue(unsigned int clk, unsigned int prescaler)
{
    unsigned int p = 0;
    unsigned int freq = clk / 2;
    while (p != prescaler) {
        p++;
        freq /= 2;
    }

    return freq;
}

static const char * state_to_str(enum JtagTransmitionFlags state) {
    switch(state) {
    case STATE_IDLE: return "IDLE";
    case STATE_SHIFT_DR: return "SHIFT_DR";
    case STATE_SHIFT_IR: return "SHIFT_IR";
    case STATE_RESET: return "RESET";
    default: return "UNKNOWN";
    }
}

static int set_state(struct jtag_controller * ctrl, enum JtagTransmitionFlags state)
{
    DBG_TRACE_BEGIN_FCT;

    pr_debug(DBG_PRFX ": switch jtag state from %s to %s\n", state_to_str(ctrl->state), state_to_str(state));

    // Check if requested state is valid
    if (state < STATE_IDLE || state > STATE_RESET) {
        pr_err(KBUILD_MODNAME DBG_NAME ": Error: set jtag state, invalid state %#X\n", state);
        return DBG_TRACE_RETURN(JTAG_ERROR);
    }

    // Gather TMS data
    uint32_t bits = 0;
    unsigned int numBits = 0;
    switch (ctrl->state) {
    case STATE_INVALID:
        // If we don't know the state, we need to reset (fall-through)
        bits = 0x1f;
        numBits = 5;
        /* fallthrough */
    case STATE_RESET:
        if (state == STATE_RESET) {
            break;
        }

        // If we are in reset, we go to idle in any case (fall-through)
        bits <<= 1;
        numBits += 1;
        /* fallthrough */
    case STATE_IDLE:
        if (state == STATE_IDLE) {
            break;
        }

        if (state == STATE_SHIFT_DR) {
            bits = (bits << 3) | 0x4;
            numBits += 3;
        } else if (state == STATE_SHIFT_IR) {
            bits = (bits << 4) | 0xc;
            numBits += 4;
        } else {  // Not a fall-through possibility
            bits = 0x7;
            numBits = 3;
        }
        break;

    case STATE_SHIFT_DR:
        if (state == STATE_SHIFT_DR) {
            break;
        }

        if (state == STATE_IDLE) {
            bits = 0x6;
            numBits = 3;
        } else if (state == STATE_SHIFT_IR) {
            bits = 0x3c;
            numBits = 6;
        } else {
            bits = 0x1f;
            numBits = 5;
        }
        break;

    case STATE_SHIFT_IR:
        if (state == STATE_SHIFT_IR) {
            break;
        }

        if (state == STATE_IDLE) {
            bits = 0x6;
            numBits = 3;
        } else if (state == STATE_SHIFT_DR) {
            bits = 0x1c;
            numBits = 5;
        } else {
            bits = 0x1f;
            numBits = 5;
        }
        break;

    case STATE_EXIT:
        if (state == STATE_IDLE) {
            bits = 0x2;
            numBits = 2;
        } else if (state == STATE_SHIFT_DR) {
            bits = 0xc;
            numBits = 4;
        } else if (state == STATE_SHIFT_IR) {
            bits = 0x1c;
            numBits = 5;
        } else {
            bits = 0xf;
            numBits = 4;
        }
        break;

    default:
        pr_err(DBG_PRFX ": Error: attempt to set JTAG from 0x%x (%s) to unknown state 0x%x\n", ctrl->state, state_to_str(ctrl->state), state);
        return DBG_TRACE_RETURN(JTAG_ERROR);
    }
    assert(numBits <= 16);

    // See if we have TMS data to write
    if (numBits) {
        write_control_register(ctrl, ctrl->prescaler_value);
        jtag_b2b_barrier(ctrl);
        uint32_t lastStatus = read_control_register(ctrl);
        write_control_register(ctrl, JTAG_CTRL_ACTIVATE | (lastStatus & JTAG_PRESCALER_MASK) | JTAG_BITCOUNT_MASK | JTAG_TMS_nTDO | ((bits << (16 - numBits)) & 0xffff));

        write_control_register(ctrl,JTAG_CTRL_ACTIVATE | (lastStatus & JTAG_PRESCALER_MASK) | ((numBits - 1) << JTAG_BITCOUNT_SHIFT) | JTAG_ENABLE);

        // Wait for completion
        uint32_t val = 0;
        do {
            val = read_control_register(ctrl);
            if (val == 0xffffffff) {
                write_control_register(ctrl,JTAG_DEFAULT_VALUE);
                pr_err(KBUILD_MODNAME DBG_NAME ": Error: set jtag state, failed by reading from register\n");
                return DBG_TRACE_RETURN(JTAG_ERROR_IO_FAILURE);
            }
        } while ((val & JTAG_DONE_TOGGLE) == (lastStatus & JTAG_DONE_TOGGLE));
        lastStatus = val;
    }
    write_control_register(ctrl, JTAG_DEFAULT_VALUE);

    // Set state
    ctrl->state = state;
    
    return DBG_TRACE_RETURN(0);
}

static int write(struct jtag_controller *ctrl, const void *data, unsigned int length, unsigned int flags)
{
    DBG_TRACE_BEGIN_FCT;
    pr_debug(DBG_PRFX ": write data length=%u, flags=0x%08x\n", length, flags);

    // Check data pointer
    if (!data) {
        pr_err(DBG_PRFX ": Error: write. invalid data buffer\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
    }

    // Check size
    if (((flags & LENGTH_IN_BITS) && length > INT32_MAX) ||
        ((flags & LENGTH_IN_BYTES) && length > (INT32_MAX / 8)) || length == 0) {
        pr_err(DBG_PRFX ": Error: write. invalid length. length=%#X\n", length);
        return DBG_TRACE_RETURN(JTAG_ERROR_INVALID_SIZE);
    }

    // Check flags
    if (((flags & LENGTH_IN_BITS) == 0 && (flags & LENGTH_IN_BYTES) == 0) ||
        ((flags & LENGTH_IN_BITS) && (flags & LENGTH_IN_BYTES)) ||
        ((flags & STATE_MASK) == 0 && (flags & RAW_MODE) == 0) ||
        ((flags & STATE_MASK) && (flags & RAW_MODE))) {
        pr_err(DBG_PRFX ": Error: write. invalid flags. flags=%#X\n", flags);
        return DBG_TRACE_RETURN(JTAG_ERROR_INVALID_RWFLAGS);
    }

    // Check flags
    if ((flags & SEND_ZEROES_ON_READ) && (flags & SEND_ONES_ON_READ)) {
        pr_err(DBG_PRFX ": Error: write. invalid read flags\n");
        return DBG_TRACE_RETURN(JTAG_ERROR_INVALID_RWFLAGS);
    }

    const unsigned int lengthInBits = (flags & LENGTH_IN_BITS) ? length: (8 * length);
    const unsigned int lengthInBytes = (lengthInBits + 7) / 8;
    const bool msbFirst = ((flags & LSB_FIRST) == 0);

    // Set state (not in RAW mode)
    if ((flags & RAW_MODE) == 0) {

        const int newState = (flags & STATE_MASK);
        if (newState != STATE_IDLE && newState != STATE_SHIFT_DR && newState != STATE_SHIFT_IR) {
            pr_err(DBG_PRFX ": State change to invalid state %u '%s' requested.\n", newState, state_to_str(newState));
            return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
        }

        int result = set_state(ctrl, newState);
        if (MEN_IS_ERROR(result)) {
            return DBG_TRACE_RETURN(result);
        }

    } else {
        // Prepare data for RAW mode

        // Sending TDO data
        if ((flags & WRITE_TMS_DATA) == 0) {
            // Generate TMS data
            if (flags & READ_MODE_MASK) {
                if (ctrl->tms_buffer) {
                    free_nonpageable_cacheable_small(ctrl->tms_buffer, MEMORY_TAG_TMS0);
                    ctrl->tms_buffer = 0;
                    ctrl->tms_buffer_valid_bits = 0;
                }

                char *buffer = (char *)alloc_nonpageable_cacheable_small(sizeof(char)*lengthInBytes, MEMORY_TAG_TMS0);
                if (flags & SEND_ZEROES_ON_READ) {
                    fill_mem(buffer, lengthInBytes, 0);
                }
                else {
                    fill_mem(buffer, lengthInBytes, 0xff);
                }
                ctrl->tms_buffer = buffer;
                ctrl->tms_buffer_valid_bits = lengthInBits;
            }
            // Check if TMS data is present and matches
            else if (ctrl->tms_buffer == NULL || ctrl->tms_buffer_valid_bits != lengthInBits) {
                DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": Error: write. data length mismatch\n"));
                return JTAG_ERROR_DATA_LENGTH_MISMATCH;
            }
        }

        // Sending TMS data
        else {
            if (ctrl->tms_buffer) {
                free_nonpageable_cacheable_small(ctrl->tms_buffer, MEMORY_TAG_TMS0);
                ctrl->tms_buffer = 0;
                ctrl->tms_buffer_valid_bits = 0;
            }

            // Copy TMS data
            char *buffer = (char *)alloc_nonpageable_cacheable_small(sizeof(char)*lengthInBytes, MEMORY_TAG_TMS0);
            copy_mem(buffer, data, lengthInBytes);
            ctrl->tms_buffer = buffer;
            ctrl->tms_buffer_valid_bits = lengthInBits;

            // Generate TDO data
            if (flags & READ_MODE_MASK) {
                const unsigned int writeFlags = flags & ~(READ_MODE_MASK | WRITE_TMS_DATA);
                char *buffer = (char *)alloc_nonpageable_cacheable_small(sizeof(char)*lengthInBytes, MEMORY_TAG_TDO1);
                if (flags & SEND_ZEROES_ON_READ) {
                    fill_mem(buffer, lengthInBytes, 0);
                }
                else {
                    fill_mem(buffer, lengthInBytes, 0xff);
                }

                // Send TDO data

                int result = write(ctrl, buffer, length, writeFlags);
                free_nonpageable_cacheable_small(buffer, MEMORY_TAG_TDO1);
                DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END write.\n"));
                return result;
            }
            else {
                DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END write.\n"));
                if (flags & LENGTH_IN_BITS) {
                    return DBG_TRACE_RETURN(lengthInBits);
                }
                else {
                    return DBG_TRACE_RETURN(lengthInBytes);
                }
            }
        }
    }

    // Allocate receive buffer
    if (ctrl->read_buffer) {
        free_nonpageable_cacheable_small(ctrl->read_buffer, MEMORY_TAG_RBR1);
        ctrl->read_buffer = 0;
        ctrl->read_buffer_valid_bits = 0;
    }

    ctrl->read_buffer = (char *)alloc_nonpageable_cacheable_small(sizeof(char)*lengthInBytes, MEMORY_TAG_RBR1);

    // Setup prescaler value and read back status register
    /* TODO: [RKN] This is not required on mE6. Is it on mE5? */
    write_control_register(ctrl, ctrl->prescaler_value);
    jtag_b2b_barrier(ctrl);
    uint32_t lastStatus = read_control_register(ctrl);

    // Send in units of 16 bit
    // Note: The JTAG chain is basically a large shift register of variable length (depending on register selection, etc.)
    //       The user *must* always know the length of the shift register, and do transfers accordingly.
    const unsigned char *sendPtr = (const unsigned char *)data;
    const unsigned char *tmsPtr = (const unsigned char *)ctrl->tms_buffer;
    char *recvPtr = ctrl->read_buffer;
    if (msbFirst) {
        sendPtr += lengthInBytes - 1;
        if (tmsPtr) {
            tmsPtr += lengthInBytes - 1;
        }
        recvPtr += lengthInBytes - 1;
    }
    unsigned int bitsLeftToWrite = lengthInBits;
    while (bitsLeftToWrite) {
        // Get frame length
        unsigned int bitsToWrite = bitsLeftToWrite % 16;
        if (bitsToWrite == 0) {
            bitsToWrite = 16;
        }

        // Get data
        uint16_t d = 0;
        if (msbFirst) {
            d = *sendPtr--;
            d <<= 8;

            if (bitsToWrite > 8) {
                d |= *sendPtr--;
            }

            /* shift the first meaningful bit to msb position */
            if (bitsToWrite <= 8) {
                d <<= (8 - bitsToWrite);
            } else if (bitsToWrite < 16) {
                d <<= (16 - bitsToWrite);
            }
        } else {
            d = reverse_bits_pointer(sendPtr++);
            d <<= 8;
            if (bitsToWrite > 8) {
                d |= reverse_bits_pointer(sendPtr++);
            }
        }

        // Write data (not in RAW mode)
        if ((flags & RAW_MODE) == 0) {
            // Unless we transmit data in idle, we need to exit the shift state with the last bit
            if (bitsLeftToWrite > 16 || (flags & STATE_MASK) == STATE_IDLE) {
                write_control_register(ctrl, JTAG_CTRL_ACTIVATE | (lastStatus & JTAG_PRESCALER_MASK) | JTAG_BITCOUNT_MASK | JTAG_TMS_nTDO);
            } else {
                write_control_register(ctrl, JTAG_CTRL_ACTIVATE | (lastStatus & JTAG_PRESCALER_MASK) | JTAG_BITCOUNT_MASK | JTAG_TMS_nTDO 
                                       | ((0x1 << (16 - bitsToWrite)) & 0xffff));
            }
            write_control_register(ctrl, JTAG_CTRL_ACTIVATE | (lastStatus & JTAG_PRESCALER_MASK) | ((bitsToWrite - 1) << JTAG_BITCOUNT_SHIFT) | JTAG_ENABLE | (d & 0xffff));
        } else {
            // Write RAW data

            // Get TMS data
            uint16_t tms = 0;
            if (msbFirst) {
                tms = *tmsPtr--;
                tms <<= 8;
                if (bitsToWrite > 8) {
                    tms |= *tmsPtr--;
                }
            }
            else {
                tms = reverse_bits_pointer(tmsPtr++);
                tms <<= 8;
                if (bitsToWrite > 8) {
                    tms |= reverse_bits_pointer(tmsPtr++);
                }
            }

            write_control_register(ctrl, JTAG_CTRL_ACTIVATE | (lastStatus & JTAG_PRESCALER_MASK) | JTAG_BITCOUNT_MASK | JTAG_TMS_nTDO | (tms & 0xffff));
            write_control_register(ctrl, JTAG_CTRL_ACTIVATE | (lastStatus & JTAG_PRESCALER_MASK) | ((bitsToWrite - 1) << JTAG_BITCOUNT_SHIFT) | JTAG_ENABLE | (d & 0xffff));
        }

        // Wait for completion
        uint32_t val = 0;
        do {
            val = read_control_register(ctrl);
            if (val == 0xffffffff) {
                write_control_register(ctrl, JTAG_DEFAULT_VALUE);
                DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": Error: write. failure by reading control register\n"));
                return DBG_TRACE_RETURN(JTAG_ERROR_IO_FAILURE);
            }
        } while ((val & JTAG_DONE_TOGGLE) == (lastStatus & JTAG_DONE_TOGGLE));
        lastStatus = val;

        // Collect read data
        if (msbFirst) {
            if (bitsToWrite > 8) {
                *recvPtr-- = (val >> 8) & 0xff;
            }
            *recvPtr-- = val & 0xff;
        }
        else {
            if (bitsToWrite > 8) {
                *recvPtr++ = reverse_bit_char((val >> 8) & 0xff);
            }
            *recvPtr++ = reverse_bit_char(val & 0xff);
        }

        // Prepare next frame
        bitsLeftToWrite -= bitsToWrite;
    }

    // Update state
    if ((flags & RAW_MODE) == 0) {
        if ((flags & STATE_MASK) != STATE_IDLE) {
            ctrl->state = STATE_EXIT;
        }
    }
    else {
        ctrl->state = STATE_INVALID;

        // Clear TMS data
        free_nonpageable_cacheable_small(ctrl->tms_buffer, MEMORY_TAG_TMS0);
        ctrl->tms_buffer = 0;
        ctrl->tms_buffer_valid_bits = 0;
    }

    // Update read buffer
    ctrl->read_buffer_valid_bits = lengthInBits;

    write_control_register(ctrl, JTAG_DEFAULT_VALUE);

    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END write\n"));
    if (flags & LENGTH_IN_BITS) {
        return DBG_TRACE_RETURN(lengthInBits);
    }
    else {
        return DBG_TRACE_RETURN(lengthInBytes);
    }

    return DBG_TRACE_RETURN(JTAG_ERROR);
}

static int read(struct jtag_controller *ctrl, void *data, unsigned int length, unsigned int flags)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN read, length=%#X, flags=%#X\n", length, flags));
    // Check data pointer
    if (!data) {
        DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": Error: read, invalid data buffer \n"));
        return JTAG_ERROR_INVALID_POINTER;
    }

    // Check size
    if (((flags & LENGTH_IN_BITS) && length > INT32_MAX) ||
        ((flags & LENGTH_IN_BYTES) && length > (INT32_MAX / 8)) || length == 0) {
        DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": Error: read, invalid length. length=%#X \n", length));
        return JTAG_ERROR_INVALID_SIZE;
    }

    // Check flags
    if (((flags & LENGTH_IN_BITS) == 0 && (flags & LENGTH_IN_BYTES) == 0) ||
        ((flags & LENGTH_IN_BITS) && (flags & LENGTH_IN_BYTES)) ||
        ((flags & STATE_MASK) == 0 && (flags & RAW_MODE) == 0) ||
        ((flags & STATE_MASK) && (flags & RAW_MODE))) {
        DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": Error: read, invalid flags \n"));
        return JTAG_ERROR_INVALID_RWFLAGS;
    }

    if ((flags & SEND_ZEROES_ON_READ) && (flags & SEND_ONES_ON_READ)) {
        DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": Error: read, invalid read flags \n"));
        return JTAG_ERROR_INVALID_RWFLAGS;
    }

    const unsigned int lengthInBits = (flags & LENGTH_IN_BITS) ? length: (8 * length);
    const unsigned int lengthInBytes = (lengthInBits + 7) / 8;

    // Check for data
    if (!ctrl->read_buffer || ctrl->read_buffer_valid_bits < lengthInBits) {
        // Not supported in RAW mode
        if (flags & RAW_MODE) {
            DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": Error: read, un supported flag in raw mode \n"));
            return JTAG_ERROR_NO_DATA;
        }

        if ((flags & READ_MODE_MASK) == WAIT_ON_READ) {
            // TODO: wait ...
            DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": Error: read, timeout \n"));
            return JTAG_ERROR_TIMEOUT;
        }
        else {
            char *buffer = (char *)alloc_nonpageable_cacheable_small(sizeof(char)*lengthInBytes, MEMORY_TAG_RED0);
            if (flags & SEND_ZEROES_ON_READ) {
                fill_mem(buffer, lengthInBytes, 0);
            }
            else {
                fill_mem(buffer, lengthInBytes, 0xff);
            }

            // Send dummy data
            int result = write(ctrl, buffer, length, flags);
            free_nonpageable_cacheable_small(buffer, MEMORY_TAG_RED0);
            if (result <= 0) {
                DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END read \n"));
                return result;
            }
        }
    }

    // Copy the data
    copy_mem(data, ctrl->read_buffer, lengthInBytes);

    // Destroy buffer
    free_nonpageable_cacheable_small(ctrl->read_buffer, MEMORY_TAG_RED0);
    ctrl->read_buffer = 0;
    ctrl->read_buffer_valid_bits = 0;

    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END read\n"));

    if (flags & LENGTH_IN_BITS) {
        return lengthInBits;
    }
    else {
        return lengthInBytes;
    }
}

static int set_frequency(struct jtag_controller * ctrl)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN set frequency. frequency=%#X\n", ctrl->frequency));
    if (ctrl->frequency > JTAG_PRESCALER_BASE_FREQUENCY) {
        DBG_STMT(pr_err(KBUILD_MODNAME DBG_NAME ": Error: set frequency, invalid frequency value = %#X\n", ctrl->frequency));
        return JTAG_ERROR_FREQUENCY_TOO_HIGH;
    }

    // Calculate and check prescaler value
    unsigned int freq = ctrl->frequency;
    unsigned int prescaler = getExp2n1PrescalerValue(JTAG_PRESCALER_BASE_FREQUENCY, &freq);
    if (prescaler > JTAG_PRESCALER_MAX_VALUE) {
        DBG_STMT(pr_err(KBUILD_MODNAME DBG_NAME ": Error: set frequency, invalid prescaler value = %#X\n", prescaler));
        return JTAG_ERROR_FREQUENCY_TOO_LOW;
    }

    // Set prescaler value
    ctrl->prescaler_value = prescaler << JTAG_PRESCALER_SHIFT;
    write_control_register(ctrl, ctrl->prescaler_value);
    jtag_b2b_barrier(ctrl);

    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END set frequency\n"));
    return freq;
}

static int get_frequency(struct jtag_controller *ctrl, unsigned int *freq)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN get frequency.\n"));
    // Read register
    uint32_t val = read_control_register(ctrl);
    // Get frequency value
    *freq = getExp2n1FrequencyValue(JTAG_PRESCALER_BASE_FREQUENCY, (val & JTAG_PRESCALER_MASK) >> JTAG_PRESCALER_SHIFT);
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END get frequency. frequency=%#X\n", *freq));
    return 0;
}

static int jtag_write_shot(struct controller_base* ctrl, const unsigned char* buffer, int length)
{
    DBG_TRACE_BEGIN_FCT;
    pr_debug(DBG_PRFX ": BEGIN write shot, length=%i\n", length);

    struct jtag_controller* self = downcast(ctrl, struct jtag_controller);
    assert(length <= JTAG_BYTES_PER_WRITE);

    if (buffer == NULL || length == 0) {
        pr_err(DBG_PRFX ": write shot, error buffer or length is invalid\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
    }

    int ret = 0;
    if (self->flags & SET_DATA_LENGTH) {
        uint32_t* leng = (uint32_t*)buffer;
        self->lengths_in_bits = leng[0];
        self->use_bits_length = true;
        pr_debug(DBG_PRFX ": write shot, set data length, Nr of bits=%#X\n", self->lengths_in_bits);
    }
    else if (self->flags & SET_FREQUENCY) {
        unsigned int* freq = (unsigned int*)(buffer);
        self->frequency = *freq;
        pr_debug(DBG_PRFX ": write shot, set frequency, frequency=%#X\n", *freq);
        set_frequency(self);
    }
    else {
        int leng = (self->use_bits_length) ? self->lengths_in_bits : length;
        self->use_bits_length = false;
        ret = write(self, buffer, leng, self->flags);
    }

    return DBG_TRACE_RETURN(ret);
}

static int jtag_read_shot(struct controller_base* ctrl, unsigned char* buffer, size_t length)
{
    DBG_TRACE_BEGIN_FCT;
    pr_debug(KBUILD_MODNAME DBG_NAME ": read shot length=%zu\n", length);

    struct jtag_controller* self = downcast(ctrl, struct jtag_controller);
    assert(length <= JTAG_BYTES_PER_READ);
    if (buffer == NULL || length == 0) {
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
    }

    int leng = (self->use_bits_length) ? self->lengths_in_bits : length;
    self->use_bits_length = false;
    int ret = read(self, buffer, leng, self->flags);

    return DBG_TRACE_RETURN(ret);
}

static int jtag_handle_pre_burst_flags(struct controller_base* ctrl, uint32_t flags)
{
    DBG_TRACE_BEGIN_FCT;
    pr_debug(DBG_PRFX ": pre burst flags = 0x%08x\n", flags);

    struct jtag_controller* self = downcast(ctrl, struct jtag_controller);

    self->flags = flags;
    int ret = 0;
    if ((flags & STATE_MASK) == STATE_RESET) {
        ret = set_state(self, STATE_RESET);
    }

    return DBG_TRACE_RETURN(ret);
}

static int jtag_handle_post_burst_flags(struct controller_base* ctrl, uint32_t flags)
{
    DBG_TRACE_BEGIN_FCT;
    pr_debug(DBG_PRFX ": post burst flags = flags 0x%08x\n", flags);
    struct jtag_controller* self = downcast(ctrl, struct jtag_controller);

    self->flags = 0;

    return DBG_TRACE_RETURN(0);
}

static int jtag_wait_for_write_fifo_empty(struct controller_base* ctrl)
{
    DBG_TRACE_BEGIN_END_FCT;
    return 0;
}

static int jtag_request_read(struct controller_base* ctrl, size_t num_bytes)
{
    DBG_TRACE_BEGIN_END_FCT;
    return 0;
}

static void jtag_cleanup(struct controller_base* ctrl) {
    DBG_TRACE_BEGIN_FCT;

    struct jtag_controller* self = downcast(ctrl, struct jtag_controller);
    if (self->read_buffer) {
        free_nonpageable_cacheable_small(self->read_buffer, MEMORY_TAG_RED0);
        self->read_buffer = NULL;
    }
    if (self->tms_buffer) {
        free_nonpageable_cacheable_small(self->tms_buffer, MEMORY_TAG_TMS0);
        self->tms_buffer = NULL;
    }
    self->flags = 0;
    self->use_bits_length = false;

    DBG_TRACE_END_FCT;
}

static void jtag_burst_aborted(struct controller_base* ctrl)
{
    DBG_TRACE_BEGIN_FCT;
    jtag_cleanup(ctrl);
    DBG_TRACE_END_FCT;
}

int jtag_controller_init(struct jtag_controller* jtag_ctr,
    struct register_interface* ri,
    user_mode_lock* lock,
    uint32_t jtag_register,
    uint32_t devCounts)
{
    DBG_TRACE_BEGIN_FCT;

    struct controller_base* ctrl = &jtag_ctr->base_type;

    controller_base_init(ctrl, ri, lock,
        JTAG_READ_FIFO_LENGTH, JTAG_BYTES_PER_READ,
        JTAG_WRITE_FIFO_LENGTH, JTAG_BYTES_PER_WRITE,
        NULL,
        NULL,
        jtag_handle_pre_burst_flags,
        jtag_handle_post_burst_flags,
        jtag_write_shot,
        jtag_request_read,
        jtag_read_shot,
        controller_base_execute_command_not_supported,
        jtag_wait_for_write_fifo_empty,
        jtag_burst_aborted,
        jtag_cleanup);

    jtag_ctr->read = read;
    jtag_ctr->write = write;
    jtag_ctr->get_frequency = get_frequency;
    jtag_ctr->set_frequency = set_frequency;
    jtag_ctr->set_state = set_state;

    jtag_ctr->jtag_control_register = jtag_register;
    jtag_ctr->flags = 0;
    jtag_ctr->frequency = 0X30D40;
    jtag_ctr->devices_counts = devCounts;
    jtag_ctr->prescaler_value = JTAG_DEFAULT_VALUE;
    jtag_ctr->state = STATE_INVALID;
    jtag_ctr->read_buffer = NULL;
    jtag_ctr->read_buffer_valid_bits = 0;
    jtag_ctr->tms_buffer = NULL;
    jtag_ctr->tms_buffer_valid_bits = 0;
    jtag_ctr->lengths_in_bits = 0;
    jtag_ctr->use_bits_length = false;

    return DBG_TRACE_RETURN(0);
}