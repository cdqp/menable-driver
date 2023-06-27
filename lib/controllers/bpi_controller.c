/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#include "../controllers/bpi_controller.h"
#include "../helpers/error_handling.h"
#include "../helpers/timeout.h"
#include "../os/assert.h"
#include "../ioctl_interface/bpi_transaction_commands.h"

 /* debugging 
#define DBG_BPI_CTRL*/
#ifdef DBG_BPI_CTRL
#undef DEBUG
#define DEBUG

#undef DBG_LEVEL
#define DBG_LEVEL 1

#define DBG_TRACE_ON 1
#endif

#define DBG_NAME " : [BPI CONTROLLER]"
#define LOG_PREFIX KBUILD_MODNAME DBG_NAME

#include "../helpers/dbg.h"
#include "../os/print.h"

#define BPI_READ_FIFO_LENGTH 16
#define BPI_BYTES_PER_READ (64 * 1024 * 1024) //64MB

#define BPI_WRITE_FIFO_LENGTH 512
#define BPI_BYTES_PER_WRITE (64 * 1024 * 1024) //64MB

enum BpiOperationFlag {
    BPI_OPERATION_SELECT                  = 0x001,
    BPI_OPERATION_DESELECT                = 0x002,
    BPI_OPERATION_WAITREADY               = 0x004,
    BPI_OPERATION_SETADDRESS              = 0x008,
    BPI_OPERATION_WRITECMD                = 0x010,
    BPI_OPERATION_WRITECMDADDRESS         = 0x020,
    BPI_OPERATION_EMPTYFIFO               = 0x040,
    BPI_OPERATION_READDATA                = 0x080,
    BPI_OPERATION_WRITEDATA               = 0x100,
};

/* Command Register */
enum BpiCommandRegister {
    Write       = 0x010000,     // (Write) Start write cycle
    Read        = 0x020000,     // (Read) Start Read cycle
    AssertCS    = 0x040000,     // (Assert Chip Select) Sets CS to 0 at the beginning of the cycle
    DeassertCS  = 0x080000,     // (Deassert Chip Select) Sets CS to 1 at the end of the cycle	
    LoadAddress = 0x100000,     // (Load Address) 1: Loads the address counter. 0: Increment current address by 1
    WaitReady   = 0x200000,     // (Wait Ready) Reads Status Register until bit 7 (ready) is set
    Burst       = 0x400000,     // (Burst) Repeats the command according to the count given in Data-In register
};

/* BPI Status */
enum BpiStatusRegister {
    WriteFifoEmpty  = 0x010000,     // Write FIFO is empty
    WriteFifoFull   = 0x020000,     // Write FIFO is full
    WriteFifoError  = 0x040000,     // Written data to Write FIFO is lost (Written from PC)
    ReadFifoEmpty   = 0x080000,     // Read FIFO is empty
    ReadFifoFull    = 0x100000,     // Read FIFO is full
    ReadFifoError   = 0x200000,     // Written data to Read FIFO is lost (Written from BPI)
};

const uint32_t WRITE_COMMAND             = LoadAddress | DeassertCS | AssertCS | Write;  // 0x3A // Load the address and write to it
const uint32_t WRITE_INC_COMMAND         = DeassertCS | AssertCS | Write;  // 0x1A // Increment current address
const uint32_t READ_COMMAND              = LoadAddress | DeassertCS | AssertCS | Read;   // 0x3C // Load the address and read from it
const uint32_t READ_BURST_COMMAND        = Burst | LoadAddress | DeassertCS | AssertCS | Read;   // 0xBC // Load the address and read from it up to 16 words
const uint32_t READ_INC_COMMAND          = DeassertCS | AssertCS | Read;   // 0x1C // Read from the incremented address
const uint32_t READ_BURST_INC_COMMAND    = Burst | DeassertCS | AssertCS | Read;   // 0x5C // Read from the incremented address up to 16 words
const uint32_t WAIT_COMMAND              = WaitReady | LoadAddress | DeassertCS | AssertCS | Read;   // 0x7C // Continuously read status register until it's ready
const uint32_t READ_TIME_OUT             = 100;

const uint32_t BANK_NUMBER_MASK = 0x7;
const uint32_t CPLD_BUSY_MASK = 0x8;

