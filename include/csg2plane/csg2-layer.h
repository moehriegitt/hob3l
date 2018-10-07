/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG2_LAYER_H
#define __CP_CSG2_LAYER_H

#include <cpmat/stream_tam.h>
#include <cpmat/pool_tam.h>
#include <csg2plane/csg2_tam.h>

/**
 * If the stack has the given layer, return it.
 * Otherwise, return NULL.
 */
extern cp_csg2_layer_t *cp_csg2_stack_get_layer(
    cp_csg2_stack_t *c,
    size_t zi);

/**
 * This generates stacks of polygon CSG2 trees.
 *
 * The tree must be either empty, or the root of the tree must have
 * type CP_CSG2_ADD.  If the result is non-empty, this will set the
 * root to a node of type CP_CSG2_ADD.
 *
 * In the polygons, only the 'path' entries are filled in, i.e.,
 * the 'triangle' entries are left empty.
 *
 * Uses \p pool for all temporary allocations (but not for constructing r).
 */
extern bool cp_csg2_tree_add_layer(
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi);

/**
 * Compute bounding box
 *
 * Runtime: O(n), n=size of vector
 */
extern void cp_v_vec2_loc_minmax(
    cp_vec2_minmax_t *m,
    cp_v_vec2_loc_t *o);

/**
 * Append all paths from a into r, destroying a.
 *
 * This does no intersection test, but simply appends vectors
 * and adjusts indices.
 *
 * This moves both the paths and the triangulation.
 *
 * NOTE: If necessary for some future algorithm, this could be
 * rewritten to only clear a->path, but leave a->point and a->triangle
 * intact, because the latter vectors do not contain any allocated
 * substructures.  Currently, for consistency, the whole poly is
 * cleared.
 */
extern void cp_csg2_poly_merge(
    cp_csg2_poly_t *r,
    cp_csg2_poly_t *a);

/**
 * Return the layer thickness of a given layer.
 */
extern cp_dim_t cp_csg2_layer_thickness(
    cp_csg2_tree_t *t,
    size_t zi __unused);

#endif /* __CP_CSG2_LAYER_H */
