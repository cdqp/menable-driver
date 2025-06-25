/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "../controllers/spi_v2_controller.h"

#include "../os/print.h"
#include "../os/string.h"
#include "../os/assert.h"
#include "../os/time.h"
#include "../helpers/error_handling.h"
#include "../helpers/helper.h"
#include "../helpers/type_hierarchy.h"
#include "../fpga/register_interface.h"

#ifdef DBG_SPI_V2_CTRL_OFF
 // OFF takes precedence over ON
#	undef DEBUG
#elif defined(DBG_SPI_V2_CTRL_ON)

    #ifndef DEBUG
    #define DEBUG
    #endif

    #define DBG_LEVEL 1

#endif
#define DBG_NAME "[SPI v2] "

#include "../helpers/dbg.h"

#define SPI_READ_FIFO_LENGTH  8
#define SPI_V2_BYTES_PER_READ 3

#define SPI_WRITE_FIFO_LENGTH 8
#define SPI_V2_BYTES_PER_WRITE 3

#define SPI_MASK_DATA           0x00FFFFFF
#define SPI_MASK_DATA_BITS      0x03000000
#define SPI_MASK_READ_WRITE     0x04000000
#define SPI_MASK_QUAD_MODE      0x08000000
#define SPI_MASK_CHIPSELECT     0x10000000
#define SPI_MASK_ACCESS_VALID   0x20000000
#define SPI_MASK_SELECT_DEVICE  0x40000000
#define SPI_MASK_WRITE_FIFO     0x04000000
#define SPI_MASK_READ_FIFO      0x08000000
#define SPI_MASK_ACTIVE_DEVICE  0x10000000
                                
#define SPI_DATA_8_BITS         0x00000000
#define SPI_DATA_16_BITS        0x01000000
#define SPI_DATA_24_BITS        0x02000000
                               
#define SPI_READ                SPI_MASK_READ_WRITE
#define SPI_WRITE               0
                                
#define SPI_QUAD_MODE_ON        SPI_MASK_QUAD_MODE
#define SPI_QUAD_MODE_OFF       0
                                
#define SPI_CHIPSELECT_OFF      SPI_MASK_CHIPSELECT
#define SPI_CHIPSELECT_ON       0
                                
#define SPI_ACCESS_VALID_ON     SPI_MASK_ACCESS_VALID
#define SPI_ACCESS_VALID_OFF    0
                                
#define SPI_READ_FIFO_EMPTY     SPI_MASK_READ_FIFO
#define SPI_WRITE_FIFO_EMPTY    SPI_MASK_WRITE_FIFO

#define SPI_DATA_IS_VALID(data)    (((data) & SPI_MASK_READ_FIFO) != SPI_READ_FIFO_EMPTY)
#define SPI_DATA_HAS_8_BITS(data)  (((data) & SPI_MASK_DATA_BITS) == SPI_DATA_8_BITS)
#define SPI_DATA_HAS_16_BITS(data) (((data) & SPI_MASK_DATA_BITS) == SPI_DATA_16_BITS)
#define SPI_DATA_HAS_24_BITS(data) (((data) & SPI_MASK_DATA_BITS) == SPI_DATA_24_BITS)

#define SPI_HEADER_STRING_MAX_LENGTH 0xFF

enum spi_cs_status {
    SPI_CS_ON,
    SPI_CS_OFF
};

static inline uint32_t get_data_size_flag(int num_bytes) {
    return (num_bytes - 1) << 24;
}

static inline uint32_t get_num_bytes(uint32_t data_size_flag) {
    return (data_size_flag >> 24) + 1;
}

static const char * spi_v2_get_register_write_info(uint32_t write_bits) {
    static char info[1024];

    int pos = snprintf(info,
                      ARRAY_SIZE(info),
                      "write - cs: %s, r/w: %s, valid: %s, bytes: %d, quad: %s, device: %d",
                      write_bits & SPI_MASK_CHIPSELECT ? "off" : " on",
                      write_bits & SPI_MASK_READ_WRITE ? "r" : "w",
                      write_bits & SPI_MASK_ACCESS_VALID ? "yes" : " no",
                      get_num_bytes(write_bits & SPI_MASK_DATA_BITS),
                      write_bits & SPI_MASK_QUAD_MODE ? " on" : "off",
                      (write_bits & SPI_MASK_SELECT_DEVICE) >> 30
                      );

    if ((write_bits & SPI_MASK_READ_WRITE) == SPI_WRITE
        && (write_bits & SPI_MASK_ACCESS_VALID) == SPI_ACCESS_VALID_ON) {
        snprintf(info + pos, ARRAY_SIZE(info) - pos, ", data: 0x%06x", write_bits & SPI_MASK_DATA);
    }

    return info;
}

