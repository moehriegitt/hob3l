/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_UTF8_TAM_H_
#define CP_UTF8_TAM_H_

#include <stddef.h>

/**
 * UTF-8 Decoder Iterator
 */
typedef struct {
    /** next input octet */
    char const *data;
    /** count number of characters left */
    size_t size;
    /** set by decoder to point to error position if one is encountered */
    char const *error_pos;
    /** set by decoder to give a human readable representation of what went wrong */
    char const *error_msg;
} cp_utf8_iterator_t;

#endif /* CP_UTF8_TAM_H_ */
