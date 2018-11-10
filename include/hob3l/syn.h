/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * SCAD input file format, syntax layer.
 */

#ifndef __CP_SYN_H
#define __CP_SYN_H

#include <stdio.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/stream_tam.h>
#include <hob3l/obj.h>
#include <hob3l/syn_tam.h>
#include <hob3l/syn-2scad.h>

/** Create an instance */
#define cp_syn_new(r, l) _cp_new(cp_syn_typeof, r, l)

/** Cast w/ dynamic check */
#define cp_syn_cast(t, s) _cp_cast(cp_syn_typeof, t, s)

/** Cast w/ dynamic check */
#define cp_syn_try_cast(t, s) _cp_try_cast(cp_syn_typeof, t, s)

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
    cp_loc_t token);

/**
 * Additional to cp_syn_get_loc, also get the source line citation
 */
extern void cp_syn_format_loc(
    cp_vchar_t *pre,
    cp_vchar_t *post,
    cp_syn_tree_t *tree,
    cp_loc_t token,
    cp_loc_t token2);

#endif /* __CP_SCAD_H */
