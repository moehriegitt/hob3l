/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <cpmat/stream.h>

/**
 * Formatted printing into a stream.
 */
__attribute__((format(printf,2,3)))
extern int cp_printf(
    cp_stream_t *s,
    char const *form,
    ...)
{
    va_list va;
    va_start(va, form);
    int i = cp_vprintf(s, form, va);
    va_end(va);
    return i;
}
