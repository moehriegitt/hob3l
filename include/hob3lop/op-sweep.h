/* -*- Mode: C -*- */

/**
 * Sweep line algorithm.
 */

#ifndef HOB3LOP_OP_SWEEP_H_
#define HOB3LOP_OP_SWEEP_H_

#include <hob3lbase/pool.h>
#include <hob3lbase/bool-bitmap.h>
#include <hob3lop/gon.h>
#include <hob3lop/hedron.h>

/**
 * Map type to type ID.
 */
#define cq_op_typeof(type) \
    _Generic(type, \
        cp_obj_t:         CP_ABSTRACT, \
        cp_csg2_vline2_t: CQ_OBJ_TYPE_SWEEP)

#define cq_op_cast(t, s)     cp_cast_(cq_op_typeof, t, s)
#define cq_op_try_cast(t, s) cp_try_cast_(cq_op_typeof, t, s)

/**
 * Opaque data structure for running a plane sweep.
 */
typedef struct cq_sweep cq_sweep_t;

/**
 * Prepare a data structure for a plane sweep.
 * See cq_sweep_delete().
 */
extern cq_sweep_t *cq_sweep_new(
    /**
     * Pool for temporary objects */
    cp_pool_t *tmp,

    /**
     * Source location */
    cp_loc_t loc,

    /**
     * Rough number of edge segments of the polygon, if known.
     * If not known, just pass 0.
     * If roughly known, pass a number a little larger.
     * This is an optimisation value to minimise memory usage,
     * but it is not that important.
     * While over-estimation is generally a little better than
     * under estimation, do not pass excessive over-estimates
     * as that causes a waste of memory usage.
     */
    size_t edge_cnt);

/**
 * Add an edge to the plane sweep data structure.
 * "member" is the bitmask of polygon IDs that the edge
 * belongs to.  The algorithm maintains which polygons an
 * edges belongs to and also outputs a corresponding
 * bitmask.
 */
extern void cq_sweep_add_edge(
    cq_sweep_t *s,
    cq_vec2_t const *a,
    cq_vec2_t const *b,
    size_t member);

/**
 * Add a polygon to the plane sweep data structure.
 */
extern void cq_sweep_add_v_line2(
    cq_sweep_t *s,
    cq_v_line2_t const *gon,
    size_t member);

/**
 * Add a polygon to the plane sweep data structure.
 */
extern void cq_sweep_add_poly(
    cq_sweep_t *s,
    cq_csg2_poly_t const *gon,
    size_t member);

/**
 * Add the result of another sweep to this sweep.
 */
extern void cq_sweep_add_sweep(
    cq_sweep_t *s,
    cq_sweep_t *other,
    size_t member);

/**
 * Get the bounding box of the segments added by cq_sweep_add_*()
 * The values are incorporated into `bb`.
 */
extern void cq_sweep_minmax(
    cq_vec2_minmax_t *bb,
    cq_sweep_t *s);

/**
 * Run a plane sweep to find all intersections.
 *
 * This takes a polygon given by a set of edges, constructured using
 * cq_sweep_init() and cq_sweep_add_edge().
 *
 * This input polygon does not need to have any particular shape, may
 * have holes, be concave, and edges may coincide, overlap, and intersect.
 * The polygon must be closed, however.
 *
 * The API is such that it can be used also for faces of a polyhedron
 * so they can be converted into triangle only form, which is needed
 * for the slicing algorithm.  This library does not need that,
 * however, because the only thing polyhedra are used for is slicing,
 * and that does not need any prior intersection search (the slicer
 * should work with even the worst input set of face shapes, even
 * self-overlapping polygons).
 *
 * The output of this is a list of edges (line segments) stored
 * for the next algorithm to run.
 *
 * The output is unstructured, i.e., it is a set of polygon segments.
 * This is suitable as an input for several other algorithms that may
 * be run after this.
 */
extern void cq_sweep_intersect(
    cq_sweep_t *sweep);

/**
 * Use the output of cq_sweep_intersect() and filter it using a
 * boolean function.
 *
 * This is implemented by another quick sweep that assumes that
 * there are no intersections or overlaps anymore. But multi-ended
 * points cannot be avoided.
 *
 * The output is a single polygon.  The edges are still marked for
 * urpolygon membership.  Like cq_sweep_intersect(), the output is
 * unstructured.
 */
extern void cq_sweep_reduce(
    cq_sweep_t *sweep,
    cp_bool_bitmap_t const *comb,
    size_t comb_size);

/**
 * Append all the output edges into a polygon.
 */
extern void cq_sweep_get_v_line2(
    cq_v_line2_t *gon,
    cq_sweep_t *sweep);

/**
 * Whether the resulting polygon is empty.
 * This is valid only after the result is generated,
 * i.e., when `s` is ready for cq_sweep_get_v_line2() or
 * cq_sweep_poly() or cq_sweep_triangle().
 */
CP_WUR
extern bool cq_sweep_empty(
    cq_sweep_t *s);

/**
 * Sweep and deallocate
 */
extern void cq_sweep_delete(
    cq_sweep_t *sweep);

/**
 * Compute a new polygon with intersections resolved as additional
 * vertices.
 */
CP_WUR
extern cq_v_line2_t *cq_sweep_copy(
    cp_pool_t *tmp,
    cq_v_line2_t const *gon);

/**
 * Check whether a polygon is free of intersections (including junctions).
 * This is slow: O(n^2), but simple and thorough.
 *
 * Return 0 if there is no intersection; equal end points are not counted as
 * intersection, full overlap is also not counted as intersection.
 *
 * Returns 1 if there is a single intersection point, which will be returned
 * in *ip.
 *
 * Return -1 if the line partially overlap.  One intersection point is
 * returned in *ip.  It will be equal to one of the end points and
 * will be a strictly inner point on the other line.
 *
 * This uses exact arithmetics, i.e., the intersection checks are done
 * without tolerance.
 */
extern int cq_has_intersect(
    cq_vec2if_t *ip,
    cq_line2_t const **ap,
    cq_line2_t const **bp,
    cq_v_line2_t const *gon);

#endif /* HOB3LOP_OP_SWEEP_H_ */
