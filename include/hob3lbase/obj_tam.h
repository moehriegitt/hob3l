/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_OBJ_TAM_H_
#define CP_OBJ_TAM_H_

#include <hob3lbase/err_tam.h>

/**
 * If type_t has the same size as unsigned,
 * type_t will be returned, otherwise, unsigned.
 *
 * This works by exploiting that
 *     (0 ? (void*)0 : (int*)0) :: int*   , but
 *     (0 ? (void*)1 : (int*)0) :: void*
 *
 * It is assumed that sizeof(type_t) <= sizeof(unsigned).
 *
 * This is used to allow better error checking in switch
 * statements if enums use unsigned type, and to work
 * in all cases otherwise.
 */
#define CP_ENUM_OR_UINT(type_t) \
    __typeof__( \
        _Generic( \
            (0 ? (void*)(sizeof(unsigned) - sizeof(type_t)) : (int*)0), \
            default: (unsigned)0, \
            int*   : (type_t)0))


/**
 * The slots that need to be at the beginning of any raw cp_obj_t,
 * which has nothing but a type.
 */
#define CP_OBJ_TYPE_(type_t) \
    CP_ENUM_OR_UINT(type_t) type;

/**
 * The slots that need to be at the beginning of any cp_obj_t.
 */
#define CP_OBJ_(type_t) \
    CP_OBJ_TYPE_(type_t) \
    cp_loc_t loc;


typedef struct {
    CP_OBJ_(unsigned)
} cp_obj_t;

typedef CP_VEC_T(cp_obj_t*) cp_v_obj_p_t;

#endif /* CP_OBJ_TAM_H_ */
