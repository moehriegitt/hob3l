/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */
/* Automatically generated by mkmat. */

#ifndef CP_MAT_GEN_INL_H_
#define CP_MAT_GEN_INL_H_

#include <hob3lmat/mat_gen_tam.h>
#include <hob3lmat/mat_gen_ext.h>
#include <hob3lmat/algo.h>

static inline int cp_vec2_lex_cmp(
    cp_vec2_t const* a,
    cp_vec2_t const* b)
{
    return cp_lex_cmp(a->v, b->v, cp_countof(a->v));
}

static inline int cp_vec3_lex_cmp(
    cp_vec3_t const* a,
    cp_vec3_t const* b)
{
    return cp_lex_cmp(a->v, b->v, cp_countof(a->v));
}

static inline int cp_vec4_lex_cmp(
    cp_vec4_t const* a,
    cp_vec4_t const* b)
{
    return cp_lex_cmp(a->v, b->v, cp_countof(a->v));
}

static inline int cp_mat2_lex_cmp(
    cp_mat2_t const* a,
    cp_mat2_t const* b)
{
    return cp_lex_cmp(a->v, b->v, cp_countof(a->v));
}

static inline int cp_mat2w_lex_cmp(
    cp_mat2w_t const* a,
    cp_mat2w_t const* b)
{
    return cp_lex_cmp(a->v, b->v, cp_countof(a->v));
}

static inline int cp_mat2i_lex_cmp(
    cp_mat2i_t const* a,
    cp_mat2i_t const* b)
{
    return cp_mat2_lex_cmp(&a->n, &b->n);
}

static inline int cp_mat2wi_lex_cmp(
    cp_mat2wi_t const* a,
    cp_mat2wi_t const* b)
{
    return cp_mat2w_lex_cmp(&a->n, &b->n);
}

static inline int cp_mat3_lex_cmp(
    cp_mat3_t const* a,
    cp_mat3_t const* b)
{
    return cp_lex_cmp(a->v, b->v, cp_countof(a->v));
}

static inline int cp_mat3w_lex_cmp(
    cp_mat3w_t const* a,
    cp_mat3w_t const* b)
{
    return cp_lex_cmp(a->v, b->v, cp_countof(a->v));
}

static inline int cp_mat3i_lex_cmp(
    cp_mat3i_t const* a,
    cp_mat3i_t const* b)
{
    return cp_mat3_lex_cmp(&a->n, &b->n);
}

static inline int cp_mat3wi_lex_cmp(
    cp_mat3wi_t const* a,
    cp_mat3wi_t const* b)
{
    return cp_mat3w_lex_cmp(&a->n, &b->n);
}

static inline int cp_mat4_lex_cmp(
    cp_mat4_t const* a,
    cp_mat4_t const* b)
{
    return cp_lex_cmp(a->v, b->v, cp_countof(a->v));
}

static inline int cp_mat4i_lex_cmp(
    cp_mat4i_t const* a,
    cp_mat4i_t const* b)
{
    return cp_mat4_lex_cmp(&a->n, &b->n);
}

static inline bool cp_vec2_eq(
    cp_vec2_t const* a,
    cp_vec2_t const* b)
{
    return cp_vec2_lex_cmp(a,b) == 0;
}

static inline bool cp_vec3_eq(
    cp_vec3_t const* a,
    cp_vec3_t const* b)
{
    return cp_vec3_lex_cmp(a,b) == 0;
}

static inline bool cp_vec4_eq(
    cp_vec4_t const* a,
    cp_vec4_t const* b)
{
    return cp_vec4_lex_cmp(a,b) == 0;
}

static inline bool cp_mat2_eq(
    cp_mat2_t const* a,
    cp_mat2_t const* b)
{
    return cp_mat2_lex_cmp(a,b) == 0;
}

static inline bool cp_mat2w_eq(
    cp_mat2w_t const* a,
    cp_mat2w_t const* b)
{
    return cp_mat2w_lex_cmp(a,b) == 0;
}

