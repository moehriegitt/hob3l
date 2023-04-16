/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_ALLOC_H_
#define CP_ALLOC_H_

#include <stdlib.h>
#include <hob3lbase/base-def.h>
#include <hob3lbase/panic.h>
#include <hob3lbase/alloc_tam.h>

#ifndef NDEBUG
#  define CP_FILE __FILE__
#  define CP_LINE __LINE__
#else
#  define CP_FILE NULL
#  define CP_LINE 0
#endif


#ifndef cp_malloc
#define cp_malloc(m, n, s) cp_malloc_(CP_FILE, CP_LINE, m, n, s)
#endif

#ifndef cp_calloc
#define cp_calloc(m, n, s) cp_calloc_(CP_FILE, CP_LINE, m, n, s)
#endif

#ifndef cp_remalloc
#define cp_remalloc(m, p, o, n, s) cp_remalloc_(CP_FILE, CP_LINE, m, p, o, n, s)
#endif

#ifndef cp_recalloc
#define cp_recalloc(m, p, o, n, s) cp_recalloc_(CP_FILE, CP_LINE, m, p, o, n, s)
#endif

#ifndef cp_free
#define cp_free(m, p) cp_free_(CP_FILE, CP_LINE, m, p)
#endif


#ifndef CP_NEW_ARR_PLUS_ALLOC
#define CP_NEW_ARR_PLUS_ALLOC(M, X, XP, N) \
    ((__typeof__(X)*)cp_calloc(M, N, sizeof(X) + (XP)))
#endif

#ifndef CP_NEW_ARR_ALLOC
#define CP_NEW_ARR_ALLOC(M, X, N) CP_NEW_ARR_PLUS_ALLOC(M, X, 0, N)
#endif

#ifndef CP_NEW_PLUS_ALLOC
#define CP_NEW_PLUS_ALLOC(M, X, XP)  CP_NEW_ARR_PLUS_ALLOC(M, X, XP, 1)
#endif

#ifndef CP_NEW_ALLOC
#define CP_NEW_ALLOC(M, X)  CP_NEW_PLUS_ALLOC(M, X,0)
#endif

#ifndef CP_CLONE_ARR_ALLOC
#define CP_CLONE_ARR_ALLOC(M,X,Y,N) \
    ({ \
        __typeof__(X) const * _y = (Y); \
        size_t _sz = (N) * sizeof(X); \
        (__typeof(X)*)memcpy(cp_malloc(M, sizeof(X), (N)), _y, _sz); \
    })
#endif

#ifndef CP_CLONE_ALLOC
#define CP_CLONE_ALLOC(M,X,Y) CP_CLONE_ARR_ALLOC(M,X,Y,1)
#endif

#ifndef CP_CLONE1_ALLOC
#define CP_CLONE1_ALLOC(M,Y) CP_CLONE_ARR_ALLOC(M,__typeof__(*(Y)),Y,1)
#endif

#ifndef CP_DELETE_ALLOC
#define CP_DELETE_ALLOC(M, obj) \
    do{ \
        __typeof__(&obj) _ref = &(obj); \
        cp_free(M, *_ref); \
        *_ref = NULL; \
    }while(0)
#endif


#ifndef CP_NEW_ARR_PLUS
#define CP_NEW_ARR_PLUS(X, XP, N) CP_NEW_ARR_PLUS(&cp_alloc_global, X, XP, N)
#endif

#ifndef CP_NEW_ARR
#define CP_NEW_ARR(X, N) CP_NEW_ARR_ALLOC(&cp_alloc_global, X, N)
#endif

#ifndef CP_NEW_PLUS
#define CP_NEW_PLUS(X, XP) CP_NEW_PLUS_ALLOC(&cp_alloc_global, X, XP)
#endif

#ifndef CP_NEW
#define CP_NEW(X) CP_NEW_ALLOC(&cp_alloc_global, X)
#endif

#ifndef CP_CLONE_ARR
#define CP_CLONE_ARR(X,Y,N) CP_CLONE_ARR_ALLOC(&cp_alloc_global, X, Y, N)
#endif

#ifndef CP_CLONE
#define CP_CLONE(X,Y) CP_CLONE_ALLOC(&cp_alloc_global, X, Y)
#endif

#ifndef CP_CLONE1
#define CP_CLONE1(X) CP_CLONE1_ALLOC(&cp_alloc_global, X)
#endif

#ifndef CP_DELETE
#define CP_DELETE(O) CP_DELETE_ALLOC(&cp_alloc_global, O)
#endif

extern cp_alloc_t cp_alloc_global;

static inline void *cp_malloc_(
    char const *file, int line, cp_alloc_t *m, size_t a, size_t b)
{
    void *x = m->x_malloc(m, a, b);
    if (x == NULL) {
        cp_panic(file, line, "Out of memory: %zu * %zu\n", a, b);
    }
    return x;
}

static inline void *cp_calloc_(
    char const *file, int line, cp_alloc_t *m, size_t a, size_t b)
{
    void *x = m->x_calloc(m, a, b);
    if (x == NULL) {
        cp_panic(file, line, "Out of memory: %zu * %zu\n", a, b);
    }
    return x;
}

static inline void *cp_remalloc_(
    char const *file, int line, cp_alloc_t *m, void *p, size_t ao, size_t an, size_t b)
{
    void *x = m->x_remalloc(m, p, ao, an, b);
    if (x == NULL) {
        cp_panic(file, line, "Out of memory: %zu * %zu\n", an, b);
    }
    return x;
}

static inline void *cp_recalloc_(
    char const *file, int line, cp_alloc_t *m, void *p, size_t ao, size_t an, size_t b)
{
    void *x = m->x_recalloc(m, p, ao, an, b);
    if (x == NULL) {
        cp_panic(file, line, "Out of memory: %zu * %zu\n", an, b);
    }
    return x;
}

static inline void cp_free_(
    char const *file, int line, cp_alloc_t *m, void *p)
{
    (void)file;
    (void)line;
    m->x_free(m, p);
}

#endif /* CP_ALLOC_H_ */
