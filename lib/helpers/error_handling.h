/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef ERROR_HANDLING_H_
#define ERROR_HANDLING_H_

// TODO: Prefix these values with MEN_ or similar to avoid conflits

#define STATUS_OK           0          /* no error */
#define STATUS_MORE_DATA_AVAILABLE 1   /* there is more data available than could be retrieved i.e. because a buffer was too small */

#define STATUS_ERROR                  -1  /* unspecified error */
#define STATUS_ERR_TIMEOUT            -2  /* a timeout has elapsed */
#define STATUS_ERR_DEV_IO             -3  /* an device I/O error occurred */
#define STATUS_ERR_NOT_FOUND          -4  /* whatever was searched for, it was not found */
#define STATUS_ERR_INVALID_ARGUMENT   -5  /* at least one invalid argument that was given to the function */
#define STATUS_ERR_INSUFFICIENT_MEM   -6  /* a memory allocation failed */
#define STATUS_ERR_INVALID_OPERATION  -7  /* The reqeusted operation is invalid */
#define STATUS_ERR_OVERFLOW           -8  /* the requested operation would have lead to an overflow (typically of some kind of buffer) */
#define STATUS_ERR_INVALID_FLAGS      -9  /* at least one invalid flag that was given to the call */
#define STATUS_ERR_INVALID_STATE     -10  /* the driver component is in an invalid state (aka "this should never happen") */
#define STATUS_ERR_UNKNOWN_BOARDTYPE -11  /* The board has an unknown type */

/* i2c */
#define STATUS_I2C_NO_ACK -100

#define MEN_IS_ERROR(retval) ((retval) < 0)
#define MEN_IS_SUCCESS(retval) (!MEN_IS_ERROR(retval))

#endif /* ERROR_HANDLING_H_ */
