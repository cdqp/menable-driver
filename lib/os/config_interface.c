/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "config_interface.h"

void config_interface_init(struct config_interface * ci,
        int (*write8)(struct config_interface *, int, uint8_t),
        int (*read8)(struct config_interface *, int, uint8_t *),
        int (*write16)(struct config_interface *, int, uint16_t),
        int (*read16)(struct config_interface *, int, uint16_t *),
        int (*write32)(struct config_interface *, int, uint32_t),
        int (*read32)(struct config_interface *, int, uint32_t *),
        int (*write)(struct config_interface *, int, const void *, int),
        int (*read)(struct config_interface *, int, void *, int),
        int (*find_cap)(struct config_interface *, int),
        int (*find_ext_cap)(struct config_interface *, int)) {
    ci->write8 = write8;
    ci->read8 = read8;
    ci->write16 = write16;
    ci->read16 = read16;
    ci->write32 = write32;
    ci->read32 = read32;
    ci->write = write;
    ci->read = read;
    ci->find_cap = find_cap;
    ci->find_ext_cap = find_ext_cap;
}
