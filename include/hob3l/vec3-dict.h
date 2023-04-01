/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_VEC3_DICT_H_
#define CP_VEC3_DICT_H_

#include <hob3lbase/mat_gen_tam.h>
#include <hob3lbase/pool_tam.h>
#include <hob3l/vec3-dict_tam.h>

/**
 * Insert or find a vec3 in the given dictionary.
 *
 * Returns the insertion position, starting at 0.
 */
extern cp_vec3_dict_node_t *cp_vec3_dict_insert(
    cp_pool_t *pool,
    cp_vec3_dict_t *dict,
    cp_vec3_t const *v,
    cp_loc_t loc);

#endif /* CP_VEC3_DICT_H_ */
