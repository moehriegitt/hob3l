/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * SCAD input file format, syntax layer.
 */

#ifndef __CP_SYN_H
#define __CP_SYN_H

#include <stdio.h>
#include <cpmat/vec.h>
#include <cpmat/stream_tam.h>
#include <csg2plane/syn_tam.h>
#include <csg2plane/syn-2scad.h>

/**
 * Parse a file into a SCAD syntax tree.
 */
extern bool cp_syn_parse(
    cp_syn_tree_t *r,
    char const *filename,
    FILE *file);

/**
 * Return a file location for a pointer to a token or any
 * other pointer into the file contents.
 *
 * This returns file and line number, but not posititon on the line,
 * because which position it is depends on tab width, so this is left
 * to the caller.
 *
 * To make it possible to count the position of the line, the original
 * contents of the line and an end-pointer of that line can be used:
 * loc->file->contents_orig can be indexed with loc-line for that.
 *
 * Note that lines are not NUL terminated, but the pointer at index
 * loc->line+1 (start of next line) defines the end of the line.
 *
 * For convenience, the cp_syn_loc_t already contains pointers to
 * orig line, orig line end, copied line (with parser inserted NULs),
 * copied line end.
 *
 * Returns whether the location was found.
 */
extern bool cp_syn_get_loc(
    cp_syn_loc_t *loc,
    cp_syn_tree_t *tree,
    char const *token);

#endif /* __CP_SCAD_H */
