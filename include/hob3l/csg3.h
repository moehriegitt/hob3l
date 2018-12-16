/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG3_H_
#define CP_CSG3_H_

#include <hob3lbase/stream.h>
#include <hob3lbase/obj.h>
#include <hob3l/csg3_tam.h>
#include <hob3l/scad_tam.h>
#include <hob3l/syn_tam.h>
#include <hob3l/csg3-2scad.h>

/** Create a CSG3 instance */
#define cp_csg3_new(r, l) cp_new_(cp_csg3_typeof, r, l)

/** Create a CSG3 object instance */
#define cp_csg3_new_obj(r, _loc, _gc) \
    ({ \
        __typeof__(r) * _rA = cp_csg3_new(r, _loc); \
        _rA->gc = (_gc); \
        _rA; \
    })

/** Cast w/ dynamic check */
#define cp_csg3_cast(t,s) cp_cast_(cp_csg3_typeof, t, s)

/** Cast w/ dynamic check */
#define cp_csg3_try_cast(t,s) cp_try_cast_(cp_csg3_typeof, t, s)

/**
 * Get bounding box of all points, including those that are
 * in subtracted parts that will be outside of the final solid.
 *
 * If max is non-false, the bb will include structures that are
 * subtracted.
 *
 * bb will not be cleared, but only updated.
 *
 * Returns whether there the structure is non-empty, i.e.,
 * whether bb has been updated.
 */
extern void cp_csg3_tree_bb(
    cp_vec3_minmax_t *bb,
    cp_csg3_tree_t const *r,
    bool max);

/**
 * Convert a SCAD AST into a CSG3 tree.
 */
extern bool cp_csg3_from_scad_tree(
    cp_pool_t *tmp,
    cp_syn_input_t *syn,
    cp_csg3_tree_t *r,
    cp_err_t *t,
    cp_scad_tree_t const *scad);

#endif /* CP_CSG3_H_ */
