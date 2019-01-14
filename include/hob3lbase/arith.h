/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_ARITH_H_
#define CP_ARITH_H_

#include <hob3lbase/def.h>
#include <hob3lbase/arith_tam.h>

#define CP_CIRCLE_ITER_INIT(n) \
    { \
        .cos = 1, \
        .sin = 0, \
        .idx = 0, \
        ._i = 0, \
        ._n = n, \
        ._a = 360 / cp_angle(n), \
    }

#define cp_circle_each(iter, n) \
    cp_circle_iter_t iter = CP_CIRCLE_ITER_INIT(n); \
    iter._i < iter._n; \
    cp_circle_iter_step(&iter)

/**
 * Epsilon for identifying point coordinates, i.e., granularity of coordinates
 * of points. */
extern cp_f_t cp_pt_epsilon;

/**
 * General epsilon for comparisons.
 *
 * Typically the square of cp_pt_epsilon.
 */
extern cp_f_t cp_eq_epsilon;

/**
 * Epsilon for comparison of squared values, or two coordinates multiplied,
 * or determinants.
 *
 * Typically the square of cp_eq_epsilon.
 */
extern cp_f_t cp_sqr_epsilon;

/**
 * Comparison using cp_eq_epsilon
 *
 * This should be the default way to compare cp_dim_t, cp_scale_t, and cp_f_t.
 */
extern int cp_lex_cmp(cp_f_t const *a, cp_f_t const *b, size_t size);

/**
 * Comparison using cp_pt_epsilon.
 *
 * This should be used to compare new point coordinates to old coordinates,
 * or to rasterize them.
 */
extern int cp_lex_pt_cmp(cp_f_t const *a, cp_f_t const *b, size_t size);

/**
 * Swap contents of memory.
 */
extern void cp_memswap(
    void *a,
    void *b,
    size_t esz);

/**
 * Return whether a piece of memory is zeroed */
extern bool cp_mem_is0(void *data, size_t size);

/**
 * This returns true if \p f is an integers, and
 * then returns that integer in \p i.
 */
extern bool cp_f_get_int(
    long long *ip,
    cp_f_t f);

/**
 * For angles that have exact rational results, this will return
 * exactly those results.  E.g. cp_sin_deg(180) == 0, not just close
 * to 0, but 0.
 */
extern cp_f_t cp_sin_deg(cp_f_t a);

/**
 * For angles that have exact rational results, this will return
 * exactly those results.  E.g. cp_cos_deg(180) == 1, not just close
 * to 1, but 1.
 */
extern cp_f_t cp_cos_deg(cp_f_t a);

/**
 * Take a step on the circle iterator
 */
extern void cp_circle_iter_step(
    cp_circle_iter_t *iter);

/**
 * Quadratic interpolation */
extern cp_f_t cp_interpol2(cp_f_t a, cp_f_t b, cp_f_t c, cp_f_t t);

/**
 * Cubic interpolation */
extern cp_f_t cp_interpol3(cp_f_t a, cp_f_t b, cp_f_t c, cp_f_t d, cp_f_t t);

/* min */
static inline cp_f_t cp_min_f_(cp_f_t a, cp_f_t b)
{
    return a <= b ? a : b;
}

static inline size_t cp_min_z_(size_t a, size_t b)
{
    return a <= b ? a : b;
}

static inline int cp_min_i(int a, int b)
{
    return a <= b ? a : b;
}

#define cp_min(a,b) \
    (_Generic(a, \
        size_t:  cp_min_z_, \
        int:     cp_min_z_, \
        default: cp_min_f_)(a,b))

#define cp_min_update(a,...) \
    do{ \
        __typeof__(a) *_ap = &(a); \
        *_ap = cp_min(*_ap, __VA_ARGS__); \
    }while(0)

/* max */
static inline cp_f_t cp_max_f_(cp_f_t a, cp_f_t b)
{
    return a >= b ? a : b;
}

static inline size_t cp_max_z_(size_t a, size_t b)
{
    return a >= b ? a : b;
}

static inline int cp_max_i(int a, int b)
{
    return a >= b ? a : b;
}

#define cp_max(a,b) \
    (_Generic(a, \
        size_t:  cp_max_z_, \
        int:     cp_max_z_, \
        default: cp_max_f_)(a,b))

#define cp_max_update(a,...) \
    do{ \
        __typeof__(a) *_ap = &(a); \
        *_ap = cp_max(*_ap, __VA_ARGS__); \
    }while(0)

