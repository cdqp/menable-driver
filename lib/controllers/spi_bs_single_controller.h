/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef SPI_BS_SINGLE_H_
#define SPI_BS_SINGLE_H_

#include "../controllers/controller_base.h"
#include "../controllers/spi_defines.h"
#include "../helpers/type_hierarchy.h"

#ifdef __cplusplus 
extern "C" {
#endif

struct spi_bs_single_controller {
	DERIVE_FROM(controller_base);
	uint32_t control_register;
};

void spi_bs_single_data_transport_init(struct spi_bs_single_controller * spi_bs_single,
                                       struct register_interface * ri,
                                       user_mode_lock * lock,
                                       uint32_t control_register);

#ifdef __cplusplus 
} // extern "C"
#endif

#endif /* SPI_BS_SINGLE_H_ */
