/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG_H_
#define CP_CSG_H_

#include <hob3l/obj_tam.h>
#include <hob3l/csg_tam.h>

/**
 * Create a CSG instance.
 *
 * \p t is either a type or an expression of the desired type.
 *
 * This creates an object of the desired type and writes into its
 * 'type' the typeid defined by cp_csg_typeof().  Casting among
 * objects of the set of types defined by cp_csg_typeof() is done
 * by cp_csg_cast().
 */
#define cp_csg_new(r,l) cp_new_(cp_csg_typeof, r, l)

/**
 * Cast to more generic or special type w/ dynamic type check.
 *
 * \p Type is the designed type or an expression of the designed type.
 * The resulting expression type is a pointer to that that is const
 * if \p x is const or \p t is const.  The value is not changed, i.e., value
 * wise, this is the identity.
 *
 * The definition of what can be cast how is based on cp_csg_typeof(), just
 * like the definition of cp_csg_new().
 *
 * \code
 *    cp_obj_t *q = ...
 *    ...
 *    cp_csg2_poly_t *p = cp_csg_cast(*p, q);
 * \endcode
 */
#define cp_csg_cast(t, x) cp_cast_(cp_csg_typeof, t, x)

/**
 * Versionof cp_csg_cast() that returns NULL instead of assert-failing. */
#define cp_csg_try_cast(t, x) cp_try_cast_(cp_csg_typeof, t, x)

static inline size_t cp_csg_add_size(
    cp_csg_add_t *a)
{
    return (a == NULL) ? 0 : a->add.size;
}

#endif /* CP_CSG_H_ */
