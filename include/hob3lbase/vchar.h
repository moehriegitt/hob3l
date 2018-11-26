/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_VCHAR_H_
#define CP_VCHAR_H_

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <hob3lbase/def.h>
#include <hob3lbase/vchar_tam.h>

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
CP_VPRINTF(2)
extern void cp_vchar_vprintf(
    cp_vchar_t *v,
    char const *format,
    va_list va);

/**
 * Formatted printing into a string */
CP_PRINTF(2,3)
extern void cp_vchar_printf(
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

/**
 * Ensure that data is not NULL and then return data pointer.
 */
static inline char *cp_vchar_cstr(
    cp_vchar_t *v)
{
    if (v->size == 0) {
        cp_vchar_push(v, 0);
        v->size = 0;
    }
    return v->data;
}

#endif /* CP_VCHAR_H_ */
