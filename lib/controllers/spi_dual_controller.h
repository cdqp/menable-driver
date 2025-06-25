/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef SPI_DUAL_CONTROLLER_H_
#define SPI_DUAL_CONTROLLER_H_

#include "../controllers/controller_base.h"
#include "../controllers/spi_defines.h"
#include "../helpers/type_hierarchy.h"

#ifdef __cplusplus
extern "C" {
#endif

struct spi_dual_controller {
    DERIVE_FROM(controller_base);
    uint32_t control_register;
    uint32_t flash_select_register;
    bool chip_selected;
};

int spi_dual_controller_init(struct spi_dual_controller* spi_dual,
                             struct register_interface* ri,
                             user_mode_lock * lock,
                             uint32_t control_register,
                             uint32_t flash_select_reg);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* SPI_DUAL_CONTROLLER_H_ */
