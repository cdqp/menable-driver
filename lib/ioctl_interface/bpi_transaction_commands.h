/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include <lib/os/types.h>


#ifndef LIB_IOCTL_INTERFACE_BPI_TRANSACTION_COMMANDS_H_
#define LIB_IOCTL_INTERFACE_BPI_TRANSACTION_COMMANDS_H_

enum bpi_transaction_command {
    BPI_COMMAND_SET_ACTIVE_BANK,
    BPI_COMMAND_GET_ACTIVE_BANK
};

#pragma pack(push, 1)

typedef struct {
    union {
        int32_t return_value;
        struct {
            uint8_t bank_number;
        } args;
    };
} bpi_set_active_bank_io;

typedef struct {
    int32_t return_value;
} bpi_get_active_bank_io;

#pragma pack(pop)

#endif // LIB_IOCTL_INTERFACE_BPI_TRANSACTION_COMMANDS_H_