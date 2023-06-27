/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef SPI_V2_H_
#define SPI_V2_H_

#include "../controllers/controller_base.h"
#include "../controllers/spi_defines.h"
#include "../helpers/type_hierarchy.h"

#ifdef __cplusplus 
extern "C" {
#endif

struct spi_v2_controller {
	DERIVE_FROM(controller_base);
	uint32_t control_register;
    uint32_t selected_device;
};

/**
 * Creates an spi_v2_controller instance.
 *
 * @attention
 * The instance is not usable until spi_v2_controller_init
 * is called for the created instance.
 */
int spi_v2_controller_init(struct spi_v2_controller * spi_v2,
                           struct register_interface * ri,
                           user_mode_lock * lock,
                           uint32_t control_register);

/**
 * Creates an spi_v2_controller instance for using a specific device via
 * an SPI V2a controller on the FPGA.

 * @attention
 * The instance is not usable until spi_v2_controller_init
 * is called for the created instance.
 */
int spi_v2a_controller_init(struct spi_v2_controller* spi_v2,
                            struct register_interface* ri,
                            user_mode_lock * lock,
                            uint32_t control_register,
                            uint32_t target_device);

#ifdef __cplusplus 
} // extern "C"
#endif

#endif /* SPI_V2_H_ */
