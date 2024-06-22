/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_UTF8_TAM_H_
#define CP_UTF8_TAM_H_

#include <stddef.h>

#define CP_UNICODE_BOM 0xfeff

#define CP_UTF8_BOM "\xEF\xBB\xBF"

/**
 * UTF-8 Decoder Iterator
 */
typedef struct {
    /**
     * Next input octet.
     * The string is terminated by NUL or by `size`.
     */
    char const *data;

    /**
     * Count number of characters left.  If this is ~(size_t)0, then there
     * is no limit.  The NUL character still terminates `data`.
     */
    size_t size;

    /**
     * Set by decoder to give a human readable representation of what went wrong
     * The error position is equal to 'data' if this is non-NULL.
     */
    char const *error_msg;
} cp_utf8_iter_t;

#endif /* CP_UTF8_TAM_H_ */
