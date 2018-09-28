/* -*- Mode: C -* */

#ifndef __CP_ALLOC_H
#define __CP_ALLOC_H

#include <stdlib.h>
#include <cpmat/def.h>
#include <cpmat/panic.h>

#ifndef CP_NEW_ARR
#define CP_NEW_ARR(X,N)  ((__typeof__(X)*)cp_calloc(CP_FILE, CP_LINE, N, sizeof(X)))
#endif

#ifndef CP_NEW
#define CP_NEW(X)  CP_NEW_ARR(X,1)
#endif

#ifndef CP_CALLOC_ARR
#define CP_CALLOC_ARR(X,N)  ((X) = CP_NEW_ARR(*(X), N))
#endif

#ifndef CP_CALLOC
#define CP_CALLOC(X)  CP_CALLOC_ARR(X,1)
#endif

#ifndef CP_CLONE
#define CP_CLONE(X) \
    ((__typeof__(X))memcpy(cp_malloc(CP_FILE, CP_LINE, sizeof(*X)), (X), sizeof(*X)))
#endif

#ifndef CP_FREE
#define CP_FREE(obj) \
    do{ \
        __typeof__(&obj) __ref = &(obj); \
        free(*__ref); \
        *__ref = NULL; \
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
        cp_panic(file, line, "Out of memory allocating %"_Pz"u bytes.", a);
    }
    return r;
}

static inline void *cp_calloc(char const *file, int line, size_t a, size_t b)
{
    void *r = calloc(a, b);
    if (r == NULL) {
        cp_panic(file, line, "Out of memory allocating %"_Pz"u * %"_Pz"u bytes.", a, b);
    }
    return r;
}

#endif /* __CP_ALLOC_H */
