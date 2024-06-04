/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_SVG_PARSE_H_
#define CP_SVG_PARSE_H_

#include <hob3lbase/base-def.h>
#include <hob3lbase/err_tam.h>
#include <hob3l/syn_tam.h>
#include <hob3l/csg2_tam.h>

/**
 * Parse an SVG file into a CSG2 polygon.
 */
extern bool cp_svg_parse(
    cp_pool_t *tmp,
    cp_err_t *err,
    cp_syn_input_t *input,
    cp_csg2_poly_t *r,
    cp_syn_xml_t *xml);

#endif /* CP_SVG_PARSE_H_ */
