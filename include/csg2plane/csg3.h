/* -*- Mode: C -*- */

#ifndef __CP_CSG3_H
#define __CP_CSG3_H

#include <csg2plane/csg3_tam.h>
#include <csg2plane/scad_tam.h>
#include <cpmat/stream.h>

extern bool cp_csg3_from_scad(
    cp_csg3_tree_t *result,
    cp_err_t *t,
    cp_scad_t *scad);

extern bool cp_csg3_from_v_scad(
    cp_csg3_tree_t *result,
    cp_err_t *t,
    cp_v_scad_p_t *scad);

extern bool cp_csg3_from_scad_tree(
    cp_csg3_tree_t *result,
    cp_err_t *t,
    cp_scad_tree_t *scad);

/**
 * Get bounding box of all points, including those that are
 * in subtracted parts that will be outside of the final solid.
 *
 * bb will not be cleared, but only updated.
 */
extern void cp_csg3_tree_max_bb(
    cp_vec3_minmax_t *bb,
    cp_csg3_tree_t const *result);

/**
 * Dump in SCAD format */
extern void cp_csg3_tree_put_scad(
    cp_stream_t *s,
    cp_csg3_tree_t *result);

/* Dynamic casts */
CP_DECLARE_CAST(csg3, sphere, CP_CSG3_SPHERE)
CP_DECLARE_CAST(csg3, cyl,    CP_CSG3_CYL)
CP_DECLARE_CAST(csg3, poly,   CP_CSG3_POLY)
CP_DECLARE_CAST(csg3, add,    CP_CSG3_ADD)
CP_DECLARE_CAST(csg3, sub,    CP_CSG3_SUB)
CP_DECLARE_CAST(csg3, cut,    CP_CSG3_CUT)

#endif /* __CP_CSG3_H */
