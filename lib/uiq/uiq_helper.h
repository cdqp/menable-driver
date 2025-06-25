/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#ifndef _LIB_UIQ_UIQ_HELPER_H_
#define _LIB_UIQ_UIQ_HELPER_H_

#include "uiq_defines.h"

#include <lib/boards/peripheral_declaration.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Determins the index of the first VA event UIQ.
 *
 * @param board_type The type of the board.
 * @return The index or a negative error code in case of failure.
 */
int get_first_va_event_uiq_idx_me5(uint32_t board_type);

/**
 * Determines the protocol for a given UIQ.
 *
 * @param uiq_id        The id of the UIQ (mE6 only)
 * @param uiq_channel   The zero based (!) channel of the uiq
 * @param board_type
 * @param out_protocol
 * @return
 */
int determine_uiq_protocol(uint32_t uiq_id, int uiq_channel, uint32_t board_type, uiq_protocol * out_protocol);

const char * men_get_uiq_protocol_name(uiq_protocol protocol);

/**
 * Gets a timestamp in the format required for pushing it into a UIQ.
 */
void men_get_uiq_timestamp(uiq_timestamp* out_timestamp);

#ifdef __linux__
void set_uiq_timestamp_offset(int64_t seconds);
#endif


#ifdef __cplusplus
} // extern "C"
#endif

#endif // _LIB_UIQ_UIQ_HELPER_H_
