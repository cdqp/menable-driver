/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LIB_DMA_MESSAGING_DMA_CONTROLLER_H_
#define LIB_DMA_MESSAGING_DMA_CONTROLLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <lib/helpers/type_hierarchy.h>
#include <lib/fpga/register_interface.h>

#include "dma_controller_base.h"

typedef struct messaging_dma_transmission_info {
    uint32_t * read_ptr;
    uint32_t num_words;
} messaging_dma_transmission_info;

typedef struct messaging_dma_controller {

    DERIVE_FROM(dma_controller_base);

    uint32_t init_register;

    uint8_t num_buffers;
    uint32_t ** buffer_addresses;
    uint32_t read_offset;

    /**
     * Copies the payload the next (earliest) trasmission into a destination buffer.
     */
    int (*copy_next_transmission_data)(struct messaging_dma_controller* self, uint32_t * dest_ringbuf_start, uint32_t dest_ringbuf_total_capacity, uint32_t dest_ringbuf_free_words, uint32_t dest_ringbuf_write_offset);

    /**
     * Gets information about the next transmission.
     * @attention The caller must ensure that there is a transmission avaiable.
     */
    int (*get_next_transmission)(struct messaging_dma_controller* self, messaging_dma_transmission_info * transmission_info);

    /**
     * Starts the Messaging DMA engine.
     *
     * @param self
     * The class instance that this method operates on. Must not be NULL!
     *
     * @returns
     * STATUS_OK on success, an error code on failure.
     */
    int (*start)(struct messaging_dma_controller* self);

    /**
     * Stops the Messaging DMA engine.
     *
     * @param self
     * The class instance that this method operates on. Must not be NULL!
     *
     * @returns
     * STATUS_OK on success, an error code on failure.
     */
    int (*stop)(struct messaging_dma_controller* self);

    /**
     * Aborts the Messaging DMA engine.
     *
     * @param self
     * The class instance that this method operates on. Must not be NULL!
     *
     * @returns
     * STATUS_OK on success, an error code on failure.
     */
    int (*abort)(struct messaging_dma_controller* self);

} messaging_dma_controller;

int messaging_dma_controller_init(messaging_dma_controller * messaging_dma, register_interface * register_interface, uint32_t ctrl_reg, uint32_t init_reg,
                                  uint8_t num_buffers, uint32_t * const buffer_addresses[], uint64_t buffer_iomem_addresses[]);

int messaging_dma_controller_destroy(messaging_dma_controller * messaging_dma);

#ifdef __cplusplus
}
#endif

#endif // LIB_DMA_MESSAGING_DMA_CONTROLLER_H_