static const char * spi_v2_get_register_read_info(uint32_t read_bits) {
    static char info[1024];

    int pos = snprintf(info, ARRAY_SIZE(info),
                      "read - read fifo empty: %s, write fifo empty: %s, device: %d",
                      read_bits & SPI_MASK_READ_FIFO ? "yes" : " no",
                      read_bits & SPI_MASK_WRITE_FIFO ? "yes" : " no",
                      (read_bits & SPI_MASK_ACTIVE_DEVICE) >> 28);

    if ((read_bits & SPI_MASK_READ_FIFO) != SPI_READ_FIFO_EMPTY) {
        snprintf(info + pos, ARRAY_SIZE(info) - pos,
                ", bytes: %d, data: 0x%06x",
                get_num_bytes(read_bits & SPI_MASK_DATA_BITS),
                read_bits & SPI_MASK_DATA);
    }

    return info;
}

static const char*
spi_v2_get_protocol_info(uint32_t reg) {
    static char info[1024];
    int pos = snprintf(info, 
                      ARRAY_SIZE(info),
                      "cs: %s, r/w: %s, valid: %s, bytes: %d, quad: %s",
                      reg & SPI_MASK_CHIPSELECT ? "off" : " on",
                      reg & SPI_MASK_READ_WRITE ? "r" : "w",
                      reg & SPI_MASK_ACCESS_VALID ? "yes" : " no",
                      get_num_bytes(reg & SPI_MASK_DATA_BITS),
                      reg & SPI_MASK_QUAD_MODE ? " on" : "off");

    if ((reg & SPI_MASK_READ_WRITE) == SPI_WRITE
        && (reg & SPI_MASK_ACCESS_VALID) == SPI_ACCESS_VALID_ON) {
        snprintf(info + pos, ARRAY_SIZE(info) - pos, ", data: 0x%06x", reg & SPI_MASK_DATA);
    }

    return info;
}

static inline void
spi_v2_write_ctrl_register(struct spi_v2_controller * self,
                           uint32_t value) {
    struct register_interface * ri = self->base_type.register_interface;
    value |= (self->selected_device & SPI_MASK_SELECT_DEVICE);
    ri->write(ri, self->control_register, value);
    DBG_STMT(pr_debug(KBUILD_MODNAME ": " DBG_NAME "%s\n", spi_v2_get_register_write_info(value)));
}

static inline uint32_t
spi_v2_read_ctrl_register(struct spi_v2_controller * self) {
    struct register_interface * ri = self->base_type.register_interface;
    uint32_t value = ri->read(ri, self->control_register);
    DBG_STMT(pr_debug(KBUILD_MODNAME ": " DBG_NAME "%s\n", spi_v2_get_register_read_info(value)));
    return value;
}

static inline int
spi_v2_wait_for_ctrl_status(struct spi_v2_controller * self,
                            uint32_t status_mask,
                            uint32_t desired_status) {
    uint32_t status;
    do {
        status = spi_v2_read_ctrl_register(self);
    } while (status != UINT32_MAX && (status & status_mask) != desired_status);

    return (status != UINT32_MAX)
            ? STATUS_OK
            : STATUS_ERROR;
}

static inline int
spi_v2_wait_for_write_fifo_empty(struct controller_base * ctrl) {
    struct spi_v2_controller * self = downcast(ctrl, struct spi_v2_controller);
    return spi_v2_wait_for_ctrl_status(self, SPI_MASK_WRITE_FIFO, SPI_WRITE_FIFO_EMPTY);
}

static inline int
spi_v2_wait_for_read_fifo_empty(struct spi_v2_controller * self) {
    return spi_v2_wait_for_ctrl_status(self, SPI_READ_FIFO_EMPTY, SPI_READ_FIFO_EMPTY);
}

static inline uint32_t
spi_v2_read_data_when_valid(struct spi_v2_controller * self) {
    uint32_t spi_reg = spi_v2_read_ctrl_register(self);
    while ((spi_reg & SPI_READ_FIFO_EMPTY) == SPI_READ_FIFO_EMPTY) {
        spi_reg = spi_v2_read_ctrl_register(self);
    }
    return spi_reg;
}

static void
spi_v2_set_chipselect(struct spi_v2_controller * self,
                      enum spi_cs_status status) {
    uint32_t value = SPI_ACCESS_VALID_OFF
                        | (status == SPI_CS_OFF
                                ? SPI_CHIPSELECT_OFF
                                : SPI_CHIPSELECT_ON);
    spi_v2_write_ctrl_register(self, value);
}

static int
spi_v2_handle_pre_burst_flags(struct controller_base * ctrl, uint32_t flags) {
    struct spi_v2_controller * self = downcast(ctrl, struct spi_v2_controller);
    spi_v2_set_chipselect(self, SPI_CS_ON);

    /* TODO Error Handling */
    return 0;
}

