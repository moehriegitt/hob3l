/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_OBJ_H
#define __CP_OBJ_H

#include <hob3l/obj_tam.h>

/** Case to abstract type cast w/ static check */
#define cp_obj(t) \
    ({ \
        assert((t)->type != 0); \
        (cp_obj_t*)(t); \
    })


#endif /* __CP_OBJ_H */
