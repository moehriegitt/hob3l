/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/alloc.h>

static void *global_malloc(
    cp_alloc_t *m CP_UNUSED,
    size_t a, size_t b)
{
    if (b > (~(size_t)0 / a)) {
        return NULL;
    }
    return malloc(a * b);
}

static void *global_calloc(
    cp_alloc_t *m CP_UNUSED,
    size_t a, size_t b)
{
    return calloc(a, b);
}

static void global_free(
    cp_alloc_t *m CP_UNUSED,
    void *p)
{
    free(p);
}

static void *global_remalloc(
    cp_alloc_t *m CP_UNUSED,
    void *p,
    size_t ao, size_t an, size_t b)
{
    if (an <= ao) {
        return p;
    }
    if (b > (~(size_t)0 / an)) {
        return NULL;
    }
    size_t nsz = an * b;
    if (nsz == 0) {
        free(p);
        return NULL;
    }
    return realloc(p, nsz);
}

static void *global_recalloc(
    cp_alloc_t *m CP_UNUSED,
    void *p,
    size_t ao, size_t an, size_t b)
{
    if (an <= ao) {
        return p;
    }
    if (b > (~(size_t)0 / an)) {
        return NULL;
    }
    size_t nsz = an * b;
    if (nsz == 0) {
        free(p);
        return NULL;
    }
    void *q = realloc(p, nsz);
    if (q == NULL) {
        return q;
    }
    size_t osz = ao * b;
    assert(osz < nsz);
    memset((char*)q + osz, 0, nsz - osz);
    return q;
}

cp_alloc_t cp_alloc_global = {
    .x_malloc   = global_malloc,
    .x_calloc   = global_calloc,
    .x_remalloc = global_remalloc,
    .x_recalloc = global_recalloc,
    .x_free     = global_free,
};