#define IS_CPLD_BUSY(register_value) (((register_value) & CPLD_BUSY_MASK) != 0)

static inline void write_address_register(struct bpi_controller *bpi_ctrl, uint32_t address)
{
    uint32_t val = address & bpi_ctrl->address_mask;
    struct register_interface *ri = bpi_ctrl->base_type.register_interface;
    ri->write(ri, bpi_ctrl->address_register, val);
}

static inline uint32_t read_address_register(struct bpi_controller *bpi_ctrl)
{
    struct register_interface *ri = bpi_ctrl->base_type.register_interface;
    uint32_t value = ri->read(ri, bpi_ctrl->address_register);
    return value;
}

static int set_address(struct bpi_controller *bpi_ctrl, uint32_t adr)
{
    pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN set address. address=%X\n", adr);

    int ret = 0;
    uint8_t bank = (uint8_t)((adr >> bpi_ctrl->address_reg_width) & bpi_ctrl->bank_mask);
    if (bank != bpi_ctrl->selected_bank) {
        ret = bpi_ctrl->select_bank(bpi_ctrl, bank);
        if (ret != 0) {
            return ret;
        }
    }

    write_address_register(bpi_ctrl, adr);

    pr_debug(KBUILD_MODNAME DBG_NAME ": END set address.\n");
    return 0;
}

static inline void  write_data_register(struct bpi_controller* bpi_ctrl, uint32_t value)
{
    struct register_interface* ri = bpi_ctrl->base_type.register_interface;
    ri->write(ri, bpi_ctrl->data_register, value);
}

static inline uint32_t read_data_register(struct bpi_controller* bpi_ctrl)
{
    struct register_interface* ri = bpi_ctrl->base_type.register_interface;
    uint32_t value = ri->read(ri, bpi_ctrl->data_register);
    return value;
}

static inline uint8_t read_bank_register(struct bpi_controller *bpi_ctrl)
{
    struct register_interface *ri = bpi_ctrl->base_type.register_interface;
    uint32_t value = ri->read(ri, bpi_ctrl->bank_register);
    return (value & 0xF);
}

static inline void write_bank_register(struct bpi_controller *bpi_ctrl, uint8_t bank)
{
    struct register_interface* ri = bpi_ctrl->base_type.register_interface;
    ri->write(ri, bpi_ctrl->bank_register, bank & bpi_ctrl->bank_mask);

    // dummy reads to ensure that the write is completed when returning from this function.
    read_bank_register(bpi_ctrl);
    read_bank_register(bpi_ctrl);
}

static int get_active_bank(struct bpi_controller* bpi_ctrl) {

    return (read_bank_register(bpi_ctrl) & BANK_NUMBER_MASK);
}

static bool is_cpld_change_busy(struct bpi_controller * bpi_ctrl) {
    const uint8_t bank_register_value = read_bank_register(bpi_ctrl);
    return IS_CPLD_BUSY(bank_register_value);
}

static bool wait_for_cpld_change_completion(struct bpi_controller * bpi_ctrl) {
    static const uint32_t initial_timeout_millis = 1500;
    
    struct timeout timeout;
    timeout_init(&timeout, initial_timeout_millis);

    while (is_cpld_change_busy(bpi_ctrl) && !timeout_has_elapsed(&timeout)) {
        // TODO: The current thread could release control here.
    }

    return !is_cpld_change_busy(bpi_ctrl);
}

static bool write_bank_register_and_wait_for_completion(struct bpi_controller * bpi_ctrl, uint8_t bank_number) {
    write_bank_register(bpi_ctrl, bank_number);
    return wait_for_cpld_change_completion(bpi_ctrl);
}

static bool switch_bank(struct bpi_controller *bpi_ctrl, uint8_t bank_number) {
    bool change_succeeded = write_bank_register_and_wait_for_completion(bpi_ctrl, bank_number);
    if (change_succeeded) {
        // Writing to the CPLD may fail (probability ~0.5 %) without a possibility of verification.
        // When writing a second time, the measured chance of failure was reduced to ~0.000006 %.
        change_succeeded = write_bank_register_and_wait_for_completion(bpi_ctrl, bank_number);
    }

    return change_succeeded;
}

