/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG2_TREE_H
#define __CP_CSG2_TREE_H

#include <hob3lbase/stream_tam.h>
#include <hob3lbase/arith_tam.h>
#include <hob3l/csg2_tam.h>
#include <hob3l/csg3_tam.h>

/**
 * Initialise a cp_csg_add_t object unless it is initialised
 * already.
 *
 * For this to work, the data must be zeroed first, then this
 * function can be used to initialise it, if it is not yet
 * initialised.
 */
extern void cp_csg_add_init_perhaps(
    cp_csg_add_t **r,
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
    cp_csg_opt_t const *o);

#endif /* __CP_CSG2_TREE_H */
