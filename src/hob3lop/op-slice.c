/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lop/gon.h>
#include <hob3lop/hedron.h>
#include <hob3lop/op-slice.h>

static inline cq_dim_t cq_slice_interpol(
    cq_dim_t z,
    cq_dim_t z0,
    cq_dim_t z1,
    cq_dim_t a0,
    cq_dim_t a1)
{
    assert(z0 < z1);
    assert(z0 <= z);
    assert(z  <= z1);
    cq_dimw_t r0 = cq_dim_mul(a0, cq_dim_sub(z1, z));
    cq_dimw_t r1 = cq_dim_mul(a1, cq_dim_sub(z, z0));
    cq_dimw_t r = cq_divw_rnd(cq_dimw_add(r0, r1), cq_dim_sub(z1, z0));
    cq_dim_t rh = (cq_dim_t)r;
    assert(r == rh);
    return rh;
}

static inline cq_vec2_t cq_slice_cut_z(
    cq_dim_t z,
    cq_vec3_t const *a,
    cq_vec3_t const *b)
{
    return CQ_VEC2(
        cq_slice_interpol(z, a->z, b->z, a->x, b->x),
        cq_slice_interpol(z, a->z, b->z, a->y, b->y));
}

static inline void cq_slice_polyline_int(
    cq_v_vec2_t *out,
    cq_dim_t z,
    cq_vec3_t const *a,
    cq_vec3_t const *b)
{
    /* We cut epsilon above z so that the bottom is in, but the top is not.
     * This means that any lines within the cut plane are ignored.
     * I.e., each line produces one or no vertex for the output polygon. */
    if (a->z > b->z) { CP_SWAP(&a, &b); }

    /* if the polyline is fully below or touches the z from below, return */
    if (b->z <= z) {
        return;
    }

    /* if the polyline is fully above, return.  If it touches, stay. */
    if (a->z > z) {
        return;
    }

    /* cannot have a line within the plane: if az==bz, it would also be equal
     * to z, and that was excluded above. */
    assert(a->z != b->z);
    cp_v_push(out, cq_slice_cut_z(z, a, b));
}

static inline void cq_slice_polyline_float(
    cq_v_vec2_t *out,
    double z,
    cp_vec3_loc_ref_t const *af,
    cp_vec3_loc_ref_t const *bf)
{
    cq_vec3_t ai = {
        .x = cq_import_dim(af->ref->coord.x),
        .y = cq_import_dim(af->ref->coord.y),
        .z = cq_import_dim(af->ref->coord.z),
    };
    cq_vec3_t bi = {
        .x = cq_import_dim(bf->ref->coord.x),
        .y = cq_import_dim(bf->ref->coord.y),
        .z = cq_import_dim(bf->ref->coord.z),
    };
    cq_slice_polyline_int(out, cq_import_dim(z), &ai, &bi);
}

static int vec2_cmp(
    cq_vec2_t const *a,
    cq_vec2_t const *b,
    cq_vec2_t *orig)
{
    return CP_CMP(cq_vec2_sqr_dist(a, orig), cq_vec2_sqr_dist(b, orig));
}

extern void cq_slice_add_face(
    cq_slice_t *slice,
    cp_a_vec3_loc_ref_t const *face)
{
    if (face->size < 3) {
        return;
    }

    size_t i0 = slice->out->size;
    assert((i0 & 1) == 0);

    /* for each poly, crossing points of the z are computed
     * and stored directly in out.  Then the out array (the part
     * generated in this invocation) is sorted by x (and secondarily
     * by y).  This should appropriately define cut lines.
     * For degenerate polygons that produce an odd number of
     * lines, we cannot do much, except cut a vector out, which
     * is most certainly wrong.
     */
    for (cp_v_each(i, face)) {
        size_t j = i + 1;
        if (j == face->size) { j = 0; }
        cq_slice_polyline_float(slice->out, slice->z, &face->data[i], &face->data[j]);
    }

    size_t n = slice->out->size - i0;
    if (n & 1) { /* odd number? => the input must be degenerated */
        assert(0 && "input polyhedron face is degenerated (open) and produces single cut points");
        slice->out->size--;
        n--;
    }

    if (n == 0) {
        return;
    }

    cq_vec2_minmax_t bb = CQ_VEC2_MINMAX_INIT;
    for (cp_size_each(i, n)) {
        cq_vec2_minmax(&bb, &slice->out->data[i0 + i]);
    }

    cp_v_qsort(slice->out, i0, n, &vec2_cmp, &bb.min);
}

extern void cq_slice_fini(
    cq_slice_t *slice)
{
    cq_v_line2_t *r = cq_v_vec2_move_v_line2(slice->out);
    assert((void*)slice->out == (void*)r);

    /* remove zero length lines: */
    for (cp_v_eachp(i, r)) {
         if (cq_vec2_eq(&i->a, &i->b)) {
             *i = cp_v_pop(r);
             cp_redo(i);
         }
    }

    /* no other postprocessing */
}

extern void cq_slice_init(
    cq_slice_t *slice,
    cq_v_line2_t *r,
    double z)
{
    assert(r->size == 0);
    slice->out = cq_v_line2_move_v_vec2(r);
    slice->z = z;
}
