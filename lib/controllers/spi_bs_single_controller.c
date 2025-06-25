/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#include "../controllers/spi_bs_single_controller.h"

#include "../os/assert.h"
#include "../os/string.h"
#include "../os/time.h"
#include "../os/print.h"
#include "../helpers/helper.h"
#include "../fpga/register_interface.h"

#ifdef DBG_BS_SINGLE_CTRL

    #ifndef DEBUG
    #define DEBUG
    #endif

    #define DBG_LEVEL 1
    #define DBG_NAME "[spi bs single] "

#endif

#include "../helpers/dbg.h"

#define SPI_SCK  0x00000001
#define SPI_CEn  0x00000002
#define SPI_MOSI 0x00000004
#define SPI_WPn  0x00000008
#define SPI_HOLD 0x00000010
#define SPI_SEL0 0x00000020
#define SPI_SEL1 0x00000040
#define SPI_OE   0x00000080
#define SPI_MISO 0x00000080

#define SPI_READ_FIFO_LENGTH  1
#define SPI_BYTES_PER_READ    1

#define SPI_WRITE_FIFO_LENGTH 1
#define SPI_BYTES_PER_WRITE   1

#define SPI_READ              SPI_OE | SPI_WPn
#define SPI_WRITE             SPI_OE | SPI_WPn

#define SPI_CHIPSELECT_OFF    SPI_CEn
#define SPI_CHIPSELECT_ON     SPI_OE | SPI_WPn

#define SPI_REG_WRITE_CS_OFF  20

enum spi_cs_status {
    SPI_CS_ON,
    SPI_CS_OFF
};

static inline void
spi_bs_single_write_ctrl_register(struct spi_bs_single_controller * self,
                                  uint32_t value) {
    struct register_interface * ri = self->base_type.register_interface;
    ri->write(ri, self->control_register, value);
}

static inline uint32_t
spi_bs_single_read_ctrl_register(struct spi_bs_single_controller * self) {
    struct register_interface * ri = self->base_type.register_interface;
    return ri->read(ri, self->control_register);
}

static int
spi_bs_single_wait_for_write_fifo_empty(struct controller_base * ctrl) {
    return 0;
}

static void
spi_bs_single_set_chipselect(struct spi_bs_single_controller * self,
                      enum spi_cs_status status) {
    if (status == SPI_CS_ON) {
        spi_bs_single_write_ctrl_register(self, SPI_CHIPSELECT_ON);
    } else {
        spi_bs_single_write_ctrl_register(self, SPI_CHIPSELECT_OFF);
        udelay(1);
    }
    pr_debug(DBG_PREFIX ": set spi chip select to %s\n", status == SPI_CS_ON ? "on" : "off");
}

static int
spi_bs_single_handle_pre_burst_flags(struct controller_base * ctrl, uint32_t flags) {
    struct spi_bs_single_controller * self = downcast(ctrl, struct spi_bs_single_controller);
    DBG_TRACE_BEGIN_FCT;
    spi_bs_single_set_chipselect(self, SPI_CS_ON);
    DBG_TRACE_END_FCT;
    return 0;
}

static int
spi_bs_single_handle_post_burst_flags(struct controller_base * ctrl, uint32_t flags) {
    struct spi_bs_single_controller * self = downcast(ctrl, struct spi_bs_single_controller);
    DBG_TRACE_BEGIN_FCT;
    if ((flags & SPI_POST_BURST_FLAG_LEAVE_CS_ASSERTED) == 0) {
        spi_bs_single_set_chipselect(self, SPI_CS_OFF);
    }

    /* TODO Error Handling */
    DBG_TRACE_END_FCT;
    return 0;
}

static int
spi_bs_single_write_shot(struct controller_base * ctrl,
                  const uint8_t * buffer, int num_bytes) {
    struct spi_bs_single_controller * self = downcast(ctrl, struct spi_bs_single_controller);
    uint8_t data = buffer[0];
    uint8_t mask;

    assert(num_bytes == SPI_BYTES_PER_WRITE);

    /* process data MSB first downto LSB */
    for (mask = 0x80; mask != 0; mask >>= 1) {
        if (data & mask) {
            spi_bs_single_write_ctrl_register(self, SPI_OE |           SPI_MOSI);
            spi_bs_single_write_ctrl_register(self, SPI_OE | SPI_SCK | SPI_MOSI);
            spi_bs_single_write_ctrl_register(self, SPI_OE | SPI_SCK | SPI_MOSI);
            spi_bs_single_write_ctrl_register(self, SPI_OE |           SPI_MOSI);
        } else {
            spi_bs_single_write_ctrl_register(self, SPI_OE          );
            spi_bs_single_write_ctrl_register(self, SPI_OE | SPI_SCK);
            spi_bs_single_write_ctrl_register(self, SPI_OE | SPI_SCK);
            spi_bs_single_write_ctrl_register(self, SPI_OE          );
        }
    }

    pr_debug(DBG_PREFIX ": written %02x to spi\n", (int) data);

    /* TODO Error Handling */
    return 0;
}

static int
spi_bs_single_request_read(struct controller_base * ctrl, size_t num_bytes) {
    return 0;
}

static int
spi_bs_single_read_shot(struct controller_base * ctrl,
                 uint8_t * buffer, size_t num_bytes) {
    struct spi_bs_single_controller * self = downcast(ctrl, struct spi_bs_single_controller);
    uint8_t data = 0;
    uint8_t mask;
    uint32_t val;

    assert(num_bytes == SPI_BYTES_PER_WRITE);

    /* process data MSB first downto LSB */
    for (mask = 0x80; mask != 0; mask >>= 1) {
        spi_bs_single_write_ctrl_register(self, SPI_OE |           SPI_MOSI);
        spi_bs_single_write_ctrl_register(self, SPI_OE | SPI_SCK | SPI_MOSI);
        val = spi_bs_single_read_ctrl_register(self);
        spi_bs_single_write_ctrl_register(self, SPI_OE |           SPI_MOSI);

        if (val & SPI_MISO) {
            data |= mask;
        }
    }

    buffer[0] = data;
    pr_debug(DBG_PREFIX ": read %02x from spi\n", (int) data);

    /* TODO Error Handling */
    return 0;
}

static void spi_bs_single_burst_aborted(struct controller_base * ctrl) {
    /* TODO implement */
}

static void spi_bs_single_cleanup(struct controller_base * ctrl) {
    /* TODO implement */
}

void spi_bs_single_data_transport_init(struct spi_bs_single_controller * spi_bs_single,
                                struct register_interface * ri,
                                user_mode_lock * lock,
                                uint32_t control_register) {
    struct controller_base * ctrl = &spi_bs_single->base_type;

    /* data transport */
    controller_base_init(ctrl, ri, lock,
                         SPI_READ_FIFO_LENGTH, SPI_BYTES_PER_READ,
                         SPI_WRITE_FIFO_LENGTH, SPI_BYTES_PER_WRITE,
                         NULL, NULL,
                         spi_bs_single_handle_pre_burst_flags,
                         spi_bs_single_handle_post_burst_flags,
                         spi_bs_single_write_shot,
                         spi_bs_single_request_read, spi_bs_single_read_shot,
                         controller_base_execute_command_not_supported,
                         spi_bs_single_wait_for_write_fifo_empty,
                         spi_bs_single_burst_aborted,
                         spi_bs_single_cleanup);

    spi_bs_single->control_register = control_register;
}
