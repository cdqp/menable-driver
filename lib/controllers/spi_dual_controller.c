/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#include "../controllers/spi_dual_controller.h"

#include "../fpga/register_interface.h"
#include "../helpers/helper.h"
#include "../os/assert.h"
#include "../os/string.h"
#include "../os/time.h"

/* debugging */
//#define DBG_SPI_DUAL_CTRL
#ifdef DBG_SPI_DUAL_CTRL
#undef DEBUG
#define DEBUG

#undef DBG_LEVEL
#define DBG_LEVEL 1

#define DBG_NAME "[SPI DUAL CORE] "
#endif

#include "../helpers/dbg.h"
#include "../os/print.h"

#define SPI_READ_FIFO_LENGTH 1
#define SPI_BYTES_PER_READ (256 * 2)

#define SPI_WRITE_FIFO_LENGTH 1
#define SPI_BYTES_PER_WRITE (256 * 2)

// SPI register bits
enum SpiRegisterBits {
    SPI_REG_MOSI_0 = 0x1,
    SPI_REG_MISO_0 = 0x2,
    SPI_REG_WPn_0 = 0x4,
    SPI_REG_HOLDn_0 = 0x8,
    SPI_REG_MOSI_1 = 0x10,
    SPI_REG_MISO_1 = 0x20,
    SPI_REG_WPn_1 = 0x40,
    SPI_REG_HOLDn_1 = 0x80,
    SPI_REG_CLK = 0x100,
    SPI_REG_RD_WRn = 0x200,
    SPI_REG_CSn = 0x400,

    SPI_REG_NAND_CEn = 0x1000,  // NAND interface disabled
    SPI_REG_NAND_WEn = 0x4000,  // NAND interface disabled

    SPI_REG_IDLE =
        SPI_REG_NAND_CEn | SPI_REG_NAND_WEn | SPI_REG_CSn | SPI_REG_RD_WRn | SPI_REG_CLK | SPI_REG_HOLDn_1 | SPI_REG_HOLDn_0,

    SPI_REG_ACTIVE_SINGLE = SPI_REG_NAND_CEn | SPI_REG_NAND_WEn | SPI_REG_HOLDn_1 | SPI_REG_HOLDn_0,
    SPI_REG_ACTIVE_QUAD = SPI_REG_NAND_CEn | SPI_REG_NAND_WEn,
};

// Select register bits
enum SelectRegisterBits {
    SEL_REG_SPI = 0x1,
    SEL_REG_NONE = 0x3,
};

enum spi_cs_status { SPI_CS_ON, SPI_CS_OFF };

static inline void write_falsh_select_register(struct spi_dual_controller* self, uint32_t value, uint32_t ntimes)
{
    struct register_interface* ri = self->base_type.register_interface;
    for (uint32_t i = 0; i < ntimes; ++i)
        ri->write(ri, self->flash_select_register, value);
}

static inline void write_ctrl_register(struct spi_dual_controller* self, uint32_t value, uint32_t ntimes)
{
    struct register_interface* ri = self->base_type.register_interface;
    for (uint32_t i = 0; i < ntimes; ++i)
        ri->write(ri, self->control_register, value);
}

static inline uint32_t read_ctrl_register(struct spi_dual_controller* self)
{
    struct register_interface* ri = self->base_type.register_interface;
    return ri->read(ri, self->control_register);
}

static void set_chipselect(struct spi_dual_controller* self, enum spi_cs_status status)
{
    if (status == SPI_CS_ON) {
        if (!self->chip_selected) {
            // select spi register
            write_falsh_select_register(self, SEL_REG_SPI, 5);
            // Pull CS# low when CLK is high
            write_ctrl_register(self, SPI_REG_ACTIVE_SINGLE | SPI_REG_CLK, 10);
            self->chip_selected = true;
        }
    } else {
        // Raise CS# high when CLK is high
        write_ctrl_register(self, SPI_REG_IDLE, 10);
        // deselect spi register
        write_falsh_select_register(self, SEL_REG_NONE, 5);
        self->chip_selected = false;
    }

    DBG_STMT(pr_debug(KBUILD_MODNAME ": spi dual controller, chip select set to %s\n", status == SPI_CS_ON ? "on" : "off"));
}

