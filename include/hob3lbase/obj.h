/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_OBJ_H_
#define CP_OBJ_H_

#include <hob3lbase/obj_tam.h>

/* BEGIN MACRO * DO NOT EDIT, use 'mkmacro' instead. */

/**
 * Create a instance of an object.
 *
 * Simple version without setting the location.
 *
 * The set of types that can be created here is defined by passing
 * get_typeof, which is supposed to be macro defining per type which
 * typeid to use.
 */
#define cp_new_type_(get_typeof,obj_t) \
    cp_new_type_1_(CP_GENSYM(_nobjGN), CP_GENSYM(_obj_tGN), get_typeof, \
        obj_t)

#define cp_new_type_1_(nobj,obj_t,get_typeof,_obj_t) \
    ({ \
        typedef __typeof__(_obj_t) obj_t; \
        obj_t * nobj = CP_NEW(*nobj); \
        CP_STATIC_ASSERT(get_typeof(*nobj) != CP_ABSTRACT); \
        nobj->type = get_typeof(*nobj); \
        nobj; \
    })

/**
 * Create a instance of an object.
 *
 * Same as cp_new_type_(), but also sets the location.
 */
#define cp_new_(get_typeof,obj_t,location) \
    cp_new_1_(CP_GENSYM(_nobjGF), CP_GENSYM(_obj_tGF), get_typeof, obj_t, \
        (location))

#define cp_new_1_(nobj,obj_t,get_typeof,_obj_t,location) \
    ({ \
        typedef __typeof__(_obj_t) obj_t; \
        obj_t * nobj = cp_new_type_(get_typeof, obj_t); \
        nobj->loc = location; \
        nobj; \
    })

/**
 * Cast to more generic or special type w/ dynamic type check.
 *
 * The set of types that can be created here is defined by passing
 * get_typeof, which is supposed to be macro defining per type which
 * typeid to use.
 */
#define cp_cast_(get_typeof,target_t,x) \
    cp_cast_1_(CP_GENSYM(_nullGQ), CP_GENSYM(_target_tGQ), \
        CP_GENSYM(_tmpGQ), CP_GENSYM(_xGQ), get_typeof, target_t, (x))

#define cp_cast_1_(null,target_t,tmp,x,get_typeof,_target_t,_x) \
    ({ \
        typedef __typeof__(_target_t) target_t; \
        __typeof__(*_x) *x = _x; \
        assert(x != NULL); \
        unsigned tmp CP_UNUSED = get_typeof(*((target_t*)0)); \
        assert(cp_is_compatible_(tmp, x->type)); \
        void * null CP_UNUSED = NULL; \
        (__typeof__(_Generic(0 ? x : null, \
            void*:        (target_t*)0, \
            void const *: (target_t const *)0)))x; \
    })

/**
 * Cast to more generic or special type w/ dynamic type check.
 *
 * This tries to cast, i.e., has no asserts, but if()s, whether
 * the cast is possible.  This allows NULL pointers as input.
 * It returns NULL also if the dynamic type is not possible because
 * the object has a different type.
 * This does not allow casting to an abstract type.  Use cp_cast()
 * for that (it cannot fail dynamically, only statically).
 *
 * The set of types that can be created here is defined by passing
 * get_typeof, which is supposed to be macro defining per type which
 * typeid to use.
 */
#define cp_try_cast_(get_typeof,target_t,x) \
    cp_try_cast_1_(CP_GENSYM(_nullDX), CP_GENSYM(_target_tDX), \
        CP_GENSYM(_tmpDX), CP_GENSYM(_xDX), get_typeof, target_t, (x))

#define cp_try_cast_1_(null,target_t,tmp,x,get_typeof,_target_t,_x) \
    ({ \
        typedef __typeof__(_target_t) target_t; \
        __typeof__(*_x) *x = _x; \
        CP_STATIC_ASSERT(get_typeof(*((target_t*)0)) != CP_ABSTRACT); \
        unsigned tmp = get_typeof(*((target_t*)0)); \
        void * null = NULL; \
        (x == NULL) || (x->type != tmp) ? NULL \
        : (__typeof__(_Generic(0 ? x : null, \
            void*:        (target_t*)0, \
            void const *: (target_t const *)0)))x; \
    })

/**
 * Cast to abstract type cast w/ static check
 */
#define cp_obj(t) \
    cp_obj_1_(CP_GENSYM(_tTN), (t))

#define cp_obj_1_(t,_t) \
    ({ \
        __typeof__(*_t) *t = _t; \
        assert(t != NULL); \
        assert(t->type != 0); \
        (cp_obj_t*)t; \
    })

/* END MACRO */

static inline bool cp_is_compatible_(
    unsigned pattern,
    unsigned type)
{
    return (pattern == CP_ABSTRACT) ||
           (pattern == type) ||
           (pattern == (type & CP_TYPE_MASK)) ||
           (pattern == (type & CP_TYPE2_MASK));
}

#endif /* CP_OBJ_H_ */
