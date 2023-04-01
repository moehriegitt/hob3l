/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <hob3lbase/arith.h>

cp_f_t cp_pt_epsilon  = CP_PT_EPSILON_DEFAULT;
cp_f_t cp_eq_epsilon  = CP_EQ_EPSILON_DEFAULT;
cp_f_t cp_sqr_epsilon = CP_SQR_EPSILON_DEFAULT;

/**
 * Comparison using cp_eq_epsilon
 *
 * This should be the default way to compare cp_dim_t, cp_scale_t, and cp_f_t.
 */
extern int cp_lex_cmp(cp_f_t const *a, cp_f_t const *b, size_t size)
{
    for (cp_size_each(i, size)) {
        if (!cp_eq(a[i], b[i])) {
            return a[i] < b[i] ? -1 : +1;
        }
    }
    return 0;
}

/**
 * Comparison using cp_pt_epsilon.
 *
 * This should be used to compare new point coordinates to old coordinates,
 * or to rasterize them.
 */
extern int cp_lex_pt_cmp(cp_f_t const *a, cp_f_t const *b, size_t size)
{
    for (cp_size_each(i, size)) {
        if (!cp_pt_eq(a[i], b[i])) {
            return a[i] < b[i] ? -1 : +1;
        }
    }
    return 0;
}

/**
 * Swap contents of memory.
 */
extern void cp_memswap(
    void *a,
    void *b,
    size_t esz)
{
    if (((esz | (size_t)a | (size_t)b) % sizeof(unsigned)) == 0) {
        for (size_t i = 0; i < esz / sizeof(unsigned); i++) {
            unsigned h = ((unsigned*)a)[i];
            ((unsigned*)a)[i] = ((unsigned*)b)[i];
            ((unsigned*)b)[i] = h;
        }
        return;
    }

    for (size_t i = 0; i < esz; i++) {
        char h = ((char*)a)[i];
        ((char*)a)[i] = ((char*)b)[i];
        ((char*)b)[i] = h;
    }
}

/**
 * Return whether a piece of memory is zeroed */
extern bool cp_mem_is0(void *data, size_t size)
{
    for (char const *i = data, *e = i + size; i != e; i++) {
        if (*i != 0) {
            return false;
        }
    }
    return true;
}

typedef struct {
    unsigned long long mant;
    int exp;
    bool neg;
    int width;
} cp_f_split_t;

/* Returns whether the number is real or not.
 *
 * width is set to the numbers of significant bits
 * extracted from the float.
 *
 * The original mantissa is always shifted so that its
 * highest represented bit in the float is at the highest
 * bit in the unsigned integer.
 *
 * The exponent is made integer so that 0 means no shift of
 * the matissa.  The mantissa's highest bit is assumed to have
 * value 1 (=2^0).  I.e., '1' is represented by the highest bit
 * in the mantissa plus an exponent of 0.
 */
static bool split_float(
    cp_f_split_t *s,
    double f)
{
    union {
        unsigned long long i;
        double f;
    } u = { .f = f };
    assert(sizeof(u.i) == sizeof(u.f));

    /* raw decode */
    int exp_raw = (u.i >> 52) & 0x7ff;
    s->exp  = exp_raw - 1023;
    s->mant = (u.i & 0x000fffffffffffffULL) << 12;
    s->neg  = (u.i >> 63);
    s->width = 52;

    /* non-numbers are not modified further */
    if (exp_raw == 0x7ff) {
        return false;
    }

    /* for normals, add implicit highest bit.  subnormals need no care. */
    if (exp_raw != 0) {
        s->mant = (s->mant >> 1) | 0x8000000000000000ULL;
        s->width++;
    }
    return true;
}

/**
 * This returns true if \p f is an integers, and
 * then returns that integer in \p i.
 */
extern bool cp_f_get_int(
    long long *ip,
    cp_f_t f)
{
    cp_f_split_t s;
    if (!split_float(&s, f)) {
        return false;
    }
    /* check we are not obviously below 1 */
    if (s.exp < 0) {
        return false;
    }
    /* check that the unit bit (bit0 of the integer) can be represented in the mantissa */
    if (s.exp >= s.width) {
        return false;
    }
    /* check that there are no zeros after decimal point */
    if ((s.mant << (s.exp + 1)) != 0) {
        return false;
    }

    /* we're int! */
    long long i = (long long)(s.mant >> ((8 * sizeof(s.mant)) - (s.exp & 0xffffff) - 1));
    if (s.neg) {
        i = -i;
    }
    *ip = i;
    return true;
}

static cp_f_t const *exact_sin(long long ai)
{
    ai = ai % 360;
    if (ai < 0) {
        ai += 360;
    }
    assert((ai >= 0) && (ai < 360));

    static const cp_f_t f_0 = 0;
    static const cp_f_t f_1 = 1;
    static const cp_f_t f_m1 = -1;
    static const cp_f_t f_05 = 0.5;
    static const cp_f_t f_m05 = -0.5;
    switch (ai & 0x7fffffff) {
    case 0:
    case 180: return &f_0;
    case 90:  return &f_1;
    case 270: return &f_m1;
    case 30:
    case 150: return &f_05;
    case 210:
    case 330: return &f_m05;
    }
    return NULL;
}

/**
 * For angles that have exact rational results, this will return
 * exactly those results.  E.g. cp_sin_deg(180) == 0, not just close
 * to 0, but 0.
 */
extern cp_f_t cp_sin_deg(cp_f_t a)
{
    long long ai;
    if (cp_f_get_int(&ai, a)) {
        cp_f_t const *r = exact_sin(ai);
        if (r != NULL) {
            return *r;
        }
    }
    return sin(cp_deg(a));
}

/**
 * For angles that have exact rational results, this will return
 * exactly those results.  E.g. cp_cos_deg(180) == 1, not just close
 * to 1, but 1.
 */
extern cp_f_t cp_cos_deg(cp_f_t a)
{
    long long ai;
    if (cp_f_get_int(&ai, a)) {
        cp_f_t const *r = exact_sin(ai + 90);
        if (r != NULL) {
            return *r;
        }
    }
    return cos(cp_deg(a));
}

/**
 * Take a step on the circle iterator
 */
extern void cp_circle_iter_step(
    cp_circle_iter_t *iter)
{
    size_t i = ++iter->_i;
    if (i & 1) {
        size_t i1 = i + 1;
        iter->idx = i1 / 2;
        cp_angle_t a = iter->_a * cp_angle(iter->idx);
        if (i1 == iter->_n) {
            iter->cos = -1;
            iter->sin = 0;
        }
        else {
            iter->cos = cp_cos_deg(a);
            iter->sin = cp_sin_deg(a);
        }
    }
    else {
        iter->sin = -iter->sin;
        iter->idx = iter->_n - iter->idx;
    }
}

/**
 * Quadratic interpolation */
extern cp_f_t cp_interpol2(cp_f_t a, cp_f_t b, cp_f_t c, cp_f_t t)
{
    double s  = 1-t;
    return (a*s*s) + (2*b*s*t) + (c*t*t);
}

/**
 * Cubic interpolation */
extern cp_f_t cp_interpol3(cp_f_t a, cp_f_t b, cp_f_t c, cp_f_t d, cp_f_t t)
{
    double s  = 1-t;
    double t2 = t*t;
    double s2 = s*s;
    return (a*s2*s) + (3*b*s2*t) + (3*c*s*t2) + (d*t*t2);
}
