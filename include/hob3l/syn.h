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
#include <hob3l/syn_tam.h>
#include <hob3l/syn-2scad.h>

/** Create a SYN instance */
#define cp_syn_new(r, _loc) \
    ({ \
        __typeof__(r) * __r = CP_NEW(*__r); \
        __r->type = cp_syn_typeof(*__r); \
        __r->loc = (_loc); \
        __r; \
    })

/** Specialising cast w/ dynamic check */
#define cp_syn_cast(_t,s) \
    ({ \
        assert((s)->type == cp_syn_typeof((s)->_t)); \
        &(s)->_t; \
    })

/**  Generalising cast w/ static check */
#define cp_syn_value(t) \
    ({ \
        cp_static_assert((cp_syn_typeof(*(t)) & CP_TYPE_MASK) == CP_SYN_VALUE_TYPE); \
        (cp_syn_value_t*)(t); \
    })

/**  Generalising cast w/ static check */
#define cp_syn_stmt(t) \
    ({ \
        cp_static_assert((cp_syn_typeof(*(t)) & CP_TYPE_MASK) == CP_SYN_STMT_TYPE); \
        (cp_syn_stmt_t*)(t); \
    })

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
