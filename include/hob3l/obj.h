/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_OBJ_H_
#define CP_OBJ_H_

#include <hob3l/obj_tam.h>

/**
 * Create a instance of an object.
 *
 * The set of types that can be created here is defined by passing
 * get_typeof, which is supposed to be macro defining per type which
 * typeid to use.
 */
#define cp_new_(get_typeof, r, _loc) \
    ({ \
        __typeof__(r) * __r = CP_NEW(*__r); \
        CP_STATIC_ASSERT(get_typeof(*__r) != CP_ABSTRACT); \
        __r->type = get_typeof(*__r); \
        __r->loc = (_loc); \
        __r; \
    })

/*
 * Helper for cp_cast_ to be able to nest cp_cast without shadow
 * warning.
 */
#define cp_cast_1_(__x, __t, __n, get_typeof, t, x) \
    ({ \
        __typeof__(x) __x = (x); \
        assert(__x != NULL); \
        unsigned __t CP_UNUSED = get_typeof(*((__typeof__(t)*)0)); \
        assert(cp_is_compatible_(__t, __x->type)); \
        void *__n CP_UNUSED = NULL; \
        (__typeof__(_Generic(0 ? __x : __n, \
            void*:        (__typeof__(t)*)0, \
            void const *: (__typeof__(t) const *)0)))__x; \
    })

/**
 * Cast to more generic or special type w/ dynamic type check.
 *
 * The set of types that can be created here is defined by passing
 * get_typeof, which is supposed to be macro defining per type which
 * typeid to use.
 */
#define cp_cast_(g, t, x) \
    cp_cast_1_(CP_GENSYM(__x), CP_GENSYM(__t), CP_GENSYM(__n), g, t, x)

/*
 * Helper for cp_try_cast_ to be able to nest cp_cast without shadow
 * warning.
 */
#define cp_try_cast_1_(__x, __t, __n, get_typeof, t, x) \
    ({ \
        __typeof__(x) __x = (x); \
        CP_STATIC_ASSERT(get_typeof(*((__typeof__(t)*)0)) != CP_ABSTRACT); \
        unsigned __t = get_typeof(*((__typeof__(t)*)0)); \
        void *__n = NULL; \
        (__x == NULL) || (__x->type != __t) ? NULL \
        : (__typeof__(_Generic(0 ? __x : __n, \
            void*:        (__typeof__(t)*)0, \
            void const *: (__typeof__(t) const *)0)))__x; \
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
#define cp_try_cast_(g, t, x) \
    cp_try_cast_1_(CP_GENSYM(__x), CP_GENSYM(__t), CP_GENSYM(__n), g, t, x)

/** Helper for cp_obj */
#define cp_obj_aux(__t, t) \
    ({ \
        __typeof__(*(t)) *__t = (t); \
        assert((__t)->type != 0); \
        (cp_obj_t*)(__t); \
    })

/** Cast to abstract type cast w/ static check */
#define cp_obj(t) cp_obj_aux(CP_GENSYM(__t), t)

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
