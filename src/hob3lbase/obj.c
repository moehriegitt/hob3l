/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* macros */
#ifdef CP_MACRO_

/**
 * Create a instance of an object.
 *
 * Simple version without setting the location.
 *
 * The set of types that can be created here is defined by passing
 * get_typeof, which is supposed to be macro defining per type which
 * typeid to use.
 */
extern macro val cp_new_type_(text get_typeof, type obj_t)
{
    obj_t *nobj = CP_NEW(*nobj);
    CP_STATIC_ASSERT(get_typeof(*nobj) != CP_ABSTRACT);
    nobj->type = get_typeof(*nobj);
    nobj;
}

/**
 * Create a instance of an object.
 *
 * Same as cp_new_type_(), but also sets the location.
 */
extern macro val cp_new_(text get_typeof, type obj_t, val location)
{
    obj_t *nobj = cp_new_type_(get_typeof, obj_t);
    nobj->loc = location;
    nobj;
}

/**
 * Cast to more generic or special type w/ dynamic type check.
 *
 * The set of types that can be created here is defined by passing
 * get_typeof, which is supposed to be macro defining per type which
 * typeid to use.
 */
extern macro val cp_cast_(text get_typeof, type target_t, val *x)
{
    assert(x != NULL);
    unsigned tmp CP_UNUSED = get_typeof(*((target_t*)0));
    assert(cp_is_compatible_(tmp, x->type));
    void *null CP_UNUSED = NULL;
    (__typeof__(_Generic(0 ? x : null,
        void*:        (target_t*)0,
        void const *: (target_t const *)0)))x;
}

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
extern macro val cp_try_cast_(text get_typeof, type target_t, val *x)
{
    CP_STATIC_ASSERT(get_typeof(*((target_t*)0)) != CP_ABSTRACT);
    unsigned tmp = get_typeof(*((target_t*)0));
    void *null = NULL;
    (x == NULL) || (x->type != tmp) ? NULL
    : (__typeof__(_Generic(0 ? x : null,
        void*:        (target_t*)0,
        void const *: (target_t const *)0)))x;
}

/**
 * Cast to abstract type cast w/ static check
 */
extern macro val cp_obj(val *t)
{
    assert(t != NULL);
    assert(t->type != 0);
    (cp_obj_t*)t;
}


#endif /*0*/