static int select_bank(struct bpi_controller *bpi_ctrl, uint8_t bank_number)
{
    DBG_TRACE_BEGIN_FCT;

    pr_debug(LOG_PREFIX "Select Bank %d", bank_number);

    if (bank_number >= bpi_ctrl->bank_count) {
        pr_err(LOG_PREFIX "Error, bank number is greater than expected one\n");
        return STATUS_ERR_INVALID_ARGUMENT;
    }

    const bool change_succeeded = switch_bank(bpi_ctrl, bank_number);
    if (!change_succeeded) {
        pr_err(LOG_PREFIX "Error, time out by reading bank register\n");
        return DBG_TRACE_RETURN(STATUS_ERR_DEV_IO);
    }

    bpi_ctrl->selected_bank = bank_number;

    return DBG_TRACE_RETURN(STATUS_OK);
}

static int bpi_write_shot(struct controller_base* ctrl, const unsigned char* buffer, int length)
{
    struct bpi_controller* self = downcast(ctrl, struct bpi_controller);
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN write shot, length=%i, flag=%X\n", length, self->flags));
    assert(length <= BPI_BYTES_PER_WRITE);

    if (buffer == NULL || length == 0)
    {
        pr_err(KBUILD_MODNAME ": write shot, error buffer or length is invalid\n");
        return -1;
    }

    int ret = 0;

    if (self->flags & BPI_OPERATION_WRITECMD)
    {
        pr_debug(KBUILD_MODNAME DBG_NAME ": write shot, flag=WRITECMD[%X], length=%d, cmd=%X", self->flags, length, (uint16_t)buffer[0]);
        self->write_command(self, (uint16_t)buffer[0]);
    }

    if (self->flags & BPI_OPERATION_SETADDRESS)
    {
        uint32_t address = 0;
        for (int i = 1; i < length; ++i)
        {
            address |= buffer[i];
            if (i + 1 < length)
                address = address << 8;
        }

        self->address = address;
        pr_debug(KBUILD_MODNAME DBG_NAME ": write shot, flag=SETADDRESS[%X], length=%d, cmd=%X, address=%X", self->flags, length, buffer[0], self->address);
    }

    if (self->flags & BPI_OPERATION_WRITECMDADDRESS)
    {
        uint32_t address = 0;
        for (int i = 1; i < length; ++i)
        {
            address |= buffer[i];
            if (i + 1 < length)
                address = address << 8;
        }

        self->address = address;
        pr_debug(KBUILD_MODNAME DBG_NAME ": write shot, flag=WRITECMDADDRESS[%X], length=%d, cmd=%X, address=%X", self->flags, length, buffer[0], self->address);
        ret = self->write_command_address(self, self->address, buffer[0]);
    }

    if (self->flags & BPI_OPERATION_WRITEDATA)
    {
        pr_debug(KBUILD_MODNAME DBG_NAME ": write shot, flag=WRITEDATA[%X], length= %d", self->flags, length);

        uint16_t *localdata = (uint16_t*)buffer;
        pr_debug(KBUILD_MODNAME DBG_NAME ": write shot, word to write, data[0]=%X", localdata[0]);
        self->write_command(self, localdata[0]);
        for (int i = 1; i < length/2; ++i)
        {
            pr_debug(KBUILD_MODNAME DBG_NAME ": write shot, word to write, data[%d]=%X", i, localdata[i]);
            self->write_command_increment_address(self, localdata[i]);
        }
    }

    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END write shot.\n"));
    return ret;
}

static int bpi_read_shot(struct controller_base* ctrl, unsigned char* buffer, size_t length)
{
    struct bpi_controller* self = downcast(ctrl, struct bpi_controller);
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN read shot, length=%d, flag=%X, address=%X\n", length, self->flags, self->address));
    assert(length <= BPI_BYTES_PER_READ);
    if (buffer == NULL || length == 0) {
        pr_err(KBUILD_MODNAME ": Error by checking buffer and length in bpi read shot\n");
        return -1;
    }
    int ret = 0;
    if (self->flags & BPI_OPERATION_READDATA)
    {
        uint16_t *buff = (uint16_t*)buffer;
        uint32_t readLength;
        uint32_t wordsLeft = length / 2;
        uint32_t index = 0;

        int ret = 0;
        while (wordsLeft > 0 && ret == 0) {

            readLength = (BPI_READ_FIFO_LENGTH > wordsLeft) ? wordsLeft : BPI_READ_FIFO_LENGTH;

            ret = self->read_data(self, self->address + index, &buff[index], readLength);

            wordsLeft -= readLength;
            index += readLength;
        }
    }

    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END read shot.\n"));
    return ret;
}

