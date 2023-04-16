/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_VEC_TAM_H_
#define CP_VEC_TAM_H_

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <hob3lbase/base-def.h>

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

typedef CP_ARR_T(unsigned short) cp_a_u16_t;

#define CP_SIZE3_T \
    struct { \
        union { \
            size_t p[3]; \
            struct { \
                size_t a, b, c; \
            }; \
        }; \
    }

typedef CP_SIZE3_T cp_size3_t;

typedef CP_VEC_T(cp_size3_t) cp_v_size3_t;

#define CP_V_INIT { .word = { 0 } }

#define CP_A_INIT_WITH(_d,_s) {{ .data = _d, .size = _s }}

/**
 * Helper macro to allow cp_v_each to have optional arguments.
 */
#define cp_v_each_1_(i,v,skipA,skipZ,...) \
    cp_size_each(i, (v)->size, skipA, skipZ)

#define cp_v_val_p_t(v) __typeof__((v)->data+0)
#define cp_v_val_t(v)   __typeof__(*((v)->data+0))
#define cp_v_vec_t(v)    __typeof__(*(v))*

/**
 * Iterator expression (for 'for') for a vector.
 *
 * See cp_size_each() for details.
 *
 * This does not allow resizing of the vector during
 * iteration -- the iteration will not terminate properly
 * in that case.
 *
 * The iterator macros cp_v_eachp() and cp_v_eachv() do
 * allow shrinking of the vector during iteration.
 */
#define cp_v_each(i,...) cp_v_each_1_(i, __VA_ARGS__, 0, 0)

#define cp_v_eachp_1_(v_, a_, e_, i, v) \
    cp_v_val_t(v) \
        *cp_advance_##i = (void*)1, \
        *v_ = (void*)(size_t)(v), \
        *a_ = ((cp_v_vec_t(v))(size_t)v_)->data, \
        *i = a_; \
    i != (a_ + ((cp_v_vec_t(v))(size_t)v_)->size); \
    i += (size_t)cp_advance_##i, \
        cp_advance_##i = (void*)1

/**
 * This is ugly (too many casts), but it iterates the pointers to the
 * elements of a vector.
 *
 * It also only evaluates (v) once.
 *
 * This allows that in an iteration, the last item of the vector may
 * be removed, i.e., the size may shrink.  It must be ensured,
 * however, that the iteration pointer points to elements [0..n], not
 * to any element with index larger than n.
 * If this is ensured, the loop termination will work fine.
 *
 * This does not allow that the vector is grown or shrunk, because the
 * pointer to data will become invalid (the iterator variable itself,
 * and some cached variables).  However, no operation except
 * cp_v_shrink() will shrink a vector.
 *
 * The cp_redo() macro can be used to run the exit check, but not
 * advance the iterator (similar to Perl's 'redo').
 */
#define cp_v_eachp(i,v) \
    cp_v_eachp_1_(CP_GENSYM(v_), CP_GENSYM(a_), CP_GENSYM(e_), i, v)

/* some auxiliary cast functions for maximum ugliness macro */
#define cp_v_elemv_put_(v,x)  ((cp_v_val_t(v))(size_t)(x))
#define cp_v_elemv_vec_(v,x)  ((cp_v_vec_t(v))(size_t)x)
#define cp_v_elemv_val_(v,x)  ((cp_v_val_p_t(v))(size_t)x)

/**
 * Helper for iterator for values of a vector.
 *
 * This may be the ugliest macro here...
 */
#define cp_v_eachv_1_(v_, a_, e_, i_, i, v) \
    cp_v_val_t(v) \
        cp_advance_##i = cp_v_elemv_put_(v, 1), \
        v_ = cp_v_elemv_put_(v, v), \
        a_ = cp_v_elemv_put_(v, cp_v_elemv_vec_(v, v_)->data), \
        i_ = a_, \
        i; \
    (i_ != cp_v_elemv_put_(v, cp_v_elemv_val_(v, a_) + cp_v_elemv_vec_(v, v_)->size)) && \
        (i = *cp_v_elemv_val_(v,i_), 1); \
    i_ = cp_v_elemv_put_(v, cp_v_elemv_val_(v, i_) + (size_t)cp_advance_##i), \
        cp_advance_##i = cp_v_elemv_put_(v, 1), \
        ({ static_assert(sizeof(v_) == sizeof(void*),""); })

/**
 * Iterator for values of a vector.  This only works if the
 * values are pointers or size_t integers.
 */
#define cp_v_eachv(i, v) \
    cp_v_eachv_1_(CP_GENSYM(v_), CP_GENSYM(a_), CP_GENSYM(e_), CP_GENSYM(i_), i, v)

#endif /* CP_VEC_TAM_H_ */
