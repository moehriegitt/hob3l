/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef HOB3LOP_GON_H_
#define HOB3LOP_GON_H_

#include <hob3lop/op-def.h>
#include <hob3lop/gon_tam.h>
#include <hob3lop/matq.h>

/**
 * Scaling factor for double<=>int coordinate.
 * Should be a power of two for exact conversion back and forth.
 *
 * Because double has more mantissa bits, it exact conversion also
 * works with non-powers of two, but STL binary format has 32-bit
 * float format, so this should be exact even with that format
 * on reasonable scales.
 *
 * This should not be optimised for exact 100th milimeter or 1000th
 * inch values, but for exact conversion to and from int.
 */
extern double cq_dim_scale;

/**
 * Convert v_vec2_t -> v_line2_t in place.
 *
 * The resulting vector will contain lines that consist
 * of the input vectors pairwise.  I.e., if input is [v1,v2,v3,v3],
 * output will be [[v1,v2], [v3,v4]].  This works in O(1) by just
 * reinterpreting the data array.  No copying is done.
 *
 * The number of elements in the vector must be even, otherwise
 * this assert fails.
 *
 * The old vector will be moved, i.e., the old structure will be
 * invalid.  But the reverse operation is available, too.
 */
static inline cq_v_line2_t *cq_v_vec2_move_v_line2(
    cq_v_vec2_t *v)
{
    assert((v->size  & 1) == 0);
    assert((v->alloc & 1) == 0);
    v->size  /= 2;
    v->alloc /= 2;
    return (cq_v_line2_t*)v;
}

/**
 * Convert v_line2_t -> v_vec2_t in place.  See the reverse function above:
 * this works the same, just in reverse.
 */
static inline cq_v_vec2_t *cq_v_line2_move_v_vec2(
    cq_v_line2_t *v)
{
    v->size  *= 2;
    v->alloc *= 2;
    return (cq_v_vec2_t*)v;
}

extern void cq_vec2_minmax(
    cq_vec2_minmax_t *r,
    cq_vec2_t const *v);

static inline void cq_line2_minmax(
    cq_vec2_minmax_t *r,
    cq_line2_t const *v)
{
    cq_vec2_minmax(r, &v->a);
    cq_vec2_minmax(r, &v->b);
}

extern void cq_v_vec2_minmax(
    cq_vec2_minmax_t *r,
    cq_v_vec2_t const *v);

extern void cq_v_line2_minmax(
    cq_vec2_minmax_t *r,
    cq_v_line2_t const *v);

/**
 * Export int to double coord */
static inline double cq_export_dim(cq_dim_t v)
{
    return ((double)v) / cq_dim_scale;
}

/**
 * Import double to int coord */
extern int cq_import_dim(double v);

/**
 * Export int to double vec2 */
static inline cp_vec2_t cq_export_vec2(cq_vec2_t const *v)
{
    return (cp_vec2_t){
        .x = cq_export_dim(v->x),
        .y = cq_export_dim(v->y)
    };
}

/**
 * Import double to int vec2 */
static inline cq_vec2_t cq_import_vec2(cp_vec2_t const *v)
{
    return (cq_vec2_t){
        .x = cq_import_dim(v->x),
        .y = cq_import_dim(v->y)
    };
}

#endif /* HOB3LOP_GON_H_ */
