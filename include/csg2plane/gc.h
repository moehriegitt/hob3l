/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_GC_H
#define __CP_GC_H

#include <csg2plane/gc_tam.h>
#include <cpmat/stream_tam.h>

/**
 * Print a string of characters that respresent a modifier in scad syntax.
 */
extern void cp_gc_modifier_put_scad(
    cp_stream_t *s,
    unsigned modifier);

#endif /* __CP_GC_H */
