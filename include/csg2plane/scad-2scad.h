/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_SCAD_2SCAD_H
#define __CP_SCAD_2SCAD_H

#include <cpmat/stream.h>
#include <csg2plane/scad_tam.h>

/**
 * Dump in SCAD format.
 */
extern void cp_scad_tree_put_scad(
    cp_stream_t *s,
    cp_scad_tree_t *r);

#endif /* __CP_SCAD_2SCAD_H */
