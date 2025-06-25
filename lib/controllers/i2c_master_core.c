/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

/* debugging first! */
#ifdef DBG_I2C_MASTER_CORE
    #undef MEN_DEBUG
    #define MEN_DEBUG
#endif

#ifdef TRACE_I2C_MASTER_CORE
    #define DBG_TRACE_ON
#endif

#define DBG_NAME "[I2C CORE] "
#define DBG_PRFX KBUILD_MODNAME " " DBG_NAME
#include "../helpers/dbg.h"


#include "../controllers/i2c_master_core.h"
#include "../os/print.h"
#include "../os/string.h"
#include "../os/types.h"
#include "../os/assert.h"

#include "../controllers/i2c_defines.h"
#include "../helpers/helper.h"
#include "../helpers/bits.h"
#include "../helpers/error_handling.h"
#include "../os/kernel_macros.h"

#define I2C_CORE_MAX_READS_PER_BURST 1
#define I2C_CORE_MAX_BYTES_PER_READ 1

#define I2C_CORE_MAX_WRITES_PER_BURST 1
#define I2C_CORE_MAX_BYTES_PER_WRITE 1

#define CORE_CTRL_MASK_ENABLE BIT_7
#define CORE_CTRL_ENABLED     BIT_7
#define CORE_CTRL_DISABLED    0

#define CORE_CTRL_MASK_IRQ_ENABLE BIT_6
#define CORE_CTRL_IRQ_ENABLED     BIT_6
#define CORE_CTRL_IRQ_DISABLED    0

#define CORE_CTRL_MASK_UNUSED_BITS (BIT_0 | BIT_1 | BIT_2 | BIT_3 | BIT_4 | BIT_5)

#define I2C_MC_STATUS_MASK_ACK_FROM_SLAVE  BIT_7
#define I2C_MC_STATUS_ACK_RECEIVED         0
#define I2C_MC_STATUS_ACK_NOT_RECEIVED     BIT_7

#define I2C_MC_STATUS_MASK_BUS_STATUS  BIT_6
#define I2C_MC_STATUS_BUS_BUSY         BIT_6
#define I2C_MC_STATUS_BUS_IDLE         0

#define I2C_MC_STATUS_MASK_ARBITRATION_STATUS BIT_5
#define I2C_MC_STATUS_ARBITRATION_LOST        BIT_5
#define I2C_MC_STATUS_ARBITRATION_OK          0

#define I2C_MC_STATUS_MASK_TRANSFER_STATUS  BIT_1
#define I2C_MC_STATUS_TRANSFER_IN_PROGRESS  BIT_1
#define I2C_MC_STATUS_TRANSFER_COMPLETE     0

#define I2C_MC_STATUS_MASK_INTERRUPT_FLAT  BIT_0
#define I2C_MC_STATUS_INTERRUPT_PENDING    BIT_0
#define I2C_MC_STATUS_NO_INTERRUPT_PENDING 0

#define I2C_MC_STATUS_MASK_UNUSED_BITS (BIT_2 | BIT_3 | BIT_4)

static const uint32_t I2C_MC_TRANSFER_TIMEOUT_MSECS = 100;

#define I2C_MC_ADDRESS_FLAG_BANK1         BIT_6
#define I2C_MC_ADDRESS_BANK1_ACTIVATED    BIT_6
#define I2C_MC_ADDRESS_BANK1_DEACTIVATED  0

/* a real status would always contain zeros */
static const uint8_t I2C_MC_GET_STATUS_FAILED = 0xFF;

enum i2c_core_registers {
    I2C_MC_REG_PRESCALE_LOW = 0x00,
    I2C_MC_REG_PRESCALE_HIGH = 0x01,
    I2C_MC_REG_CONTROL = 0x02,
    I2C_MC_REG_TRANSMIT = 0x03,
    I2C_MC_REG_RECEIVE = 0x03,
    I2C_MC_REG_COMMAND = 0x04,
    I2C_MC_REG_STATUS = 0x04
};

enum i2c_core_command {
    I2C_COMMAND_NONE = 0,
    I2C_COMMAND_START = 0x80,
    I2C_COMMAND_STOP = 0x40,
    I2C_COMMAND_READ = 0x20,
    I2C_COMMAND_WRITE = 0x10,
    I2C_COMMAND_NOT_ACKNOWLEDGE = 0x8,
    I2C_COMMAND_IRQ_ACKNOWLEDGE = 0x1
};

