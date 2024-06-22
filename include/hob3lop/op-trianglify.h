/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef HOB3LOP_OP_TRIANGLIFY_H_
#define HOB3LOP_OP_TRIANGLIFY_H_

#include <hob3lop/gon.h>
#include <hob3lop/hedron.h>
#include <hob3lop/op-sweep.h>

/**
 * Use the output of cq_sweep_intersect() or cq_sweep_reduce()
 * to find a triangulation.
 *
 * Typically, using this function means to do something like:
 *
 *    cq_sweep_t *s = cq_sweep_new(pool, 0);
 *    cq_sweep_add_...(...); // add edges
 *    cq_sweep_intersect(s); // find intersections
 *    cq_sweep_reduce(s, comb, comb_size); // to boolean operation(s)
 *    cp_csg2_poly_t *r = ...
 *    bool ok = cq_sweep_trianglify(s, r);
 *    cq_sweep_delete(s);
 *
 * `r` must be empty when this is invoked, as this does not
 * compare the `point` array for equal points.
 *
 * This is implemented using Hertel&Mehlhorn's algorithm, i.e.,
 * running another plane sweep.  It is assumed that no
 * intersections or overlap exists anymore.  But multi-ended
 * points cannot be avoided.
 *
 * The result is stored in `r`, in the `point` and `triangle`
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
extern bool cq_sweep_trianglify(
    cp_err_t *err,
    cq_sweep_t *sweep,
    cq_csg2_poly_t *r);

#endif /* HOB3LOP_OP_TRIANGLIFY_H_ */
