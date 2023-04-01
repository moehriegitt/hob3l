/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * Stream abstraction
 *
 * To print to file or vchar.
 */

#ifndef CP_STREAM_H_
#define CP_STREAM_H_

#include <hob3lbase/stream_tam.h>
#include <hob3lbase/vchar.h>
#include <stdio.h>

#define CP_STREAM_FROM_FILE(file) \
    (&(cp_stream_t){ \
        .data = (file), \
        .vprintf = (cp_stream_vprintf_t)cp_stream_vfprintf, \
        .write = (cp_stream_write_t)cp_stream_fwrite, \
    })

#define CP_STREAM_FROM_VCHAR(vchar) \
    (&(cp_stream_t){ \
        .data = (vchar), \
        .vprintf = (cp_stream_vprintf_t)cp_vchar_printf, \
        .write = (cp_stream_write_t)cp_vchar_append_arr, \
    })

/**
 * Formatted printing into a stream.
 */
CP_PRINTF(2,3)
extern void cp_printf(
    cp_stream_t *s,
    char const *form,
    ...);

/**
 * Use fprintf, checking for fatal errors.
 */
CP_VPRINTF(2)
extern void cp_stream_vfprintf(
    FILE *f, char const *form, va_list va);

/**
 * Use fputc, checking for fatal errors.
 */
extern void cp_stream_fwrite(
    FILE *f,
    void const *buff,
    size_t size);

/**
 * Print into stream via va list
 */
CP_VPRINTF(2)
static inline void cp_vprintf(
    cp_stream_t *s,
    char const *form,
    va_list va)
{
    s->vprintf(s->data, form, va);
}

/**
 * Print into stream via va list
 */
static inline void cp_write(
    cp_stream_t *s,
    void const *buff,
    size_t size)
{
    s->write(s->data, buff, size);
}

#endif /* CP_STREAM_H_ */
