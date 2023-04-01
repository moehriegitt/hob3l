/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_BITMAP_H_
#define CP_CSG2_BITMAP_H_

#include <hob3lbase/stream_tam.h>
#include <hob3lbase/pool_tam.h>
#include <hob3lbase/bool-bitmap_tam.h>

/**
 * Expand a bitmap bitwise, i.e., duplicate each bit a number of times.
 *
 * The expansion is done in-place.
 */
extern void cp_bool_bitmap_spread(
    cp_bool_bitmap_t *c,
    /** The size of the current bitmap as log2() of the number of bits in use. */
    size_t have,
    /** By what factor to multiply each bit, as log2() of the factor. */
    size_t add);

/**
 * Duplicate r's bitmap so that r->size can be incremented.
 *
 * I.e., the first (1U << size) bits are copied into
 * the second block of that size in the r->comb array.
 *
 * The expansion is done in-place.
 */
extern void cp_bool_bitmap_repeat(
    cp_bool_bitmap_t *r,
    /** The size of the current bitmap as log2() of the number of bits in use. */
    size_t have,
    /** How many times to repeat the bitmap, as log2() of the factor. */
    size_t add);

/**
 * Combine two bitmaps bitwise according to the given operation.
 *
 * r := r op b
 */
extern void cp_bool_bitmap_combine(
    cp_bool_bitmap_t *r,
    cp_bool_bitmap_t const *b,
    /** number of bits given as log2() of the bitcount */
    size_t size,
    cp_bool_op_t op);

/**
 * Get a given bit from the bitmap
 */
static inline bool cp_bool_bitmap_get(
    cp_bool_bitmap_t const *b,
    size_t i)
{
    size_t k = i >> 3;
    size_t j = i & 0x7;
    assert(k < cp_countof(b->b));
    return !!(b->b[k] & (1U << j));
}

/**
 * Set/unset a given bit in the bitmap
 */
static inline bool cp_bool_bitmap_set(
    cp_bool_bitmap_t *b,
    size_t i,
    bool v)
{
    size_t k = i >> 3;
    size_t j = i & 0x7;
    assert(k < cp_countof(b->b));
    if (v) {
        b->b[k] |= (unsigned char)(1U << j);
    }
    else {
        b->b[k] &= (unsigned char)(~(1U << j));
    }
}

#endif /* CP_CSG2_BITMAP_H_ */
