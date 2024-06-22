/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_SCAD_2SCAD_H_
#define CP_SCAD_2SCAD_H_

#include <hob3lbase/stream.h>
#include <hob3l/scad_tam.h>

/**
 * Dump in SCAD format.
 */
extern void cp_scad_tree_put_scad(
    cp_stream_t *s,
    cp_scad_tree_t const *r);

#endif /* CP_SCAD_2SCAD_H_ */
