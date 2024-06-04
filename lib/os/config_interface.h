/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef CONFIG_INTERFACE_H_
#define CONFIG_INTERFACE_H_

#include "types.h"

/* forward declaration for typedefs */
struct config_interface;

struct config_interface
{
    int (*write8)(struct config_interface * self, int address, uint8_t value);
    int (*read8)(struct config_interface * self, int address, uint8_t * value);
    int (*write16)(struct config_interface * self, int address, uint16_t value);
    int (*read16)(struct config_interface * self, int address, uint16_t * value);
    int (*write32)(struct config_interface * self, int address, uint32_t value);
    int (*read32)(struct config_interface * self, int address, uint32_t * value);
    int (*write)(struct config_interface * self, int address, const void * value, int size);
    int (*read)(struct config_interface * self, int address, void * value, int size);
    int (*find_cap)(struct config_interface * self, int capability);
    int (*find_ext_cap)(struct config_interface * self, int capability);
};

#ifdef __cplusplus
extern "C" {
#endif

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
        int (*find_ext_cap)(struct config_interface *, int));

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_INTERFACE_H_ */
