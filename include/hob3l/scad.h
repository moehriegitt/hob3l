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

#endif /* __CP_SCAD_H */
