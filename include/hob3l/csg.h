/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG_H
#define __CP_CSG_H

#include <hob3l/csg_tam.h>

/** Create a CSG instance */
#define cp_csg_new(r, _loc) \
    ({ \
        __typeof__(r) * __r = CP_NEW(*__r); \
        cp_static_assert(cp_csg_typeof(*__r) != CP_ABSTRACT); \
        __r->type = cp_csg_typeof(*__r); \
        __r->loc = (_loc); \
        __r; \
    })

/** Case to abstract type cast w/ static check */
#define cp_csg(t) \
    ({ \
        cp_static_assert(cp_csg_typeof(*(t)) != 0); \
        (cp_csg_t*)(t); \
    })

/** Specialising cast w/ dynamic check */
#define cp_csg_cast(_t,s) \
    ({ \
        cp_csg_t __s = cp_csg(s); \
        assert(__s->type == cp_csg_typeof(__s->_t)); \
        &__s->_t; \
    })

static inline size_t cp_csg_add_size(
    cp_csg_add_t *a)
{
    return (a == NULL) ? 0 : a->add.size;
}

#endif /* __CP_CSG_H */
