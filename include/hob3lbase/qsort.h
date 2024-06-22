/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_QSORT_H_
#define CP_QSORT_H_

#include <hob3ldef/arch.h>

#ifndef cp_qsort_r

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
    void *arg);

#endif /* cp_qsort_r */

#endif /* CP_QSORT_H_ */
