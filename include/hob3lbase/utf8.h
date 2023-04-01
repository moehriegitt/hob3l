/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_UTF8_H_
#define CP_UTF8_H_

#include <hob3lbase/utf8_tam.h>

/**
 * Same as cp_utf8_decode, but with a cast to make it suitable
 * for cp_font_print.
 */
#define cp_utf8_next ((unsigned(*)(void*))cp_utf8_decode)

/**
 * Same as cp_utf8_escaped_decode, but with a cast to make it
 * suitable for cp_font_print.
 */
#define cp_utf8_escaped_next ((unsigned(*)(void*))cp_utf8_escaped_decode)

/**
 * Decode a single UTF-8 character, stop at NUL or when
 * iter->size becomes 0, and stop at decoding errors and
 * set the iter->error pointer in that case.
 */
extern unsigned cp_utf8_decode(cp_utf8_iterator_t *iter);

/**
 * Similar to cp_utf8_decode, but also decodes the OpenSCAD
 * string escapes, like \n, \u0020, etc.
 */
extern unsigned cp_utf8_escaped_decode(cp_utf8_iterator_t *iter);

#endif /* CP_UTF8_H_ */
