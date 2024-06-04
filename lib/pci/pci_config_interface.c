/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "pci_config_interface.h"

#include <lib/helpers/bits.h>
#include <lib/helpers/helper.h>
#include <lib/helpers/error_handling.h>
#include <lib/helpers/dbg.h>
#include <lib/os/print.h>

#define PCI_BARS_ADDR 0x10
#define PCI_CAPS_PTR 0x34
#define PCIE_EXT_CAPS_PTR 0x100

#define PCI_HEADER_ADDR_ID 0x00
#define PCI_HEADER_ADDR_CMD_STATUS 0x04
#define PCI_HEADER_ADDR_SUBSYS_ID 0x2C


static int get_pci_header(struct pci_config_interface * self, struct men_pci_header * header) {
    uint32_t header_data;

    int ret = self->read32(self, PCI_HEADER_ADDR_ID, &header_data);
    if (MEN_IS_ERROR(ret))
        return STATUS_ERR_DEV_IO;

    header->vendor_id = GET_BITS_32(header_data,  0, 15);
    header->device_id = GET_BITS_32(header_data, 16, 31);

    ret = self->read32(self, PCI_HEADER_ADDR_CMD_STATUS, &header_data);
    if (MEN_IS_ERROR(ret))
        return STATUS_ERR_DEV_IO;

    header->command = GET_BITS_32(header_data, 0, 15);
    header->status = GET_BITS_32(header_data, 16, 31);

    ret = self->read32(self, PCI_HEADER_ADDR_CMD_STATUS, &header_data);
    if (MEN_IS_ERROR(ret))
        return STATUS_ERR_DEV_IO;

    header->subsys_vendor_id = GET_BITS_32(header_data, 0, 15);
    header->subsys_id = GET_BITS_32(header_data, 16, 31);

    return STATUS_OK;
}

static int find_capability_address(struct pci_config_interface * cfg,
                                   enum men_pci_capability_id id) {

    DBG_STMT(pr_debug(DBG_PREFIX ": Searching for PCI Capability with id 0x%02x\n", id));

    uint8_t cap_addr = 0;
    int ret = cfg->read8(cfg, PCI_CAPS_PTR, &cap_addr);
    if (MEN_IS_ERROR(ret)) goto Error;

    while (cap_addr != 0) {
        DBG_STMT(pr_debug(DBG_PREFIX ": Getting PCI Capability at address 0x%02x\n", cap_addr));

        uint16_t cap_data;
        ret = cfg->read16(cfg, cap_addr, &cap_data);
        if (MEN_IS_ERROR(ret)) goto Error;

        uint8_t cap_id = GET_BITS_16(cap_data, 0, 7);
        uint8_t cap_next = GET_BITS_16(cap_data, 8, 15);

        DBG_STMT(pr_debug(DBG_PREFIX ": PCI capability id = 0x%02x, next = 0x%02x\n", cap_id, cap_next));
        if (cap_id == id) {
            return cap_addr;
        }

        cap_addr = cap_next;
    }

    return STATUS_ERR_NOT_FOUND;

Error:
    return STATUS_ERR_DEV_IO;
}

static int find_extended_cap_address(struct pci_config_interface * self, 
                                     enum men_pci_express_capability_id id)
{
    int next = PCIE_EXT_CAPS_PTR;

    do {
        uint32_t header;
        int ret = self->read32(self, next, (uint32_t *)&header);
        if (MEN_IS_ERROR(ret)) {
            return STATUS_ERR_DEV_IO;
        }

        uint16_t cap_id = header & GEN_BITS_32(16);

        if (cap_id == id) {
            return next;
        } else {
            next = GET_BITS_32(header, 20, 31);
        }

    } while (next != 0);

    return STATUS_ERR_NOT_FOUND;
}

