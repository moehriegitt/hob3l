/* -*- Mode: C -*- */

#ifndef __CP_QSORT_H
#define __CP_QSORT_H

#include <cpmat/arch.h>

#ifndef cp_qsort_r

extern void cp_qsort_r(
    void *base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void *, const void *, void *),
    void *arg);

#endif /* cp_qsort_r */

#endif /* __CP_QSORT_H */
