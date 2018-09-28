/* -*- Mode: C -*- */

#ifndef __CP_VEC_TAM_H
#define __CP_VEC_TAM_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <cpmat/def.h>

#define CP_ARR_T(TYPE) \
    union{ \
        struct { \
            TYPE *data; \
            size_t size; \
        }; \
        size_t word[2]; \
    }

#define CP_VEC_T(TYPE) \
    union{ \
        struct { \
            TYPE *data; \
            size_t size; \
            size_t alloc; \
        }; \
        CP_ARR_T(TYPE) arr; \
        size_t word[3]; \
    }

typedef CP_VEC_T(void) cp_v_t;
typedef CP_VEC_T(size_t) cp_v_size_t;

typedef CP_ARR_T(double) cp_a_double_t;
typedef CP_ARR_T(size_t) cp_a_size_t;

typedef struct {
    size_t p[3];
} cp_size3_t;

typedef CP_VEC_T(cp_size3_t) cp_v_size3_t;

#define CP_V_INIT { .word = { 0 } }

#define CP_A_INIT_WITH(_d,_s) {{ .data = _d, .size = _s }}

#define CP_A_INIT_WITH_ARR(_d) {{ .data = _d, .size = cp_countof(_d) }}

/**
 * Helper macro to allos cp_v_each to have optional arguments.
 */
#define __cp_v_each_aux(i,v,skipA,skipZ,...) \
    cp_size_each(i, (v)->size, skipA, skipZ)

/**
 * Iterator expression (for 'for') for a vector.
 *
 * See cp_size_each() for details.
 */
#define cp_v_each(i,...) __cp_v_each_aux(i, __VA_ARGS__, 0, 0)

#endif /* __CP_VEC_TAM_H */