static int bpi_execute_command(struct controller_base * ctrl, command_burst_header * header, uint8_t * command_data, size_t command_data_size) {

    bpi_controller * self = downcast(ctrl, bpi_controller);

    switch (header->command_id) {

    case BPI_COMMAND_GET_ACTIVE_BANK: {
        if (command_data_size < sizeof(bpi_get_active_bank_io)) {
            pr_err(LOG_PREFIX "Provided buffer is too small to hold the return value.");
            return STATUS_ERR_INVALID_ARGUMENT;
        }

        bpi_get_active_bank_io * command_io = (bpi_get_active_bank_io*)command_data;
        command_io->return_value = get_active_bank(self);

    } break;

    case BPI_COMMAND_SET_ACTIVE_BANK: {
        if (command_data_size < sizeof(bpi_set_active_bank_io)) {
            pr_err(LOG_PREFIX "Provided buffer is too small to hold the command arguments.");
            return STATUS_ERR_INVALID_ARGUMENT;
        }

        bpi_set_active_bank_io * command_io = (bpi_set_active_bank_io*)command_data;
        command_io->return_value = select_bank(self, command_io->args.bank_number);
    } break;
        
    default:
        pr_err(LOG_PREFIX "Invalid command id %u.", header->command_id);
        return STATUS_ERR_INVALID_ARGUMENT;
    }

    return STATUS_OK;
}

static int bpi_handle_pre_burst_flags(struct controller_base* ctrl, uint32_t flags)
{
    struct bpi_controller* self = downcast(ctrl, struct bpi_controller);
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN handle pre burst flags, flags %#X\n", flags));
    self->flags = flags;
    if (flags & BPI_OPERATION_SELECT)
    {
        self->invalidate_bank(self);
    }

    int ret = 0;
    if (flags & BPI_OPERATION_WAITREADY)
    {
        ret = self->wait_ready(self);
    }

    if (flags & BPI_OPERATION_EMPTYFIFO)
    {
        self->empty_fifo(self);
    }

    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END handle pre burst flags.\n"));
    return ret;
}

static int bpi_handle_post_burst_flags(struct controller_base* ctrl, uint32_t flags)
{
    struct bpi_controller* self = downcast(ctrl, struct bpi_controller);
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN handle post burst flags, flags %#X\n", flags));
    if (flags & BPI_OPERATION_DESELECT)
    {
        self->invalidate_bank(self);
    }

    self->flags = 0;
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END handle post burst flags.\n"));
    return 0;
}

static int bpi_wait_for_write_fifo_empty(struct controller_base* ctrl)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN/END wait for write fifo empty, no op.\n"));
    return 0;
}

static int bpi_request_read(struct controller_base* ctrl, size_t num_bytes)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN/END request read. no op.\n"));
    return 0;
}

static void bpi_cleanup(struct controller_base * ctrl) {
    struct bpi_controller* self = downcast(ctrl, struct bpi_controller);
    self->invalidate_bank(self);
    self->address = 0XFFFFFFFF;
    self->flags = 0;
}

static void bpi_burst_aborted(struct controller_base* ctrl)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN burst aborted\n"));
    bpi_cleanup(ctrl);
    DBG_STMT(pr_debug(KBUILD_MODNAME DBG_NAME ": END burst aborted.\n"));
}

static void invalidate_bank(struct bpi_controller * bpi_ctrl)
{
    DBG_TRACE_BEGIN_FCT;
    bpi_ctrl->selected_bank = 0xFF;
    DBG_TRACE_END_FCT;
}

