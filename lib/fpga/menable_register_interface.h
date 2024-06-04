/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/



#ifndef LIB_FPGA_MENABLE_REGISTER_INTERFACE_H_
#define LIB_FPGA_MENABLE_REGISTER_INTERFACE_H_

#include "register_interface.h"
#include "../helpers/type_hierarchy.h"

#ifndef __iomem
#define __iomem
#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes a register interface for the specific OS.
 * Implementations see inside os folder.
 */
void menable_register_interface_init(struct register_interface * ri,
                                     uint32_t __iomem * base_address);

#ifdef __cplusplus
}
#endif

#endif /* LIB_FPGA_MENABLE_REGISTER_INTERFACE_H_ */
