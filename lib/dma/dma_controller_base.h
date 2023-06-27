/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_DMA_DMA_CONTROLLER_BASE_H_
#define LIB_DMA_DMA_CONTROLLER_BASE_H_

#include <lib/fpga/register_interface.h>

struct dma_controller_base;

typedef bool(*dma_controller_base_is_enabled_and_running_fct)(struct dma_controller_base * self);
typedef int(*dma_controller_base_start_fct)(struct dma_controller_base * self);
typedef int(*dma_controller_base_stop_fct)(struct dma_controller_base * self);
typedef int(*dma_controller_base_abort_fct)(struct dma_controller_base * self);

/**
 * Class that provides common functionalities for all DMA controllers.
 */
typedef struct dma_controller_base {

	/**
	 * PUBLIC
	 * Returns true if the dma engine is currently running and no disable request was issued.
	 */
	dma_controller_base_is_enabled_and_running_fct is_enabled_and_running;

	/**
	 * PUBLIC
	 * Start the DMA engine, if not already running.
	 *
	 * @param self
	 * The class instance that this method operates on.
	 */
	dma_controller_base_start_fct start;

	/**
	 * PUBLIC
	 * Stops the DMA engine if running, leaving it in the
	 * current internal state.
	 *
	 * @param self
	 * The class instance that this method operates on.
	 */
	dma_controller_base_stop_fct stop;

	/**
	 * PUBLIC
	 * Stops the DMA engine if running and resets it internal state.
	 *
	 * @param self
	 * The class instance that this method operates on.
	 */
	dma_controller_base_abort_fct abort;


	/**
	 * PRIVATE
	 * Register for enable/disable/reset operations.
	 * This usually equals the `status_resgister`.
	 */
	uint32_t control_register;

	/**
	 * PRIVATE
	 * Register for reading the operational state
	 * This usually equals the `control_resgister`.
	 */
	uint32_t status_register;

	/**
	 * PRIVATE
	 * The interface to access the fpga registers.
	 */
	register_interface * register_interface;

} dma_controller_base;

/**
 * Initializer for a dma_controller_base struct.
 * 
 * @attention
 * This is meant to be used only by the initilializer of derived classes.
 *
 * @param dma_ctrl
 * An uninitialized dma_controller_base struct.
 * 
 * @param [in,out]	register_interface
 * The register interface to access the fpga registers.
 * 
 * @param control_register
 * Address of the target DMA engine's control register.
 * 
 * @param status_register
 * Address of the target DMA engine's status register.
 *
 * @returns
 * A status code that indicates success or failure. Use the MEN_IS_ERROR(val) macro to check for success or failure.
 */
int dma_controller_base_init(dma_controller_base * dma_ctrl, register_interface * register_interface, uint32_t control_register, uint32_t status_register);

#endif // LIB_DMA_DMA_CONTROLLER_BASE_H_