static int spi_dual_handle_pre_burst_flags(struct controller_base* ctrl, uint32_t flags)
{
    struct spi_dual_controller* self = downcast(ctrl, struct spi_dual_controller);
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN spi dual controller, handle pre burst flags, flags %#X\n", flags));
    set_chipselect(self, SPI_CS_ON);
    DBG_STMT(pr_debug(KBUILD_MODNAME ": END spi dual controller, handle pre burst flags.\n"));
    return 0;
}

static int spi_dual_handle_post_burst_flags(struct controller_base* ctrl, uint32_t flags)
{
    struct spi_dual_controller* self = downcast(ctrl, struct spi_dual_controller);
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN spi dual controller, handle post burst flags, flags %#X\n", flags));
    if ((flags & SPI_POST_BURST_FLAG_LEAVE_CS_ASSERTED) == 0) {
        set_chipselect(self, SPI_CS_OFF);
    }
    DBG_STMT(pr_debug(KBUILD_MODNAME ": END spi dual controller, handle post burst flags.\n"));
    return 0;
}

static void write_chip_single(struct spi_dual_controller* self, unsigned char data)
{
    const unsigned char mask = 0x00;
    //(self->chip_index == -1) ? 0x00 : ((self->chip_index == 0) ? SPI_REG_MOSI_1 : SPI_REG_MOSI_0);
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN spi dual controller, write single mode, chip index=%i, data=%#X\n", -1,
                      data));

    // Set direction to write and raise WP#
    write_ctrl_register(self, SPI_REG_ACTIVE_SINGLE | SPI_REG_CLK | SPI_REG_WPn_1 | SPI_REG_WPn_0, 5);

    // Write MSB first ...
    for (unsigned char bit = 0x80; bit != 0; bit >>= 1) {
        const unsigned char c = (data & bit) == bit ? (SPI_REG_MOSI_0 | SPI_REG_MOSI_1) : mask;

        // We produce one clock cycle, setting up data when CLK is low, and leaving data "dangling"
        write_ctrl_register(self, SPI_REG_ACTIVE_SINGLE | SPI_REG_WPn_1 | SPI_REG_WPn_0, 5);                    // CLK low
        write_ctrl_register(self, SPI_REG_ACTIVE_SINGLE | SPI_REG_WPn_1 | SPI_REG_WPn_0 | c, 5);                // Setup data
        write_ctrl_register(self, SPI_REG_ACTIVE_SINGLE | SPI_REG_CLK | SPI_REG_WPn_1 | SPI_REG_WPn_0 | c, 5);  // CLK high
        write_ctrl_register(self, SPI_REG_ACTIVE_SINGLE | SPI_REG_CLK | SPI_REG_WPn_1 | SPI_REG_WPn_0, 5);
    }
    DBG_STMT(pr_debug(KBUILD_MODNAME ": END spi dual controller, write single mode.\n"));
}

static uint32_t write_quad_data(struct spi_dual_controller* self, const unsigned char* data, size_t length)
{
    assert(length != 0);

    const unsigned char c = data[0];
    const unsigned char d = (length > 1) ? data[1] : 0xff;
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN spi dual controller, write quad mode, length=%zu, data[0]=%#X, data[1]=%#X\n", length,
                      c, d));

    // Set direction to write and raise WP#
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | SPI_REG_CLK, 5);

    // We produce two clock cycles, setting up data when CLK is low, and leaving data "dangling"
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD, 5);                    // CLK low
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | c, 5);                // Setup data
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | SPI_REG_CLK | c, 5);  // CLK high
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | SPI_REG_CLK, 5);
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD, 5);                    // CLK low
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | d, 5);                // Setup data
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | SPI_REG_CLK | d, 5);  // CLK high
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | SPI_REG_CLK, 5);

    DBG_STMT(pr_debug(KBUILD_MODNAME ": END spi dual controller, write quad mode.\n"));
    return (length > 1) ? 2 : 1;
}

