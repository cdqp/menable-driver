/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "dbg.h"
#include "../os/print.h"
#include "../os/string.h"

static const int indent_size = 4;
static int dbg_trace_indent_level = 0;

static const char * get_indent(int indent_level) {
    static char indent[1024];
    memset(indent, 0, sizeof(indent));

    for (int i = 0; i < indent_level * indent_size; ++i) {
        indent[i] = (i % indent_size == 0) ? ':' : ' ';
    }

    return indent;
}

void dbg_trace_begin(const char* prefix, const char* func) {
    const char * last_dir_separator = strrchr(prefix, '/');
    if (last_dir_separator)
        prefix = last_dir_separator + 1;

    pr_debug(KBUILD_MODNAME ": [TRACE] %s| BEGIN %s%s\n", get_indent(dbg_trace_indent_level++), prefix, func);
}

void dbg_trace_end(const char* prefix, const char* func) {
    const char * last_dir_separator = strrchr(prefix, '/');
    if (last_dir_separator)
        prefix = last_dir_separator + 1;

    pr_debug(KBUILD_MODNAME ": [TRACE] %s| END   %s%s \n", get_indent(--dbg_trace_indent_level), prefix, func);
}

void dbg_trace_begin_end(const char* prefix, const char* func) {
    const char * last_dir_separator = strrchr(prefix, '/');
    if (last_dir_separator)
        prefix = last_dir_separator + 1;

    pr_debug(KBUILD_MODNAME ": [TRACE] %s| BEGIN/END %s%s\n", get_indent(dbg_trace_indent_level + 1), prefix, func);
}

