/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_LAYER_H_
#define CP_CSG2_LAYER_H_

#include <hob3lbase/stream_tam.h>
#include <hob3lbase/pool_tam.h>
#include <hob3l/csg2_tam.h>

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
extern void cp_csg2_tree_add_layer(
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi);

/**
 * Return the layer thickness of a given layer.
 */
extern cp_dim_t cp_csg2_layer_thickness(
    cp_csg2_tree_t *t,
    size_t zi CP_UNUSED);

#endif /* CP_CSG2_LAYER_H_ */