static int spi_dual_write_shot(struct controller_base* ctrl, const unsigned char* buffer, int length)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN spi dual controller, write shot, length=%i\n", length));
    assert(length <= SPI_BYTES_PER_WRITE);
    struct spi_dual_controller* self = downcast(ctrl, struct spi_dual_controller);

    if ((ctrl->current_burst_flags & SPI_BURST_FLAG_DATA_ACCESS) == SPI_BURST_FLAG_DATA_ACCESS) {
        if ((ctrl->current_burst_flags & SPI_BURST_FLAG_QUAD_MODE) != SPI_BURST_FLAG_QUAD_MODE) {
            pr_err(KBUILD_MODNAME ": Error, spi dual write shot, data access only allowed in quad mode");
            return -1;
        }

        for (int i = 0, written = 0; i < length; i += written) {
            written = write_quad_data(self, buffer + i, length - i);
        }
    } else {
        if ((ctrl->current_burst_flags & SPI_BURST_FLAG_QUAD_MODE) == SPI_BURST_FLAG_QUAD_MODE) {
            pr_err(KBUILD_MODNAME ": Error, spi dual writ shot, chip access only allowed in single mode");
            return -1;
        }

        for (int i = 0; i < length; ++i) {
            write_chip_single(self, buffer[i]);
        }
    }

    DBG_STMT(pr_debug(KBUILD_MODNAME ": END spi dual controller, write shot.\n"));
    return 0;
}

static int spi_dual_wait_for_write_fifo_empty(struct controller_base* ctrl)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN/END spi dual controller, wait for write fifo empty, no op.\n"));
    return 0;
}

static uint32_t read_chip_single(struct spi_dual_controller* self, unsigned char* buffer, size_t length)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN spi dual controller, read single mode, length=%zu\n", length));
    assert(length != 0);

    buffer[0] = 0;
    if (length > 1) {
        buffer[1] = 0;
    }

    // Set direction to read
    write_ctrl_register(self, SPI_REG_ACTIVE_SINGLE | SPI_REG_RD_WRn | SPI_REG_CLK, 5);

    // Read MSB first
    for (unsigned char bit = 0x80; bit != 0; bit >>= 1) {
        // We produce one clock cycle, reading the data when CLK is high
        write_ctrl_register(self, SPI_REG_ACTIVE_SINGLE | SPI_REG_RD_WRn, 10);               // CLK low
        write_ctrl_register(self, SPI_REG_ACTIVE_SINGLE | SPI_REG_RD_WRn | SPI_REG_CLK, 5);  // CLK high
        const unsigned int val = read_ctrl_register(self);

        if ((val & SPI_REG_MISO_0) != 0) {
            buffer[0] |= bit;
        }

        if ((val & SPI_REG_MISO_1) != 0 && length > 1) {
            buffer[1] |= bit;
        }
    }

    DBG_STMT(pr_debug(KBUILD_MODNAME ": END spi dual controller, read single mode. data[0]=%#X, data[1]=%#X\n", buffer[0],
                      (length > 1 ? buffer[1] : 0xff)));
    return (length > 1) ? 2 : 1;
}

static uint32_t read_quad_data(struct spi_dual_controller* self, unsigned char* buffer, size_t length)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN spi dual controller, read quad mode, length=%zu\n", length));
    assert(length != 0);

    // Set direction to read
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | SPI_REG_RD_WRn | SPI_REG_CLK, 5);

    // We produce two clock cycles, reading the data when CLK is high
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | SPI_REG_RD_WRn, 10);               // CLK low
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | SPI_REG_RD_WRn | SPI_REG_CLK, 5);  // CLK high
    buffer[0] = read_ctrl_register(self) & 0xff;
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | SPI_REG_RD_WRn, 10);               // CLK low
    write_ctrl_register(self, SPI_REG_ACTIVE_QUAD | SPI_REG_RD_WRn | SPI_REG_CLK, 5);  // CLK high
    unsigned char d = read_ctrl_register(self) & 0xff;

    if (length > 1) {
        buffer[1] = d;
    }

    DBG_STMT(pr_debug(KBUILD_MODNAME ": END spi dual controller, read quad mode. data[0]=%#X, data[1]=%#X\n", buffer[0],
                      (length > 1 ? buffer[1] : 0xff)));
    return (length > 1) ? 2 : 1;
}

