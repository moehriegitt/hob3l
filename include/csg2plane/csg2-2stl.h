/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG2_2STL_H
#define __CP_CSG2_2STL_H

#include <cpmat/stream_tam.h>
#include <csg2plane/csg2_tam.h>

/**
 * Print as STL file.
 *
 * This generates one 3D solid for each layer.
 *
 * This uses both the triangle and the polygon data for printing.  The
 * triangles are used for the xy plane (top and bottom) and the path
 * for connecting top and bottom at the edges of the slice.
 *
 * This prints in 1:10 scale, i.e., if the input in MM,
 * the STL output is in CM.
 */
extern void cp_csg2_tree_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t);

#endif /* __CP_CSG2_2STL_H */
