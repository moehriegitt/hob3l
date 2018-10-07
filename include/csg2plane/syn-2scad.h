/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_SYN_2SCAD_H
#define __CP_SYN_2SCAD_H

#include <cpmat/stream_tam.h>
#include <csg2plane/syn_tam.h>

/**
 * Dump in SCAD format */
extern void cp_syn_tree_put_scad(
    cp_stream_t *s,
    cp_syn_tree_t *result);

#endif /* __CP_SCAD_H */
