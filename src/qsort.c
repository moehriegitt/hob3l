/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */
/* Compat layer for qsort_r under Mingw */

#include <stddef.h>
#include <stdlib.h>
#include <cpmat/qsort.h>

#ifndef cp_qsort_r

typedef struct {
    int (*compar)(const void *, const void *, void *);
    void *arg;
} qsort_ctxt_t;

static int qsort_ctxt_compar(
    void *_u,
    const void *a,
    const void *b)
{
    qsort_ctxt_t *u = _u;
    return u->compar(a, b, u->arg);
}

/**
 * Wrapper for qsort_r under Windows, where a similar
 * but incompatible function qsort_s exists.
 *
 * This is an emulation of qsort_r when we only have qsort_s.
 */
extern void cp_qsort_r(
    void *base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void *, const void *, void *),
    void *arg)
{
    qsort_ctxt_t user = { .compar = compar, .arg = arg };
    qsort_s(base, nmemb, size, qsort_ctxt_compar, &user);
}

#endif /* cp_qsort_r */
