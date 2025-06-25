/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_PCI_CONFIG_INTERFACE_H_
#define LIB_PCI_CONFIG_INTERFACE_H_

#include "pci_defines.h"
#include <lib/os/types.h>

struct pci_config_interface
{
    int (*write8)(struct pci_config_interface * self, int address, uint8_t value);
    int (*read8)(struct pci_config_interface * self, int address, uint8_t * value);
    int (*write16)(struct pci_config_interface * self, int address, uint16_t value);
    int (*read16)(struct pci_config_interface * self, int address, uint16_t * value);
    int (*write32)(struct pci_config_interface * self, int address, uint32_t value);
    int (*read32)(struct pci_config_interface * self, int address, uint32_t * value);
    int (*write)(struct pci_config_interface * self, int address, const void * value, int size);
    int (*read)(struct pci_config_interface * self, int address, void * value, int size);
    int (*get_pci_header)(struct pci_config_interface * self, struct men_pci_header * header);
    int (*find_cap_address)(struct pci_config_interface * self, enum men_pci_capability_id id);
    int (*find_ext_cap_address)(struct pci_config_interface * self, enum men_pci_express_capability_id id);
    int (*find_capability)(struct pci_config_interface * self, enum men_pci_capability_id id, struct pci_capability * out_capability);
    int (*find_ext_capability)(struct pci_config_interface * self, enum men_pci_express_capability_id id, struct pci_express_capability * out_capability);
};

#ifdef __cplusplus
extern "C" {
#endif

void pci_config_interface_init(struct pci_config_interface * ci,
        int (*write8)(struct pci_config_interface *, int, uint8_t),
        int (*read8)(struct pci_config_interface *, int, uint8_t *),
        int (*write16)(struct pci_config_interface *, int, uint16_t),
        int (*read16)(struct pci_config_interface *, int, uint16_t *),
        int (*write32)(struct pci_config_interface *, int, uint32_t),
        int (*read32)(struct pci_config_interface *, int, uint32_t *),
        int (*write)(struct pci_config_interface *, int, const void *, int),
        int (*read)(struct pci_config_interface *, int, void *, int));

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_INTERFACE_H_ */
