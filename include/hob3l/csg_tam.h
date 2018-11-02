/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG_TAM_H
#define __CP_CSG_TAM_H

#include <hob3l/obj_tam.h>
#include <hob3l/csg_fwd.h>

#define cp_csg_typeof(type) \
    _Generic(type, \
        cp_obj_t:     CP_ABSTRACT, \
        cp_csg_add_t: CP_CSG_ADD, \
        cp_csg_sub_t: CP_CSG_SUB, \
        cp_csg_cut_t: CP_CSG_CUT)

/**
 * CSG basic objects that are shared by 2D and 3D subsystems.
 */
typedef enum {
    /**
     * Bool op: union (boolean '|') */
    CP_CSG_ADD = CP_CSG_TYPE + 1,

    /**
     * Bool op: difference (boolean '&~')*/
    CP_CSG_SUB,

    /**
     * Bool op: cut (boolean '&') */
    CP_CSG_CUT,
} cp_csg_type_t;

typedef struct {
    /**
     * type is CP_CSG_ADD */
    _CP_OBJ
    cp_v_obj_p_t add;
} cp_csg_add_t;

typedef struct {
    /**
     * type is CP_CSG_SUB */
    _CP_OBJ
    cp_csg_add_t *add;
    cp_csg_add_t *sub;
} cp_csg_sub_t;

typedef CP_VEC_T(cp_csg_add_t*) cp_v_csg_add_p_t;

typedef struct {
    /**
     * type is CP_CSG_CUT */
    _CP_OBJ
    cp_v_csg_add_p_t cut;
} cp_csg_cut_t;

union cp_csg {
    struct { _CP_OBJ };
    cp_obj_t _obj;
    cp_csg_add_t _add;
    cp_csg_sub_t _sub;
    cp_csg_cut_t _cut;
};

#endif /* __CP_CSG_TAM_H */
