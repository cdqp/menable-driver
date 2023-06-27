/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_PCI_PCI_DEFINES_H_
#define LIB_PCI_PCI_DEFINES_H_

#include <lib/os/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum men_pci_capability_id {
    MEN_PCI_CAPABILITY_ID_POWER_MANAGEMENT  = 0x01,
    MEN_PCI_CAPABILITY_ID_AGP               = 0x02,
    MEN_PCI_CAPABILITY_ID_VPD               = 0x03,
    MEN_PCI_CAPABILITY_ID_SLOT_ID           = 0x04,
    MEN_PCI_CAPABILITY_ID_MSI               = 0x05,
    MEN_PCI_CAPABILITY_ID_CPCI_HOTSWAP      = 0x06,
    MEN_PCI_CAPABILITY_ID_PCIX              = 0x07,
    MEN_PCI_CAPABILITY_ID_HYPERTRANSPORT    = 0x08,
    MEN_PCI_CAPABILITY_ID_VENDOR_SPECIFIC   = 0x09,
    MEN_PCI_CAPABILITY_ID_DEBUG_PORT        = 0x0A,
    MEN_PCI_CAPABILITY_ID_CPCI_RES_CTRL     = 0x0B,
    MEN_PCI_CAPABILITY_ID_SHPC              = 0x0C,
    MEN_PCI_CAPABILITY_ID_P2P_SSID          = 0x0D,
    MEN_PCI_CAPABILITY_ID_AGP_TARGET        = 0x0E,
    MEN_PCI_CAPABILITY_ID_SECURE            = 0x0F,
    MEN_PCI_CAPABILITY_ID_PCI_EXPRESS       = 0x10,
    MEN_PCI_CAPABILITY_ID_MSIX              = 0x11,
    MEN_PCI_CAPABILITY_ID_SATA_CONFIG       = 0x12,
    MEN_PCI_CAPABILITY_ID_ADVANCED_FEATURES = 0x13,
    MEN_PCI_CAPABILITY_ID_FPB               = 0x15
};

enum men_pci_express_capability_id {
    MEN_PCIE_CAP_ID_ADVANCED_ERR_REPORTING       = 0x0001,
    MEN_PCIE_CAP_ID_VIRTUAL_CHANNEL              = 0x0002,
    MEN_PCIE_CAP_ID_VIRTUAL_CHANNEL_MFVC         = 0x0009,
    MEN_PCIE_CAP_ID_DEVICE_SERIAL_NUMBER         = 0x0003,
    MEN_PCIE_CAP_ID_ROOT_COMPLEX_LINK_DECL       = 0x0005,
    MEN_PCIE_CAP_ID_ROOT_CPLX_INTERNAL_LINK_CTRL = 0x0006,
    MEN_PCIE_CAP_ID_POWER_BUDGETING              = 0x0004,
    MEN_PCIE_CAP_ID_ACS                          = 0x000d,
    MEN_PCIE_CAP_ID_ROOT_CPLX_EVENTT_CLLOCTOR_EP = 0x0007,
    MEN_PCIE_CAP_ID_MFVC                         = 0x0008,
    MEN_PCIE_CAP_ID_VENDOR_SPECIFIC              = 0x000b,
    MEN_PCIE_CAP_ID_RCRB                         = 0x000a,
    MEN_PCIE_CAP_ID_MULTICAST                    = 0x0012,
    MEN_PCIE_CAP_ID_RESIZABLE_BAR                = 0x0015,
    MEN_PCIE_CAP_ID_ARI                          = 0x000e,
    MEN_PCIE_CAP_ID_DYNAMIC_POWER_ALLOCATION     = 0x0016,
    MEN_PCIE_CAP_ID_LATENCY_TOLERANCE_REPORTING  = 0x0018,
    MEN_PCIE_CAP_ID_TPH_REQUESTER                = 0x0017,
    MEN_PCIE_CAP_ID_SECONDARY_PCIE               = 0x0019
};

struct pci_capability_header {
    uint8_t id;
    uint8_t address;
};

struct men_pci_header {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subsys_vendor_id;
    uint16_t subsys_id;
    uint16_t command;
    uint16_t status;
};

struct pci_capability {
    enum men_pci_capability_id id;
    uint8_t address;
    union {
        struct {
            struct {
                bool msix_enabled;
                bool masked;
                uint16_t table_size;
            } message_ctrl;
            uint8_t table_bar;
            uint32_t table_offset;
            uint8_t pba_bar;
            uint32_t pba_offset;
        } msix;
        struct {
            uint32_t device_capabilities;
            uint16_t device_control;
            uint16_t device_status;
            uint32_t link_capabilities;
            uint16_t link_control;
            uint16_t link_status;
        } pci_express;
    };
};

struct pci_express_capability {

    enum men_pci_express_capability_id id;
    uint16_t address;
    uint8_t version;

    union {
        struct {
            union {
                uint64_t sn;
                struct {
                    uint32_t sn_low;
                    uint32_t sn_hi;
                };
            };
        } device_serial_number;
    };

};

struct men_pci_bars_info {
    struct {
        bool is_valid;
        bool is_memory;
        bool is_64_bit;
        uint64_t address;
    } bars[6];
};

#ifdef __cplusplus
}
#endif

#endif
