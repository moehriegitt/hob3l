/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_H_
#define CP_CSG2_H_

#include <hob3lbase/mat_tam.h>
#include <hob3lbase/obj.h>
#include <hob3lbase/alloc.h>
#include <hob3l/csg2_tam.h>
#include <hob3l/csg3_tam.h>
#include <hob3l/csg2-bool.h>
#include <hob3l/csg2-layer.h>
#include <hob3l/csg2-triangle.h>
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

#endif /* CP_CSG2_H_ */
