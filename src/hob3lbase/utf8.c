/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <hob3lbase/utf8.h>

static inline uint8_t iter_get(cp_utf8_iter_t *iter)
{
    if (iter->size == 0) {
        return 0;
    }
    return (uint8_t)*iter->data;
}

static inline void iter_advance(cp_utf8_iter_t *iter)
{
    assert(*iter->data != 0);
    assert(iter->size > 0);
    iter->data++;
    iter->size--;
}

static inline void iter_error(
    cp_utf8_iter_t *iter, char const *back, char const *msg)
{
    if (back != NULL) {
        iter->data = back;
    }
    iter->error_msg = msg;
}

static inline unsigned digit_value(unsigned c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - (uint8_t)'0';
    }
    if ((c >= 'a') && (c <= 'z')) {
        return c - (uint8_t)'a' + 10;
    }
    if ((c >= 'A') && (c <= 'Z')) {
        return c - (uint8_t)'A' + 10;
    }
    return 99;
}

static inline bool get_cont(unsigned *result, char const *start, cp_utf8_iter_t *iter)
{
    unsigned c2 = iter_get(iter);
    if (!((c2 >= 0x80) && (c2 <= 0xbf))) {
        iter_error(iter, start, "illegal continuation byte in UTF-8 sequence");
        return false;
    }
    iter_advance(iter);
    *result = c2;
    return true;
}

static inline unsigned check_valid(
    char const *start,
    cp_utf8_iter_t *iter,
    unsigned minimum,
    unsigned c1,
    unsigned c2,
    unsigned c3,
    unsigned c4)
{
    unsigned r = (c1 << 18) | (c2 << 12) | (c3 << 6) | c4;
    if (r < minimum) {
        iter_error(iter, start, "overlong UTF-8 encoding");
        return 0;
    }
    if (r > 0x10ffff) {
        iter_error(iter, start, "out of range of Unicode in UTF-8 sequence");
        return 0;
    }
    if ((r >= 0xd800) && (r < 0xe000)) {
        iter_error(iter, start, "encoded surrogate in UTF-8 sequence");
        return 0;
    }
    return r;
}

/* ********************************************************************** */

/**
 * Decode a single UTF-8 character, stop at NUL or when
 * iter->size becomes 0, and stop at decoding errors and
 * set the iter->error pointer in that case.
 */
extern unsigned cp_utf8_decode(cp_utf8_iter_t *iter)
{
    char const *start = iter->data;

    uint8_t c1 = iter_get(iter);
    if (c1 == 0) {
        return 0;
    }
    iter_advance(iter);

    if (c1 < 0x80) {
        return c1;
    }
    if (c1 < 0xc2) {
        iter_error(iter, start, "illegal start byte in UTF-8 sequence");
        return 0;
    }

    unsigned c2;
    if (!get_cont(&c2, start, iter)) {
        return 0;
    }
    if (c1 < 0xe0) {
        return check_valid(start, iter, (1 << 7), 0, 0, c1 & 0x1f, c2);
    }

    unsigned c3;
    if (!get_cont(&c3, start, iter)) {
        return 0;
    }
    if (c1 < 0xf0) {
        return check_valid(start, iter, (1 << 11), 0, c1 & 0xf, c2, c3);
    }

    unsigned c4;
    if (!get_cont(&c4, start, iter)) {
        return 0;
    }
    if (c1 <= 0xf4) {
        return check_valid(start, iter, (1 << 16), c1 & 0x7, c2, c3, c4);
    }

    iter_error(iter, start, "illegal start byte in UTF-8 sequence");
    return 0;
}

/**
 * Similar to cp_utf8_decode, but also decodes the OpenSCAD
 * string escapes, like \n, \u0020, etc.
 */
extern unsigned cp_utf8_escaped_decode(cp_utf8_iter_t *iter)
{
    char const *start = iter->data;

    uint8_t c1 = iter_get(iter);
    if (c1 == 0) {
        return 0;
    }
    if (c1 >= 0x80) {
        return cp_utf8_decode(iter);
    }
    iter_advance(iter);

    if (c1 != '\\') {
        return c1;
    }

    uint8_t c2 = iter_get(iter);
    if (c2 == 0) {
        assert(0 && "Broken SCAD lexer: backslash at end of string");
        iter_error(iter, start, "backslash at end of string");
        return 0;
    }

    size_t len = 0;
    unsigned max = 0x10ffff;
    unsigned base = 16;
    unsigned code = 0;
    switch (c2) {
    default:
        iter_error(iter, start, "unrecognised escape character");
        return 0;

    case '\\':
    case '\'':
    case '\"': code = c2; break;

    case 't':  code = '\t'; break;
    case 'n':  code = '\n'; break;
    case 'r':  code = '\r'; break;

    case 'x':  len = 2; max = 0x7f; break;
    case 'u':  len = 4; break;
    case 'U':  len = 6; break;
    }
    iter_advance(iter);

    while (len > 0) {
        --len;
        uint8_t cx = iter_get(iter);
        if (cx == 0) {
            break;
        }
        unsigned val = digit_value(cx);
        if (val >= base) {
            break;
        }
        iter_advance(iter);
        code = (code * base) + val;
    }
    if (len > 0) {
        iter_error(iter, start, "premature end of hexadecimal escape");
        return 0;
    }
    if (code > max) {
        iter_error(iter, start, "character code is too large for this escape sequence");
        return 0;
    }
    return code;
}

/**
 * Encode/write a UTF-8 sequence based on an Unicode codepoint.
 * This returns the length of the sequence or 0 in case it
 * does not fit into the given buffer.
 * This also returns 0 for encoding errors, i.e., when trying to
 * encode values >0x10ffff or trying to encode surrogates.
 */
extern size_t cp_utf8_encode(char *dst, size_t dst_size, unsigned cp)
{
    assert(cp_utf8_ok(cp));
    if (cp <= 0x7f) {
        if (dst_size < 1) {
            return 0;
        }
        dst[0] = cp & 0x7f;
        return 1;
    }

    if (cp <= 0x7ff) {
        if (dst_size < 2) {
            return 0;
        }
        dst[0] = (char)(0xc0 | ((cp >> 6) & 0x1f));
        dst[1] = (char)(0x80 | ((cp >> 0) & 0x3f));
        return 2;
    }

    if (cp <= 0xffff) {
        if (dst_size < 3) {
            return 0;
        }
        dst[0] = (char)(0xe0 | ((cp >> 12) & 0x0f));
        dst[1] = (char)(0x80 | ((cp >> 6)  & 0x3f));
        dst[2] = (char)(0x80 | ((cp >> 0)  & 0x3f));
        return 3;
    }

    if (dst_size < 4) {
        return 0;
    }
    dst[0] = (char)(0xf0 | ((cp >> 18) & 0x07));
    dst[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    dst[2] = (char)(0x80 | ((cp >> 6)  & 0x3f));
    dst[3] = (char)(0x80 | ((cp >> 0)  & 0x3f));
    return 4;
}