static int32_t wait_ready(struct bpi_controller *bpi_ctrl)
{
    pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN wait ready\n");
    // Typical/Max. time for Program commands:      0.9/3.1 ms
    // Typical      time for Blank check command:   3.2 ms
    // Typical/Max. time for Erase commands:        0.8/4 s

    uint32_t r;

    // Worst case timeout is for the Erase process,
    // might take up to 4 seconds.
    uint32_t TIMEOUT_MS = 5000;

    write_data_register(bpi_ctrl, WAIT_COMMAND);

    struct timeout timeout;
    timeout_init(&timeout, TIMEOUT_MS);

    do {
        r = read_data_register(bpi_ctrl);
        if ((r & ReadFifoFull) != 0) {
            DBG_STMT(pr_info(KBUILD_MODNAME DBG_NAME ": read fifo reaching critical level\n"));
        }
        if ((r & ReadFifoError) != 0) {
            DBG_STMT(pr_err(KBUILD_MODNAME DBG_NAME ": read fifo overflow\n"));
        }
        if ((r & WriteFifoFull) != 0) {
            DBG_STMT(pr_info(KBUILD_MODNAME DBG_NAME ": write fifo reaching critical level\n"));
        }
        if ((r & WriteFifoError) != 0) {
            DBG_STMT(pr_err(KBUILD_MODNAME DBG_NAME ": write fifo overflow\n"));
        }
    } while ((r & ReadFifoEmpty) && !timeout_has_elapsed(&timeout));


    if (r & ReadFifoEmpty) { // BPI is not responding
        DBG_STMT(pr_err(KBUILD_MODNAME DBG_NAME ": Bpi is not responding\n"));
        return -1;
    }
    else if ((r & 0x80) == 0) {   // NOT Ready (should never happen)
        DBG_STMT(pr_err(KBUILD_MODNAME DBG_NAME ": Bpi is not ready\n"));
        return -1;
    }

    pr_debug(KBUILD_MODNAME DBG_NAME ": END wait ready.\n");
    return 0;
}

static void write_command(struct bpi_controller *bpi_ctrl, uint16_t cmd)
{
    pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN write command, cmd=%X\n", cmd);
    write_data_register(bpi_ctrl, WRITE_COMMAND | cmd);
    pr_debug(KBUILD_MODNAME DBG_NAME ": END write command.\n");
}

static int write_command_address(struct bpi_controller *bpi_ctrl, unsigned int adr, uint16_t cmd)
{
    pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN write command address. address=%X, cmd=%X\n", adr, cmd);
    int ret;

    ret = bpi_ctrl->set_address(bpi_ctrl, (uint32_t)(adr));
    if (ret) {
        return ret;
    }
    bpi_ctrl->write_command(bpi_ctrl, cmd);

    pr_debug(KBUILD_MODNAME DBG_NAME ": END write command address.\n");
    return 0;
}

static void write_command_increment_address(struct bpi_controller *bpi_ctrl, uint16_t cmd)
{
    pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN write command increment address. cmd=%X\n", cmd);
    write_data_register(bpi_ctrl, WRITE_INC_COMMAND | cmd);
    pr_debug(KBUILD_MODNAME DBG_NAME ": END write command increment address.\n");
}

static int read_data(struct bpi_controller *bpi_ctrl, unsigned int adr, uint16_t * data, unsigned int length)
{
    pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN read data. address=%X, length=%d", adr, length);

    uint32_t readBuffer, i;
    int ret = bpi_ctrl->set_address(bpi_ctrl, (uint32_t)adr);
    if (ret) {
        return ret;
    }

    // We send at least 4 commands before start reading, in order to get
    // the data on the 1st read.
    write_data_register(bpi_ctrl, READ_COMMAND);
    if (length > 1) {
        write_data_register(bpi_ctrl, READ_INC_COMMAND);
        if (length < 5) {
            for (i = 0; i < length - 2; i++) {
                write_data_register(bpi_ctrl, READ_INC_COMMAND);
            }
        }
        else {
            write_data_register(bpi_ctrl, READ_INC_COMMAND);
            write_data_register(bpi_ctrl, READ_BURST_INC_COMMAND | (length - 4));
        }
    }

    struct timeout timeout;
    for (i = 0; i < length; i++) {
        timeout_init(&timeout, READ_TIME_OUT);
        do {
            readBuffer = read_data_register(bpi_ctrl);
            if ((readBuffer & ReadFifoFull) != 0) {
                DBG_STMT(pr_info(KBUILD_MODNAME DBG_NAME ": BPI read fifo reaching critical level"));
            }
            if ((readBuffer & ReadFifoError) != 0) {
                DBG_STMT(pr_err(KBUILD_MODNAME DBG_NAME ": BPI read fifo overflow"));
            }
            if ((readBuffer & WriteFifoFull) != 0) {
                DBG_STMT(pr_info(KBUILD_MODNAME DBG_NAME ": BPI write fifo reaching critical level"));
            }
            if ((readBuffer & WriteFifoError) != 0) {
                DBG_STMT(pr_err(KBUILD_MODNAME DBG_NAME ": BPI write fifo overflow"));
            }
        } while ((readBuffer & ReadFifoEmpty) && !timeout_has_elapsed(&timeout));
        if (timeout_has_elapsed(&timeout)) {
            DBG_STMT(pr_err(KBUILD_MODNAME DBG_NAME ": BPI time out by reading data\n"));
            return -1;
        }
        data[i] = readBuffer & 0xFFFF;
    }

    pr_debug(KBUILD_MODNAME DBG_NAME ": END read data\n");
    return 0;
}

