/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef BITS_H_
#define BITS_H_

#include "../os/types.h"

#define BIT_0 0x01
#define BIT_1 0x02
#define BIT_2 0x04
#define BIT_3 0x08
#define BIT_4 0x10
#define BIT_5 0x20
#define BIT_6 0x40
#define BIT_7 0x80

#ifndef BIT
#define BIT(n) (1u << (n))
#endif

#ifndef BIT_ULL
#define BIT_ULL(n) (1ull << (n))
#endif

#define GET_BIT_0(word) (word & BIT_0)
#define GET_BIT_1(word) ((word & BIT_1) >> 1)
#define GET_BIT_2(word) ((word & BIT_2) >> 2)
#define GET_BIT_3(word) ((word & BIT_3) >> 3)
#define GET_BIT_4(word) ((word & BIT_4) >> 4)
#define GET_BIT_5(word) ((word & BIT_5) >> 5)
#define GET_BIT_6(word) ((word & BIT_6) >> 6)
#define GET_BIT_7(word) ((word & BIT_7) >> 7)

#define GET_BIT(word, n) ((word >> (n)) & 1)

#define _men_GEN_BITS_IMPL(type, num_bits, n) (((type)-1) >> ((num_bits) - (n)))
#define _men_GEN_BITS_INV_IMPL(type, n) (((type)-1) << (n))
#define _men_GEN_MASK_IMPL(type, num_bits, from, to) (_men_GEN_BITS_IMPL(type, (num_bits), (to)+1) & _men_GEN_BITS_INV_IMPL(type, (from)))
#define _men_GET_BITS_IMPL(type, num_bits, word, from, to) (((type)(word) & _men_GEN_MASK_IMPL(type, (num_bits), (from), (to))) >> (from))
#define _men_SET_BITS_IMPL(type, num_bits, word, val, from, to) (word = (word & ~_men_GEN_MASK_IMPL(type, (num_bits), (from), (to))) | (((type)val << from) & _men_GEN_MASK_IMPL(type, (num_bits), (from), (to))))

#define GEN_BITS_64(n) (_men_GEN_BITS_IMPL(uint64_t, 64, n))
#define GEN_BITS_INV_64(n) (_men_GEN_BITS_INV_IMPL(uint64_t, n))
#define GEN_MASK_64(from, to) (_men_GEN_MASK_IMPL(uint64_t, 64, (from), (to)))
#define GET_BITS_64(word, from, to) (_men_GET_BITS_IMPL(uint64_t, 64, (word), (from), (to)))
#define SET_BITS_64(word, val, from, to) (_men_SET_BITS_IMPL(uint64_t, 64, (word), (val), (from), (to)))

#define GEN_BITS_32(n) (_men_GEN_BITS_IMPL(uint32_t, 32, n))
#define GEN_BITS_INV_32(n) (_men_GEN_BITS_INV_IMPL(uint32_t, n))
#define GEN_MASK_32(from, to) (_men_GEN_MASK_IMPL(uint32_t, 32, (from), (to)))
#define GET_BITS_32(word, from, to) (_men_GET_BITS_IMPL(uint32_t, 32, (word), (from), (to)))
#define SET_BITS_32(word, val, from, to) (_men_SET_BITS_IMPL(uint32_t, 32, (word), (val), (from), (to)))

#define GEN_BITS_16(n) (_men_GEN_BITS_IMPL(uint16_t, 16, n))
#define GEN_BITS_INV_16(n) (_men_GEN_BITS_INV_IMPL(uint16_t, n))
#define GEN_MASK_16(from, to) (_men_GEN_MASK_IMPL(uint16_t, 16, (from), (to)))
#define GET_BITS_16(word, from, to) (_men_GET_BITS_IMPL(uint16_t, 16, (word), (from), (to)))
#define SET_BITS_16(word, val, from, to) (_men_SET_BITS_IMPL(uint16_t, 16, (word), (val), (from), (to)))

#define GEN_BITS_8(n) (_men_GEN_BITS_IMPL(uint8_t, 8, n))
#define GEN_BITS_INV_8(n) (_men_GEN_BITS_INV_IMPL(uint8_t, n))
#define GEN_MASK_8(from, to) (_men_GEN_MASK_IMPL(uint8_t, 8, (from), (to)))
#define GET_BITS_8(word, from, to) (_men_GET_BITS_IMPL(uint8_t, 8, (word), (from), (to)))
#define SET_BITS_8(word, val, from, to) (_men_SET_BITS_IMPL(uint8_t, 8, (word), (val), (from), (to)))

#define BIT_TO_STR(bit_value, on_msg, off_msg) ((bit_value) ? (on_msg) : (off_msg))
#define BIT_TO_ON_OFF_STR(bit_value) BIT_TO_STR(bit_value, "on", "off")
#define BIT_TO_YES_NO_STR(bit_value) BIT_TO_STR(bit_value, "yes", "no")
#define BIT_TO_NO_YES_STR(bit_value) BIT_TO_STR(bit_value, "no", "yes")
#define BIT_TO_ERR_OK_STR(bit_value) BIT_TO_STR(bit_value, "error", "ok")
#define BIT_TO_EN_DIS_STR(bit_value) BIT_TO_STR(bit_value, "enabled", "disabled")
#define BIT_TO_DIS_EN_STR(bit_value) BIT_TO_STR(bit_value, "disabled", "enabled")

#define ARE_BITS_SET(word, mask) (((word) & (mask)) == (mask))
#define IS_BIT_SET(word, bit_position) (((word) & (1 << (bit_position))) != 0)

#define SET_FLAGS(word, flags) ((word) |= (flags))

static inline uint8_t modify_bits(uint8_t bits, uint8_t modify_mask, uint8_t new_vals) {
    return (bits & ~modify_mask) | (new_vals & modify_mask);
}

static inline bool are_flags_set(uint8_t bits, uint8_t flags) {
    return (bits & flags) == flags;
}

/* Generic implementation for to_bin_x.
   
   Creates a string with the binary representation of an integer value and
   returns the poiner.
   
   The pointer remains valid until the next call of the
   function.
   
   This is not threadsafe!
*/
#define _men_int_to_bin_impl \
    static char bin[(sizeof(val) * 8) + 1]; \
    for (int i = 0; i < (sizeof(val) * 8); ++i) { \
        bin[(sizeof(val) * 8) - 1 - i] = ((val & (1ull << i)) == 0) ? '0' : '1'; \
    } \
    bin[(sizeof(val) * 8)] = '\0'; \
    return bin;


static inline const char * to_bin_64(uint64_t val) {
    _men_int_to_bin_impl
}

static inline const char * to_bin_32(uint32_t val) {
    _men_int_to_bin_impl
}

static inline const char * to_bin_16(uint16_t val) {
    _men_int_to_bin_impl
}

static inline const char * to_bin_8(uint8_t val) {
    _men_int_to_bin_impl
}


#endif /* BITS_H_ */
