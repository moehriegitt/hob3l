/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_VCHAR_H
#define __CP_VCHAR_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <hob3lbase/def.h>
#include <hob3lbase/vchar.h>

typedef struct {
    size_t alloc;
    size_t size;
    /** Either NULL or a 0 terminated string */
    char *data;
} cp_vchar_t;

/**
 * Finalise/discard a vector.
 */
extern void cp_vchar_fini(
    cp_vchar_t *v);

/**
 * Clear to size 0, but keep allocated size. */
extern void cp_vchar_clear(
    cp_vchar_t *v);

/**
 * Append raw string */
extern void cp_vchar_append_arr(
    cp_vchar_t *v,
    char const *data,
    size_t size);

/**
 * Swap contents of two vectors */
extern void cp_vchar_swap(
    cp_vchar_t *a,
    cp_vchar_t *b);

/**
 * Formatted printing into a string */
__attribute__((format(printf,2,0)))
extern int cp_vchar_vprintf(
    cp_vchar_t *v,
    char const *format,
    va_list va);

/**
 * Formatted printing into a string */
__attribute__((format(printf,2,3)))
extern int cp_vchar_printf(
    cp_vchar_t *v,
    char const *format,
    ...);

/**
 * Initialise a character vector.
 *
 * Zeroing is good initialisation.
 */
static inline void cp_vchar_init(
    cp_vchar_t *v)
{
    CP_ZERO(v);
}

/**
 * Append a character */
static inline void cp_vchar_push(
    cp_vchar_t *v,
    char data)
{
    cp_vchar_append_arr(v, &data, 1);
}

/**
 * Append another vector */
static inline void cp_vchar_append(
    cp_vchar_t *v,
    cp_vchar_t const *w)
{
    assert(w != NULL);
    cp_vchar_append_arr(v, w->data, w->size);
}

#endif /* __CP_VCHAR_H */
