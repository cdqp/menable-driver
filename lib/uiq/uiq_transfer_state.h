/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef _LIB_UIQ_UIQ_TRANSFER_STATE_H_
#define _LIB_UIQ_UIQ_TRANSFER_STATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "uiq_base.h"
#include <lib/dma/messaging_dma_controller.h>

typedef struct {

    /** If a messaging dma engine is used, then this holds information about the currently processed transmission */
    struct messaging_dma_transmission_info current_dma_transmission;

    /** index of the currently used UIQ. -1 indicates that there is no current UIQ. */
    uiq_base * current_uiq;

    /** The current UIQ header word */
    uint32_t current_header;

    /** The number of words that are still to be processed. */
    uint16_t remaining_packet_words;
} uiq_transfer_state;

#ifdef __cplusplus
}
#endif

#endif /* Include Guard */