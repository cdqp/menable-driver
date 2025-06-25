/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef SPI_V2_DEFINES_H_
#define SPI_V2_DEFINES_H_

enum spi_burst_flags
{
    /* general */
    SPI_BURST_FLAG_NONE        = 0x0, /* The default is PRE_ASSERT_CS, SINGLE_MODE, CHIP_ACCESS and POST_DEASSERT_CS */
    SPI_BURST_FLAG_QUAD_MODE   = 0x1,
    SPI_BURST_FLAG_DATA_ACCESS = 0x2,

    /* pre burst */

    /* post burst */
    SPI_POST_BURST_FLAG_LEAVE_CS_ASSERTED = 0x10,
};


#endif /* SPI_V2_DEFINES_H_ */
