/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG3_H
#define __CP_CSG3_H

#include <hob3lbase/stream.h>
#include <hob3l/csg3_tam.h>
#include <hob3l/scad_tam.h>
#include <hob3l/csg3-2scad.h>

/**
 * Get bounding box of all points, including those that are
 * in subtracted parts that will be outside of the final solid.
 *
 * bb will not be cleared, but only updated.
 */
extern void cp_csg3_tree_max_bb(
    cp_vec3_minmax_t *bb,
    cp_csg3_tree_t const *r);

/**
 * Convert a SCAD AST into a CSG3 tree.
 */
extern bool cp_csg3_from_scad_tree(
    cp_csg3_tree_t *r,
    cp_err_t *t,
    cp_scad_tree_t *scad);

/* Dynamic casts */
CP_DECLARE_CAST_(csg3, sphere, CP_CSG3_SPHERE)
CP_DECLARE_CAST_(csg3, cyl,    CP_CSG3_CYL)
CP_DECLARE_CAST_(csg3, poly,   CP_CSG3_POLY)
CP_DECLARE_CAST_(csg3, add,    CP_CSG3_ADD)
CP_DECLARE_CAST_(csg3, sub,    CP_CSG3_SUB)
CP_DECLARE_CAST_(csg3, cut,    CP_CSG3_CUT)
CP_DECLARE_CAST_(csg3, 2d,     CP_CSG3_2D)

#endif /* __CP_CSG3_H */
