/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_H_
#define CP_CSG2_H_

#include <hob3lmat/mat_tam.h>
#include <hob3lbase/obj.h>
#include <hob3lbase/alloc.h>
#include <hob3lop/gon.h>
#include <hob3l/csg2_tam.h>
#include <hob3l/csg3_tam.h>
#include <hob3l/csg2-bool.h>
#include <hob3l/csg2-layer.h>
#include <hob3l/csg2-tree.h>
#include <hob3l/csg2-2ps.h>
#include <hob3l/csg2-2scad.h>
#include <hob3l/csg2-2stl.h>
#include <hob3l/csg2-2js.h>

/** Create a CSG2 instance */
#define cp_csg2_new(r, l) cp_new_(cp_csg2_typeof, r, l)

/** Cast w/ dynamic check */
#define cp_csg2_cast(t, s) cp_cast_(cp_csg2_typeof, t, s)

/** Cast w/ dynamic check */
#define cp_csg2_try_cast(t, s) cp_try_cast_(cp_csg2_typeof, t, s)

/**
 * Free a poly with all substructures.
 *
 * Note that the embedded polys 'diff_below' and 'diff_above' are not
 * deleted by this function.
 */
extern void cp_csg2_poly_fini(
    cp_csg2_poly_t *p);

/**
 * Compute bounding box
 *
 * Runtime: O(n), n=size of vector
 */
extern void cp_v_vec2_loc_minmax(
    cp_vec2_minmax_t *m,
    cp_v_vec2_loc_t *o);

/**
 * Compute bounding box
 *
 * This uses only the points, neither the triangles nor the paths.
 *
 * Runtime: O(n), n=number of points
 */
static inline void cp_csg2_poly_minmax(
    cp_vec2_minmax_t *m,
    cp_csg2_poly_t *o)
{
    cp_v_vec2_loc_minmax(m, &o->point);
}

/**
 * cp_v_vec2_loc_t nth function for cp_vec2_arr_ref_t
 */
extern cp_vec2_t *_cp_v_vec2_loc_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i);

/**
 * cp_v_vec2_loc_t idx function for cp_vec2_arr_ref_t
 */
extern size_t cp_v_vec2_loc_idx_(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a);

/**
 * cp_v_vec3_loc_t nth function for cp_vec2_arr_ref_t, XY plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_xy_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i);

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, XY plane
 */
extern size_t cp_v_vec3_loc_xy_idx_(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a);

/**
 * cp_v_vec3_loc_ref_t nth function for cp_vec2_arr_ref_t, XY plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_ref_xy_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i);

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, XY plane
 */
extern size_t cp_v_vec3_loc_ref_xy_idx_(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a);

/**
 * cp_v_vec3_loc_ref_t nth function for cp_vec2_arr_ref_t, YZ plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_ref_yz_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i);

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, YZ plane
 */
extern size_t cp_v_vec3_loc_ref_yz_idx_(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a);

static inline cp_vec2_t *cp_vec2_arr_ref(
    cp_vec2_arr_ref_t const *a,
    size_t i)
{
    return a->nth(a, i);
}

static inline size_t cp_vec2_arr_idx(
    cp_vec2_arr_ref_t const *a,
    cp_vec2_t const *p)
{
    return a->idx(a, p);
}

/**
 * Get a point of a path in a poly.
 */
static inline cp_vec2_loc_t *cp_csg2_path_nth(
    cp_csg2_poly_t *poly,
    cp_csg2_path_t *path,
    size_t i)
{
    assert(i < path->point_idx.size);
    size_t j = path->point_idx.data[i];
    assert(j < poly->point.size);
    return &poly->point.data[j];
}

/**
 * Init values for a cp_csg2_poly_t */
#define CP_CSG2_POLY_INIT  ((cp_csg2_poly_t){ .type = CP_CSG2_POLY })

/**
 * Initialise a polygon
 */
static inline void cp_csg2_poly_delete(
    cp_csg2_poly_t *p)
{
    cp_csg2_poly_fini(p);
    CP_DELETE(p);
}

/**
 * Convert to vec2 array.
 */
static inline void cp_vec2_arr_ref_from_v_vec2_loc(
    cp_vec2_arr_ref_t *a,
    cp_v_vec2_loc_t const *v)
{
    a->nth = _cp_v_vec2_loc_nth;
    a->idx = cp_v_vec2_loc_idx_;
    a->user1 = v;
    a->user2 = NULL;
}

/**
 * Convert to vec2 array.
 */
static inline void cp_vec2_arr_ref_from_a_vec3_loc_xy(
    cp_vec2_arr_ref_t *a,
    cp_a_vec3_loc_t const *v)
{
    a->nth = _cp_v_vec3_loc_xy_nth;
    a->idx = cp_v_vec3_loc_xy_idx_;
    a->user1 = v;
    a->user2 = NULL;
}

/**
 * Convert to vec2 array.
 */
static inline void cp_vec2_arr_ref_from_a_vec3_loc_ref(
    cp_vec2_arr_ref_t *a,
    cp_a_vec3_loc_t const *v,
    cp_a_vec3_loc_ref_t const *w,
    bool yz_plane)
{
    a->nth = yz_plane ? _cp_v_vec3_loc_ref_yz_nth : _cp_v_vec3_loc_ref_xy_nth;
    a->idx = yz_plane ? cp_v_vec3_loc_ref_yz_idx_ : cp_v_vec3_loc_ref_xy_idx_;
    a->user1 = v;
    a->user2 = w;
}

#endif /* CP_CSG2_H_ */
