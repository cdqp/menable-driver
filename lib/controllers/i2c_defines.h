/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef I2C_DEFINES_H_
#define I2C_DEFINES_H_

#define I2C_ADDRESS_FOR_WRITE(address) ((address) << 1)
#define I2C_ADDRESS_FOR_READ(address) (((address) << 1) | 1)

enum burst_flags {
    I2C_BURST_FLAG_NONE                 = 0x00,
    I2C_PRE_BURST_FLAG_START_CONDITION  = 0x01,
    I2C_POST_BURST_FLAG_STOP_CONDITION  = 0x02,
    I2C_POST_BURST_FLAG_SEND_NACK       = 0x04,
    I2C_POST_BURST_FLAG_ACK_POLLING     = 0x08,
    I2C_BURST_FLAG_WRITE_ENABLE         = 0x10
};

#endif /* I2C_DEFINES_H_ */
