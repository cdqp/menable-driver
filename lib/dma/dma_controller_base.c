/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifdef DBG_DMA_BASE_OFF
 // OFF takes precedence over ON
#	undef DEBUG
#elif defined(DBG_DMA_BASE)

#ifndef DEBUG
#define DEBUG
#endif

#define DBG_LEVEL 1

#endif

#define DBG_MSG_PRFX KBUILD_MODNAME " [DMA Base]: "

#include <lib/helpers/bits.h>
#include <lib/helpers/timeout.h>
#include <lib/helpers/error_handling.h>
#include <lib/helpers/dbg.h>

#include "dma_controller_base.h"


#define DMA_CTRL_BIT_ENABLE BIT(0)
#define DMA_CTRL_BIT_RESET  BIT(1)

#define DMA_CTRL_MASK (DMA_CTRL_BIT_RESETTING | DMA_CTRL_BIT_ENABLED)

#define DMA_STATUS_BIT_ENABLED   BIT(0)
#define DMA_STATUS_BIT_RESETTING BIT(1)
#define DMA_STATUS_BIT_RUNNING   BIT(2)

#define DMA_STATUS_MASK (DMA_STATUS_BIT_ENABLED | DMA_STATUS_BIT_RESETTING | DMA_STATUS_BIT_RUNNING)

#define DMA_CTRL_RESET_TIMEOUT_MILLIS   1
#define DMA_CTRL_ENABLE_TIMEOUT_MILLIS  1
#define DMA_CTRL_DISABLE_TIMEOUT_MILLIS 100


static inline bool are_status_bits_set(dma_controller_base * self, uint32_t status_bits) {
	DBG_TRACE_BEGIN_FCT;

	uint32_t reg_value = self->register_interface->read(self->register_interface, self->status_register);
	bool ret = ARE_BITS_SET(reg_value, status_bits);

	return DBG_TRACE_RETURN(ret);
}

static inline bool is_status_one_of(dma_controller_base * self, uint32_t status_flags) {
	DBG_TRACE_BEGIN_FCT;

	uint32_t reg_value = (self->register_interface->read(self->register_interface, self->status_register) & DMA_STATUS_MASK);
	bool ret = ((reg_value & status_flags) != 0);

	return DBG_TRACE_RETURN(ret);
}

static bool dma_controller_base_is_enabled_and_running(dma_controller_base * self) {
	DBG_TRACE_BEGIN_FCT;

	bool ret = are_status_bits_set(self, DMA_STATUS_BIT_ENABLED | DMA_STATUS_BIT_RUNNING);

	return DBG_TRACE_RETURN(ret);
}

static int dma_controller_base_start(dma_controller_base * self) {
	DBG_TRACE_BEGIN_FCT;

	pr_debug(DBG_MSG_PRFX "Starting Engine. Status: 0x%08x\n", self->register_interface->read(self->register_interface, self->status_register));

	/* TODO: [RKN] What if the engine is running but not enabled? It should be about to become not running than which means we have a race condition... */
	/* TODO: [RKN] What if the reset bit is set? Can we simply rely on that it is only set on power up and is cleared during initialization? */
	if (!are_status_bits_set(self, DMA_STATUS_BIT_ENABLED)) {

		// enable and wait for dma engine to start running
		self->register_interface->write(self->register_interface, self->control_register, DMA_CTRL_BIT_ENABLE);

		struct timeout timeout;
		timeout_init(&timeout, DMA_CTRL_ENABLE_TIMEOUT_MILLIS);

		while (!are_status_bits_set(self, (DMA_STATUS_BIT_ENABLED | DMA_STATUS_BIT_RUNNING)) && !timeout_has_elapsed(&timeout)) {
			// just loop
		}

		if (!are_status_bits_set(self, (DMA_STATUS_BIT_ENABLED | DMA_STATUS_BIT_RUNNING))) {
			pr_err(DBG_MSG_PRFX "Timed out while enabling DMA engine.\n");
			return DBG_TRACE_RETURN(STATUS_ERR_TIMEOUT);
		}
	}

	pr_debug(DBG_MSG_PRFX "Started Engine successfully. Status: 0x%08x\n", self->register_interface->read(self->register_interface, self->status_register));

	return DBG_TRACE_RETURN(STATUS_OK);
}