static int spi_dual_read_shot(struct controller_base* ctrl, unsigned char* buffer, size_t length)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN spi dual controller, read shot, length=%zu\n", length));
    assert(length <= SPI_BYTES_PER_READ);
    struct spi_dual_controller* self = downcast(ctrl, struct spi_dual_controller);

    if ((ctrl->current_burst_flags & SPI_BURST_FLAG_DATA_ACCESS) == SPI_BURST_FLAG_DATA_ACCESS) {
        if ((ctrl->current_burst_flags & SPI_BURST_FLAG_QUAD_MODE) != SPI_BURST_FLAG_QUAD_MODE) {
            pr_err(KBUILD_MODNAME ": Error, spi dual read shot, data access only allowed in quad mode");
            return -1;
        }

        for (uint32_t i = 0, read = 0; i < length; i += read) {
            read = read_quad_data(self, buffer + i, length - i);
        }
    } else {
        if ((ctrl->current_burst_flags & SPI_BURST_FLAG_QUAD_MODE) == SPI_BURST_FLAG_QUAD_MODE) {
            pr_err(KBUILD_MODNAME ": Error, spi dual read shot, chip access only allowed in single mode");
            return -1;
        }

        for (uint32_t i = 0, read = 0; i < length; i += read) {
            read = read_chip_single(self, buffer + i, length - i);
        }
    }

    DBG_STMT(pr_debug(KBUILD_MODNAME ": END spi dual controller, read shot.\n"));
    return 0;
}

static int spi_dual_request_read(struct controller_base* ctrl, size_t num_bytes)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN/END spi dual controller, request read. no op.\n"));
    return 0;
}

static void spi_dual_burst_aborted(struct controller_base* ctrl)
{
    DBG_STMT(pr_debug(KBUILD_MODNAME ": BEGIN spi dual controller, burst aborted\n"));
    struct spi_dual_controller* self = downcast(ctrl, struct spi_dual_controller);
    set_chipselect(self, SPI_CS_OFF);
    DBG_STMT(pr_debug(KBUILD_MODNAME ": END spi dual controller, burst aborted.\n"));
}

static void spi_dual_cleanup(struct controller_base * ctrl) {
    struct spi_dual_controller* self = downcast(ctrl, struct spi_dual_controller);
    set_chipselect(self, SPI_CS_OFF);
}

int spi_dual_controller_init(struct spi_dual_controller* spi_dual,
                             struct register_interface* ri,
                             user_mode_lock * lock,
                             uint32_t control_register,
                             uint32_t flash_select_reg)
{
    struct controller_base* ctrl = &spi_dual->base_type;

    controller_base_init(ctrl, ri, lock,
                         SPI_READ_FIFO_LENGTH, SPI_BYTES_PER_READ,
                         SPI_WRITE_FIFO_LENGTH, SPI_BYTES_PER_WRITE,
                         NULL,
                         NULL,
                         spi_dual_handle_pre_burst_flags,
                         spi_dual_handle_post_burst_flags,
                         spi_dual_write_shot,
                         spi_dual_request_read,
                         spi_dual_read_shot,
                         controller_base_execute_command_not_supported,
                         spi_dual_wait_for_write_fifo_empty,
                         spi_dual_burst_aborted,
                         spi_dual_cleanup);

    spi_dual->control_register = control_register;
    spi_dual->flash_select_register = flash_select_reg;
    spi_dual->chip_selected = false;

    return 0;
}