static inline int pci_read_array_32(struct pci_config_interface * cfg, int address, int num_elements, uint32_t * out_data) {
    for (int i = 0; i < num_elements; ++i) {
        int ret = cfg->read32(cfg, address  + (i * 4), &out_data[i]);
        if (MEN_IS_ERROR(ret)) {
            return STATUS_ERR_DEV_IO;
        }
    }
    return STATUS_OK;
}

static int pci_get_capability_data(struct pci_config_interface * cfg,
                                   struct pci_capability * capability) {
    switch (capability->id) {
    case MEN_PCI_CAPABILITY_ID_MSIX:
    {
        DBG_STMT(pr_debug(DBG_PREFIX ": Retreive MSI-X capability from address 0x%02x\n", capability->address));
        uint32_t data[3];
        int ret = pci_read_array_32(cfg, capability->address, 3, data);
        if (MEN_IS_ERROR(ret)) {
            return STATUS_ERR_DEV_IO;
        }

        uint16_t message_ctrl_data = GET_BITS_32(data[0], 16, 31);
        capability->msix.message_ctrl.table_size   = GET_BITS_16(message_ctrl_data,  0, 10) + 1;
        capability->msix.message_ctrl.masked       = GET_BITS_16(message_ctrl_data, 14, 14) ? true : false;
        capability->msix.message_ctrl.msix_enabled = GET_BITS_16(message_ctrl_data, 15, 15) ? true : false;

        capability->msix.table_bar = GET_BITS_32(data[1], 0, 2);
        capability->msix.table_offset = data[1] & GEN_BITS_INV_32(3);

        capability->msix.pba_bar = GET_BITS_32(data[2], 0, 2);
        capability->msix.pba_offset = data[2] & GEN_BITS_INV_32(3);

        return STATUS_OK;
    }

    case MEN_PCI_CAPABILITY_ID_PCI_EXPRESS:
    {
        DBG_STMT(pr_debug(DBG_PREFIX ": Retreive PCI Express capability from address 0x%02x\n", capability->address));

        uint32_t data[15];
        pci_read_array_32(cfg, capability->address, 15, data);

        capability->pci_express.device_capabilities = data[1];
        capability->pci_express.device_control = GET_BITS_32(data[2],  0, 15);
        capability->pci_express.device_status  = GET_BITS_32(data[2], 16, 31);

        capability->pci_express.link_capabilities = data[3];
        capability->pci_express.link_control = GET_BITS_32(data[4],  0, 15);
        capability->pci_express.link_status  = GET_BITS_32(data[4], 16, 31);

        return STATUS_OK;
    }

    default:
        break;
    }

    return STATUS_ERR_NOT_FOUND;
}

static int find_capability(struct pci_config_interface * self, enum men_pci_capability_id id, struct pci_capability * out_capability) {
    int result = find_capability_address(self, id);
    if (MEN_IS_ERROR(result)) {
        return result;
    }

    out_capability->id = id;
    out_capability->address = GET_BITS_32(result, 0, 7);
    return pci_get_capability_data(self, out_capability);
}

static int get_pcie_capbability_data(struct pci_config_interface * cfg, struct pci_express_capability * capability) {
    switch(capability->id) {
    case MEN_PCIE_CAP_ID_DEVICE_SERIAL_NUMBER:
    {
        uint32_t data[2];
        int ret = pci_read_array_32(cfg, capability->address + 4, 2, data);
        if (MEN_IS_ERROR(ret)) {
            return STATUS_ERR_DEV_IO;
        }

        capability->device_serial_number.sn = (uint64_t)data[0] | (uint64_t)data[1] << 32;
        return STATUS_OK;
    }

    default:
        break;
    }

    DBG_STMT(pr_debug(DBG_PREFIX ": Unsupported PCIe capability id 0x%0x.\n", capability->id));
    return STATUS_ERROR; // TODO: make this sth. like STATUS_INVALID_OPERATION or STATUS_INVALID_ARGUMENT?
}

