/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#include <linux/pci.h>

#include "../config_interface.h"

static int config_write8(struct config_interface * ci, int address, uint8_t value) {
    struct linux_config_interface * self = downcast(ci, struct linux_config_interface);
    int result = pci_write_config_byte(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_read8(struct config_interface * ci, int address, uint8_t * value) {
    struct linux_config_interface * self = downcast(ci, struct linux_config_interface);
    int result = pci_read_config_byte(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_write16(struct config_interface * ci, int address, uint16_t value) {
    struct linux_config_interface * self = downcast(ci, struct linux_config_interface);
    int result = pci_write_config_word(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_read16(struct config_interface * ci, int address, uint16_t * value) {
    struct linux_config_interface * self = downcast(ci, struct linux_config_interface);
    int result = pci_read_config_word(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_write32(struct config_interface * ci, int address, uint32_t value) {
    struct linux_config_interface * self = downcast(ci, struct linux_config_interface);
    int result = pci_write_config_dword(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_read32(struct config_interface * ci, int address, uint32_t * value) {
    struct linux_config_interface * self = downcast(ci, struct linux_config_interface);
    int result = pci_read_config_dword(self->pdev, address, value);
    if (result != 0) {
        result = pcibios_err_to_errno(result);
    }

    return result;
}

static int config_write(struct config_interface * ci, int start, const void * value, int size) {
    struct linux_config_interface * self = downcast(ci, struct linux_config_interface);
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

static int config_read(struct config_interface * ci, int start, void * value, int size) {
    struct linux_config_interface * self = downcast(ci, struct linux_config_interface);
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

static int config_find_cap(struct config_interface * ci, int capability)
{
    struct linux_config_interface * self = downcast(ci, struct linux_config_interface);
    return pci_find_capability(self->pdev, capability);
}

static int config_find_ext_cap(struct config_interface * ci, int capability)
{
    struct linux_config_interface * self = downcast(ci, struct linux_config_interface);
    int offs = 0x100;

    /* workaround because pci_find_ext_capability doesn't seem to work reliably */
    do {
        uint32_t header = 0;
        int ret = pci_read_config_dword(self->pdev, offs, &header);
        if (ret != 0) {
            /*dev_warn(&men->dev, "Failed to scan PCIe extended capabilities\n");*/
            break;
        }

        if (PCI_EXT_CAP_ID(header) == capability) break;
        offs = PCI_EXT_CAP_NEXT(header);
    } while (offs != 0);

    return offs;
}

void linux_config_interface_init(struct linux_config_interface * ci,
                                 struct pci_dev * pdev) {
    config_interface_init(upcast(ci),
            config_write8, config_read8,
            config_write16, config_read16,
            config_write32, config_read32,
            config_write, config_read,
            config_find_cap, config_find_ext_cap);
    ci->pdev = pdev;
}
