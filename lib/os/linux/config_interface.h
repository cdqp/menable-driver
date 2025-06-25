/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef LIB_OS_CONFIG_INTERFACE_H_
#define LIB_OS_CONFIG_INTERFACE_H_

#include "../config_interface.h"
#include "../../helpers/type_hierarchy.h"

struct pci_dev;

/**
 * Declaration only.
 * Needs to be implemented in OS specific code.
 */
struct linux_config_interface {
    DERIVE_FROM(config_interface);
    struct pci_dev * pdev;
};


/**
 * Initializes a register interface for the specific OS.
 * Implementations see inside os folder.
 */
void linux_config_interface_init(struct linux_config_interface * ci,
                                 struct pci_dev * pdev);

#endif /* LIB_OS_CONFIG_INTERFACE_H_ */
