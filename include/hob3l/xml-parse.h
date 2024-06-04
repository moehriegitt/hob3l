/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_XML_PARSE_H_
#define CP_XML_PARSE_H_

#include <hob3lbase/base-def.h>
#include <hob3lbase/err_tam.h>
#include <hob3l/syn_tam.h>

/**
 * Parse an XML file into a CSG3 polyhedron.
 */
extern bool cp_xml_parse(
    cp_pool_t *tmp,
    cp_err_t *err,
    cp_syn_input_t *input,
    cp_syn_xml_t *r,
    cp_syn_file_t *file);

#endif /* CP_XML_PARSE_H_ */
