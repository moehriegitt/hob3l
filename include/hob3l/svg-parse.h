/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_SVG_PARSE_H_
#define CP_SVG_PARSE_H_

#include <hob3lbase/base-def.h>
#include <hob3lbase/err_tam.h>
#include <hob3l/syn_tam.h>
#include <hob3l/csg2_tam.h>
#include <hob3l/csg3.h>

#define CP_SVG_NS cp_svg_ns_

extern char const cp_svg_ns_[];

/**
 * Parse an SVG file and pushes the objects into 'r'.
 *
 * This also gets an initial transformation matrix.  It is
 * compatible with functions from csg3.c, only reads the
 * stuff from SVG instead of from SCAD syntax.  The source
 * is an XML tree.
 *
 * This ignores all nodes that have a different namespace than
 * CP_SVG_NS (must be token-identical => use cp_xml_set_ns()).
 */
extern bool cp_svg_parse(
    cp_v_obj_p_t *r,
    cp_csg3_ctxt_t const *p,
    cp_csg3_local_t const *local,
    cp_detail_t const *detail,
    double dpi,
    bool center,
    cp_xml_t *xml);

#endif /* CP_SVG_PARSE_H_ */
