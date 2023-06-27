/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */
 
#pragma once

#define MULTICHAR32(c1, c2, c3, c4) ((c1 << 24) | (c2 << 16) | (c3 << 8) | (c4))
#define MULTICHAR16(c1, c2) ((c1 << 8) | (c2))

// The following very cool stuff is only for user space C++ applications
#if defined(__cplusplus)

extern "C++" {

#include <stdint.h>
#include <stddef.h>

/**
 * Creates an integer value from the characters of a string
 * by concatenating each character in memory. The first character
 * becomes the most significant byte.
 *
 * The length of the string (excluding the terminating '\0') must match the size of T.
 *
 * @remark
 * This is a well defined and portable alternative to multichar literals.
 *
 * @example
 * auto constexpr x = multichar<uint32_t>("TEST"); // x will be evaluated at compiletime to 0x54455354 (T=0x53, E=0x45, S=0x53)
 */
template<typename T, size_t N>
constexpr T multichar(const char(&str)[N], size_t pos = 0) {
    static_assert(N > 1 && sizeof(T) == N - 1, "Length of str must match sizeof(T)");
    return (pos == N - 1)
        ? str[pos]
        : (static_cast<T>(str[pos]) << ((N - 2 - pos) * 8)) | multichar<T>(str, pos + 1);
}

constexpr uint32_t multichar32(char c1, char c2, char c3, char c4) {
    return static_cast<uint32_t>(MULTICHAR32(c1, c2, c3, c4));
}

constexpr uint16_t multichar16(char c1, char c2) {
    return static_cast<uint16_t>(MULTICHAR16(c1, c2));
}

template<int N>
constexpr uint32_t multichar32(char arr[N]) {
    static_assert(N == 4, "Only char[4] allowed!");
    return multichar32(arr[0], arr[1], arr[2], arr[3]);
}

#if !defined(_KERNEL_MODE) && !defined(USERMODE_DRIVER_TEST)

#include <array>
#include <stdexcept>

#if 0
constexpr std::array<char, 4> multichar32ToArray(uint32_t mc) {
    return std::array<char, 4> { char(mc >> 24), char(mc >> 16), char(mc >> 8), char(mc) };
}

constexpr std::array<char, 2> multichar16ToArray(uint16_t mc) {
    return std::array<char, 2> { char(mc >> 8), char(mc) };
}
#endif

inline std::string multichar32ToString(uint32_t mc) {
    return std::string{ char(mc >> 24), char(mc >> 16), char(mc >> 8), char(mc) };
}

inline std::string multichar16ToString(uint16_t mc) {
    return std::string{ char(mc >> 8), char(mc) };
}

template<typename TRet>
TRet multichar(const std::string& str) {
    using CharArray = const char[sizeof(TRet) + 1]; // +1 for terminating '\0'

    if (str.size() != sizeof(TRet))
        throw std::runtime_error("Invalid string size!");

    CharArray* arr = reinterpret_cast<CharArray*>(str.c_str());
    return multichar<TRet>(*arr);
}

inline uint32_t multichar32(const std::string& str) {
    if (str.size() != 4)
        throw std::runtime_error("Invalid string size!");

    return multichar32(str[0], str[1], str[2], str[3]);
}

inline uint16_t multichar16(const std::string& str) {
    if (str.size() != 2)
        throw std::runtime_error("Invalid string size!");

    return multichar16(str[0], str[1]);
}

#endif

} // extern "C++"

#endif