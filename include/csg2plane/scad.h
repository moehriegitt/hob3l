/* -*- Mode: C -*- */

/**
 * SCAD actional commands that are supported.
 *
 * There are some differences wrt. OpenSCAD:
 *   - parameters not specified in OpenSCAD documentation as of 08/2018
 *     to be optional or have default values, are mandatory.  E.g.
 *     scale and translate need xyz and rotate needs an angle.
 *     There are defaults in OpenSCAD, but they are not always
 *     documented.
 */

#ifndef __CP_SCAD_H
#define __CP_SCAD_H

#include <csg2plane/scad_tam.h>
#include <csg2plane/syn_tam.h>
#include <cpmat/stream.h>

/**
 * Construct a SCAD CSG from the given function, recursively,
 * and append the CSG node(s) to \p result.
 */
extern bool cp_v_scad_from_syn_func(
    cp_scad_tree_t *result,
    cp_err_t *err,
    cp_syn_func_t *func);

/**
 * Same as cp_scad_from_syn_func, applied to each element
 * of the 'func' vector.
 *
 * On success, returns true.
 * In case of error, returns false and fills in tree->err_loc and tree->err_msg.
 */
extern bool cp_v_scad_from_v_syn_func(
    cp_scad_tree_t *result,
    cp_err_t *err,
    cp_v_syn_func_p_t *func);

/**
 * Dump in SCAD format.
 *
 * If present (triangle list size > 0), prefers the triangles over the path.
 */
extern void cp_scad_tree_put_scad(
    cp_stream_t *s,
    cp_scad_tree_t *result);

#endif /* __CP_SCAD_H */
