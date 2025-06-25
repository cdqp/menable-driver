/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#include <linux/pci.h>

#include "../pci_config_interface_linux.h"

static int config_write8(struct pci_config_interface * ci, int address, uint8_t value) {
    struct pci_config_interface_linux * self = downcast(ci, struct pci_config_interface_linux);
    int result = pci_write_config_byte(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_read8(struct pci_config_interface * ci, int address, uint8_t * value) {
    struct pci_config_interface_linux * self = downcast(ci, struct pci_config_interface_linux);
    int result = pci_read_config_byte(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_write16(struct pci_config_interface * ci, int address, uint16_t value) {
    struct pci_config_interface_linux * self = downcast(ci, struct pci_config_interface_linux);
    int result = pci_write_config_word(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_read16(struct pci_config_interface * ci, int address, uint16_t * value) {
    struct pci_config_interface_linux * self = downcast(ci, struct pci_config_interface_linux);
    int result = pci_read_config_word(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_write32(struct pci_config_interface * ci, int address, uint32_t value) {
    struct pci_config_interface_linux * self = downcast(ci, struct pci_config_interface_linux);
    int result = pci_write_config_dword(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_read32(struct pci_config_interface * ci, int address, uint32_t * value) {
    struct pci_config_interface_linux * self = downcast(ci, struct pci_config_interface_linux);
    int result = pci_read_config_dword(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_write(struct pci_config_interface * ci, int start, const void * value, int size) {
    struct pci_config_interface_linux * self = downcast(ci, struct pci_config_interface_linux);
    const int end = start + size;
    const uint32_t * value_ptr = (const uint32_t *) value;
    int address;
    int result = 0;

    if (start < 0 || end <= start || (size % 4) != 0)
        return -EINVAL;

    for (address = start; address < end; address += 4, value_ptr++) {
        result = pci_write_config_dword(self->pdev, address, (* value_ptr));
        if (result != 0) {
            result = pcibios_err_to_errno(result);
            break;
        }
    }

    return result;
}

static int config_read(struct pci_config_interface * ci, int start, void * value, int size) {
    struct pci_config_interface_linux * self = downcast(ci, struct pci_config_interface_linux);
    const int end = start + size;
    uint32_t * value_ptr = (uint32_t *) value;
    int address;
    int result = 0;

    if (start < 0 || end <= start || (size % 4) != 0)
        return -EINVAL;

    for (address = start; address < end; address += 4, value_ptr++) {
        result = pci_read_config_dword(self->pdev, address, value_ptr);
        if (result != 0) {
            result = pcibios_err_to_errno(result);
            break;
        }
    }

    return result;
}

void pci_config_interface_linux_init(struct pci_config_interface_linux * ci,
                                 struct pci_dev * pdev) {
    pci_config_interface_init(upcast(ci),
            config_write8, config_read8,
            config_write16, config_read16,
            config_write32, config_read32,
            config_write, config_read);
    ci->pdev = pdev;
}
