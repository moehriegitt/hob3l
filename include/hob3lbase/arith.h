/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_ARITH_H_
#define CP_ARITH_H_

#include <hob3lbase/base-def.h>
#include <hob3lbase/arith_tam.h>
#include <hob3lmat/mat.h>

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