static inline bool cp_mat2i_eq(
    cp_mat2i_t const* a,
    cp_mat2i_t const* b)
{
    return cp_mat2i_lex_cmp(a,b) == 0;
}

static inline bool cp_mat2wi_eq(
    cp_mat2wi_t const* a,
    cp_mat2wi_t const* b)
{
    return cp_mat2wi_lex_cmp(a,b) == 0;
}

static inline bool cp_mat3_eq(
    cp_mat3_t const* a,
    cp_mat3_t const* b)
{
    return cp_mat3_lex_cmp(a,b) == 0;
}

static inline bool cp_mat3w_eq(
    cp_mat3w_t const* a,
    cp_mat3w_t const* b)
{
    return cp_mat3w_lex_cmp(a,b) == 0;
}

static inline bool cp_mat3i_eq(
    cp_mat3i_t const* a,
    cp_mat3i_t const* b)
{
    return cp_mat3i_lex_cmp(a,b) == 0;
}

static inline bool cp_mat3wi_eq(
    cp_mat3wi_t const* a,
    cp_mat3wi_t const* b)
{
    return cp_mat3wi_lex_cmp(a,b) == 0;
}

static inline bool cp_mat4_eq(
    cp_mat4_t const* a,
    cp_mat4_t const* b)
{
    return cp_mat4_lex_cmp(a,b) == 0;
}

static inline bool cp_mat4i_eq(
    cp_mat4i_t const* a,
    cp_mat4i_t const* b)
{
    return cp_mat4i_lex_cmp(a,b) == 0;
}

static inline double cp_vec2_sqr_len(
    cp_vec2_t const* a)
{
    return cp_vec2_dot(a,a);
}

static inline double cp_vec3_sqr_len(
    cp_vec3_t const* a)
{
    return cp_vec3_dot(a,a);
}

static inline double cp_vec4_sqr_len(
    cp_vec4_t const* a)
{
    return cp_vec4_dot(a,a);
}

static inline double cp_vec2_len(
    cp_vec2_t const* a)
{
    return sqrt(cp_vec2_sqr_len(a));
}

static inline double cp_vec3_len(
    cp_vec3_t const* a)
{
    return sqrt(cp_vec3_sqr_len(a));
}

static inline double cp_vec4_len(
    cp_vec4_t const* a)
{
    return sqrt(cp_vec4_sqr_len(a));
}

static inline bool cp_vec2_has_len1(
    cp_vec2_t const* a)
{
    return cp_sqr_eq(cp_vec2_sqr_len(a), 1);
}

static inline bool cp_vec3_has_len1(
    cp_vec3_t const* a)
{
    return cp_sqr_eq(cp_vec3_sqr_len(a), 1);
}

static inline bool cp_vec4_has_len1(
    cp_vec4_t const* a)
{
    return cp_sqr_eq(cp_vec4_sqr_len(a), 1);
}

static inline bool cp_vec2_has_len0_or_1(
    cp_vec2_t const* a)
{
    return cp_vec2_has_len0(a) || cp_vec2_has_len1(a);
}

static inline bool cp_vec3_has_len0_or_1(
    cp_vec3_t const* a)
{
    return cp_vec3_has_len0(a) || cp_vec3_has_len1(a);
}

static inline bool cp_vec4_has_len0_or_1(
    cp_vec4_t const* a)
{
    return cp_vec4_has_len0(a) || cp_vec4_has_len1(a);
}

static inline double cp_vec2_dist(
    cp_vec2_t const* a,
    cp_vec2_t const* b)
{
    return sqrt(cp_vec2_sqr_dist(a,b));
}

static inline double cp_vec3_dist(
    cp_vec3_t const* a,
    cp_vec3_t const* b)
{
    return sqrt(cp_vec3_sqr_dist(a,b));
}

static inline double cp_vec4_dist(
    cp_vec4_t const* a,
    cp_vec4_t const* b)
{
    return sqrt(cp_vec4_sqr_dist(a,b));
}

