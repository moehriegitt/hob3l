/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_OBJ_TAM_H_
#define CP_OBJ_TAM_H_

#include <hob3lbase/err_tam.h>

/**
 * The slots that need to be at the beginning of any cp_obj_t.
 */
#define CP_OBJ_ \
    unsigned type; \
    cp_loc_t loc;

typedef struct {
    CP_OBJ_
} cp_obj_t;

typedef CP_VEC_T(cp_obj_t*) cp_v_obj_p_t;

#endif /* CP_OBJ_TAM_H_ */
