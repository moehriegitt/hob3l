/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3l/csg2.h>

/**
 * Free a poly with all substructures.
 *
 * Note that the embedded polys 'diff_below' and 'diff_above' are not
 * deleted by this function.
 */
extern void cp_csg2_poly_fini(
    cp_csg2_poly_t *p)
{
    for (cp_v_eachp(i, &p->path)) {
        cp_v_fini(i);
    }
    cp_v_fini(&p->point);
    cp_v_fini(&p->path);
    cp_v_fini(&p->tri);
}

/**
 * cp_v_vec2_loc_t nth function for cp_vec2_arr_ref_t
 */
extern cp_vec2_t *_cp_v_vec2_loc_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i)
{
    cp_v_vec2_loc_t const *v = u->user1;
    return &cp_v_nth(v, i).coord;
}

/**
 * cp_v_vec2_loc_t idx function for cp_vec2_arr_ref_t
 */
extern size_t cp_v_vec2_loc_idx_(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a)
{
    cp_v_vec2_loc_t const *v = u->user1;
    return cp_v_idx(v, CP_BOX_OF(a, cp_vec2_loc_t const, coord));
}

/**
 * cp_v_vec3_loc_t nth function for cp_vec2_arr_ref_t, XY plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_xy_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i)
{
    cp_v_vec3_loc_t const *v = u->user1;
    return &cp_v_nth(v, i).coord.b;
}

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, XY plane
 */
extern size_t cp_v_vec3_loc_xy_idx_(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a)
{
    cp_v_vec3_loc_t const *v = u->user1;
    return cp_v_idx(v, CP_BOX_OF(a, cp_vec3_loc_t const, coord.b));
}

/**
 * cp_v_vec3_loc_ref_t nth function for cp_vec2_arr_ref_t, XY plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_ref_xy_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i)
{
    cp_a_vec3_loc_ref_t const *v = u->user2;
    return &cp_v_nth(v, i).ref->coord.b;
}

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, XY plane
 */
extern size_t cp_v_vec3_loc_ref_xy_idx_(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a)
{
    cp_a_vec3_loc_t const *v = u->user1;
    return cp_v_idx(v, CP_BOX_OF(a, cp_vec3_loc_t const, coord.b));
}

/**
 * cp_v_vec3_loc_ref_t nth function for cp_vec2_arr_ref_t, YZ plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_ref_yz_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i)
{
    cp_a_vec3_loc_ref_t const *v = u->user2;
    return &cp_v_nth(v, i).ref->coord.be;
}

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, YZ plane
 */
extern size_t cp_v_vec3_loc_ref_yz_idx_(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a)
{
    cp_a_vec3_loc_t const *v = u->user1;
    return cp_v_idx(v, CP_BOX_OF(a, cp_vec3_loc_t const, coord.be));
}

/**
 * Compute bounding box
 *
 * Runtime: O(n), n=size of vector
 */
extern void cp_v_vec2_loc_minmax(
    cp_vec2_minmax_t *m,
    cp_v_vec2_loc_t *o)
{
    for (cp_v_each(i, o)) {
        cp_vec2_minmax(m, &cp_v_nth(o,i).coord);
    }
}