static inline void cp_mat2_scale(
    cp_mat2_t * r,
    double x,
    double y)
{
    CP_ZERO(r);
    r->m[0][0] = x;
    r->m[1][1] = y;
}

static inline void cp_mat2w_scale(
    cp_mat2w_t * r,
    double x,
    double y)
{
    CP_ZERO(r);
    r->b.m[0][0] = x;
    r->b.m[1][1] = y;
}

static inline void cp_mat3_scale(
    cp_mat3_t * r,
    double x,
    double y,
    double z)
{
    CP_ZERO(r);
    r->m[0][0] = x;
    r->m[1][1] = y;
    r->m[2][2] = z;
}

static inline void cp_mat3w_scale(
    cp_mat3w_t * r,
    double x,
    double y,
    double z)
{
    CP_ZERO(r);
    r->b.m[0][0] = x;
    r->b.m[1][1] = y;
    r->b.m[2][2] = z;
}

static inline void cp_mat4_scale(
    cp_mat4_t * r,
    double x,
    double y,
    double z)
{
    CP_ZERO(r);
    r->m[0][0] = x;
    r->m[1][1] = y;
    r->m[2][2] = z;
    r->m[3][3] = 1;
}

static inline void cp_mat2_scale1(
    cp_mat2_t * r,
    double a)
{
    cp_mat2_scale(r, a, a);
}

static inline void cp_mat2w_scale1(
    cp_mat2w_t * r,
    double a)
{
    cp_mat2w_scale(r, a, a);
}

static inline void cp_mat2i_scale1(
    cp_mat2i_t * r,
    double a)
{
    cp_mat2i_scale(r, a, a);
}

static inline void cp_mat2wi_scale1(
    cp_mat2wi_t * r,
    double a)
{
    cp_mat2wi_scale(r, a, a);
}

static inline void cp_mat3_scale1(
    cp_mat3_t * r,
    double a)
{
    cp_mat3_scale(r, a, a, a);
}

static inline void cp_mat3w_scale1(
    cp_mat3w_t * r,
    double a)
{
    cp_mat3w_scale(r, a, a, a);
}

static inline void cp_mat3i_scale1(
    cp_mat3i_t * r,
    double a)
{
    cp_mat3i_scale(r, a, a, a);
}

static inline void cp_mat3wi_scale1(
    cp_mat3wi_t * r,
    double a)
{
    cp_mat3wi_scale(r, a, a, a);
}

static inline void cp_mat4_scale1(
    cp_mat4_t * r,
    double a)
{
    cp_mat4_scale(r, a, a, a);
}

static inline void cp_mat4i_scale1(
    cp_mat4i_t * r,
    double a)
{
    cp_mat4i_scale(r, a, a, a);
}

static inline void cp_mat2_scale_v(
    cp_mat2_t * r,
    cp_vec2_t const* a)
{
    cp_mat2_scale(r, a->x, a->y);
}

static inline void cp_mat2w_scale_v(
    cp_mat2w_t * r,
    cp_vec2_t const* a)
{
    cp_mat2w_scale(r, a->x, a->y);
}

static inline void cp_mat2i_scale_v(
    cp_mat2i_t * r,
    cp_vec2_t const* a)
{
    cp_mat2i_scale(r, a->x, a->y);
}

static inline void cp_mat2wi_scale_v(
    cp_mat2wi_t * r,
    cp_vec2_t const* a)
{
    cp_mat2wi_scale(r, a->x, a->y);
}

static inline void cp_mat3_scale_v(
    cp_mat3_t * r,
    cp_vec3_t const* a)
{
    cp_mat3_scale(r, a->x, a->y, a->z);
}

static inline void cp_mat3w_scale_v(
    cp_mat3w_t * r,
    cp_vec3_t const* a)
{
    cp_mat3w_scale(r, a->x, a->y, a->z);
}

