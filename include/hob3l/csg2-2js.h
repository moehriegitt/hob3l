/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_2JS_H_
#define CP_CSG2_2JS_H_

#include <hob3lbase/stream_tam.h>
#include <hob3l/csg2_tam.h>

/**
 * Print as JavaScript file containing a WebGL scene configuration.
 *
 * This uses both the triangle and the polygon data for printing.  The
 * triangles are used for the xy plane (top and bottom) and the path
 * for connecting top and bottom at the edges of the slice.
 */
extern void cp_csg2_tree_put_js(
    cp_stream_t *s,
    cp_csg2_tree_t *t);

#endif /* CP_CSG2_2JS_H_ */
