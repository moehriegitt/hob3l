/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_2PS_H_
#define CP_CSG2_2PS_H_

#include <hob3lbase/stream_tam.h>
#include <hob3l/csg2_tam.h>
#include <hob3l/ps_tam.h>

/**
 * Print as PS file.
 *
 * Each layer is printed on a separate page.
 *
 * This prints both the triangle and the path data in different
 * colours, overlayed so that the shape can be debugged.
 *
 * If xform is NULL, this assumes that the input is in MM.  Otherwise,
 * any other scaling transformation can be applied.
 */
extern void cp_csg2_tree_put_ps(
    cp_stream_t *s,
    cp_ps_opt_t const *opt,
    cp_csg2_tree_t *t);

#endif /* CP_CSG2_2PS_H_ */
