/* -*- Mode: C -*- */

/**
 * Stream abstraction
 *
 * To print to file or vchar.
 */

#ifndef __CP_STREAM_TAM_H
#define __CP_STREAM_TAM_H

#include <stdarg.h>

typedef int (*cp_stream_vprintf_t)(void *data, char const *form, va_list va);

typedef struct {
    void *data;
    cp_stream_vprintf_t vprintf;
} cp_stream_t;

#endif /* __CP_STREAM_TAM_H */
