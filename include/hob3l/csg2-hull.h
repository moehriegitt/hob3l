/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_HULL_H_
#define CP_CSG2_HULL_H_

#include <hob3l/csg2.h>

/**
 * Compute the convex hull of a set of points.
 *
 * \a point will be rearranged to contain the points of the hull in
 * clockwise order.
 */
extern void cp_csg2_hull(
    cp_v_vec2_loc_t *point);

#endif /* CP_CSG2_HULL_H_ */
