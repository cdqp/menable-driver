/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "uiq_helper.h"

#include <lib/helpers/error_handling.h>
#include <lib/helpers/bits.h>
#include <lib/os/print.h>
#include <sisoboards.h>

typedef enum uiq_packet_type {
    UIQ_PACKET_TYPE_VA_EVENT = 0,
    UIQ_PACKET_TYPE_FW_READ_UIQ = 1,
    UIQ_PACKET_TYPE_FW_WRITE_UIQ = 2,
    UIQ_PACKET_TYPE_MESSAGING_DMA_ARM = 3,
    UIQ_PACKET_TYPE_VA_METADATA_DMA = 4
} uiq_packet_type;

int get_first_va_event_uiq_idx_me5(uint32_t board_type) {

    if (!SisoBoardIsMe5(board_type)) {
        return STATUS_ERR_INVALID_ARGUMENT;
    }

    /* For CameraLink boards we usually have some RS232 channels followed by event channels.
     * Non-CameraLinkg boards only have event channels. */

    if (SisoBoardIsCL(board_type)) {
        if (SisoBoardIsMarathon(board_type)) {
            return 4;
        } else if (SisoBoardIsIronMan(board_type)) {
            return 2;
        }
    }

    return 0;
}

int determine_uiq_protocol(uint32_t uiq_id, int uiq_channel, uint32_t board_type, uiq_protocol * out_protocol) {

    if (SisoBoardIsMe6(board_type)) {

        /* For mE6 we can derive the protocol from packet type part of the UIQ's id.
         * All data, except VA events, is received in RAW format on an mE6 board. */
        const int packet_type = GET_BITS_32(uiq_id, 8, 15);
        *out_protocol = (packet_type == UIQ_PACKET_TYPE_VA_EVENT)
                                ? UIQ_PROTOCOL_VA_EVENT
                                : UIQ_PROTOCOL_RAW;

    } else if (SisoBoardIsMe5(board_type)) {

        /* For CameraLink boards we usually have some RS232 channels followed by event channels.
         * Non-CameraLinkg boards only have event channels. */

        int first_event_channel = get_first_va_event_uiq_idx_me5(board_type);

        if (MEN_IS_ERROR(first_event_channel)) {
            return first_event_channel;
        }

        *out_protocol = (uiq_channel < first_event_channel)
                            ? UIQ_PROTOCOL_LEGACY
                            : UIQ_PROTOCOL_VA_EVENT;

    } else if (SisoBoardIsMe4(board_type)) {

        /* mE4 does not support the event system. We simply assume Legacy. */
        *out_protocol = UIQ_PROTOCOL_LEGACY;

    } else {
        pr_err(KBUILD_MODNAME ": [UIQ] Cannot determine protocol. Unknown board Type 0x%04x.\n", board_type);
        return STATUS_ERR_UNKNOWN_BOARDTYPE;
    }

    return STATUS_OK;
}

const char * men_get_uiq_protocol_name(uiq_protocol protocol) {
    switch (protocol) {
    case UIQ_PROTOCOL_RAW:
        return "RAW";

    case UIQ_PROTOCOL_LEGACY:
        return "LEGACY";

    case UIQ_PROTOCOL_VA_EVENT:
        return "VA_EVENT";

    default:
        return "UNKNWON";
    }
}
