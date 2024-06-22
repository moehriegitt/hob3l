/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG3_2SCAD_H_
#define CP_CSG3_2SCAD_H_

#include <hob3l/csg3_tam.h>
#include <hob3l/scad_tam.h>
#include <hob3lbase/stream.h>

/**
 * Dump a CSG3 tree in SCAD format
 */
extern void cp_csg3_tree_put_scad(
    cp_stream_t *s,
    cp_csg3_tree_t *r);

#endif /* CP_CSG3_2SCAD_H_ */
