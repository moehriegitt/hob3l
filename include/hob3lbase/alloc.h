/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_ALLOC_H_
#define CP_ALLOC_H_

#include <stdlib.h>
#include <hob3lbase/def.h>
#include <hob3lbase/panic.h>

#ifndef CP_NEW_ARR
#define CP_NEW_ARR(X,N)  ((__typeof__(X)*)cp_calloc(CP_FILE, CP_LINE, N, sizeof(X)))
#endif

#ifndef CP_NEW
#define CP_NEW(X)  CP_NEW_ARR(X,1)
#endif

#ifndef CP_CLONE_ARR
#define CP_CLONE_ARR(X,Y,N) \
    ({ \
        __typeof__(X) const * _y = (Y); \
        size_t _sz = (N) * sizeof(X); \
        (__typeof(X)*)memcpy(cp_malloc(CP_FILE, CP_LINE, _sz), _y, _sz); \
    })
#endif

#ifndef CP_CLONE
#define CP_CLONE(X,Y) CP_CLONE_ARR(X,Y,1)
#endif

#ifndef CP_FREE
#define CP_FREE(obj) \
    do{ \
        __typeof__(&obj) _ref = &(obj); \
        free(*_ref); \
        *_ref = NULL; \
    }while(0)
#endif

#ifndef NDEBUG
#  define CP_FILE __FILE__
#  define CP_LINE __LINE__
#else
#  define CP_FILE NULL
#  define CP_LINE 0
#endif

static inline void *cp_malloc(char const *file, int line, size_t a)
{
    void *r = malloc(a);
    if (r == NULL) {
        cp_panic(file, line, "Out of memory allocating %"CP_Z"u bytes.", a);
    }
    return r;
}

static inline void *cp_calloc(char const *file, int line, size_t a, size_t b)
{
    void *r = calloc(a, b);
    if (r == NULL) {
        cp_panic(file, line, "Out of memory allocating %"CP_Z"u * %"CP_Z"u bytes.", a, b);
    }
    return r;
}

#endif /* CP_ALLOC_H_ */