static inline void cp_mat3i_scale_v(
    cp_mat3i_t * r,
    cp_vec3_t const* a)
{
    cp_mat3i_scale(r, a->x, a->y, a->z);
}

static inline void cp_mat3wi_scale_v(
    cp_mat3wi_t * r,
    cp_vec3_t const* a)
{
    cp_mat3wi_scale(r, a->x, a->y, a->z);
}

static inline void cp_mat4_scale_v(
    cp_mat4_t * r,
    cp_vec3_t const* a)
{
    cp_mat4_scale(r, a->x, a->y, a->z);
}

static inline void cp_mat4i_scale_v(
    cp_mat4i_t * r,
    cp_vec3_t const* a)
{
    cp_mat4i_scale(r, a->x, a->y, a->z);
}

static inline void cp_mat2_unit(
    cp_mat2_t * r)
{
    cp_mat2_scale1(r, 1);
}

static inline void cp_mat2w_unit(
    cp_mat2w_t * r)
{
    cp_mat2w_scale1(r, 1);
}

static inline void cp_mat2i_unit(
    cp_mat2i_t * r)
{
    cp_mat2i_scale1(r, 1);
}

static inline void cp_mat2wi_unit(
    cp_mat2wi_t * r)
{
    cp_mat2wi_scale1(r, 1);
}

static inline void cp_mat3_unit(
    cp_mat3_t * r)
{
    cp_mat3_scale1(r, 1);
}

static inline void cp_mat3w_unit(
    cp_mat3w_t * r)
{
    cp_mat3w_scale1(r, 1);
}

static inline void cp_mat3i_unit(
    cp_mat3i_t * r)
{
    cp_mat3i_scale1(r, 1);
}

static inline void cp_mat3wi_unit(
    cp_mat3wi_t * r)
{
    cp_mat3wi_scale1(r, 1);
}

static inline void cp_mat4_unit(
    cp_mat4_t * r)
{
    cp_mat4_scale1(r, 1);
}

static inline void cp_mat4i_unit(
    cp_mat4i_t * r)
{
    cp_mat4i_scale1(r, 1);
}

static inline void cp_mat3_rot_unit(
    cp_mat3_t * r,
    cp_vec3_t const* u,
    cp_vec2_t const* sc)
{
    cp_dim3_rot_unit(&r->row[0], &r->row[1], &r->row[2], u, sc);
}

static inline void cp_mat3w_rot_unit(
    cp_mat3w_t * r,
    cp_vec3_t const* v,
    cp_vec2_t const* sc)
{
    cp_mat3_rot_unit(&r->b, v, sc);
    CP_ZERO(&r->w);
}

static inline void cp_mat4_rot_unit(
    cp_mat4_t * r,
    cp_vec3_t const* u,
    cp_vec2_t const* sc)
{
    CP_ZERO(r);
    r->m[3][3] = 1;
    cp_dim3_rot_unit(&r->row[0].b, &r->row[1].b, &r->row[2].b, u, sc);
}

static inline void cp_mat3_rot_unit_into_z(
    cp_mat3_t * r,
    cp_vec3_t const* u)
{
    cp_dim3_rot_unit_into_z(&r->row[0], &r->row[1], &r->row[2], u);
}

static inline void cp_mat3w_rot_unit_into_z(
    cp_mat3w_t * r,
    cp_vec3_t const* v)
{
    cp_mat3_rot_unit_into_z(&r->b, v);
    CP_ZERO(&r->w);
}

static inline void cp_mat4_rot_unit_into_z(
    cp_mat4_t * r,
    cp_vec3_t const* u)
{
    CP_ZERO(r);
    r->m[3][3] = 1;
    cp_dim3_rot_unit_into_z(&r->row[0].b, &r->row[1].b, &r->row[2].b, u);
}

static inline void cp_mat2w_rot_ij(
    cp_mat2w_t * r,
    size_t i,
    size_t j,
    cp_vec2_t const* sc)
{
    cp_mat2_rot_ij(&r->b, i, j, sc);
    CP_ZERO(&r->w);
}

