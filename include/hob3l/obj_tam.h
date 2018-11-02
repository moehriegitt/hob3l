/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_OBJ_TAM_H
#define __CP_OBJ_TAM_H

#include <hob3lbase/err_tam.h>

#define _CP_OBJ \
    unsigned type; \
    cp_loc_t loc;

typedef struct {
    _CP_OBJ
} cp_obj_t;

typedef CP_VEC_T(cp_obj_t*) cp_v_obj_p_t;

#endif /* __CP_OBJ_TAM_H */
