/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG2_BOOL_H
#define __CP_CSG2_BOOL_H

#include <hob3lbase/stream_tam.h>
#include <hob3lbase/pool_tam.h>
#include <hob3l/csg2_tam.h>

/**
 * Actually reduce a lazy poly to a single poly.
 *
 * The result is either empty (r->size == 0) or will have a single entry
 * (r->size == 1) stored in r->data[0].  If the result is empty, this
 * ensures that r->data[0] is NULL.
 *
 * Note that because lazy polygon structures have no dedicated space to store
 * a polygon, they must reuse the space of the input polygons, so applying
 * this function with more than 2 polygons in the lazy structure will reuse
 * space from the polygons for storing the result.
 */
extern void cp_csg2_op_reduce(
    cp_pool_t *pool,
    cp_csg2_lazy_t *r);

/**
 * Boolean operation on two lazy polygons.
 *
 * This does 'r = r op b'.
 *
 * Only the path information is used, not the triangles.
 *
 * \p r and/or \p b are reused and cleared to construct r.  This may happen
 * immediately or later in cp_csg2_op_reduce().
 *
 * Uses \p pool for all temporary allocations (but not for constructing r).
 *
 * This uses the algorithm of Martinez, Rueda, Feito (2009), based on a
 * Bentley-Ottmann plain sweep.  The algorithm is modified:
 *
 * (1) The original algorithm (both paper and sample implementation)
 *     does not focus on reassembling into polygons the sequence of edges
 *     the algorithm produces.  This library replaces the polygon
 *     reassembling by an O(n log n) algorithm.
 *
 * (2) The original algorithm's in/out determination strategy is not
 *     extensible to processing multiple polygons in one algorithm run.
 *     It was replaceds by a bitmask xor based algorithm.  This also lifts
 *     the restriction that no self-overlapping polygons may exist.
 *
 * (3) There is handling of corner cases in than what Martinez implemented.
 *     The float business is really tricky...
 *
 * (4) Intersection points are always computed from the original line slope
 *     and offset to avoid adding up rounding errors for edges with many
 *     intersections.
 *
 * (5) Float operations have all been mapped to epsilon aware versions.
 *     (The reference implementation failed on one of my tests because of
 *     using plain floating point '<' comparison.)
 *
 * Runtime: O(k log k),
 * Space: O(k)
 * Where
 *     k = n + m + s,
 *     n = number of edges in r,
 *     m = number of edges in b,
 *     s = number of intersection points.
 *
 * Note: the operation may not actually be performed, but may be delayed until
 * cp_csg2_apply.  The runtimes are given under the assumption that cp_csg2_apply
 * follows.  Best case runtime for delaying the operation is O(1).
 */
extern void cp_csg2_op_lazy(
    cp_csg_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_lazy_t *r,
    cp_csg2_lazy_t *b,
    cp_bool_op_t op);

/**
 * Add a layer to a tree by reducing it from another tree.
 *
 * The tree must have been initialised by cp_csg2_op_tree_init(),
 * and the layer ID must be in range.
 *
 * r is filled from a.  In the process, a is cleared/reused, if necessary.
 *
 * Runtime: O(j * k log k)
 * Space O(k)
 *    k = see cp_csg2_op_poly()
 *    j = number of polygons + number of bool operations in tree
 */
extern void cp_csg2_op_add_layer(
    cp_csg_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_csg2_tree_t *a,
    size_t zi);

/**
 * Reduce a set of 2D CSG items into a single polygon.
 *
 * This does not triangulate, but only create the path.
 *
 * The result is filled from root.  In the process, the elements in root are
 * cleared/reused, if necessary.
 *
 * If the result is empty. this either returns an empty
 * polygon, or NULL.  Which one is returned depends on what
 * causes the polygon to be empty.
 *
 * In case of an error, e.g. 3D objects that cannot be handled, this
 * assert-fails, so be sure to not pass anything this is unhandled.
 *
 * Runtime and space: see cp_csg2_op_add_layer.
 */
extern cp_csg2_poly_t *cp_csg2_flatten(
    cp_csg_opt_t const *opt,
    cp_pool_t *pool,
    cp_v_obj_p_t *root);

/**
 * Diff a layer with the next and store the result in diff_above/diff_below.
 *
 * The tree must have been processed with cp_csg2_op_add_layer(),
 * and the layer ID must be in range.
 *
 * r is modified and a diff_below polygon is added.  The original polygons
 * are left untouched.
 *
 * Runtime and space: see cp_csg2_op_add_layer.
 */
extern void cp_csg2_op_diff_layer(
    cp_csg_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *a,
    size_t zi);

/**
 * Initialise a tree for cp_csg2_op_add_layer() operations.
 *
 * The tree is initialised with a single stack of layers of the given
 * size taken from \p a.  Also, the z values are copied from \p a.
 *
 * Runtime: O(n)
 * Space O(n)
 *    n = number of layers
 */
extern void cp_csg2_op_tree_init(
    cp_csg2_tree_t *r,
    cp_csg2_tree_t const *a);

#endif /* __CP_CSG2_BOOL_H */
