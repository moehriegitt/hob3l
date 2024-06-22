/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

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
extern unsigned cp_utf8_decode(cp_utf8_iter_t *iter);

/**
 * Similar to cp_utf8_decode, but also decodes the OpenSCAD
 * string escapes, like \n, \u0020, etc.
 */
extern unsigned cp_utf8_escaped_decode(cp_utf8_iter_t *iter);

/**
 * Encode/write a UTF-8 sequence based on an Unicode codepoint.
 * This returns the length of the sequence or 0 in case it
 * does not fit into the given buffer.
 * This also returns 0 for encoding errors, i.e., when trying to
 * encode values >0x10ffff or trying to encode surrogates.
 */
extern size_t cp_utf8_encode(char *dst, size_t dst_size, unsigned cp);

/**
 * Returns 0 at end of string (must be 0 terminated).
 * This assumes there is no error.
 */
#define cp_utf8_take(s) \
    ({ \
        __typeof__(s) s_ = (s); \
        cp_utf8_iter_t iter_ = { .data = *s_, .size = ~(size_t)0, .error_msg = NULL }; \
        unsigned u_ = cp_utf8_decode(&iter_); \
        assert(iter_.error_msg == NULL); \
        *s_ += iter_.data - *s_; \
        (u_); \
    })



/**
 * This returns whether the codepoint can be UTF-8 encoded.
 *
 * 1 = it can.
 * 0 = it cannot, because it is 0, >0x10ffff or a surrogate.
 */
static inline bool cp_utf8_ok(unsigned cp)
{
    return !((cp == 0) || (cp > 0x10ffff) || ((cp >= 0xd800) && (cp <= 0xdfff)));
}


#endif /* CP_UTF8_H_ */
