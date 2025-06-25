/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "helper.h"

#include "../os/kernel_macros.h"
#include "../os/types.h"
#include "../os/string.h"
#include "../os/print.h"

void dump_buffer(uint8_t* buffer, int buffer_size, const char* buffer_name) {
    pr_info("\nBuffer Dump: %s", buffer_name);
    pr_info("\n~~~~~~~~~~~~~");
    for (size_t i = 0; i < strlen(buffer_name); ++i)
        pr_info("~");
    pr_info("\n");

    for (int i = 0; i < buffer_size; ++i) {
        char c = buffer[i];
        pr_info("%04x: 0x%02x [%c], ", i, (uint8_t ) c,
                (c < 0x20 || c > 0x7e) ? '.' : c);
        if ((i + 1) % 8 == 0)
            pr_info("\n");
    }
    pr_info("\n");
}

