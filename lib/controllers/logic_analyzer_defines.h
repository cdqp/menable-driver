/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef LOGIC_ANALYZER_DEFINES_H_
#define LOGIC_ANALYZER_DEFINES_H_

enum spi_burst_flags
{
    /* general */
    LA_BURST_FLAG_NONE          = 0x0,
    LA_BURST_FLAG_SET_ADDRESS   = 0x1,
    LA_BURST_FLAG_SET_INCREMENT = 0x2,

    /* pre burst */

    /* post burst */
    LA_POST_BURST_FLAG_RESET = 0x10,
};


#endif /* LOGIC_ANALYZER_DEFINES_H_ */
