/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */
/* -*- Mode: C -* */

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
