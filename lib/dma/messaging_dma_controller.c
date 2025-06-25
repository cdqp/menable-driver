/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#if defined(DBG_MESSAGING_DMA) && !defined(DBG_MESSAGING_DMA_OFF)
// OFF takes precedence over ON
    #define DEBUG
    #define DBG_LEVEL 1
#endif

#ifdef TRACE_MESSAGING_DMA
    #undef DBG_TRACE_ON
    #define DBG_TRACE_ON
#endif

#define OUTPUT_PREFIX KBUILD_MODNAME " [MSG DMA]: "

#include <lib/helpers/dbg.h>

#include "messaging_dma_controller.h"

#include <lib/helpers/helper.h>
#include <lib/helpers/bits.h>
#include <lib/os/string.h>

#include <lib/helpers/error_handling.h>
#include <lib/helpers/memory.h>

#define BYTES_PER_PAGE 4096
#define WORDS_PER_PAGE 1024


static int copy_next_transmission_data(struct messaging_dma_controller* self, uint32_t * dest_ringbuf_start, uint32_t dest_ringbuf_total_capacity, uint32_t dest_ringbuf_free_words, uint32_t dest_ringbuf_write_offset) {
    DBG_TRACE_BEGIN_FCT;

    /* TODO: [RKN] Implement UIQs (or generic ringbuffers) in library and pass such a ringbuffer as destination to this function */

    uint32_t src_buf_id = self->read_offset / WORDS_PER_PAGE;
    uint32_t src_buf_read_offset = self->read_offset % WORDS_PER_PAGE;

    uint32_t * src_buf_end = self->buffer_addresses[src_buf_id] + WORDS_PER_PAGE;
    uint32_t * read_ptr = self->buffer_addresses[src_buf_id] + src_buf_read_offset;

    const uint32_t transfer_size = *(read_ptr++);
    uint32_t words_to_process = transfer_size - 1; // the transfer size includes the size information

    int ret = STATUS_OK;
    if (words_to_process <= dest_ringbuf_free_words) {
        while (words_to_process > 0) {
            const uint32_t words_in_current_buf = (uint32_t)((self->buffer_addresses[src_buf_id] + WORDS_PER_PAGE) - read_ptr);
            const uint32_t free_words_until_rinbuf_wraparound = dest_ringbuf_total_capacity - dest_ringbuf_write_offset;
            const uint32_t chunk_size = MIN3(words_to_process, words_in_current_buf, free_words_until_rinbuf_wraparound);

            memcpy(dest_ringbuf_start + dest_ringbuf_write_offset, read_ptr, chunk_size * sizeof(*read_ptr));

            words_to_process -= chunk_size;

            if (words_to_process > 0) {
                /* very complicated updates ... */
                dest_ringbuf_write_offset = (dest_ringbuf_write_offset + chunk_size) % dest_ringbuf_total_capacity;
                if ((words_in_current_buf - chunk_size) > 0) {
                    /* there is data left in the current source buffer */
                    read_ptr += chunk_size;
                } else {
                    /* we reached the end of our current source buffer, move to the next one */
                    src_buf_id = (src_buf_id + 1) % self->num_buffers;
                    src_buf_end = self->buffer_addresses[src_buf_id] + WORDS_PER_PAGE;
                    read_ptr = self->buffer_addresses[src_buf_id];
                }
            }
        }

        ret = transfer_size - 1;
    } else {
        pr_err(OUTPUT_PREFIX "Target buffer too small to copy messaging DMA payload.\n");
        ret = STATUS_ERR_OVERFLOW;
    }

    const uint32_t total_buffer_capacity = self->num_buffers * WORDS_PER_PAGE;

    /* update the read offset to after the payload */
    self->read_offset = (self->read_offset + transfer_size) % total_buffer_capacity;

    /* if the next payload size would be written to the last word of a buffer, it ist written to the first word of the next buffer instead */
    if ((self->read_offset % WORDS_PER_PAGE) == (WORDS_PER_PAGE - 1)) {
        self->read_offset = (self->read_offset + 1) % total_buffer_capacity;
    }

    return DBG_TRACE_RETURN(ret);
}

