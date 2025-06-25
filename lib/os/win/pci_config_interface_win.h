/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef LIB_OS_PCI_CONFIG_INTERFACE_WIN_H_
#define LIB_OS_PCI_CONFIG_INTERFACE_WIN_H_

#include <lib/pci/pci_config_interface.h>
#include "../../helpers/type_hierarchy.h"

struct _BUS_INTERFACE_STANDARD;

/**
 * Declaration only.
 * Needs to be implemented in OS specific code.
 */
struct pci_config_interface_win {
    DERIVE_FROM(pci_config_interface);
    struct _BUS_INTERFACE_STANDARD* bis;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes a register interface for the specific OS.
 * Implementations see inside os folder.
 */
void pci_config_interface_win_init(struct pci_config_interface_win* ci, struct _BUS_INTERFACE_STANDARD* bis);

#ifdef __cplusplus
}
#endif

#endif /* LIB_OS_WIN_CONFIG_INTERFACE_H_ */
