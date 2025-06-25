/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef BPI_CONTROLLER_H_
#define BPI_CONTROLLER_H_

#include "../controllers/controller_base.h"
#include "../fpga/register_interface.h"
#include "../helpers/type_hierarchy.h"


#ifdef __cplusplus
extern "C" {
#endif

    typedef struct bpi_controller {
        DERIVE_FROM(controller_base);

        /* public */
        void (*invalidate_bank)(struct bpi_controller * bpi_ctrl);
        int32_t (*wait_ready)(struct bpi_controller *bpi_ctrl);
        void (*write_command)(struct bpi_controller *bpi_ctrl, uint16_t cmd);
        int (*write_command_address)(struct bpi_controller *bpi_ctrl, unsigned int address, uint16_t cmd);
        void (*write_command_increment_address)(struct bpi_controller *bpi_ctrl, uint16_t cmd);
        int (*set_address)(struct bpi_controller *bpi_ctrl, uint32_t adr);
        int (*select_bank)(struct bpi_controller *bpi_ctrl, uint8_t bank);
        int (*get_active_bank)(struct bpi_controller *bpi_ctrl);
        int (*read_data)(struct bpi_controller *bpi_ctrl, unsigned int adr, uint16_t * data, unsigned int length);
        int (*empty_fifo)(struct bpi_controller *bpi_ctrl);


        /* private */
        uint32_t address_register;
        uint32_t data_register;
        uint32_t bank_register;
        uint32_t selected_bank;
        uint32_t address_reg_width;
        uint32_t address_mask;
        uint32_t bank_mask;
        uint32_t bank_count;
        uint32_t address;
        uint32_t flags;
    } bpi_controller;

    int bpi_controller_init(struct bpi_controller* bpi_controller,
        struct register_interface* ri,
        user_mode_lock * lock,
        uint32_t address_register,
        uint32_t bpi_data_register,
        uint32_t bpi_bank_select_status_register,
        uint32_t address_width,
        uint32_t bank_width);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* BPI_CONTROLLER_H_ */
