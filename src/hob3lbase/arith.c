/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <hob3lbase/arith.h>

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