static int
spi_v2_handle_post_burst_flags(struct controller_base * ctrl, uint32_t flags) {
    struct spi_v2_controller * self = downcast(ctrl, struct spi_v2_controller);
    if ((flags & SPI_POST_BURST_FLAG_LEAVE_CS_ASSERTED) == 0) {
        udelay(5);
        spi_v2_set_chipselect(self, SPI_CS_OFF);
    }

    /* TODO Error Handling */
    return 0;
}

static int
spi_v2_write_shot(struct controller_base * ctrl,
                  const uint8_t * buf, int num_bytes) {
    struct spi_v2_controller * self = downcast(ctrl, struct spi_v2_controller);
    static uint32_t cmd_base =
            SPI_WRITE | SPI_CHIPSELECT_ON | SPI_ACCESS_VALID_ON;

    uint32_t cmd = cmd_base | get_data_size_flag(num_bytes);
    if (ctrl->current_burst_flags & SPI_BURST_FLAG_QUAD_MODE) {
        cmd |= SPI_QUAD_MODE_ON;
    }
    uint32_t data = 0;

    for (int i = 0; i < num_bytes; ++i) {
        data |= (buf[i] << (8 * i));
    }

    spi_v2_write_ctrl_register(self, cmd | data);

    /* TODO Error Handling */
    return 0;
}

static int
spi_v2_request_read(struct controller_base * ctrl, size_t num_bytes) {
    struct spi_v2_controller * self = downcast(ctrl, struct spi_v2_controller);
    static uint32_t cmd_base =
            SPI_READ | SPI_CHIPSELECT_ON | SPI_ACCESS_VALID_ON;

    uint32_t cmd = cmd_base | get_data_size_flag(num_bytes);
    if (ctrl->current_burst_flags & SPI_BURST_FLAG_QUAD_MODE) {
        cmd |= SPI_QUAD_MODE_ON;
    }

    spi_v2_write_ctrl_register(self, cmd);

    /* TODO Error Handling */
    return 0;
}

static int
spi_v2_read_shot(struct controller_base * ctrl,
                 uint8_t * buffer, size_t num_bytes) {
    assert(num_bytes <= SPI_V2_BYTES_PER_READ);
    struct spi_v2_controller * self = downcast(ctrl, struct spi_v2_controller);
    uint32_t spi_reg = spi_v2_read_data_when_valid(self);
    for (size_t i = 0; i < num_bytes; ++i) {
        *buffer++ = extract_byte(spi_reg, num_bytes - 1 - i);
    }

    /* TODO Error Handling */
    return 0;
}

static void spi_v2_burst_aborted(struct controller_base * ctrl) {
    /* TODO implement */
}

static void spi_v2_cleanup(struct controller_base * ctrl) {
    /* TODO implement */
}

int spi_v2_controller_init(struct spi_v2_controller* spi_v2,
                           struct register_interface* ri,
                           user_mode_lock * lock,
                           uint32_t control_register) {
    return spi_v2a_controller_init(spi_v2, ri, lock, control_register, 0);
}

int spi_v2a_controller_init(struct spi_v2_controller * spi_v2,
                            struct register_interface * ri,
                            user_mode_lock * lock,
                            uint32_t control_register,
                            uint32_t target_device) {
    struct controller_base * ctrl = &spi_v2->base_type;

    /* data transport */
    controller_base_init(ctrl, ri, lock,
                         SPI_READ_FIFO_LENGTH, SPI_V2_BYTES_PER_READ,
                         SPI_WRITE_FIFO_LENGTH, SPI_V2_BYTES_PER_WRITE,
                         NULL,
                         NULL,
                         spi_v2_handle_pre_burst_flags,
                         spi_v2_handle_post_burst_flags,
                         spi_v2_write_shot,
                         spi_v2_request_read, spi_v2_read_shot,
                         controller_base_execute_command_not_supported,
                         spi_v2_wait_for_write_fifo_empty,
                         spi_v2_burst_aborted,
                         spi_v2_cleanup);

    spi_v2->control_register = control_register;
    spi_v2->selected_device = (target_device << 30) & SPI_MASK_SELECT_DEVICE;
    
    /* The XILINX STARTUP primitive can swallow up to three clock cycles after FPGA configuration.
     * To prevent important clock cycles from getting lost, we generate some dummy clock cycles
     * with chip select deasserted. */
    spi_v2_write_ctrl_register(spi_v2, SPI_CHIPSELECT_OFF | SPI_ACCESS_VALID_ON | SPI_READ);

    /* throw away the read data.
     * Strange phenomenon: Not throwing away the dummy read data does not always cause the next read to read garbage.
     * Comment the following line and try for say 1000 times. There will be some cases where everything works fine
     * instead of getting a preceding garbage character character */
    return spi_v2_wait_for_read_fifo_empty(spi_v2);
}
