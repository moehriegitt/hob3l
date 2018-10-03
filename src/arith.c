/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <cpmat/arith.h>

cp_f_t cp_equ_epsilon = CP_EQU_EPSILON_DEFAULT;
cp_f_t cp_sqr_epsilon = CP_SQR_EPSILON_DEFAULT;

static unsigned cp_gcd_1(unsigned a, unsigned b)
{
    while (b > 0) {
        unsigned h = a % b;
        a = b;
        b = h;
    }
    return a;
}

extern unsigned cp_gcd_a(unsigned g, unsigned const *data, size_t size)
{
    for (cp_size_each(i, size)) {
        g = cp_gcd_1(g, data[i]);
    }
    return g;
}

extern int cp_lex_cmp(cp_f_t const *a, cp_f_t const *b, size_t size)
{
    for (cp_size_each(i, size)) {
        if (!cp_equ(a[i], b[i])) {
            return a[i] < b[i] ? -1 : +1;
        }
    }
    return 0;
}

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