static int get_next_transmission(messaging_dma_controller * self, messaging_dma_transmission_info * transmission_info) {
    DBG_TRACE_BEGIN_FCT;

    if (!upcast(self)->is_enabled_and_running(upcast(self))) {
        pr_err(OUTPUT_PREFIX "DMA engine is not running.\n");
        return STATUS_ERR_INVALID_STATE;
    }

    uint32_t buf_id = self->read_offset / WORDS_PER_PAGE;
    if (buf_id >= self->num_buffers) {
        pr_err(OUTPUT_PREFIX "Buffer offset %u ( = buffer id %u) for next transmission is too large.\n", self->read_offset, buf_id);
        return STATUS_ERR_OVERFLOW;
    }

    uint32_t buf_read_offset = self->read_offset % WORDS_PER_PAGE;
    if (buf_read_offset == (WORDS_PER_PAGE - 1)) {
        /* according to firmware specification, a transmission will never start at the last word of a buffer. */
        pr_err(OUTPUT_PREFIX "Invalid read offest %u ( = %u within buffer). Transmission may not start at last word of a buffer.\n", self->read_offset, buf_read_offset);
        return STATUS_ERR_INVALID_STATE;
    }

    uint32_t * read_ptr = self->buffer_addresses[buf_id] + buf_read_offset;

    /* first word is the transmission length including the length word */
    uint32_t transmission_length = *read_ptr;

    if (transmission_length == 0) {
        pr_err(OUTPUT_PREFIX "Received a transmission with invalid length 0.\n");
        return STATUS_ERR_INVALID_STATE;
    }

    if (buf_read_offset + transmission_length > WORDS_PER_PAGE) {
        pr_err(OUTPUT_PREFIX "Invalid transmission length. Length of %u words with offset %u crosses page boundary.\n", transmission_length, buf_read_offset);
        return STATUS_ERR_OVERFLOW;
    }

    /* the payload starts at the next word */
    transmission_info->read_ptr = read_ptr + 1;
    transmission_info->num_words = transmission_length - 1;

    pr_debug(OUTPUT_PREFIX "next transmission: bufferId=%u bufferOffset=%u transmissionLength=%u readPtr=0x%016llx firstWord=0x%08x\n",
                                               buf_id,     buf_read_offset, transmission_length, (uint64_t)read_ptr, *(read_ptr));

    /* Update the read offset.
     * Note: We rely on the transmission length not to cross a page boundary */

    self->read_offset += transmission_length;

    /* If we have only one word left in the page, we jump to the next buffer. */
    const uint32_t remaining_words_in_buffer = WORDS_PER_PAGE - (self->read_offset % WORDS_PER_PAGE);
    if (remaining_words_in_buffer == 1) {
        self->read_offset += 1;
    }

    /* Handle Buffer Wraparound */
    const uint32_t total_buffer_capacity = self->num_buffers * WORDS_PER_PAGE;
    self->read_offset %= total_buffer_capacity;

    return DBG_TRACE_RETURN(STATUS_OK);
}

static void messaging_dma_declare_buffer(messaging_dma_controller * self, uint8_t buffer_id, uint64_t buffer_iomem_address) {
    DBG_TRACE_BEGIN_FCT;

    register_interface * reg_iface = upcast(self)->register_interface;

    pr_debug(OUTPUT_PREFIX "Declaring Messaging DMA buffer at 0x%016llx with id %d\n", buffer_iomem_address, buffer_id);

    uint32_t reg_val_1 = buffer_iomem_address & UINT32_MAX;
    SET_BITS_32(reg_val_1, buffer_id, 0, 6); // Note: difference to mini and b2b dma: 6 bits for buffer id instead of 5!
    SET_BITS_32(reg_val_1, 0, 7, 11);
    pr_debug(OUTPUT_PREFIX "   writing to Messaging DMA init reg at 0x%08x: 0x%08x\n", self->init_register, reg_val_1);
    reg_iface->write(reg_iface, self->init_register, reg_val_1);

    uint32_t reg_val_2 = ((buffer_iomem_address >> 32) & UINT32_MAX);
    pr_debug(OUTPUT_PREFIX "   writing to Messaging DMA init reg at 0x%08x: 0x%08x\n", self->init_register, reg_val_2);
    reg_iface->write(reg_iface, self->init_register, reg_val_2);

    DBG_TRACE_END_FCT;
}

