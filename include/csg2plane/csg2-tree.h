/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG2_TREE_H
#define __CP_CSG2_TREE_H

#include <cpmat/stream_tam.h>
#include <cpmat/arith_tam.h>
#include <csg2plane/csg2_tam.h>
#include <csg2plane/csg3_tam.h>

/**
 * Manual initialisation for stack allocated objects.
 *
 * Note: This does not zero the object, this has to be done before (with the
 * right size of the corresponding struct type).
 */
extern void cp_csg2_init(
    cp_csg2_t *r,
    cp_csg2_type_t type,
    cp_loc_t loc);

/**
 * Initialise a cp_csg2_add_t object unless it is initialised
 * already.
 *
 * For this to work, the data must be zeroed first, then this
 * function can be used to initialise it, if it is not yet
 * initialised.
 */
extern void cp_csg2_add_init_perhaps(
    cp_csg2_add_t *r,
    cp_loc_t loc);

/**
 * Internal: allocate a new CSG2 object.
 */
extern cp_csg2_t *__cp_csg2_new(
    char const *file,
    int line,
    cp_csg2_type_t type,
    cp_loc_t loc);

/**
 * Initialises a CSG2 structure with a tree derived from a CSG3
 * structure, and reserves, for each simple object in the tree, an
 * array of layers of size layer_cnt.
 *
 * This assumes a freshly zeroed r to be initialised.
 */
extern void cp_csg2_tree_from_csg3(
    cp_csg2_tree_t *r,
    cp_csg3_tree_t const *d,
    cp_range_t const *s,
    cp_csg2_tree_opt_t const *o);

#endif /* __CP_CSG2_TREE_H */
