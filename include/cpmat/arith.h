/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */
/* -*- Mode: C -* */

#ifndef __CP_ARITH_H
#define __CP_ARITH_H

#include <cpmat/def.h>
#include <cpmat/arith_tam.h>

extern cp_f_t cp_equ_epsilon;
extern cp_f_t cp_sqr_epsilon;

/** min */
static inline cp_f_t __cp_min_f(cp_f_t a, cp_f_t b)
{
    return a <= b ? a : b;
}

static inline size_t __cp_min_z(size_t a, size_t b)
{
    return a <= b ? a : b;
}

#define __cp_min(a,b) \
    (_Generic(a, \
        size_t:  __cp_min_z, \
        int:     __cp_min_z, \
        default: __cp_min_f)(a,b))

#define cp_min(...) \
    CP_CALL(CP_CONCAT(__CP_FOLD_,CP_COUNT(__VA_ARGS__)),__cp_min, __VA_ARGS__)

#define cp_min_update(a,...) \
    do{ \
        __typeof__(a) *__ap = &(a); \
        *__ap = cp_min(*__ap, __VA_ARGS__); \
    }while(0)

/** max */
static inline cp_f_t __cp_max_f(cp_f_t a, cp_f_t b)
{
    return a >= b ? a : b;
}

static inline size_t __cp_max_z(size_t a, size_t b)
{
    return a >= b ? a : b;
}

#define __cp_max(a,b) \
    (_Generic(a, \
        size_t:  __cp_max_z, \
        int:     __cp_max_z, \
        default: __cp_max_f)(a,b))

#define cp_max(...) \
    CP_CALL(CP_CONCAT(__CP_FOLD_,CP_COUNT(__VA_ARGS__)),__cp_max, __VA_ARGS__)

#define cp_max_update(a,...) \
    do{ \
        __typeof__(a) *__ap = &(a); \
        *__ap = cp_max(*__ap, __VA_ARGS__); \
    }while(0)

/** gcd */
extern unsigned cp_gcd_a(unsigned data0, unsigned const *data, size_t size);

#define cp_gcd(...) \
    ({ \
        unsigned CP_GENSYM(__d)[] = { __VA_ARGS__ }; \
        cp_gcd_a(CP_GENSYM(__d)[0], CP_GENSYM(__d)+1, cp_countof(CP_GENSYM(__d))-1); \
    })

extern int cp_lex_cmp(cp_f_t const *a, cp_f_t const *b, size_t size);

/* ** static inline ** */

static inline bool cp_equ(cp_f_t a, cp_f_t b)
{
    return cp_abs(a - b) < cp_equ_epsilon;
}

/**
 * Same as cp_equ, but uses cp_sqr_epsilon, i.e., this should
 * be used of you are dealing with squared values.
 *
 * Note: all the other function that use cp_equ_epsilon currently
 * have no alternative that uses cp_sqr_epsilon instead.
 */
static inline bool cp_sqr_equ(cp_f_t a, cp_f_t b)
{
    return cp_abs(a - b) < cp_sqr_epsilon;
}

/**
 * Divide, avoiding division by zero by returning 0.  This is often a
 * sound error propagation with matrices, e.g., if a determinant gets
 * 0, the inverted matrix also has determinant 0.  If a vector has
 * length 0, then its unit is also length 0.  Because this happens to
 * be useful often, it is a service function here, and many utility
 * functions, instead of assert failing or raising div-by-0, they just
 * work by continuing with 0.
 */
static inline cp_f_t cp_div0(cp_f_t a, cp_f_t b)
{
    return cp_equ(b,0) ? 0 : a / b;
}

static inline bool cp_leq(cp_f_t a, cp_f_t b)
{
    return (a - b) < cp_equ_epsilon;
}

static inline bool cp_lt(cp_f_t a, cp_f_t b)
{
    return (a - b) < -cp_equ_epsilon;
}

static inline bool cp_geq(cp_f_t a, cp_f_t b)
{
    return cp_leq(b,a);
}

static inline bool cp_gt(cp_f_t a, cp_f_t b)
{
    return cp_lt(b,a);
}

static inline int cp_cmp(cp_f_t a, cp_f_t b)
{
    return cp_equ(a,b) ? 0 : a < b ? -1 : +1;
}

static inline cp_f_t cp_sin_deg(cp_f_t a)
{
    return sin(cp_deg(a));
}

static inline cp_f_t cp_cos_deg(cp_f_t a)
{
    return cos(cp_deg(a));
}

static inline bool cp_between(cp_f_t x, cp_f_t a, cp_f_t b)
{
    return (a < b) ? ((x >= a) && (x <= b)) : ((x >= b) && (x <= a));
}

static inline size_t cp_wrap_add1(size_t i, size_t n)
{
    i++;
    if (i == n) {
        return 0;
    }
    assert(i < n);
    return i;
}

static inline size_t cp_wrap_sub1(size_t i, size_t n)
{
    if (i == 0) {
        return n - 1;
    }
    assert(i < n);
    return i - 1;
}

static inline cp_f_t cp_sqr(cp_f_t a)
{
    return a * a;
}

/**
 * Linear interpolation between a and b for input t=0..1.
 *
 * At t == 0, a is used,
 * at t == 1, b is used.
 */
static inline cp_f_t cp_lerp(cp_f_t a, cp_f_t b, cp_f_t t)
{
    return a + ((b - a) * t);
}

/**
 * Linear interpolation between a and b for input t=-1..+1
 *
 * At t == -1, a is used,
 * at t == +1, b is used.
 *
 * If you have a choice, prefer the pair cp_t01/cp_lerp over
 * cp_t_pm/cp_lerp_pm, because the former needs less floating point
 * operations.
 */
static inline cp_f_t cp_lerp_pm(cp_f_t a, cp_f_t b, cp_f_t t)
{
    return cp_lerp(a, b, (t + 1) / 2);
}

/**
 * Get lerp index 0..1 of val between src and dst.
 *
 * With t = cp_t01(a,x,b) it will hold that x = cp_lerp(a,b,t).
 */
static inline cp_f_t cp_t01(cp_f_t src, cp_f_t val, cp_f_t dst)
{
    return cp_div0(val - src, dst - src);
}

/**
 * Get lerp index -1..1 of val between src and dst.
 *
 * With t = cp_t_pm(a,x,b) it will hold that x = cp_lerp_pm(a,b,t).
 *
 * If you have a choice, prefer the pair cp_t01/cp_lerp over
 * cp_t_pm/cp_lerp_pm, because the former needs less floating point
 * operations.
 */
static inline cp_f_t cp_t_pm(cp_f_t src, cp_f_t val, cp_f_t dst)
{
    return (cp_t01(src, val, dst) * 2) - 1;
}

/**
 * Swap contents of memory.
 */
extern void cp_memswap(void *a, void *b, size_t esz);

/**
 * Initialise a discrete range.
 */
static inline void cp_range_init(
    cp_range_t *r,
    double min,
    double max,
    double step)
{
    r->step = step;
    r->min = min;
    r->cnt = 0;
    long cnt_i = lrint(ceil(((max - min) / step) - cp_equ_epsilon));
    if (cnt_i > 0) {
        r->cnt = cnt_i & CP_MAX_OF(cnt_i);
    }
}

#endif /* __CP_ARITH_H */
