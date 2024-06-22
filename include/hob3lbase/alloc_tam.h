/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_ALLOC_TAM_H_
#define CP_ALLOC_TAM_H_

#include <stdlib.h>

typedef struct cp_alloc cp_alloc_t;

struct cp_alloc {
    void *(*x_malloc)   (cp_alloc_t *alloc, size_t a, size_t b);
    void *(*x_calloc)   (cp_alloc_t *alloc, size_t a, size_t b);
    void *(*x_remalloc) (cp_alloc_t *alloc, void *p, size_t ao, size_t an, size_t b);
    void *(*x_recalloc) (cp_alloc_t *alloc, void *p, size_t ao, size_t an, size_t b);
    void  (*x_free)     (cp_alloc_t *alloc, void *p);
};

#endif /* CP_ALLOC_TAM_H_ */