static int dma_controller_base_stop(dma_controller_base * self) {
	DBG_TRACE_BEGIN_FCT;

    pr_debug(DBG_MSG_PRFX "Stopping Engine. Status: 0x%08x\n", self->register_interface->read(self->register_interface, self->status_register));
    
    /* Only stop if the engine is in 'enabled' state. Running and not enabled means, 
	 * it is currently being stopped. Being in reset may only happen, if the engine has been stopped before. 
	 */
	if (are_status_bits_set(self, DMA_STATUS_BIT_ENABLED)) {
		// remove enabled bit
		pr_debug(DBG_MSG_PRFX "Engine is enabled. Remove enabled bit.");
		self->register_interface->write(self->register_interface, self->control_register, 0);
		pr_debug(DBG_MSG_PRFX "New status: 0x%08x\n", self->register_interface->read(self->register_interface, self->status_register));
	} else {
		pr_debug(DBG_MSG_PRFX "Engine is already disabled.");
	}

	/* wait for the running bit to be cleared */
	if (are_status_bits_set(self, DMA_STATUS_BIT_RUNNING)) {

		pr_debug(DBG_MSG_PRFX "Engine is running. Wait for running bit to be removed.");

		// wait until dma engine has stopped
		struct timeout timeout;
		timeout_init(&timeout, DMA_CTRL_DISABLE_TIMEOUT_MILLIS);

		while (are_status_bits_set(self, DMA_STATUS_BIT_RUNNING) && !timeout_has_elapsed(&timeout)) {
			// just loop
		}

		if (are_status_bits_set(self, DMA_STATUS_BIT_RUNNING)) {
			pr_err(DBG_MSG_PRFX "Timed out while disabling DMA engine.\n");
			pr_debug(DBG_MSG_PRFX "Status: 0x%08x\n", self->register_interface->read(self->register_interface, self->status_register));
			return DBG_TRACE_RETURN(STATUS_ERR_TIMEOUT);
		}
	}

    pr_debug(DBG_MSG_PRFX "Engine stopped. Status: 0x%08x\n", self->register_interface->read(self->register_interface, self->status_register));

	return DBG_TRACE_RETURN(STATUS_OK);
}

static int dma_controller_base_abort(dma_controller_base * self) {
	DBG_TRACE_BEGIN_FCT;

	pr_debug(DBG_MSG_PRFX "Aborting Engine. Status: 0x%08x\n", self->register_interface->read(self->register_interface, self->status_register));

	/* Stop the engine first. */
	int ret = self->stop(self);
	if (MEN_IS_ERROR(ret)) {
		pr_err(DBG_MSG_PRFX "Could not abort DMA Engine.\n");
		return DBG_TRACE_RETURN(ret);
	}

	/* at this point the dma engine is neither running nor enabled, but it might be in reset state. */
	if (!are_status_bits_set(self, DMA_STATUS_BIT_RESETTING)) {
		/* perform reset and wait for reset bit to become 0 again */
		self->register_interface->write(self->register_interface, self->control_register, DMA_CTRL_BIT_RESET);
		self->register_interface->b2b_barrier(self->register_interface);
	}

	/* At this point the reset bit is set. Clear it.*/
	self->register_interface->write(self->register_interface, self->control_register, 0);

	/* Make sure that the reset bit is cleared. */
	struct timeout timeout;
	timeout_init(&timeout, DMA_CTRL_RESET_TIMEOUT_MILLIS);

	while (are_status_bits_set(self, DMA_CTRL_BIT_RESET) && !timeout_has_elapsed(&timeout)) {
		// just loop
	}

	if (are_status_bits_set(self, DMA_CTRL_BIT_RESET)) {
		pr_err(DBG_MSG_PRFX "Timed out while resetting DMA engine.\n");
		return DBG_TRACE_RETURN(STATUS_ERR_TIMEOUT);
	}

	pr_debug(DBG_MSG_PRFX "Aborted Engine successfully. Status: 0x%08x\n", self->register_interface->read(self->register_interface, self->status_register));

	return DBG_TRACE_RETURN(STATUS_OK);
}

int dma_controller_base_init(dma_controller_base * dma_ctrl, register_interface * register_interface, uint32_t control_register, uint32_t status_register)
{
	DBG_TRACE_BEGIN_FCT;

	if (dma_ctrl == NULL) {
		pr_debug(DBG_MSG_PRFX "dma_ctrl is NULL.\n");
		return DBG_TRACE_RETURN(STATUS_ERR_INVALID_ARGUMENT);
	}

	dma_ctrl->register_interface = register_interface;
	dma_ctrl->control_register = control_register;
	dma_ctrl->status_register = status_register;

	dma_ctrl->is_enabled_and_running = dma_controller_base_is_enabled_and_running;
	dma_ctrl->start = dma_controller_base_start;
	dma_ctrl->stop = dma_controller_base_stop;
	dma_ctrl->abort = dma_controller_base_abort;

	return DBG_TRACE_RETURN(STATUS_OK);
}