enum i2c_core_slave_rw {
    I2C_MC_READ = 0,
    I2C_MC_WRITE = 1
};

enum i2c_core_reg_access_direction {
    CORE_REG_READ,
    CORE_REG_WRITE
};

#include "../os/print.h"

static const char * get_register_name(uint8_t address, enum i2c_core_reg_access_direction direction) {
    switch (address) {
    case 0x00:
        return "prescale (low)";
    case 0x01:
        return "prescale (high)";
    case 0x02:
        return "control";
    case 0x03:
        return (direction == CORE_REG_READ) ? "receive" : "transmit";
    case 0x04:
        return (direction == CORE_REG_READ) ? "status" : "command";
    default:
        return "INVALID";
    }
}

static const char * get_register_value_meaning(uint8_t address, uint8_t bits,
                                               enum i2c_core_reg_access_direction direction) {
    static char buf[256] = { 0 };
    switch (address) {
    case I2C_MC_REG_PRESCALE_LOW:
        snprintf(buf, ARRAY_SIZE(buf), "value: %u", bits);
        break;
    case I2C_MC_REG_PRESCALE_HIGH:
        snprintf(buf, ARRAY_SIZE(buf), "value: %u", ((uint16_t) bits) << 8);
        break;
    case I2C_MC_REG_CONTROL:
        snprintf(buf, ARRAY_SIZE(buf), "enable: %d, irq enable: %d", GET_BIT_7(bits), GET_BIT_6(bits));
        break;
    case I2C_MC_REG_RECEIVE /* == I2C_MC_REG_TRANSMIT */:
        snprintf(buf, ARRAY_SIZE(buf), "byte: 0x%02x", bits);
        break;
    case I2C_MC_REG_COMMAND /* == I2C_MC_REG_STATUS */:
        if (direction == CORE_REG_READ)
            snprintf(buf, ARRAY_SIZE(buf), "ack: %d, bus busy: %d, arbitration lost: %d, transfer in progress: %d, irq pending: %d",
                    GET_BIT_7(bits),
                    GET_BIT_6(bits), GET_BIT_5(bits), GET_BIT_1(bits), GET_BIT_0(bits));
        else
            snprintf(buf, ARRAY_SIZE(buf), "create start: %d, create stop: %d, read from slave: %d, write to slave: %d"
                    ", acknowledge: %s, clear pending irq: %d",
                    GET_BIT_7(bits), GET_BIT_6(bits), GET_BIT_5(bits), GET_BIT_4(bits),
                    BIT_TO_STR(GET_BIT_3(bits), "NACK", "ACK"),
                    GET_BIT_0(bits));
        break;
    default:
        return "INVALID REGISTER";
    }
    return buf;
}

static int insert_str(char * str, int pos, int max_len, const char * ins) {
    if (pos != 0)
        pos += snprintf(str + pos, max_len - pos, ", ");

    return pos + snprintf(str + pos, max_len - pos, "%s", ins);
}

static const char * get_command_names(uint8_t cmd) {
    static char buf[1024];
    int pos = 0;
    if (cmd & I2C_COMMAND_START)
        pos = insert_str(buf, pos, ARRAY_SIZE(buf), "Start");

    if (cmd & I2C_COMMAND_STOP)
        pos = insert_str(buf, pos, ARRAY_SIZE(buf), "Stop");

    if (cmd & I2C_COMMAND_READ)
        pos = insert_str(buf, pos, ARRAY_SIZE(buf), "Read");

    if (cmd & I2C_COMMAND_WRITE)
        pos = insert_str(buf, pos, ARRAY_SIZE(buf), "Write");

    if (cmd & I2C_COMMAND_NOT_ACKNOWLEDGE)
        pos = insert_str(buf, pos, ARRAY_SIZE(buf), "NAck");

    if (cmd & I2C_COMMAND_IRQ_ACKNOWLEDGE)
        pos = insert_str(buf, pos, ARRAY_SIZE(buf), "IAck");

    if (pos == 0)
        insert_str(buf, pos, ARRAY_SIZE(buf), "None");

    return buf;
}

/* debugging end */

/**
 * Writes to the address register of the master core and ensures
 * that the write is completed when this function returns.
 */
