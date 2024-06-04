/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef LIB_OS_CONFIG_INTERFACE_H_
#define LIB_OS_CONFIG_INTERFACE_H_

#include <lib/pci/pci_config_interface.h>
#include "../../helpers/type_hierarchy.h"

struct pci_dev;

/**
 * Declaration only.
 * Needs to be implemented in OS specific code.
 */
struct pci_config_interface_linux {
    DERIVE_FROM(pci_config_interface);
    struct pci_dev * pdev;
};


/**
 * Initializes a register interface for the specific OS.
 * Implementations see inside os folder.
 */
void pci_config_interface_linux_init(struct pci_config_interface_linux * ci,
                                     struct pci_dev * pdev);

#endif /* LIB_OS_CONFIG_INTERFACE_H_ */