static int empty_fifo(struct bpi_controller * bpi_ctrl)
{
    pr_debug(KBUILD_MODNAME DBG_NAME ": BEGIN empty fifo\n");
    uint32_t registerValue;
    struct timeout timeout;
    timeout_init(&timeout, 1500);

    do {
        registerValue = read_data_register(bpi_ctrl);
    } while ((!(registerValue & WriteFifoEmpty) || !(registerValue & ReadFifoEmpty)) && !timeout_has_elapsed(&timeout));

    if (timeout_has_elapsed(&timeout)) {
        DBG_STMT(pr_err(KBUILD_MODNAME DBG_NAME": Error by empty fifo. timeout\n"));
        return -1;
    }
    pr_debug(KBUILD_MODNAME DBG_NAME ": END empty fifo\n");
    return 0;
}

int bpi_controller_init(struct bpi_controller* bpi_ctr,
    struct register_interface* ri,
    user_mode_lock * lock,
    uint32_t address_register,
    uint32_t bpi_data_register,
    uint32_t bpi_bank_selection_status_register,
    uint32_t address_width,
    uint32_t bank_width)
{
    struct controller_base* ctrl = &bpi_ctr->base_type;

    controller_base_init(ctrl, ri, lock,
        BPI_READ_FIFO_LENGTH, BPI_BYTES_PER_READ,
        BPI_WRITE_FIFO_LENGTH, BPI_BYTES_PER_WRITE,
        NULL,
        NULL,
        bpi_handle_pre_burst_flags,
        bpi_handle_post_burst_flags,
        bpi_write_shot,
        bpi_request_read,
        bpi_read_shot,
        bpi_execute_command,
        bpi_wait_for_write_fifo_empty,
        bpi_burst_aborted,
        bpi_cleanup);

    /* init member functions */
    bpi_ctr->invalidate_bank = invalidate_bank;
    bpi_ctr->wait_ready = wait_ready;
    bpi_ctr->write_command = write_command;
    bpi_ctr->write_command_address = write_command_address;
    bpi_ctr->write_command_increment_address = write_command_increment_address;
    bpi_ctr->set_address = set_address;
    bpi_ctr->select_bank = select_bank;
    bpi_ctr->get_active_bank = get_active_bank;
    bpi_ctr->read_data = read_data;
    bpi_ctr->empty_fifo = empty_fifo;

    bpi_ctr->address_register = address_register;
    bpi_ctr->data_register = bpi_data_register;
    bpi_ctr->bank_register = bpi_bank_selection_status_register;
    bpi_ctr->selected_bank = 0XFF;
    bpi_ctr->address_reg_width = address_width - bank_width; // for 64MB = 22;
    bpi_ctr->address_mask = ((1u << bpi_ctr->address_reg_width) - 1); // =4194303;
    bpi_ctr->bank_mask = ((1 << bank_width) - 1); // = 7
    bpi_ctr->bank_count = (1 << bank_width); // = 8;
    bpi_ctr->address = 0XFFFFFFFF;
    bpi_ctr->flags = 0;

    return 0;
}