/* comparisons using any epsilon */
static inline bool cp_e_eq (cp_f_t e, cp_f_t a, cp_f_t b) { return cp_abs(a - b) < e; }
static inline bool cp_e_le (cp_f_t e, cp_f_t a, cp_f_t b) { return (a - b) < e; }
static inline bool cp_e_lt (cp_f_t e, cp_f_t a, cp_f_t b) { return (a - b) < -e; }
static inline bool cp_e_ge (cp_f_t e, cp_f_t a, cp_f_t b) { return cp_e_le(e, b, a); }
static inline bool cp_e_gt (cp_f_t e, cp_f_t a, cp_f_t b) { return cp_e_lt(e, b, a); }
static inline int  cp_e_cmp(cp_f_t e, cp_f_t a, cp_f_t b)
{
    return cp_e_eq(e, a, b) ? 0 : a < b ? -1 : +1;
}

/* comparisons using cp_eq_epsilon */
static inline bool cp_eq (cp_f_t a, cp_f_t b) { return cp_e_eq (cp_eq_epsilon, a, b); }
static inline bool cp_le (cp_f_t a, cp_f_t b) { return cp_e_le (cp_eq_epsilon, a, b); }
static inline bool cp_lt (cp_f_t a, cp_f_t b) { return cp_e_lt (cp_eq_epsilon, a, b); }
static inline bool cp_ge (cp_f_t a, cp_f_t b) { return cp_e_ge (cp_eq_epsilon, a, b); }
static inline bool cp_gt (cp_f_t a, cp_f_t b) { return cp_e_gt (cp_eq_epsilon, a, b); }
static inline int  cp_cmp(cp_f_t a, cp_f_t b) { return cp_e_cmp(cp_eq_epsilon, a, b); }

/* comparisons using cp_pt_epsilon */
static inline bool cp_pt_eq (cp_f_t a, cp_f_t b) { return cp_e_eq (cp_pt_epsilon, a, b); }
static inline bool cp_pt_le (cp_f_t a, cp_f_t b) { return cp_e_le (cp_pt_epsilon, a, b); }
static inline bool cp_pt_lt (cp_f_t a, cp_f_t b) { return cp_e_lt (cp_pt_epsilon, a, b); }
static inline bool cp_pt_ge (cp_f_t a, cp_f_t b) { return cp_e_ge (cp_pt_epsilon, a, b); }
static inline bool cp_pt_gt (cp_f_t a, cp_f_t b) { return cp_e_gt (cp_pt_epsilon, a, b); }
static inline int  cp_pt_cmp(cp_f_t a, cp_f_t b) { return cp_e_cmp(cp_pt_epsilon, a, b); }

/* comparisons using cp_sqr_epsilon */
static inline bool cp_sqr_eq (cp_f_t a, cp_f_t b) { return cp_e_eq (cp_sqr_epsilon, a, b); }
static inline bool cp_sqr_le (cp_f_t a, cp_f_t b) { return cp_e_le (cp_sqr_epsilon, a, b); }
static inline bool cp_sqr_lt (cp_f_t a, cp_f_t b) { return cp_e_lt (cp_sqr_epsilon, a, b); }
static inline bool cp_sqr_ge (cp_f_t a, cp_f_t b) { return cp_e_ge (cp_sqr_epsilon, a, b); }
static inline bool cp_sqr_gt (cp_f_t a, cp_f_t b) { return cp_e_gt (cp_sqr_epsilon, a, b); }
static inline int  cp_sqr_cmp(cp_f_t a, cp_f_t b) { return cp_e_cmp(cp_sqr_epsilon, a, b); }

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

/** Subtract, but never be smaller than 0.
 *
 * = max(0, a-b)
 */
static inline cp_f_t cp_monus(cp_f_t a, cp_f_t b)
{
    return a > b ? a - b : 0;
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
    return cp_eq(b,0) ? 0 : a / b;
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
 * Initialise a discrete range.
 */
static inline void cp_range_init(
    cp_range_t *r,
    double min,
    double max,
    double step)
{
    r->step = step;
    if (cp_eq(min, max)) {
        r->min = min;
        r->cnt = 1;
    }
    else if (min > max) {
        r->min = 0;
        r->cnt = 0;
    }
    else {
        r->min = min;
        r->cnt = 0;
        long cnt_i = lrint(ceil(((max - min) / step) - cp_eq_epsilon));
        if (cnt_i > 0) {
            r->cnt = cnt_i & CP_MAX_OF(cnt_i);
        }
    }
}

#endif /* CP_ARITH_H_ */