int find_extended_capability(struct pci_config_interface * self, enum men_pci_express_capability_id id, struct pci_express_capability * out_capability) {
    int address = find_extended_cap_address(self, id);
    if (MEN_IS_ERROR(address)) {
        return address;
    } else {
        out_capability->id = id;
        out_capability->address = GET_BITS_32(address, 0, 15);
        return get_pcie_capbability_data(self, out_capability);
    }
}

static int get_bars_info(struct pci_config_interface * cfg, struct men_pci_bars_info * out_info) {
    uint32_t address = PCI_BARS_ADDR;
    
    DBG_STMT(pr_debug(DBG_PREFIX ": Retrieve information about %d BARs\n", (int)ARRAY_SIZE(out_info->bars)));
    for (int i = 0; i < ARRAY_SIZE(out_info->bars); ++i) {
        uint32_t value;
        
        DBG_STMT(pr_debug(DBG_PREFIX ": Reading info for BAR%d from address 0x%02x\n", i, address + i * 4));

        int ret = cfg->read32(cfg, address + i * 4, &value);
        if (MEN_IS_ERROR(ret)) return STATUS_ERR_DEV_IO;

        DBG_STMT(pr_debug(DBG_PREFIX ": register value = 0x%08x\n", value));

        if (value == 0) {
            DBG_STMT(pr_debug(DBG_PREFIX ": no BAR specified\n"));
            out_info->bars[i].is_valid = false;
        } else {
            out_info->bars[i].is_valid = true;
            out_info->bars[i].is_memory = (GET_BIT(value, 0) == 0);
            if (out_info->bars[i].is_memory) {
                out_info->bars[i].address = value & GEN_MASK_32(4, 31);
                out_info->bars[i].is_64_bit = (GET_BITS_32(value, 1, 2) == 2);

                if (out_info->bars[i].is_64_bit) {
                    // the next BAR descriptor represents the higher 32 bits of this address.
                    ++i;
                    int ret = cfg->read32(cfg, address + i * 4, &value);
                    if (MEN_IS_ERROR(ret)) return STATUS_ERR_DEV_IO;

                    out_info->bars[i - 1].address |= ((uint64_t)value) << 32;
                    out_info->bars[i].is_valid = false;
                    DBG_STMT(pr_debug(DBG_PREFIX ": BAR%d is 'memory' with 64 bit addressing at 0x%016llx\n", i-1, out_info->bars[i-1].address));
                } else {
                    DBG_STMT(pr_debug(DBG_PREFIX ": BAR%d is 'memory' with 32 bit addressing at 0x%08x\n", i, (uint32_t)out_info->bars[i].address));
                }
            } else {
                out_info->bars[i].is_64_bit = false;
                out_info->bars[i].address = value & GEN_MASK_32(2, 31);
                DBG_STMT(pr_debug(DBG_PREFIX ": BAR%d is 'I/O' at 0x%08x\n", i, (uint32_t)out_info->bars[i].address));
            }
        }
    }

    return STATUS_OK;
}

void pci_config_interface_init(struct pci_config_interface * ci,
        int (*write8)(struct pci_config_interface *, int, uint8_t),
        int (*read8)(struct pci_config_interface *, int, uint8_t *),
        int (*write16)(struct pci_config_interface *, int, uint16_t),
        int (*read16)(struct pci_config_interface *, int, uint16_t *),
        int (*write32)(struct pci_config_interface *, int, uint32_t),
        int (*read32)(struct pci_config_interface *, int, uint32_t *),
        int (*write)(struct pci_config_interface *, int, const void *, int),
        int (*read)(struct pci_config_interface *, int, void *, int)) {
    ci->write8 = write8;
    ci->read8 = read8;
    ci->write16 = write16;
    ci->read16 = read16;
    ci->write32 = write32;
    ci->read32 = read32;
    ci->write = write;
    ci->read = read;
    ci->get_pci_header = get_pci_header;
    ci->find_cap_address = find_capability_address;
    ci->find_ext_cap_address = find_extended_cap_address;
    ci->find_capability = find_capability;
    ci->find_ext_capability = find_extended_capability;
}
