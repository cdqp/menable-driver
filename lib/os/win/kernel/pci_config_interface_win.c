/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/



#include <wdm.h>
#include <errno.h>

#include "../pci_config_interface_win.h"

static int config_write8(struct pci_config_interface * ci, int address, uint8_t value) {
    struct pci_config_interface_win * self = downcast(ci, struct pci_config_interface_win);
    int result = self->bis->SetBusData(self->bis->Context, PCI_WHICHSPACE_CONFIG, &value, address, 1);
    if (result != 1) {
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

static int config_read8(struct pci_config_interface * ci, int address, uint8_t * value) {
    struct pci_config_interface_win * self = downcast(ci, struct pci_config_interface_win);
    int result = self->bis->GetBusData(self->bis->Context, PCI_WHICHSPACE_CONFIG, value, address, 1);
    if (result != 1) {
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

static int config_write16(struct pci_config_interface * ci, int address, uint16_t value) {
    struct pci_config_interface_win * self = downcast(ci, struct pci_config_interface_win);
    int result = self->bis->SetBusData(self->bis->Context, PCI_WHICHSPACE_CONFIG, &value, address, 2);
    if (result != 2) {
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

static int config_read16(struct pci_config_interface * ci, int address, uint16_t * value) {
    struct pci_config_interface_win * self = downcast(ci, struct pci_config_interface_win);
    int result = self->bis->GetBusData(self->bis->Context, PCI_WHICHSPACE_CONFIG, value, address, 2);
    if (result != 2) {
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

static int config_write32(struct pci_config_interface * ci, int address, uint32_t value) {
    struct pci_config_interface_win * self = downcast(ci, struct pci_config_interface_win);
    int result = self->bis->SetBusData(self->bis->Context, PCI_WHICHSPACE_CONFIG, &value, address, 4);
    if (result != 4) {
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

static int config_read32(struct pci_config_interface * ci, int address, uint32_t * value) {
    struct pci_config_interface_win * self = downcast(ci, struct pci_config_interface_win);
    int result = self->bis->GetBusData(self->bis->Context, PCI_WHICHSPACE_CONFIG, value, address, 4);
    if (result != 4) {
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

static int config_write(struct pci_config_interface * ci, int start, const void * value, int size) {
    struct pci_config_interface_win * self = downcast(ci, struct pci_config_interface_win);
    const int end = start + size;
    int result = 0;

    if (start < 0 || end <= start)
        return STATUS_INVALID_PARAMETER;

    result = self->bis->SetBusData(self->bis->Context, PCI_WHICHSPACE_CONFIG, (void *) value, start, size);
    if (result != size) {
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

static int config_read(struct pci_config_interface * ci, int start, void * value, int size) {
    struct pci_config_interface_win * self = downcast(ci, struct pci_config_interface_win);
    const int end = start + size;
    int result = 0;

    if (start < 0 || end <= start)
        return STATUS_INVALID_PARAMETER;

    result = self->bis->GetBusData(self->bis->Context, PCI_WHICHSPACE_CONFIG, value, start, size);
    if (result != size) {
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

void pci_config_interface_win_init(struct pci_config_interface_win * ci,
                                   struct _BUS_INTERFACE_STANDARD * bis) {
    pci_config_interface_init(upcast(ci),
            config_write8, config_read8,
            config_write16, config_read16,
            config_write32, config_read32,
            config_write, config_read);
    ci->bis = bis;
}
