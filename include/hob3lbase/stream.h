/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

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
    (&(cp_stream_t){ .data = (file), .vprintf = (cp_stream_vprintf_t)cp_stream_vfprintf })

#define CP_STREAM_FROM_VCHAR(vchar) \
    (&(cp_stream_t){ .data = (vchar), .vprintf = (cp_stream_vprintf_t)cp_vchar_printf })

/**
 * Formatted printing into a stream.
 */
__attribute__((format(printf,2,3)))
extern void cp_printf(
    cp_stream_t *s,
    char const *form,
    ...);

/**
 * Use fprintf, checking for fatal errors.
 */
__attribute__((format(printf,2,0)))
extern void cp_stream_vfprintf(
    FILE *f, char const *form, va_list va);

/**
 * Print into stream via va list
 */
__attribute__((format(printf,2,0)))
static inline void cp_vprintf(
    cp_stream_t *s,
    char const *form,
    va_list va)
{
    s->vprintf(s->data, form, va);
}

#endif /* CP_STREAM_H_ */
