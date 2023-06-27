/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef HELPER_H_
#define HELPER_H_

#include "../os/types.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) ((size_t)(sizeof(arr) / sizeof(arr[0])))
#endif

#define IGNORE_RETVAL(stmt) (void)(stmt)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define MIN3(a, b, c) MIN(MIN((a), (b)), (c))
#define MAX3(a, b, c) MAX(MAX((a), (b)), (c))

#define MIN4(a, b, c, d) MIN(MIN((a), (b)), MIN((c), (d))
#define MAX4(a, b, c, d) MAX(MAX((a), (b)), MAX((c), (d))

#define CEIL_DIV(a, b) (((a) + (b) - 1) / (b))

#ifndef min
static inline int min(int a, int b) {
    return (a < b) ? a : b;
}
#endif

#ifndef max
static inline int max(int a, int b) {
    return (a < b) ? b : a;
}
#endif

static inline uint8_t extract_byte(uint32_t word, int pos) {
    return ((uint8_t*) &word)[pos];
}

void dump_buffer(uint8_t* buffer, int buffer_size, const char* buffer_name);

#endif /* HELPER_H_ */
