/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG2_TRIANGLE_H
#define __CP_CSG2_TRIANGLE_H

#include <hob3lbase/stream_tam.h>
#include <hob3lbase/pool_tam.h>
#include <hob3l/csg2_tam.h>

/**
 * Triangulate a set of polygons.
 *
 * Each polygon must be simple and there must be no intersecting edges
 * neither with the same polygon nor with any other polygon.
 * Polygons, however, may be fully contained with in other polygons,
 * i.e., they must not intersect, but may fully overlap.
 *
 * Polygons are defined by setting up an array of nodes \p node.  The
 * algorithm assumes that the structure was zeroed for initialisation
 * and then each node's \a in, \a out, and \p point slots need to be
 * initialised to represent the set of polygons.  The \p loc slot
 * is optional (meaning it may remain NULL), but highly recommended
 * for good error messages.
 *
 * Implicitly, edges need to be stored somewhere (they are pointed to
 * by each node).  Each edge is also assumed to having been zeroed for
 * initialisation.  The edges \p src and \p dst slots may be
 * initialised, but do not need to be, as they will be initialised by
 * the algorithm from each point's n->in and n->out information so
 * that n->in->dst = n->out->src = n.
 *
 * This uses the Hertel & Mehlhorn (1983) algorithm (non-optimised).
 *
 * The algorithm is extended in several ways:
 *
 * (1) It also handles subsequent collinear edges, i.e., three (and more)
 *     subsequent points in the polygon on the same line.  This
 *     situation will introduce more triangles than necessary, however,
 *     because each point will become a corner of a triangle.  This
 *     is implemented by applying a 2 dimensional lexicographical
 *     order to the points in the sweep line queue instead of the
 *     original x-only order.
 *
 * (2) It also handles coincident vertices in the same polygon.  This
 *     is what the CSG2 boolop algorithm outputs if the input is such
 *     that points coincide.  There is no way to fix this: it is just
 *     how the polygons are.  The boolop algorithm will never output
 *     vertices in the middle of an edge, so the triangulation does
 *     not need to handle that.  Also, I think no bends with coincident
 *     edges will ever be output, so this is untested (and probably
 *     will not work), only ends (proper and improper) and starts (proper
 *     and improper) are tested.
 *     This is implemented by again extending the sweep line point order
 *     by considering the type or corner (first ends, then starts).
 *     Additionally, the improper start case has a special case if vertices
 *     coincide.
 *
 * Uses \p pool for all temporary allocations (but not for constructing
 * point_arr or tri).
 *
 * Runtime: O(n log n)
 * Space: O(n)
 * Where n = number of points.
 */
extern bool cp_csg2_tri_set(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_vec2_arr_ref_t *point_arr,
    cp_v_size3_t *tri,
    cp_csg2_3node_t *node,
    size_t n);

/**
 * Triangulate a single path.
 *
 * This does not clear the list of triangles, but the new
 * triangles are appended to the polygon's triangle vector.
 *
 * This uses cp_csg2_tri_set() internally, so the path is contrained
 * in the way described for that function.
 *
 * Uses \p pool for all temporary allocations (but not for constructing g).
 *
 * Runtime: O(n log n)
 * Space: O(n)
 * Where n = number of points.
 */
extern bool cp_csg2_tri_path(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_poly_t *g,
    cp_csg2_path_t *s);

/**
 * Triangulate a single polygon.
 *
 * Note that a polygon may consist of multiple paths.
 *
 * This uses cp_csg2_tri_set() internally, it is invoked once with all
 * the paths in one data structure, so the set of paths of the given
 * polygon is contrained in the way described for that function.
 *
 * Uses \p pool for all temporary allocations (but not for constructing r).
 *
 * Runtime: O(n log n)
 * Space: O(n)
 * Where n = number of points.
 */
extern bool cp_csg2_tri_poly(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_poly_t *g);

/**
 * Triangulate a given layer
 *
 * This clears all 'triangle' vectors in all polygons of the layer and
 * refills them with a set of triangles derived from the 'path'
 * entries of the polygons.
 *
 * Note that this algorithm ignores the order of points on a path and
 * always produces clockwise triangles from any path.
 *
 * This uses cp_csg2_tri_set() internally for each polygon in the
 * tree, so the set of paths of each polygon in the tree is contrained
 * in the way described for that function.
 *
 * Uses \p pool for all temporary allocations (but not for constructing r).
 *
 * Runtime: O(m * n log n)
 * Space: O(max(n))
 * Where
 *     m = number of polygons (=cp_csg2_poly_t objects),
 *     n = number of points of a polygon,
 *     max(n) = the maximum n among the polygons.
 */
extern bool cp_csg2_tri_layer(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    size_t zi);

/**
 * Triangulate a given layer's diff_above polygons.
 *
 * This is just like cp_csg2_tri_layer, but works only on the
 * diff_above polygons.
 *
 * Runtime and space: see cp_csg2_tri_layer.
 */
extern bool cp_csg2_tri_layer_diff(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    size_t zi);

#endif /* __CP_CSG2_TRIANGLE_H */
