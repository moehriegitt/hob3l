/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/stream.h>
#include <hob3lbase/panic.h>

/**
 * Formatted printing into a stream.
 */
CP_PRINTF(2,3)
extern void cp_printf(
    cp_stream_t *s,
    char const *form,
    ...)
{
    va_list va;
    va_start(va, form);
    cp_vprintf(s, form, va);
    va_end(va);
}

/**
 * Use fprintf, checking for fatal errors.
 */
CP_VPRINTF(2)
extern void cp_stream_vfprintf(
    FILE *f, char const *form, va_list va)
{
    int i = vfprintf(f, form, va);
    if (i < 0) {
        cp_panic(CP_FILE, CP_LINE, "Unable to write output file: %s\n",
            strerror(ferror(f)));
    }
}

/**
 * Use fputc, checking for fatal errors.
 */
extern void cp_stream_fwrite(
    FILE *f,
    void const *buff,
    size_t size)
{
    size_t i = fwrite(buff, 1, size, f);
    if (i == 0) {
        cp_panic(CP_FILE, CP_LINE, "Unable to write output file: %s\n",
            strerror(ferror(f)));
    }
}
