/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */
/* -*- Mode: C -* */

#ifndef __CP_MAT_TAM_H
#define __CP_MAT_TAM_H

#include <cpmat/vec.h>
#include <cpmat/mat_gen_tam.h>
#include <csg2plane/err_tam.h>

typedef struct {
    cp_vec2_t coord;
    cp_loc_t loc;
} cp_vec2_loc_t;

typedef CP_ARR_T(cp_vec2_loc_t) cp_a_vec2_loc_t;
typedef CP_VEC_T(cp_vec2_loc_t) cp_v_vec2_loc_t;

typedef struct {
    cp_vec2_loc_t *ref;
    cp_loc_t loc;
} cp_vec2_loc_ref_t;

typedef CP_ARR_T(cp_vec2_loc_ref_t) cp_a_vec2_loc_ref_t;

typedef struct {
    cp_vec3_t coord;
    cp_loc_t loc;
} cp_vec3_loc_t;

typedef CP_ARR_T(cp_vec3_loc_t) cp_a_vec3_loc_t;
typedef CP_VEC_T(cp_vec3_loc_t) cp_v_vec3_loc_t;

typedef struct {
    cp_vec3_loc_t *ref;
    cp_loc_t loc;
} cp_vec3_loc_ref_t;

typedef CP_ARR_T(cp_vec3_loc_ref_t) cp_a_vec3_loc_ref_t;

/**
 * Pointer to base of array of entries with vec2 slot plus info to access array.
 */
typedef struct {
    void  *base;
    size_t size;
    size_t count;
} cp_vec2_arr_ref_t;

static inline cp_vec2_t *cp_vec2_arr_ref(
    cp_vec2_arr_ref_t *a,
    size_t i)
{
    assert(i < a->count);
    assert(((i * a->size) / a->size) == i);
    void *r = ((char*)a->base) + (a->size * i);
    return r;
}

static inline size_t cp_vec2_arr_idx(
    cp_vec2_arr_ref_t *a,
    cp_vec2_t *p)
{
    size_t o = CP_PTRDIFF((char*)p, (char*)a->base);
    assert((o % a->size) == 0);
    return o / a->size;
}

static inline cp_vec2_arr_ref_t *__cp_vec2_arr_ref_set(
    cp_vec2_arr_ref_t *a,
    cp_vec2_arr_ref_t x)
{
    *a = x;
    return a;
}

#define CP_VEC2_ARR_REF(arr, slot) \
    (*__cp_vec2_arr_ref_set( \
        &(cp_vec2_arr_ref_t){ .base=0 }, \
        ({ \
            __typeof__(*(arr)) *__arr = (arr); \
            void *__base = __arr->data; \
            cp_vec2_arr_ref_t __r = { \
                .base = (char*)__base + cp_offsetof(__typeof__(__arr->data[0]), slot), \
                .size = sizeof(__arr->data[0]), \
                .count = __arr->size \
            }; \
            __r; \
        })))

#define CP_INIT(m, ...) \
    ({ \
        __typeof__(*(m)) CP_GENSYM(r) = {{ __VA_ARGS__ }}; \
        _Static_assert(CP_COUNT(__VA_ARGS__) == cp_countof(CP_GENSYM(r).v), ""); \
        *(m) = CP_GENSYM(r); \
    })

#define CP_V01(p)   (p).v[0], (p).v[1]
#define CP_V012(p)  (p).v[0], (p).v[1], (p).v[2]
#define CP_V0123(p) (p).v[0], (p).v[1], (p).v[2], (p).v[3]

#define CP_VEC2(a,b) \
    ((cp_vec2_t){ .v={ a,b } })

#define CP_VEC3(a,b,c) \
    ((cp_vec3_t){ .v={ a,b,c } })

#define CP_VEC4(a,b,c,d) \
    ((cp_vec4_t){ .v={ a,b,c,d } })

#define CP_MAT2(a,b, c,d) \
    ((cp_mat2_t){ .v={ a,b, c,d } })

#define CP_MAT3(a,b,c, d,e,f, g,h,i) \
    ((cp_mat3_t){ .v={ a,b,c, d,e,f, g,h,i } })

#define CP_MAT4(a,b,c,d, e,f,g,h, i,j,k,l, m,n,o,p) \
    ((cp_mat3_t){ .v={ a,b,c,d, e,f,g,h, i,j,k,l, m,n,o,p } })

#define CP_MAT2W(_a,_b,_c, _d,_e,_f) \
    ((cp_mat2w_t){ .b={.v={ _a,_b, _d,_e }}, .w={.v={_c,_f}} })

#define CP_MAT3W(a,b,c,d, e,f,g,h, i,j,k,l) \
    ((cp_mat3w_t){ .b={.v={ a,b,c, e,f,g, i,j,k }}, .w={.v={d,h,l}} })

#define cp_mat2_init2 CP_INIT

#define cp_mat3_init3 CP_INIT

#define CP_MINMAX_EMPTY { .min={ .v={0} }, .max={ .v={-1} } }

static inline void cp_mat4_init3(
    cp_mat4_t *m,
    cp_f_t a, cp_f_t b, cp_f_t c,
    cp_f_t d, cp_f_t e, cp_f_t f,
    cp_f_t g, cp_f_t h, cp_f_t i)
{
    CP_INIT(m,
        a, b, c, 0,
        d, e, f, 0,
        g, h, i, 0,
        0, 0, 0, 1);
}

#endif /* __CP_MAT_H */
