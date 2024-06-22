/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_BASE_DEF_H_
#define CP_BASE_DEF_H_

#include <hob3ldef/def.h>

#define CP_IND 2

/** Square root of 2 */
#define CP_SQRT2 1.4142135623730950488

/* To make object IDs unique to catch bugs, we define an offset
 * for each object type enum here. */
#define CP_TYPE_MASK       0xff00
#define CP_TYPE2_MASK      0xf000

#define CP_SYN_VALUE_TYPE  0x1100

#define CP_SYN_STMT_TYPE   0x2100

#define CP_SCAD_TYPE       0x3000
#define CP_SCAD_REC_TYPE   0x3100

#define CP_CSG_TYPE        0x4000

#define CP_CSG2_TYPE       0x4100
#define CP_OP_TYPE         0x4180 /* cq_sweep_t is handled like a CSG2 structure */

#define CP_CSG3_TYPE       0x4200

/** Type ID that is never given to any object */
#define CP_ABSTRACT       0xffff

/**
 * Boolean operation
 *
 * Used for the low-level algorithm.
 */
typedef enum {
    CP_OP_CUT = 0,
    CP_OP_XOR = 1,
    CP_OP_SUB = 2,
    CP_OP_ADD = 3,
} cp_bool_op_t;

static inline size_t cp_size_align(size_t x)
{
    return x & -x;
}

static inline int strcmp0(char const *a, char const *b)
{
    return (a == b) ? 0 : (a == NULL) ? -1 : (b == NULL) ? +1 : strcmp(a,b);
}

static inline bool strequ(char const *a, char const *b)
{
    return strcmp(a, b) == 0;
}

static inline bool strequ0(char const *a, char const *b)
{
    return strcmp0(a, b) == 0;
}

static inline bool strnequ(char const *a, char const *b, size_t n)
{
    return strncmp(a, b, n) == 0;
}

static inline size_t strpref(char const *haystack, char const *needle)
{
    size_t n = strlen(needle);
    return strnequ(haystack, needle, strlen(needle)) ? n : 0;
}

/**
 * Returns NULL if needle is not a prefix of haystack.
 * Returns the pointer to haystack+strlen(needle) otherwise.
 */
static inline char const *is_prefix(char const *haystack, char const *needle)
{
    size_t len = strlen(needle);
    if (strnequ(haystack, needle, len)) {
        return haystack + len;
    }
    return NULL;
}

static inline size_t cp_align_down(size_t n, size_t a)
{
    assert((a != 0) && "Alignment is zero");
    assert(((a & (a-1)) == 0) && "Alignment is not a power of 2");
    return n & -a;
}

static inline size_t cp_align_down_diff(size_t n, size_t a)
{
    return n - cp_align_down(n,a);
}

static inline size_t cp_align_up(size_t n, size_t a)
{
    return cp_align_down(n + a - 1, a);
}

static inline size_t cp_align_up_diff(size_t n, size_t a)
{
    return cp_align_up(n,a) - n;
}

/**
 * Array index with bound check: if i<n, then i, otherwise 0.
 */
static inline size_t cp_idx0(size_t n, size_t i)
{
    return i & -(size_t)(i < n);
}

/**
 * cp_idx0() applied to index a given array */
#define CP_IDX0(arr, i) ((arr)[cp_idx0(cp_countof(arr), i)])

#endif /* CP_MAT_H_ */