static inline void cp_mat3w_rot_ij(
    cp_mat3w_t * r,
    size_t i,
    size_t j,
    cp_vec2_t const* sc)
{
    cp_mat3_rot_ij(&r->b, i, j, sc);
    CP_ZERO(&r->w);
}

static inline void cp_mat3_rot_x(
    cp_mat3_t * r,
    cp_vec2_t const* sc)
{
    cp_mat3_rot_ij(r, 1, 2, sc);
}

static inline void cp_mat3w_rot_x(
    cp_mat3w_t * r,
    cp_vec2_t const* sc)
{
    cp_mat3_rot_x(&r->b, sc);
    CP_ZERO(&r->w);
}

static inline void cp_mat4_rot_x(
    cp_mat4_t * r,
    cp_vec2_t const* sc)
{
    cp_mat4_rot_ij(r, 1, 2, sc);
}

static inline void cp_mat3_rot_y(
    cp_mat3_t * r,
    cp_vec2_t const* sc)
{
    cp_mat3_rot_ij(r, 2, 0, sc);
}

static inline void cp_mat3w_rot_y(
    cp_mat3w_t * r,
    cp_vec2_t const* sc)
{
    cp_mat3_rot_y(&r->b, sc);
    CP_ZERO(&r->w);
}

static inline void cp_mat4_rot_y(
    cp_mat4_t * r,
    cp_vec2_t const* sc)
{
    cp_mat4_rot_ij(r, 2, 0, sc);
}

static inline void cp_mat2_rot(
    cp_mat2_t * r,
    cp_vec2_t const* sc)
{
    cp_mat2_rot_ij(r, 0, 1, sc);
}

static inline void cp_mat2w_rot(
    cp_mat2w_t * r,
    cp_vec2_t const* sc)
{
    cp_mat2_rot(&r->b, sc);
    CP_ZERO(&r->w);
}

static inline void cp_mat3_rot_z(
    cp_mat3_t * r,
    cp_vec2_t const* sc)
{
    cp_mat3_rot_ij(r, 0, 1, sc);
}

static inline void cp_mat3w_rot_z(
    cp_mat3w_t * r,
    cp_vec2_t const* sc)
{
    cp_mat3_rot_z(&r->b, sc);
    CP_ZERO(&r->w);
}

static inline void cp_mat4_rot_z(
    cp_mat4_t * r,
    cp_vec2_t const* sc)
{
    cp_mat4_rot_ij(r, 0, 1, sc);
}

static inline void cp_mat2_mirror_unit(
    cp_mat2_t * r,
    cp_vec2_t const* u)
{
    cp_dim2_mirror_unit(&r->row[0], &r->row[1], u);
}

static inline void cp_mat2w_mirror_unit(
    cp_mat2w_t * r,
    cp_vec2_t const* v)
{
    cp_mat2_mirror_unit(&r->b, v);
    CP_ZERO(&r->w);
}

static inline void cp_mat3_mirror_unit(
    cp_mat3_t * r,
    cp_vec3_t const* u)
{
    cp_dim3_mirror_unit(&r->row[0], &r->row[1], &r->row[2], u);
}

static inline void cp_mat3w_mirror_unit(
    cp_mat3w_t * r,
    cp_vec3_t const* v)
{
    cp_mat3_mirror_unit(&r->b, v);
    CP_ZERO(&r->w);
}

static inline void cp_mat4_mirror_unit(
    cp_mat4_t * r,
    cp_vec3_t const* u)
{
    CP_ZERO(r);
    r->m[3][3] = 1;
    cp_dim3_mirror_unit(&r->row[0].b, &r->row[1].b, &r->row[2].b, u);
}

static inline void cp_mat2w_xlat(
    cp_mat2w_t * r,
    double x,
    double y)
{
    cp_mat2_unit(&r->b);
    r->w = CP_VEC2(x, y);
}

