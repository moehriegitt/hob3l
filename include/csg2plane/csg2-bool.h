/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG2_BOOL_H
#define __CP_CSG2_BOOL_H

#include <cpmat/stream_tam.h>
#include <cpmat/pool_tam.h>
#include <csg2plane/csg2_tam.h>

/**
 * Boolean operation on two polygons.
 *
 * This uses the path information, not the triangles.
 *
 * 'r' will be overwritten and initialised (it may be passed uninitialised).
 *
 * \p a and/or \p b are reused and cleared to construct r.
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
 *     n = number of edges in a,
 *     m = number of edges in b,
 *     s = number of intersection points.
 */
extern void cp_csg2_op_poly(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_poly_t *r,
    cp_loc_t loc,
    cp_csg2_poly_t *a,
    cp_csg2_poly_t *b,
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
extern bool cp_csg2_op_add_layer(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
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
