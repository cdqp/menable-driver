/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef LIB_OS_LINUX_USER_TYPES_H_
#define LIB_OS_LINUX_USER_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define bswap_16(x) \
  ((__uint16_t) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8)))

#define bswap_32(x) \
  ((((x) & 0xff000000u) >> 24) | (((x) & 0x00ff0000u) >> 8)	\
   | (((x) & 0x0000ff00u) << 8) | (((x) & 0x000000ffu) << 24))

#define bswap_64(x) \
  ((((x) & 0xff00000000000000ull) >> 56)	\
   | (((x) & 0x00ff000000000000ull) >> 40)	\
   | (((x) & 0x0000ff0000000000ull) >> 24)	\
   | (((x) & 0x000000ff00000000ull) >> 8)	\
   | (((x) & 0x00000000ff000000ull) << 8)	\
   | (((x) & 0x0000000000ff0000ull) << 24)	\
   | (((x) & 0x000000000000ff00ull) << 40)	\
   | (((x) & 0x00000000000000ffull) << 56))


#define swab32(x) bswap_32(x)
#define swab16(x) bswap_16(x)
#define swab64(x) bswap_64(x)

#include <pthread.h>
typedef pthread_mutex_t user_mode_lock;

#endif /* LIB_OS_LINUX_USER_TYPES_H_ */
