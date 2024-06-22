/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_BASE_MAT_TAM_H_
#define CP_BASE_MAT_TAM_H_

#include <hob3lmat/mat.h>
#include <hob3lbase/err_tam.h>
#include <hob3lbase/color_tam.h>

typedef CP_VEC_T(cp_vec2_t) cp_v_vec2_t;
typedef CP_VEC_T(cp_vec2_t*) cp_v_vec2_p_t;
typedef CP_VEC_T(cp_vec3_t) cp_v_vec3_t;
typedef CP_VEC_T(cp_vec3_t*) cp_v_vec3_p_t;
typedef CP_VEC_T(cp_vec4_t) cp_v_vec4_t;
typedef CP_VEC_T(cp_vec4_t*) cp_v_vec4_p_t;
typedef CP_VEC_T(cp_mat2_t) cp_v_mat2_t;
typedef CP_VEC_T(cp_mat2_t*) cp_v_mat2_p_t;
typedef CP_VEC_T(cp_mat2w_t) cp_v_mat2w_t;
typedef CP_VEC_T(cp_mat2w_t*) cp_v_mat2w_p_t;
typedef CP_VEC_T(cp_mat2i_t) cp_v_mat2i_t;
typedef CP_VEC_T(cp_mat2i_t*) cp_v_mat2i_p_t;
typedef CP_VEC_T(cp_mat2wi_t) cp_v_mat2wi_t;
typedef CP_VEC_T(cp_mat2wi_t*) cp_v_mat2wi_p_t;
typedef CP_VEC_T(cp_mat3_t) cp_v_mat3_t;
typedef CP_VEC_T(cp_mat3_t*) cp_v_mat3_p_t;
typedef CP_VEC_T(cp_mat3w_t) cp_v_mat3w_t;
typedef CP_VEC_T(cp_mat3w_t*) cp_v_mat3w_p_t;
typedef CP_VEC_T(cp_mat3i_t) cp_v_mat3i_t;
typedef CP_VEC_T(cp_mat3i_t*) cp_v_mat3i_p_t;
typedef CP_VEC_T(cp_mat3wi_t) cp_v_mat3wi_t;
typedef CP_VEC_T(cp_mat3wi_t*) cp_v_mat3wi_p_t;
typedef CP_VEC_T(cp_mat4_t) cp_v_mat4_t;
typedef CP_VEC_T(cp_mat4_t*) cp_v_mat4_p_t;
typedef CP_VEC_T(cp_mat4i_t) cp_v_mat4i_t;
typedef CP_VEC_T(cp_mat4i_t*) cp_v_mat4i_p_t;

typedef CP_ARR_T(cp_vec2_t) cp_a_vec2_t;
typedef CP_ARR_T(cp_vec2_t*) cp_a_vec2_p_t;
typedef CP_ARR_T(cp_vec3_t) cp_a_vec3_t;
typedef CP_ARR_T(cp_vec3_t*) cp_a_vec3_p_t;
typedef CP_ARR_T(cp_vec4_t) cp_a_vec4_t;
typedef CP_ARR_T(cp_vec4_t*) cp_a_vec4_p_t;
typedef CP_ARR_T(cp_mat2_t) cp_a_mat2_t;
typedef CP_ARR_T(cp_mat2_t*) cp_a_mat2_p_t;
typedef CP_ARR_T(cp_mat2w_t) cp_a_mat2w_t;
typedef CP_ARR_T(cp_mat2w_t*) cp_a_mat2w_p_t;
typedef CP_ARR_T(cp_mat2i_t) cp_a_mat2i_t;
typedef CP_ARR_T(cp_mat2i_t*) cp_a_mat2i_p_t;
typedef CP_ARR_T(cp_mat2wi_t) cp_a_mat2wi_t;
typedef CP_ARR_T(cp_mat2wi_t*) cp_a_mat2wi_p_t;
typedef CP_ARR_T(cp_mat3_t) cp_a_mat3_t;
typedef CP_ARR_T(cp_mat3_t*) cp_a_mat3_p_t;
typedef CP_ARR_T(cp_mat3w_t) cp_a_mat3w_t;
typedef CP_ARR_T(cp_mat3w_t*) cp_a_mat3w_p_t;
typedef CP_ARR_T(cp_mat3i_t) cp_a_mat3i_t;
typedef CP_ARR_T(cp_mat3i_t*) cp_a_mat3i_p_t;
typedef CP_ARR_T(cp_mat3wi_t) cp_a_mat3wi_t;
typedef CP_ARR_T(cp_mat3wi_t*) cp_a_mat3wi_p_t;
typedef CP_ARR_T(cp_mat4_t) cp_a_mat4_t;
typedef CP_ARR_T(cp_mat4_t*) cp_a_mat4_p_t;
typedef CP_ARR_T(cp_mat4i_t) cp_a_mat4i_t;
typedef CP_ARR_T(cp_mat4i_t*) cp_a_mat4i_p_t;

typedef struct {
    cp_vec3_t coord;
    cp_loc_t loc;
} cp_vec3_loc_t;

typedef CP_ARR_T(cp_vec3_loc_t) cp_a_vec3_loc_t;
typedef CP_VEC_T(cp_vec3_loc_t) cp_v_vec3_loc_t;

typedef struct {
    cp_vec3_loc_t *ref;
    cp_loc_t loc;
} cp_vec3_loc_ref_t;

typedef CP_ARR_T(cp_vec3_loc_ref_t) cp_a_vec3_loc_ref_t;

typedef struct cp_vec2_arr_ref cp_vec2_arr_ref_t;

struct cp_vec2_arr_ref {
    cp_vec2_t *(*nth)(cp_vec2_arr_ref_t const *, size_t);
    size_t (*idx)(cp_vec2_arr_ref_t const *, cp_vec2_t const *);
    void const *user1;
    void const *user2;
};

#endif /* CP_BASE_MAT_TAM_H_ */
