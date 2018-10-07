/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG3_2SCAD_H
#define __CP_CSG3_2SCAD_H

#include <csg2plane/csg3_tam.h>
#include <csg2plane/scad_tam.h>
#include <cpmat/stream.h>

/**
 * Dump a CSG3 tree in SCAD format
 */
extern void cp_csg3_tree_put_scad(
    cp_stream_t *s,
    cp_csg3_tree_t *r);

#endif /* __CP_CSG3_2SCAD_H */
