/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_2SCAD_H_
#define CP_CSG2_2SCAD_H_

#include <hob3lbase/stream_tam.h>
#include <hob3l/csg2_tam.h>

/**
 * Print as scad file.
 *
 * Note that SCAD is actually 3D and OpenSCAD does not really
 * honor Z translations for 2D objects.  The F5 view is OK, but
 * the F6 view maps everything to z==0.
 *
 * This prefers the triangle info.  If not present, it uses the
 * polygon path info.
 *
 * This prints in 1:1 scale, i.e., if the finput is in MM,
 * the SCAD output is in MM, too.
 */
extern void cp_csg2_tree_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t);

#endif /* CP_CSG2_2SCAD_H_ */
