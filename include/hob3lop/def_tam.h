/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef HOB3LOP_DEF_TAM_H_
#define HOB3LOP_DEF_TAM_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <hob3lbase/base-def.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/dict.h>
#include <hob3lbase/alloc.h>

#ifndef CQ_TRACE
#define CQ_TRACE 0
#endif

/**
 * This uses integer coordinates throughout to try to handle mathematical
 * instabilities.  32-bit are used so that for multiplication, 64-bit
 * operations are available.
 * This uses signed coordinates, although unsigned has less weirdness
 * in C concerning rounding and undefined behaviour.  But we often
 * need subtraction to work without checking the order of operands, so we
 * need negative values.
 */
typedef int      cq_dim_t;
typedef unsigned cq_udim_t;

#define CQ_DIM_MAX (0x7fffffff)
#define CQ_DIM_MIN (-CQ_DIM_MAX-1)


/**
 * For squares (or arbritrary products of two cq_dim_t), a type double the
 * size of cq_dim_t is needed.
 */
typedef long long cq_dimw_t;
typedef unsigned long long cq_udimw_t;

#define CQ_DIMW_MAX (0x7fffffffffffffff)
#define CQ_DIMW_MIN (-CQ_DIMW_MAX-1)

#define CQ_DIM_BITS  (sizeof(cq_dim_t)  * 8)
#define CQ_DIMW_BITS (sizeof(cq_dimw_t) * 8)

#define CQ_DIM_W     (((cq_dimw_t)1) << CQ_DIM_BITS)

#define CQ_REDUCE(F, A, ...) \
({ \
    __typeof__(A) r_ = (A); \
    __typeof__(A) s_[] = { __VA_ARGS__ }; \
    for (cp_arr_each(i, s_)) { \
        r_ = F(r_, s_[i]); \
    } \
    r_; \
})

#ifdef __SIZEOF_INT128__

#define CQ_HAVE_INTQ 1

typedef unsigned __int128 cq_udimq_raw_t;

/**
 * A quad width type on machines that have it natively */
typedef struct {
    unsigned __int128 x;
} cq_udimq_t;

#else

/**
 * A quad width type on machines that do not have it natively. */
typedef struct {
    cq_udimw_t l, h;
} cq_udimq_t;

#endif

static_assert(sizeof(cq_dim_t)   == 4,  "unexpected integer size");
static_assert(sizeof(cq_udim_t)  == 4,  "unexpected integer size");
static_assert(sizeof(cq_dimw_t)  == 8,  "unexpected integer size");
static_assert(sizeof(cq_udimw_t) == 8,  "unexpected integer size");
static_assert(sizeof(cq_udimq_t) == 16, "unexpected integer size");

/**
 * An assertion that, when compiling with -DNDEBUG, makes the
 * compiler still assume that the condition is true.
 *
 * This evaluates the parameter exactly once, in contrast to
 * an assert.
 */
#define cq_assert(...) \
    cq_assert_1_(CP_GENSYM(condA_), __VA_ARGS__, __builtin_unreachable())

#ifndef NDEBUG

#define cq_assert_1_(cond, cond_, cmd, ...) \
({ \
    bool cond = (cond_); \
    assert(cond && #cond_); \
    if (!(cond)) { \
        cmd; \
    } \
})

#define cq_each_assert_max(n) \
    cq_each_assert_max_1_(CP_GENSYM(i_), CP_GENSYM(n_), n)

#define cq_each_assert_max_1_(i_, n_, n) \
    size_t i_ = n; (assert((i_ > 0) && "maximum iteration count exceeded"), 1) ; i_--

#define cq_debug 1

#else /* NDEBUG */

#define cq_assert_1_(cond, cond_, ...) \
({ \
    if (!(cond_)) { __builtin_unreachable(); } \
})

#define cq_each_assert_max(n) ;;

#define cq_debug 0

#endif /* NDEBUG */

typedef struct {
    cq_dim_t div, mod;
} cq_divmod_t;

typedef struct {
    cq_dimw_t div, mod;
} cq_divmodw_t;

/**
 * Type for the object cq_sweep_t object (used by csg2-bool module).
 */
#define CQ_OBJ_TYPE_SWEEP (CP_OP_TYPE + 1)

#endif /* HOB3LOP_DEF_TAM_H_ */
