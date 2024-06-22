/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_BOOL_H_
#define CP_CSG2_BOOL_H_

#include <hob3lbase/stream_tam.h>
#include <hob3lbase/pool_tam.h>
#include <hob3l/csg2_tam.h>

/**
 * The output of this algorithm is used in several different ways:
 *   - for generating the output file (STL): for this, we need a
 *     full triangulation (in a cp_csg2_poly_t object).
 *
 *   - internally in this file to split the operation into multiple
 *     steps.  This needs a cp_csg2_vline2_t object stored within
 *     the input stack.
 *
 *   - in SCAD/CSG3 directly:
 *
 *       - for linear_extrude(): the algorithm cannot handle 'subtracting'
 *         polygons, i.e., this ignores the path winding order.
 *         I.e., this only works with simple polygons.
 *         Trivially, this works with triangulated polygons, but then
 *         produces suboptimal solutions for simple polygons.
 *
 *       - for rotate_extrude(): same as linear_extrude().
 *
 *       - for circle(), square(), ...: the basic shapes are all
 *         simple (and convex) polygons.
 *
 *       - for hull(): a convex hull is trivially a simple (and convex)
 *         polygon
 *
 *       - for polygon(): this can have any shape and may, therefore,
 *         not be simple.
 *
 *       - for text(): the resulting 2D shapes are often not simple
 *         polygons as letters have holes: 'l' is probably simple
 *         but 'o' is probably not.
 *
 * The *_extrude() steps all run this algorithm as their body is an implicit
 * group, so they can take care themselves to generate the necessary
 * triangulation/simplification of the polygons.  I.e., any 2D objects can
 * be stored in the way that SCAD does it, i.e., no need for 'text' or
 * 'polygon' to triangulate right away.  The *_extrude() functions can
 * optimise: a single polygon with a single path will be simple, so then
 * there's no need to flatten (cq_sweep_poly() generates simple polygons for
 * each path).
 */
typedef enum {
    /**
     * Produce a cp_csg2_vline2_t object for internal continuation of
     * the boolean algorithm.  This is used internally within the layer
     * stacks.
     *
     * => use internally in this module only (and the type is not exported)
     *
     * This uses 'cq_sweep_get_v_line2()' to produce the resulting
     * cp_csg2_vline2_t (the 'q' member of it).
     */
    CP_CSG2_BOOL_MODE_VLINE2,

    /**
     * Produce a cp_csg2_poly_t object for further handling within the
     * SCAD/CSG3 engine, e.g., for projection(cut=true) and other 2D
     * operations within the framework.  The 'path' is filled in, but
     * the 'triangle' is not.
     *
     * This is not suited for *_extrude().
     *
     * => can be used for any 2D flattening operations that do not
     * end in *_extrude().
     *
     * This uses 'cq_sweep_poly()' to produce the resulting
     * cp_csg2_poly_t.
     */
    CP_CSG2_BOOL_MODE_PATH,

    /**
     * Produce a cp_csg2_poly_t object from a layer stack for final
     * dump in STL (or other JS) format.  The 'triangle' is filled,
     * but the 'path' is empty (FIXME: do we need 'path'?).
     *
     * => use for final output generation (internally), and for
     * flattening for *_extrude().
     *
     * This uses 'cq_sweep_triangle()' to produce the resulting
     * cp_csg2_poly_t.
     *
     * This also produces a PATH, because it is needed that way
     * be the later stages (e.g., STL generates the vertical faces
     * from the path).
     */
    CP_CSG2_BOOL_MODE_TRI,
} cp_csg2_bool_mode_t;

/**
 * Add a layer to a tree by reducing it from another tree.
 *
 * This runs the algorithm in such a way that the new layer is suitable
 * for output in various formats (e.g., STL).  The resulting layer has
 * type cp_csg2_poly_t (while the input layer leaves are expected
 * to be in cp_csg2_vline2_t format from the cp_csg2_tree_add_layer() step).
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
extern bool cp_csg2_op_flatten_layer(
    cp_err_t *err,
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_csg2_tree_t *r,
    cp_csg2_tree_t *a,
    size_t zi);

/**
 * Reduce a set of 2D CSG items into a single polygon.
 *
 * This does not triangulate, but only create the path.  It is meant for
 * use within the SCAD/CSG3 framework (see csg3.c).
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
 * Runtime and space: see cp_csg2_op_flatten_layer.
 */
extern cp_csg2_poly_t *cp_csg2_flatten(
    cp_err_t *err,
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_v_obj_p_t *root,
    cp_csg2_bool_mode_t mode);

/**
 * Initialise a tree for cp_csg2_op_flatten_layer() operations.
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

#if 0
/* FIXME: currently disabled until the algorithm is fixed */
#endif


#endif /* CP_CSG2_BOOL_H_ */
