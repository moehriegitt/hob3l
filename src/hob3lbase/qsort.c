/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */
/* Iterative qsort_r to avoid stack overflow on deeply nested/ordered input */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <hob3lbase/qsort.h>

/* Stack depth is at most 2*ceil(log2(n)), so 128 handles arrays up to 2^64 */
#define QSORT_STACK_MAX 128

static void iter_qsort_r(
    char *base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void *, const void *, void *),
    void *arg)
{
    size_t stack[QSORT_STACK_MAX];
    size_t top = 0;
    char *tmp;

    if (nmemb <= 1) {
        return;
    }

    tmp = malloc(size);
    if (tmp == NULL) {
        return;
    }

    stack[top++] = 0;
    stack[top++] = nmemb - 1;

    while (top >= 2) {
        size_t hi = stack[--top];
        size_t lo = stack[--top];

        if (lo >= hi) {
            continue;
        }

        /* Partition: pivot at lo */
        char *pivot = base + lo * size;
        size_t i = lo;
        size_t j = hi + 1;

        for (;;) {
            do {
                i++;
            } while (i <= hi && compar(base + i * size, pivot, arg) < 0);
            do {
                j--;
            } while (j > lo && compar(base + j * size, pivot, arg) > 0);
            if (i >= j) {
                break;
            }
            memcpy(tmp, base + i * size, size);
            memcpy(base + i * size, base + j * size, size);
            memcpy(base + j * size, tmp, size);
        }
        memcpy(tmp, base + lo * size, size);
        memcpy(base + lo * size, base + j * size, size);
        memcpy(base + j * size, tmp, size);

        /* Push larger partition first to bound stack at O(log n) */
        if (j - lo > hi - j) {
            if (j > lo && top + 2 <= QSORT_STACK_MAX) {
                stack[top++] = lo;
                stack[top++] = j - 1;
            }
            if (hi > j && top + 2 <= QSORT_STACK_MAX) {
                stack[top++] = j + 1;
                stack[top++] = hi;
            }
        } else {
            if (hi > j && top + 2 <= QSORT_STACK_MAX) {
                stack[top++] = j + 1;
                stack[top++] = hi;
            }
            if (j > lo && top + 2 <= QSORT_STACK_MAX) {
                stack[top++] = lo;
                stack[top++] = j - 1;
            }
        }
    }

    free(tmp);
}

extern void cp_qsort_r(
    void *base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void *, const void *, void *),
    void *arg)
{
    iter_qsort_r((char *)base, nmemb, size, compar, arg);
}
