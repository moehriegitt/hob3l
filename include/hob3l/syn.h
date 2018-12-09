/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * SCAD input file format, syntax layer.
 */

#ifndef CP_SYN_H_
#define CP_SYN_H_

#include <stdio.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/stream_tam.h>
#include <hob3l/obj.h>
#include <hob3l/syn_tam.h>
#include <hob3l/syn-2scad.h>

/** Create an instance */
#define cp_syn_new(r, l) cp_new_(cp_syn_typeof, r, l)

/** Cast w/ dynamic check */
#define cp_syn_cast(t, s) cp_cast_(cp_syn_typeof, t, s)

/** Cast w/ dynamic check */
#define cp_syn_try_cast(t, s) cp_try_cast_(cp_syn_typeof, t, s)

/**
 * Read a file into memory and store it in the input file table.
 *
 * include_loc is the location of the file name in a parent file.  This
 * is stored in f, and used to resolve relative file names.
 *
 * If file is NULL, this function tries to open the file in binary mode
 * (all parsers work with binary mode), otherwise, the file is taken as is.
 */
extern bool cp_syn_read(
    cp_syn_file_t *f,
    cp_err_t *err,
    cp_syn_input_t *input,
    cp_loc_t include_loc,
    char const *filename,
    FILE *file);

/**
 * Parse a file into a SCAD syntax tree.
 */
extern bool cp_syn_parse(
    cp_err_t *err,
    cp_syn_input_t *input,
    cp_syn_tree_t *r,
    cp_syn_file_t *file);

/**
 * Whether c is ASCII white space character.
 */
extern bool syn_is_space(char c);

/**
 * Whether c is an ASCII digit character.
 */
extern bool syn_is_digit(char c);

/**
 * Whether c is an ASCII alphabetic character.
 */
extern bool syn_is_alpha(char c);

#endif /* CP_SCAD_H_ */
