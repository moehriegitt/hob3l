/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_SYN_2SCAD_H_
#define CP_SYN_2SCAD_H_

#include <hob3lbase/stream_tam.h>
#include <hob3l/syn_tam.h>

/**
 * Dump in SCAD format */
extern void cp_syn_tree_put_scad(
    cp_stream_t *s,
    cp_syn_tree_t *result);

#endif /* CP_SCAD_H_ */
