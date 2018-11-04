/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_OBJ_H
#define __CP_OBJ_H

#include <hob3l/obj_tam.h>

/**
 * Create a instance of an object.
 *
 * The set of types that can be created here is defined by passing
 * get_typeof, which is supposed to be macro defining per type which
 * typeid to use.
 */
#define _cp_new(get_typeof, r, _loc) \
    ({ \
        __typeof__(r) * __r = CP_NEW(*__r); \
        cp_static_assert(get_typeof(*__r) != CP_ABSTRACT); \
        __r->type = get_typeof(*__r); \
        __r->loc = (_loc); \
        __r; \
    })

/*
 * Helper for _cp_cast to be able to nest cp_cast without shadow
 * warning.
 */
#define _cp_cast_aux(__x, __t, __n, get_typeof, t, x) \
    ({ \
        __typeof__(x) __x = (x); \
        assert(__x != NULL); \
        unsigned __t __unused = get_typeof(*((__typeof__(t)*)0)); \
        assert(_cp_is_compatible(__t, __x->type)); \
        void *__n __unused = NULL; \
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
#define _cp_cast(g, t, x) \
    _cp_cast_aux(CP_GENSYM(__x), CP_GENSYM(__t), CP_GENSYM(__n), g, t, x)

/*
 * Helper for _cp_try_cast to be able to nest cp_cast without shadow
 * warning.
 */
#define _cp_try_cast_aux(__x, __t, __n, get_typeof, t, x) \
    ({ \
        __typeof__(x) __x = (x); \
        cp_static_assert(get_typeof(*((__typeof__(t)*)0)) != CP_ABSTRACT); \
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
#define _cp_try_cast(g, t, x) \
    _cp_try_cast_aux(CP_GENSYM(__x), CP_GENSYM(__t), CP_GENSYM(__n), g, t, x)

/** Cast to abstract type cast w/ static check */
#define cp_obj(t) \
    ({ \
        assert((t)->type != 0); \
        (cp_obj_t*)(t); \
    })

static inline bool _cp_is_compatible(
    unsigned pattern,
    unsigned type)
{
    return (pattern == CP_ABSTRACT) ||
           (pattern == type) ||
           (pattern == (type & CP_TYPE_MASK)) ||
           (pattern == (type & CP_TYPE2_MASK));
}

#endif /* __CP_OBJ_H */
