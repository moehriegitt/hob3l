/* -*- Mode: C -*- */

#ifndef HOB3LOP_OP_POLY_H_
#define HOB3LOP_OP_POLY_H_

#include <hob3lop/gon.h>
#include <hob3lop/hedron.h>
#include <hob3lop/op-sweep.h>

/**
 * Use the output of cq_sweep_intersect() or cq_sweep_reduce()
 * and construct a correct polygon (with edge order and all).
 *
 * The output paths are broken up so that no vertex is
 * duplicate in a path.  Each path is a simple polygon, but
 * not necessarily convex.
 *
 * The whole polygon may not be simple, i.e., this may produce 'inner'
 * paths (or 'holes') that need to be subtracted from an outer path to
 * describe the polygon.  The inner paths run counter-clockwise while
 * the outer paths run clock-wise.
 *
 * I.e., this does not break up polygons to produce coincident
 * edges to avoid inner paths.  Another algorithm could be provided
 * for this (e.g., the polygons in Kicad do not allow subtracting
 * overlayed paths, but do allow coincident edges in different
 * polygon paths).  ATM, the triangulation provides this.
 *
 * Typically, using this function means to do something like:
 *
 *    cq_sweep_t *s = cq_sweep_new(pool, 0);
 *    cq_sweep_add_...(...); // add edges
 *    cq_sweep_intersect(s); // find intersections
 *    cq_sweep_reduce(s, comb, comb_size); // to boolean operation(s)
 *    cp_csg2_poly_t *r = ...
 *    bool ok = cq_sweep_poly(s, r);
 *    cq_sweep_delete(s);
 *
 * `r` must be empty when this is invoked, as this does not
 * compare the `point` array for equal points.
 *
 * This returns true on success, or false if there were open
 * polygons or if a line crossing the given arrangement crosses
 * an odd number of segments (i.e., there is simple definition
 * of 'inside').  If this happens, the arrangement needs to be
 * repaired.
 *
 * The result is stored in `r`, in the `point` and `path`
 * members.
 *
 * Passing sweep==NULL is OK: the request will be ignored and
 * `true` is returned, i.e., this indicates an empty polygon.
 *
 * If the set of input lines is not a polygon, then an error
 * is printed to `err` and this returns false.
 *
 * On success, this returns true.
 */
CP_WUR
extern bool cq_sweep_poly(
    cp_err_t *err,
    cq_sweep_t *sweep,
    cq_csg2_poly_t *r);

#endif /* HOB3LOP_OP_POLY_H_ */