static int messaging_dma_init_buffers(messaging_dma_controller * self, uint8_t num_buffers, uint32_t * const buffer_addresses[], uint64_t const buffer_iomem_addresses[]) {
    self->num_buffers = num_buffers;
    self->read_offset = 0;

    // This class may be used from inside an interrupt handler or its bottom half, so we need non-pageable memory.
    self->buffer_addresses = alloc_nonpageable_cacheable_small(num_buffers * sizeof(self->buffer_addresses[0]), 'GSEM');
    if (self->buffer_addresses == NULL) {
        pr_err(OUTPUT_PREFIX "Failed to allocate memory for messaging dma.\n");
        return STATUS_ERROR;
    }

    for (uint8_t bufId = 0; bufId < num_buffers; ++bufId) {
        pr_debug(OUTPUT_PREFIX "Declare Messaging DMA Buffer[%u] with virtaddr=0x%016llx, physaddr=0x%016llx\n", bufId, (uint64_t)buffer_addresses[bufId], buffer_iomem_addresses[bufId]);
        self->buffer_addresses[bufId] = buffer_addresses[bufId];
        messaging_dma_declare_buffer(self, bufId, buffer_iomem_addresses[bufId]);
    }

    return STATUS_OK;
}

static int start(messaging_dma_controller * self) {
    return upcast(self)->start(upcast(self));
}

static int stop(messaging_dma_controller * self) {
    return upcast(self)->stop(upcast(self));
}

static int stop_and_reset(messaging_dma_controller * self) {

    /* TODO: [RKN][!!!] After aborting, the fpga has lost its buffer addresses, 
                        so starting again would require a re-initialization of the buffers.
                        How can we deal with this? Could we simply rely on the user to 
                        perform another full initialization after aborting?
     */

    return upcast(self)->abort(upcast(self));
}

int messaging_dma_controller_init(messaging_dma_controller * messaging_dma, register_interface * register_interface, uint32_t ctrl_reg, uint32_t init_reg, 
                                  uint8_t num_buffers, uint32_t * const buffer_addresses[], uint64_t buffer_iomem_addresses[]) {

    /* TODO: [RKN] Try to implement the allocation of DMA memory in the library and remove the arguments for the buffer adresses. 
     *             On linux `kmalloc` + `dma_map_single` looks promising but it requires a device instance, on windows we can stick to `MmAllocateNonCachedMemory` + `MmGetPhysicalAddress`.
    */
    
    DBG_TRACE_BEGIN_FCT;

    pr_debug(OUTPUT_PREFIX "Messaging DMA Status before initialization: 0x%08x\n", register_interface->read(register_interface, ctrl_reg));

    int ret = dma_controller_base_init(upcast(messaging_dma), register_interface, ctrl_reg, ctrl_reg);
    if(MEN_IS_ERROR(ret)) {
        pr_err(OUTPUT_PREFIX "Failed to init Messaging DMA Controller.\n");
        return STATUS_ERROR;
    }

    messaging_dma->init_register = init_reg;

    messaging_dma->start = start;
    messaging_dma->stop = stop;
    messaging_dma->abort = stop_and_reset;
    messaging_dma->copy_next_transmission_data = copy_next_transmission_data;
    messaging_dma->get_next_transmission = get_next_transmission;

    /* basic struct initializations done */

    messaging_dma->abort(messaging_dma);

    ret = messaging_dma_init_buffers(messaging_dma, num_buffers, buffer_addresses, buffer_iomem_addresses);
    if (MEN_IS_ERROR(ret)) {
        pr_err(OUTPUT_PREFIX "Failed to init messaging DMA buffers.\n");
        return STATUS_ERROR;
    }

    return DBG_TRACE_RETURN(STATUS_OK);
}

int messaging_dma_controller_destroy(messaging_dma_controller * messaging_dma) {

    if (messaging_dma->abort != NULL) {
        messaging_dma->abort(messaging_dma);
    }

    if (messaging_dma->buffer_addresses != NULL) {
        free_nonpageable_cacheable_small(messaging_dma->buffer_addresses, 'GSEM');
        messaging_dma->buffer_addresses = NULL;
    }

    return STATUS_OK;
}