static inline void cp_mat2wi_xlat(
    cp_mat2wi_t * r,
    double x,
    double y)
{
    cp_mat2w_xlat(&r->n, x, y);
    cp_mat2w_xlat(&r->i, -x, -y);
    r->d = 1;
}

static inline void cp_mat3_xlat(
    cp_mat3_t * r,
    double x,
    double y)
{
    cp_mat3_unit(r);
    r->m[0][2] = x;
    r->m[1][2] = y;
}

static inline void cp_mat3w_xlat(
    cp_mat3w_t * r,
    double x,
    double y,
    double z)
{
    cp_mat3_unit(&r->b);
    r->w = CP_VEC3(x, y, z);
}

static inline void cp_mat3i_xlat(
    cp_mat3i_t * r,
    double x,
    double y)
{
    cp_mat3_xlat(&r->n, x, y);
    cp_mat3_xlat(&r->i, -x, -y);
    r->d = 1;
}

static inline void cp_mat3wi_xlat(
    cp_mat3wi_t * r,
    double x,
    double y,
    double z)
{
    cp_mat3w_xlat(&r->n, x, y, z);
    cp_mat3w_xlat(&r->i, -x, -y, -z);
    r->d = 1;
}

static inline void cp_mat4_xlat(
    cp_mat4_t * r,
    double x,
    double y,
    double z)
{
    cp_mat4_unit(r);
    r->m[0][3] = x;
    r->m[1][3] = y;
    r->m[2][3] = z;
}

static inline void cp_mat4i_xlat(
    cp_mat4i_t * r,
    double x,
    double y,
    double z)
{
    cp_mat4_xlat(&r->n, x, y, z);
    cp_mat4_xlat(&r->i, -x, -y, -z);
    r->d = 1;
}

static inline void cp_mat2w_xlat_v(
    cp_mat2w_t * r,
    cp_vec2_t const* v)
{
    cp_mat2w_xlat(r, v->x, v->y);
}

static inline void cp_mat2wi_xlat_v(
    cp_mat2wi_t * r,
    cp_vec2_t const* v)
{
    cp_mat2wi_xlat(r, v->x, v->y);
}

static inline void cp_mat3_xlat_v(
    cp_mat3_t * r,
    cp_vec2_t const* v)
{
    cp_mat3_xlat(r, v->x, v->y);
}

static inline void cp_mat3w_xlat_v(
    cp_mat3w_t * r,
    cp_vec3_t const* v)
{
    cp_mat3w_xlat(r, v->x, v->y, v->z);
}

static inline void cp_mat3i_xlat_v(
    cp_mat3i_t * r,
    cp_vec2_t const* v)
{
    cp_mat3i_xlat(r, v->x, v->y);
}

static inline void cp_mat3wi_xlat_v(
    cp_mat3wi_t * r,
    cp_vec3_t const* v)
{
    cp_mat3wi_xlat(r, v->x, v->y, v->z);
}

static inline void cp_mat4_xlat_v(
    cp_mat4_t * r,
    cp_vec3_t const* v)
{
    cp_mat4_xlat(r, v->x, v->y, v->z);
}

static inline void cp_mat4i_xlat_v(
    cp_mat4i_t * r,
    cp_vec3_t const* v)
{
    cp_mat4i_xlat(r, v->x, v->y, v->z);
}

static inline double cp_mat2i_det(
    cp_mat2i_t const* a)
{
    return a->d;
}

static inline double cp_mat2wi_det(
    cp_mat2wi_t const* a)
{
    return a->d;
}

static inline double cp_mat3i_det(
    cp_mat3i_t const* a)
{
    return a->d;
}

static inline double cp_mat3wi_det(
    cp_mat3wi_t const* a)
{
    return a->d;
}

static inline double cp_mat4i_det(
    cp_mat4i_t const* a)
{
    return a->d;
}

#endif /* CP_MAT_GEN_INL_H_ */
