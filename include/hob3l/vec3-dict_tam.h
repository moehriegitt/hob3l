/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_VEC3_DICT_TAM_H_
#define CP_VEC3_DICT_TAM_H_

#include <hob3lbase/dict_tam.h>

typedef struct {
    cp_dict_t *root;
    size_t size;
} cp_vec3_dict_t;

typedef struct {
    cp_dict_t node;
    cp_vec3_t coord;
    cp_loc_t loc;
    size_t idx;
} cp_vec3_dict_node_t;


#endif /* CP_VEC3_DICT_TAM_H_ */
