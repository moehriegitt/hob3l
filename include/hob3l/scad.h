/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * SCAD format defined by OpenSCAD.
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

#include <hob3lbase/stream.h>
#include <hob3l/scad_tam.h>
#include <hob3l/syn_tam.h>
#include <hob3l/scad-2scad.h>

/**
 * Same as cp_scad_from_syn_stmt_item, applied to each element
 * of the 'func' vector.
 *
 * On success, returns true.
 * In case of error, returns false and fills in tree->err_loc and tree->err_msg.
 */
extern bool cp_scad_from_syn_tree(
    cp_scad_tree_t *result,
    cp_syn_tree_t *syn);

CP_DECLARE_CAST_(scad, union,        CP_SCAD_UNION)
CP_DECLARE_CAST_(scad, difference,   CP_SCAD_DIFFERENCE)
CP_DECLARE_CAST_(scad, intersection, CP_SCAD_INTERSECTION)
CP_DECLARE_CAST_(scad, sphere,       CP_SCAD_SPHERE)
CP_DECLARE_CAST_(scad, cube,         CP_SCAD_CUBE)
CP_DECLARE_CAST_(scad, cylinder,     CP_SCAD_CYLINDER)
CP_DECLARE_CAST_(scad, polyhedron,   CP_SCAD_POLYHEDRON)
CP_DECLARE_CAST_(scad, multmatrix,   CP_SCAD_MULTMATRIX)
CP_DECLARE_CAST_(scad, translate,    CP_SCAD_TRANSLATE)
CP_DECLARE_CAST_(scad, mirror,       CP_SCAD_MIRROR)
CP_DECLARE_CAST_(scad, scale,        CP_SCAD_SCALE)
CP_DECLARE_CAST_(scad, rotate,       CP_SCAD_ROTATE)
CP_DECLARE_CAST_(scad, circle,       CP_SCAD_CIRCLE)
CP_DECLARE_CAST_(scad, square,       CP_SCAD_SQUARE)
CP_DECLARE_CAST_(scad, polygon,      CP_SCAD_POLYGON)
CP_DECLARE_CAST_(scad, color,        CP_SCAD_COLOR)

#endif /* __CP_SCAD_H */
