/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef MEN_LIB_HELPERS_DBG_H_
#define MEN_LIB_HELPERS_DBG_H_

#ifdef __cplusplus
extern "C" {
#endif

// Try to get rid of dynamic debug. To make this work, this file must be included 
// before `linux/kconfig.h` is included, so probably before any kernel headers.
// Alternatively, the following undefs can be placed on top of source files.
#undef CONFIG_DYNAMIC_DEBUG
#undef CONFIG_DYNAMIC_DEBUG_CORE

/* Activate `pr_debug` etc. if `MEN_DEBUG` is defined.
 * On the other hand, if `MEN_DEBUG` is not defined, we do not want any
 * debugging output, regardless of DEBUG being defined or not. So we
 * undefine `DEBUG`in this case.
 *
 * If debug output for certain components is desired, define `MEN_DEBUG`
 * in this component before including `dbg.h`.
 *
 * Note that this only works, if the headers, which define debugging macros
 * have not been included, yet. So place the include for `dbg.h` as early as
 * possible.
*/
#ifdef MEN_DEBUG
    #define DEBUG
#else
    #undef DEBUG
#endif
#include "../os/types.h"
#include "../os/kernel_macros.h"
#include "../os/print.h"

#ifndef DBG_PREFIX
    #ifdef DBG_PRFX
        #define DBG_PREFIX DBG_PRFX
    #else
        #define DBG_PREFIX KBUILD_MODNAME
    #endif
#endif

#define TO_BINARY_IMPL(val, type, num_bits) { \
    static char buf[64 + 15 + 1]; \
    int pos = 0; \
    int bits = (num_bits); \
    for (int i = bits - 1; i >= 0; --i) { \
        buf[pos++] = (val & (((type)1) << i)) ? '1' : '0'; \
        if (i % 4 == 0 && i != 0) \
            buf[pos++] = ' '; \
    } \
    buf[pos] = '\0'; \
    return buf; \
}

static inline const char * to_binary_64(uint64_t val) TO_BINARY_IMPL(val, uint64_t, 64)
static inline const char * to_binary_32(uint32_t val) TO_BINARY_IMPL(val, uint32_t, 32)
static inline const char * to_binary_16(uint16_t val) TO_BINARY_IMPL(val, uint16_t, 16)
static inline const char * to_binary_8(uint8_t val) TO_BINARY_IMPL(val, uint8_t, 8)
static inline const char * to_binary(uint32_t val, uint8_t n) TO_BINARY_IMPL(val, uint32_t, n)

void dbg_trace_begin(const char* prefix, const char* func);
void dbg_trace_end(const char* prefix, const char* func);
void dbg_trace_begin_end(const char* prefix, const char* func);

static inline void dbg_trace_nop(void) {}


/*
 * To have debugging and/or trace output only for some source files,
 * DBG_LEVEL can be defined there prior to including dbg.h
 * instead of defining them here.
 */
#ifdef MEN_DEBUG

    #ifndef DBG_LEVEL
    #define DBG_LEVEL 0
    #endif


#else

    #ifndef DBG_LEVEL
    #define DBG_LEVEL -1
    #endif

#endif


/* General Debugging */

#undef DBG_STMT
#undef DBG1

#if DBG_LEVEL >= 0
#define DBG_STMT(stmt) stmt
#if DBG_LEVEL >= 1
#define DBG1(stmt) stmt
#else
#define DBG1(stmt) do {} while (0)
#endif
#else
#define DBG_STMT(stmt) do {} while (0)
#define DBG1(stmt) do {} while (0)
#endif


/* Function Tracing */

#undef DBG_TRACE_BEGIN_FCT
#undef DBG_TRACE_END_FCT

#ifdef DBG_TRACE_ON
    /*
     * Logs the start of the current function.
     */
    #define DBG_TRACE_BEGIN_FCT dbg_trace_begin(KBUILD_BASENAME ":",  __FUNCTION__)

    /** 
     * Logs the end of the current function.
     */
    #define DBG_TRACE_END_FCT dbg_trace_end(KBUILD_BASENAME ":", __FUNCTION__)

    /**
     * Logs the beginning and end of an empty function.
     */
    #define DBG_TRACE_BEGIN_END_FCT dbg_trace_begin_end(KBUILD_BASENAME ":", __FUNCTION__)
    
    /**
     * Logs the end of the current function and evaluates to the given retval.
     * Usage example:
     * @code return DBG_TRACE_RETURN(STATUS_OK);
     * 
     * Prefer this over DBG_TRACE_END_FCT_AND_RETURN, to have the return statement visible in the source code.
     * 
     * @param 	retval
     * The value of the macro expansion.
     */
    #define DBG_TRACE_RETURN(retval) (dbg_trace_end(KBUILD_BASENAME ":", __FUNCTION__), (retval))

#else
    #define DBG_TRACE_BEGIN_FCT dbg_trace_nop()
    #define DBG_TRACE_END_FCT dbg_trace_nop()
    #define DBG_TRACE_BEGIN_END_FCT dbg_trace_nop()
    #define DBG_TRACE_RETURN(retval) (retval)
#endif

/* Register I/O */

#undef DBG_REG_WRITE
#undef DBG_REG_READ
#undef DBG_REG_IO

#ifdef DBG_REG_IO_ON
    #define DBG_REG_WRITE(address, val) do {pr_debug(DBG_PREFIX " [REG_IO]: written to 0x%04x: 0x%08x   (bin)%s\n", (address), (val), to_binary_32((val)));} while(0)
    #define DBG_REG_READ(address, val) do {pr_debug(DBG_PREFIX " [REG_IO]: read from  0x%04x: 0x%08x   (bin)%s\n", (address), (val), to_binary_32((val)));} while(0)
    #define DBG_REG_IO(stmt) stmt
#else
    #define DBG_REG_WRITE(address, val)
    #define DBG_REG_READ(address, val)
    #define DBG_REG_IO(stmt) do {} while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif // Include Guard