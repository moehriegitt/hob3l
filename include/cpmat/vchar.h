/* -*- Mode: C -*- */

#ifndef __CP_VCHAR_H
#define __CP_VCHAR_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <cpmat/def.h>
#include <cpmat/vchar.h>

typedef struct {
    size_t alloc;
    size_t size;
    /** Either NULL or a 0 terminated string */
    char *data;
} cp_vchar_t;

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

extern void cp_vchar_fini(
    cp_vchar_t *v);

/**
 * Clear to size 0, but keep allocated size. */
extern void cp_vchar_clear(
    cp_vchar_t *);

/**
 * Append raw string */
extern void cp_vchar_append_arr(
    cp_vchar_t *,
    char const *data,
    size_t size);

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

#endif /* __CP_VCHAR_H */