static void safe_write(struct i2c_master_core * self,
                       uint32_t address, uint8_t value) {
    struct register_interface * ri = upcast(self)->register_interface;

    ri->reorder_barrier(ri);
    ri->write(ri, address, value);
    ri->reorder_barrier(ri);

    /* write multiple times to a read only register to give the
     * real write the time it needs. */
    for (int i = 0; i < self->num_safety_writes; ++i) {
        ri->write(ri, 0, 0);
    }
}

/**
 * @attention
 * The i2c core must be activated before performing any operations on it
 * and it must be deactivated again after the operations!
 *
 * @param self
 * @param activated
 */
static void activate_i2c_core_on_bank(struct i2c_master_core * self, bool activate) {
    DBG_TRACE_BEGIN_FCT;

    if (self->active_bus == NULL) {
        pr_err(DBG_PRFX "Cannot activate core. No active bus.\n");
        DBG_TRACE_END_FCT;
        return;
    }

    if (self->active_bus->bank_activation_bit) {
        struct register_interface * ri = upcast(self)->register_interface;
        ri->reorder_barrier(ri);
        ri->write(ri, self->i2c_ctrl_address_register, activate ? self->active_bus->bank_activation_bit : 0);
    }
    DBG_TRACE_END_FCT;
}

static void write_core_register(struct i2c_master_core * self, uint8_t address, uint8_t value) {
    DBG_TRACE_BEGIN_FCT;

    if (address == I2C_MC_REG_TRANSMIT && self->active_bus == NULL) {
        pr_err(DBG_PRFX " Attempted data transmission without an active bus\n");
        DBG_TRACE_END_FCT;
        return;
    }

    uint8_t additional_control_bits = 0;
    if (self->active_bus != NULL) {
        additional_control_bits |= self->active_bus->bank_number << 3;
        additional_control_bits |= self->active_bus->bank_activation_bit;
        struct controller_base * ctrl = upcast(self);
        if (ctrl->are_burst_flags_set(ctrl, I2C_BURST_FLAG_WRITE_ENABLE)) {
            additional_control_bits |= self->active_bus->write_enable_bit;
        }
    }

    struct register_interface * ri = upcast(self)->register_interface;
    pr_debug("menable: " DBG_NAME " write core register: 0x%02x (%s) <- 0x%02x on bank %lu (wren: %s, raw value: 0x%02x)\n",
         address, get_register_name(address, CORE_REG_WRITE), value,
         (additional_control_bits & (BIT(3) | BIT(4) | BIT(5))) >> 3 ,
         (self->active_bus
             && self->active_bus->write_enable_bit
             && (additional_control_bits & self->active_bus->write_enable_bit)) ? "on" : "off",
         address | additional_control_bits);
    pr_debug("menable: " DBG_NAME "   meaning: %s\n",
                  get_register_value_meaning(address, value, CORE_REG_WRITE));
    ri->reorder_barrier(ri);
    safe_write(self, self->i2c_ctrl_address_register, address | additional_control_bits);
    ri->reorder_barrier(ri);
    safe_write(self, self->i2c_ctrl_write_register, value);

    DBG_TRACE_END_FCT;
}

static uint8_t read_core_register(struct i2c_master_core * self, uint8_t address) {
    DBG_TRACE_BEGIN_FCT;

    if (address == I2C_MC_REG_RECEIVE && self->active_bus == NULL) {
        pr_err(DBG_PRFX " Attempted data reception without an active bus\n");
        return DBG_TRACE_RETURN(0xff);
    }

    uint8_t additional_control_bits = 0;
    if (self->active_bus != NULL) {
        additional_control_bits |= self->active_bus->bank_number << 3;
        additional_control_bits |= self->active_bus->bank_activation_bit;
        struct controller_base * ctrl = upcast(self);
        if (ctrl->are_burst_flags_set(ctrl, I2C_BURST_FLAG_WRITE_ENABLE)) {
            additional_control_bits |= self->active_bus->write_enable_bit;
        }
    }

    struct register_interface * ri = upcast(self)->register_interface;
    DBG1(pr_debug("menable: " DBG_NAME " read core register:  0x%02x (%s) on bank %lu (wren: %s, raw value: 0x%02x)\n",
         address, get_register_name(address, CORE_REG_READ),
         (additional_control_bits & (BIT(3) | BIT(4) | BIT(5))) >> 3,
         (self->active_bus
             && self->active_bus->write_enable_bit
             && (additional_control_bits & self->active_bus->write_enable_bit)) ? "on" : "off",
         address | additional_control_bits));

    ri->reorder_barrier(ri);
    safe_write(self, self->i2c_ctrl_address_register, address | additional_control_bits);
    ri->reorder_b2b_barrier(ri);
    uint8_t value = (uint8_t) ri->read(ri, self->i2c_ctrl_read_register);

    DBG1(pr_debug("menable: " DBG_NAME " read result:    0x%02x (%s) -> 0x%02x\n",
         address, get_register_name(address, CORE_REG_READ), value));
    DBG1(pr_debug("menable: " DBG_NAME "   meaning: %s\n",
                  get_register_value_meaning(address, value, CORE_REG_READ)));

    return DBG_TRACE_RETURN(value);
}

