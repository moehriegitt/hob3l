/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * Stream abstraction
 *
 * To print to file or vchar.
 */

#ifndef __CP_STREAM_TAM_H
#define __CP_STREAM_TAM_H

#include <stdarg.h>

typedef void (*cp_stream_vprintf_t)(void *data, char const *form, va_list va);

typedef struct {
    void *data;
    cp_stream_vprintf_t vprintf;
} cp_stream_t;

#endif /* __CP_STREAM_TAM_H */
