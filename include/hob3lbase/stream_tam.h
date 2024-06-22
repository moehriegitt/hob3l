/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * Stream abstraction
 *
 * To print to file or vchar.
 */

#ifndef CP_STREAM_TAM_H_
#define CP_STREAM_TAM_H_

#include <stddef.h>
#include <stdarg.h>

typedef void (*cp_stream_vprintf_t)(
    void *data,
    char const *form,
    va_list va);

typedef void (*cp_stream_write_t)(
    void *data,
    void const *buff,
    size_t size);

typedef struct {
    void *data;
    cp_stream_vprintf_t vprintf;
    cp_stream_write_t write;
} cp_stream_t;

#endif /* CP_STREAM_TAM_H_ */
