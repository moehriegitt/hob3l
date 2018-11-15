/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_VEC3_DICT_H
#define __CP_VEC3_DICT_H

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

#endif /* __CP_VEC3_DICT_H */
