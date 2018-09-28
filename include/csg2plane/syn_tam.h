/* -*- Mode: C++ -*- */

/**
 * SCAD input file.
 */

#ifndef __CP_SYN_TAM_H
#define __CP_SYN_TAM_H

#include <stdio.h>
#include <cpmat/vec.h>
#include <cpmat/vchar.h>
#include <csg2plane/err_tam.h>
#include <csg2plane/gc_tam.h>
#include <csg2plane/syn_fwd.h>

typedef CP_VEC_T(cp_syn_func_t*)   cp_v_syn_func_p_t;
typedef CP_VEC_T(cp_syn_arg_t*)    cp_v_syn_arg_p_t;
typedef CP_VEC_T(cp_syn_value_t*)  cp_v_syn_value_p_t;

/**
 * SCAD parser node in syntax tree.
 *
 * This is uninterpreted, so there is only one
 * node type of the generic form:
 *
 * For groups that start with '{' instead of 'group(){', functor is
 * set to the static string "{", outside the file content string.  But
 * \a loc is set to the location of the '{' anyway.
 */
struct cp_syn_func {
    char const *functor;
    cp_loc_t loc;
    cp_v_syn_arg_p_t arg;
    cp_v_syn_func_p_t body;
    unsigned modifier;
};

/**
 * SCAD parser argument to function.
 */
struct cp_syn_arg {
    /**
     * name of the argument, or NULL if no name was given */
    char const *key;
    /**
     * value of the argument */
    cp_syn_value_t *value;
};

/**
 * SCAD parser value type */
typedef enum {
    CP_SYN_VALUE_ID = CP_SYN_VALUE_TYPE + 1,
    CP_SYN_VALUE_INT,
    CP_SYN_VALUE_FLOAT,
    CP_SYN_VALUE_RANGE,
    CP_SYN_VALUE_ARRAY,
} cp_syn_value_type_t;

#define CP_SYN_VALUE_BASE \
    cp_syn_value_type_t type; \
    cp_loc_t loc;

/**
 * SCAD parser identifier value
 */
typedef struct {
    CP_SYN_VALUE_BASE
    char const *value;
} cp_syn_value_id_t;

/**
 * SCAD parser int value
 */
typedef struct {
    CP_SYN_VALUE_BASE
    long long value;
} cp_syn_value_int_t;

/**
 * SCAD parser float value
 */
typedef struct {
    CP_SYN_VALUE_BASE
    cp_f_t value;
} cp_syn_value_float_t;

/**
 * SCAD parser range value
 */
typedef struct {
    CP_SYN_VALUE_BASE

    cp_syn_value_t *start;
    cp_syn_value_t *end;

    /**
     * If not given, remains NULL (i.e., this has no default in
     * the syntax tree, but only the semantics will add tha default
     * value of 1.) */
    cp_syn_value_t *inc;
} cp_syn_value_range_t;

/**
 * SCAD parser array value
 */
typedef struct {
    CP_SYN_VALUE_BASE
    cp_v_syn_value_p_t value;
} cp_syn_value_array_t;

/**
 * SCAD parser generic value
 */
union cp_syn_value {
    struct {
        CP_SYN_VALUE_BASE
    };
    cp_syn_value_id_t    _id;
    cp_syn_value_int_t   _int;
    cp_syn_value_float_t _float;
    cp_syn_value_range_t _range;
    cp_syn_value_array_t _array;
};

typedef CP_VEC_T(char const *) cp_v_cstr_t;

/**
 * SCAD parser file
 *
 * This is a description of the input file, including
 * all content.  It can be used to derive file/line number
 * from a token file pointer.  All the 'const char *'
 * stored in the syntax tree also function as source location
 * pointers.
 */
typedef struct {
    /**
     * Full file name as passed to fopen() to read the file.
     */
    cp_vchar_t filename;

    /**
     * File pointer
     */
    FILE *file;

    /**
     * The newly allocated array of file content.  Note that this is
     * destructively updated by the parser to insert NUL characters to
     * terminate strings.  The length of lines and file content
     * cannot, therefore, be based on NUL characters.  Instead, the
     * line vector contains pointers to lines, each delimiting its
     * previous line.
     *
     * The parser adds a terminating '\0' to the file contents after
     * reading the file, so upon successful file reading, this is 1
     * character longer than the file.  This is also the reason why
     * this string cannot reasonably be used to display the erroneous
     * line in an error message.  To do that, use content_orig.
     */
    cp_vchar_t content;

    /**
     * The original content of the file, without inserted NUL characters,
     * for display in error messages.
     */
    cp_vchar_t content_orig;

    /**
     * List of lines.  Each C pointer points into content.  The last
     * entry in this array points to the terminating '\0' that the
     * parser adds to 'content'.  This contains one (if the last line
     * contains \n) or two entries (if the last line does not contain
     * \n) more than the number of '\n' in the source file so that the
     * last line can be delimited (with the pointer to the additional
     * '\0' at the end of the file content).  E.g., the following two
     * examples indicate with '.' where the line pointers are placed.
     *
     * Without terminating '\n': two lines more than '\n' count in file:
     *
     *   .abc\n
     *   .a\n
     *   .c.
     *
     * With terminating '\n': one line more than '\n' count in file:
     *
     *   .abc\n
     *   .a\n
     *   .c\n
     *   .
     *
     * Note again that while most lines end in a \n character, the
     * last line of the file might lack the terminating \n.
     */
    cp_v_cstr_t line;

    /**
     * If it was included, this is the location of the
     * first inclusion command.
     */
    char const *include_loc;
} cp_syn_file_t;

typedef CP_VEC_T(cp_syn_file_t *) cp_v_syn_file_p_t;

/**
 * SCAD parser result
 */
typedef struct {
    /**
     * List of files that where read for this parse.
     * The entry at index 0 is the top-level file.
     */
    cp_v_syn_file_p_t file;

    /**
     * The top-level list of funcalls in the body
     * of the file(s).
     */
    cp_v_syn_func_p_t toplevel;

    /**
     * In case of an error: the error location and message.
     */
    cp_err_t err;
} cp_syn_tree_t;

/**
 * SCAD parser source location
 */
typedef struct {
    /**
     * Pointer to the file structure
     */
    cp_syn_file_t *file;

    /**
     * Index in file->line vector (= line number - 1, as the vector
     * starts counting at 0).
     */
    size_t line;

    /**
     * Location as passed into cp_syn_get_loc.
     */
    char const *loc;

    /**
     * Start of copied line, modified by parser.  This is where
     * the error position points into, so this can be used
     * to compute the position on the line by comparison
     * with \a loc.
     */
    char const *copy;

    /**
     * End of copied line (i.e., start of next line).
     */
    char const *copy_end;

    /**
     * Start of original line (can be used for printing and
     * calculation of position on line).
     */
    char const *orig;

    /**
     * End of original line (i.e., start of next line).
     */
    char const *orig_end;
} cp_syn_loc_t;

#endif /* __CP_SYN_TAM_H */
