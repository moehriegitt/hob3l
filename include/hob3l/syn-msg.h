/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_SYN_MSG_H_
#define CP_SYN_MSG_H_

#include <hob3lbase/err_tam.h>
#include <hob3l/syn_tam.h>

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
    cp_syn_input_t *tree,
    cp_loc_t token);

/**
 * Additional to cp_syn_get_loc, also get the source line citation
 *
 * This ensures that pre and post can be dereferenced as a C string.
 */
extern void cp_syn_format_loc(
    cp_vchar_t *pre,
    cp_vchar_t *post,
    cp_syn_input_t *tree,
    cp_loc_t token,
    cp_loc_t token2);

/**
 * Raise an error message with a location.
 *
 * This takes a parameter \p ign to decide whether this will become
 * an error, a warning, or whether the error will be ignored.  In
 * case of an error, this returns true, otherwise false.
 *
 * va_list version of cp_syn_msg().
 */
__attribute__((format(printf,6,0)))
extern bool cp_syn_vmsg(
    cp_syn_input_t *syn,
    cp_err_t *e,
    unsigned ign,
    cp_loc_t loc,
    cp_loc_t loc2,
    char const *msg, va_list va);

/**
 * Raise an error message with a location.
 *
 * This takes a parameter \p ign to decide whether this will become
 * an error, a warning, or whether the error will be ignored.  In
 * case of an error, this returns true, otherwise false.
 *
 * '...' version of cp_syn_vmsg().
 */
__attribute__((format(printf,6,7)))
extern bool cp_syn_msg(
    cp_syn_input_t *syn,
    cp_err_t *e,
    unsigned ign,
    cp_loc_t loc,
    cp_loc_t loc2,
    char const *msg, ...);

#endif /* CP_SYN_MSG_H_ */