static void modify_rw_register(struct i2c_master_core * self, uint8_t address, uint8_t modify_mask,
                               uint8_t modified_bits) {
    DBG_TRACE_BEGIN_FCT;

    uint8_t value = read_core_register(self, address);
    value = modify_bits(value, modify_mask, modified_bits);
    write_core_register(self, address, value);
    DBG_TRACE_END_FCT;
}

static uint8_t get_core_status(struct i2c_master_core * self) {
    DBG_TRACE_BEGIN_FCT;

    uint8_t status = read_core_register(self, I2C_MC_REG_STATUS);
    /* These bits should always be zero. If they aren't, it's an error */
    if ((status & I2C_MC_STATUS_MASK_UNUSED_BITS) != 0) {
        pr_err(DBG_PRFX "invalid status register value: 0x%x (disallowed bits: 0x%x)\n", status, I2C_MC_STATUS_MASK_UNUSED_BITS);
    }

    return DBG_TRACE_RETURN(status);
}

static bool has_slave_acknowledged(struct i2c_master_core * self) {
    DBG_TRACE_BEGIN_FCT;

    if (self->active_bus == NULL) {
        pr_err(DBG_PRFX "Cannot check slave acknowledge. No active bus.\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_STATE);
    }

    uint8_t status = get_core_status(self);
    const int wasAckReceived = ((status & I2C_MC_STATUS_MASK_ACK_FROM_SLAVE) == I2C_MC_STATUS_ACK_RECEIVED);
    DBG_STMT(pr_debug("menable: " DBG_NAME " has slave acknowledged: %s\n",
             (wasAckReceived ? "yes" : "no")));

    return DBG_TRACE_RETURN(wasAckReceived);
}

static bool core_has_status(struct i2c_master_core * self, uint8_t status_mask, uint8_t desired_status) {
    return (get_core_status(self) & status_mask) == desired_status;
}

static uint8_t wait_for_core_status(struct i2c_master_core * self, uint8_t status_mask,
                                    uint8_t desired_status,
                                    uint32_t timeout_msecs) {
    DBG_TRACE_BEGIN_FCT;

    uint8_t status;
    struct timeout timeout;
    timeout_init(&timeout, timeout_msecs);

    do {
        status = get_core_status(self);
    } while ((status & status_mask) != desired_status
             && !timeout_has_elapsed(&timeout));

    uint8_t ret = ((status & status_mask) != desired_status)
                   ? I2C_MC_GET_STATUS_FAILED
                   : status;

    return DBG_TRACE_RETURN(ret);
}

static int wait_for_transfer_complete(struct i2c_master_core * self) {
    DBG_TRACE_BEGIN_FCT;

    if (self->active_bus == NULL) {
        pr_err(DBG_PRFX "Cannot wait for transfer completion. No active bus.\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_STATE);
    }

    uint8_t status = wait_for_core_status(self, I2C_MC_STATUS_MASK_TRANSFER_STATUS,
                                          I2C_MC_STATUS_TRANSFER_COMPLETE,
                                          I2C_MC_TRANSFER_TIMEOUT_MSECS);
    int ret = (status != I2C_MC_GET_STATUS_FAILED)
               ? STATUS_OK
               : STATUS_ERROR;

    if (ret != STATUS_OK) {
        pr_err("timed out while waiting for i2c transfer to complete\n");
    }
    
    return DBG_TRACE_RETURN(ret);
}

static int write_stop(struct i2c_master_core * self) {
    DBG_TRACE_BEGIN_FCT;

    if (self->active_bus == NULL) {
        pr_err(DBG_PRFX "Cannot write stop. No active bus.\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_STATE);
    }

    write_core_register(self, I2C_MC_REG_COMMAND, I2C_COMMAND_STOP);
    uint8_t status = wait_for_core_status(self, I2C_MC_STATUS_MASK_BUS_STATUS, I2C_MC_STATUS_BUS_IDLE, 100);

    int ret = (status == I2C_MC_GET_STATUS_FAILED)
                   ? STATUS_OK
                   : STATUS_ERROR;

    if (ret != STATUS_OK) {
        pr_err("timed out while waiting for i2c transfer to issue stop condition\n");
    }
    
    return DBG_TRACE_RETURN(ret);
}

static int write_byte(struct i2c_master_core * self, uint8_t byte, enum i2c_core_command cmd) {
    struct controller_base * ctrl = upcast(self);
    
    DBG_TRACE_BEGIN_FCT;
    if (self->active_bus == NULL) {
        pr_err(DBG_PRFX "Cannot write byte. No active bus.\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_STATE);
    }

    DBG_STMT(pr_debug("menable: " DBG_NAME "write byte: 0x%02x (bin)%s, commands: %s\n",
                 byte, to_binary_8(byte), get_command_names(cmd)));

    write_core_register(self, I2C_MC_REG_TRANSMIT, byte);
    write_core_register(self, I2C_MC_REG_COMMAND, I2C_COMMAND_WRITE | cmd);

    int ret = wait_for_transfer_complete(self);
    if (MEN_IS_ERROR(ret)) {
        DBG_TRACE_END_FCT;
        return STATUS_ERROR;
    }

    bool ack = has_slave_acknowledged(self);
    if (!ack && !ctrl->are_burst_flags_set(ctrl, I2C_POST_BURST_FLAG_ACK_POLLING)) {
        pr_err("ack/nak mismatch while writing to i2c\n");
    }

    return DBG_TRACE_RETURN(ack ? STATUS_OK : STATUS_I2C_NO_ACK);
}

static int read_byte(struct i2c_master_core * self, uint8_t * out_byte, enum i2c_core_command cmd) {
    DBG_TRACE_BEGIN_FCT;

    if (self->active_bus == NULL) {
        pr_err(DBG_PRFX "Cannot read byte. No active bus.\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_STATE);
    }

    DBG_STMT(pr_debug("menable: " DBG_NAME "read byte, commands: %s\n", get_command_names(cmd)));
    write_core_register(self, I2C_MC_REG_COMMAND, I2C_COMMAND_READ | cmd);
    int ret = wait_for_transfer_complete(self);
    if (MEN_IS_ERROR(ret))
        return DBG_TRACE_RETURN(STATUS_ERROR);

    *out_byte = read_core_register(self, I2C_MC_REG_RECEIVE);
    DBG_STMT(pr_debug("menable: " DBG_NAME " byte read: 0x%02x   (bin)%s\n", *out_byte, to_binary_8(*out_byte)));

    return DBG_TRACE_RETURN(STATUS_OK);
}

/* virtual method implementations */

static int handle_begin_transaction(struct controller_base * ctrl) {
    struct i2c_master_core * self = downcast(ctrl, struct i2c_master_core);

    /* if applicable: activate i2c_core on the bank */
    activate_i2c_core_on_bank(self, true);

    return STATUS_OK;
}

static void handle_end_transaction(struct controller_base * ctrl) {
    struct i2c_master_core * self = downcast(ctrl, struct i2c_master_core);

    /* if applicable: deactivate i2c_core on the bank */
    activate_i2c_core_on_bank(self, false);
}

static int handle_pre_burst_flags(struct controller_base * ctrl, uint32_t flags) {
    struct i2c_master_core * self = downcast(ctrl, struct i2c_master_core);

    /* flags must be handled in first write shot */

    return STATUS_OK;
}

static int handle_post_burst_flags(struct controller_base * ctrl, uint32_t flags) {
    struct i2c_master_core * self = downcast(ctrl, struct i2c_master_core);

    /* flags must be handled in last shot */

    return STATUS_OK;
}

static int write_shot(struct controller_base * ctrl, const uint8_t * buf, int num_bytes) {
    DBG_TRACE_BEGIN_FCT;
    DBG1(if (ctrl->is_first_shot) pr_debug("menable: " DBG_NAME "   ** first shot\n"));
    DBG1(if (ctrl->is_last_shot) pr_debug("menable: " DBG_NAME "   ** last shot\n"));
    struct i2c_master_core * self = downcast(ctrl, struct i2c_master_core);

    if (num_bytes != 1) {
        pr_err(DBG_PRFX "Write shot with %d bytes, but only one byte allowed.\n", num_bytes);
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_OPERATION);
    }

    DBG_STMT(read_core_register(self, I2C_MC_REG_STATUS));

    uint8_t i2c_cmd = I2C_COMMAND_NONE;
    if (ctrl->is_first_shot
        && ctrl->are_burst_flags_set(ctrl, I2C_PRE_BURST_FLAG_START_CONDITION)) {
        i2c_cmd |= I2C_COMMAND_START;
    }
    if (ctrl->is_last_shot
        && ctrl->are_burst_flags_set(ctrl, I2C_POST_BURST_FLAG_STOP_CONDITION)) {
        i2c_cmd |= I2C_COMMAND_STOP;
    }

    int ret = write_byte(self, *buf, i2c_cmd);

    return DBG_TRACE_RETURN(ret);
}

static int request_read(struct controller_base * ctrl, size_t num_bytes) {
    if (num_bytes != 1) {
        pr_err(DBG_PRFX "Read request with %zu bytes, but only one byte allowed.\n", num_bytes);
        return STATUS_ERR_INVALID_OPERATION;
    }

    /* nop */
    return STATUS_OK;
}

static int read_shot(struct controller_base * ctrl, uint8_t * buffer, size_t num_bytes) {
    DBG_TRACE_BEGIN_FCT;
    DBG1(if (ctrl->is_first_shot) pr_debug("menable: " DBG_NAME "   ** first shot\n"));
    DBG1(if (ctrl->is_last_shot) pr_debug("menable: " DBG_NAME "   ** last shot\n"));
    struct i2c_master_core * self = downcast(ctrl, struct i2c_master_core);

    if (num_bytes != 1) {
        pr_err(DBG_PRFX "Read shot with %zu bytes, but only one byte allowed.\n", num_bytes);
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_OPERATION);
    }

    /* The last read shot has to send a NACK and possibly a stop condition */
    uint8_t i2c_cmd = I2C_COMMAND_NONE;
    if (ctrl->is_last_shot) {
        if (ctrl->are_burst_flags_set(ctrl, I2C_POST_BURST_FLAG_SEND_NACK)) {
            i2c_cmd |= I2C_COMMAND_NOT_ACKNOWLEDGE;
        }
        if (ctrl->are_burst_flags_set(ctrl, I2C_POST_BURST_FLAG_STOP_CONDITION)) {
            i2c_cmd |= I2C_COMMAND_STOP;
        }
    }

    int ret = read_byte(self, buffer, i2c_cmd);
    if (MEN_IS_ERROR(ret))
        return DBG_TRACE_RETURN(STATUS_ERROR);

    DBG1(pr_debug("menable: " DBG_NAME" read shot: 0x%02x   (bin)%s\n", *buffer, to_binary_8(*buffer)));

    return DBG_TRACE_RETURN(STATUS_OK);
}

static int wait_for_write_queue_empty(struct controller_base * ctrl) {
    /* there is no write queue */
    return STATUS_OK;
}

static void burst_aborted(struct controller_base * ctrl) {
    struct i2c_master_core * self = downcast(ctrl, struct i2c_master_core);
    write_stop(self);
    activate_i2c_core_on_bank(self, false);
}

static bool is_bus_busy(struct i2c_master_core * self) {
    return core_has_status(self, I2C_MC_STATUS_MASK_BUS_STATUS, I2C_MC_STATUS_BUS_BUSY);
}

static bool is_core_enabled(struct i2c_master_core * self) {
    DBG_TRACE_BEGIN_FCT;

    uint8_t ctrl = read_core_register(self, I2C_MC_REG_CONTROL);
    if ((ctrl & CORE_CTRL_MASK_UNUSED_BITS) != 0) {
        pr_err(DBG_PRFX "Control register contains invalid value: 0x%x (disallowed bits: 0x%x\n", ctrl, CORE_CTRL_MASK_UNUSED_BITS);
    }

    return DBG_TRACE_RETURN((ctrl & CORE_CTRL_MASK_ENABLE) == CORE_CTRL_ENABLED);
}

static int stop_transfer_if_busy(struct i2c_master_core * self) {
    DBG_TRACE_BEGIN_FCT;

    if (is_bus_busy(self)) {
        int ret = write_stop(self);
        if (MEN_IS_ERROR(ret))
            return DBG_TRACE_END_FCT , STATUS_ERROR;
    }
    DBG_TRACE_END_FCT;
    return STATUS_OK;
}

static int enable_core(struct i2c_master_core * self)
{
    DBG_TRACE_BEGIN_FCT;
    if (!is_core_enabled(self)) {
        modify_rw_register(self, I2C_MC_REG_CONTROL, CORE_CTRL_MASK_ENABLE, CORE_CTRL_ENABLED);
    }

    /* just in case ... */
    int ret = stop_transfer_if_busy(self);

    DBG_TRACE_END_FCT;
    return ret;
}

static int disable_core(struct i2c_master_core * self) {
    DBG_TRACE_BEGIN_FCT;
    /* if enabled, release the bus in case the controller is currently using it */
    if (is_core_enabled(self)) {
        stop_transfer_if_busy(self);
        /* TODO Error Handling: Can writing the control register fail? */
        modify_rw_register(self, I2C_MC_REG_CONTROL, CORE_CTRL_MASK_ENABLE, CORE_CTRL_DISABLED);
    }

    DBG_TRACE_END_FCT;
    return STATUS_OK;
}

static uint32_t frequency_bus2core(uint32_t bus_frequency, uint32_t fw_clock_frequency)
{
    return (fw_clock_frequency / (5 * bus_frequency)) - 1;
}

static uint32_t frequency_core2bus(uint32_t core_frequency, uint32_t fw_clock_frequency)
{
    return fw_clock_frequency / (5 * (core_frequency + 1));
}

static int adjust_core_frequency(struct i2c_master_core * self, uint32_t bus_frequency) {
    DBG_TRACE_BEGIN_FCT;

    if (self->active_bus == NULL) {
        pr_err(DBG_PRFX "Cannot adjust core frequency. No active bus.\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_STATE);
    }

    uint32_t core_freq = (self->firmware_clock_frequency / (5 * bus_frequency)) - 1;
    DBG_STMT(pr_debug("menable: " DBG_NAME "adjust core prescaler to 0x%04x (fpga: %dHz, bus: %dHz)\n",
                 core_freq, self->firmware_clock_frequency, bus_frequency));

    if (core_freq >= 0x10000) {
        pr_err(DBG_PRFX "Cannot adjust core frequency to %u. Frequency too high\n", bus_frequency);
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
    }

    write_core_register(self, I2C_MC_REG_PRESCALE_LOW, core_freq & 0xFF);
    write_core_register(self, I2C_MC_REG_PRESCALE_HIGH, (core_freq >> 8) & 0xFF);

    /* verify the frequency */
    uint32_t new_core_freq = (uint16_t) read_core_register(self, I2C_MC_REG_PRESCALE_LOW)
                             | (uint16_t) read_core_register(self, I2C_MC_REG_PRESCALE_HIGH) << 8;

    const bool success = (new_core_freq == core_freq);
    if (!success) {
        pr_err(DBG_PRFX "Failed to set core frequency to %u (%u Hz). Board reports %u (%u Hz)\n",
               core_freq, bus_frequency, new_core_freq, frequency_core2bus(new_core_freq, self->firmware_clock_frequency));
    }

    const int result = success ? STATUS_OK : STATUS_ERROR;
    return DBG_TRACE_RETURN(result);
}

/* API method */
static void activate_bank(struct i2c_master_core * self, uint8_t bank_number) {
    DBG_TRACE_BEGIN_FCT;

    if (self == NULL) {
        pr_err(DBG_PRFX "Argument `self` must not be NULL.\n");
        DBG_TRACE_END_FCT;
        return;
    }

    if (self->bus_configurations[bank_number].bus_frequency == 0) {
        pr_err(DBG_PRFX "Attempt to activate uninitialized bank\n");
        DBG_TRACE_END_FCT;
        return;
    }

    /* only reconfigure if the bank is not already active */
    if (self->active_bus != &self->bus_configurations[bank_number]) {
        /* TODO If applicable, wait for active transfer to complete? */
        disable_core(self);
        self->active_bus = &self->bus_configurations[bank_number];
        adjust_core_frequency(self, self->active_bus->bus_frequency);
        enable_core(self);
    }
    DBG_TRACE_END_FCT;
}

static void cleanup(struct controller_base * ctrl) {
    struct i2c_master_core * self = downcast(ctrl, struct i2c_master_core);
    disable_core(self);
}

/* API function */
int i2c_master_core_init(struct i2c_master_core * core,
                         struct register_interface * ri,
                         user_mode_lock * lock,
                         uint32_t i2c_ctrl_address_register,
                         uint32_t i2c_ctrl_write_register,
                         uint32_t i2c_ctrl_read_register,
                         uint32_t firmware_clock_frequency,
                         uint32_t num_safety_writes) {
    DBG_TRACE_BEGIN_FCT;
    DBG_STMT(pr_debug(KBUILD_MODNAME ": address register=%04x, write register=%04x, read register=%04x\n",
                 i2c_ctrl_address_register, i2c_ctrl_write_register, i2c_ctrl_read_register));

    if (core == NULL) {
        pr_err(DBG_PRFX "Cannot init i2c core. Argument `core` is NULL.\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
    }

    if (ri == NULL) {
        pr_err(DBG_PRFX "Cannot init i2c core. Argument `ri` is NULL.\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
    }

    if (lock == NULL) {
        pr_err(DBG_PRFX "Cannot init i2c core. Argument `ri` is NULL.\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
    }

    controller_base_init(upcast(core), ri, lock,
                         I2C_CORE_MAX_READS_PER_BURST,
                         I2C_CORE_MAX_BYTES_PER_READ,
                         I2C_CORE_MAX_WRITES_PER_BURST,
                         I2C_CORE_MAX_BYTES_PER_WRITE,
                         handle_begin_transaction,
                         handle_end_transaction,
                         handle_pre_burst_flags,
                         handle_post_burst_flags,
                         write_shot,
                         request_read, read_shot,
                         controller_base_execute_command_not_supported,
                         wait_for_write_queue_empty,
                         burst_aborted, cleanup);

    /* data members */
    core->num_safety_writes = (uint8_t) num_safety_writes;
    core->i2c_ctrl_address_register = i2c_ctrl_address_register;
    core->i2c_ctrl_write_register = i2c_ctrl_write_register;
    core->i2c_ctrl_read_register = i2c_ctrl_read_register;
    core->firmware_clock_frequency = firmware_clock_frequency;
    core->active_bus = NULL;
    memset(&core->bus_configurations, 0, sizeof(core->bus_configurations));

    /* methods */
    core->activate_bank = activate_bank;

    /*
     * It seems that on the first read after startup,
     * reading the control register returns invalid data
     * (0x21 = 0010 0001). When reading a second time,
     * 0x00 is returned as expected.
     */
    read_core_register(core, I2C_MC_REG_CONTROL);
    DBG_STMT(read_core_register(core, I2C_MC_REG_STATUS));

    DBG_STMT(pr_debug("menable: " DBG_NAME " Enable i2c core\n"));
    int ret = enable_core(core);
    if (MEN_IS_ERROR(ret)) {
        pr_err("menable: " DBG_NAME " init failed. Could re-enable i2c core.\n");
        return DBG_TRACE_RETURN(ret);
    }

    return DBG_TRACE_RETURN(STATUS_OK);
}

/* API function */
int i2c_master_core_configure_bus(struct i2c_master_core * core,
                                  uint8_t bank_number,
                                  uint8_t bank_activation_bitmask,
                                  uint8_t write_enable_bitmask,
                                  uint32_t bus_frequency) {
    DBG_TRACE_BEGIN_FCT;

    if (core == NULL) {
        pr_err(DBG_PRFX "Cannot confingure i2c bus. Argument `core` is NULL.\n");
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
    }

    if (bank_number >= 8) {
        pr_err(DBG_PRFX "Attempt to configure i2c master core with invalid bank number %u\n", bank_number);
        return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
    }

    struct i2c_master_core_bus_cfg * cfg = &core->bus_configurations[bank_number];
    cfg->bank_number = bank_number;
    cfg->bank_activation_bit = bank_activation_bitmask;
    cfg->write_enable_bit = write_enable_bitmask;
    cfg->bus_frequency = bus_frequency;

    return DBG_TRACE_RETURN(STATUS_OK);
}

