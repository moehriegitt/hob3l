/* -*- Mode: C -*- */

/**
 * Stream abstraction
 *
 * To print to file or vchar.
 */

#ifndef __CP_STREAM_H
#define __CP_STREAM_H

#include <cpmat/stream_tam.h>
#include <cpmat/vchar.h>
#include <stdio.h>

#define CP_STREAM_FROM_FILE(file) \
    (&(cp_stream_t){ .data = (file), .vprintf = (cp_stream_vprintf_t)vfprintf })

#define CP_STREAM_FROM_VCHAR(vchar) \
    (&(cp_stream_t){ .data = (vchar), .vprintf = (cp_stream_vprintf_t)cp_vchar_printf })

__attribute__((format(printf,2,0)))
static inline int cp_vprintf(
    cp_stream_t *s,
    char const *form,
    va_list va)
{
    return s->vprintf(s->data, form, va);
}

__attribute__((format(printf,2,3)))
extern int cp_printf(
    cp_stream_t *s,
    char const *form,
    ...);

#endif /* __CP_STREAM_H */
