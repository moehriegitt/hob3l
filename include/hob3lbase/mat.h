/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_MAT_H
#define __CP_MAT_H

#include <hob3lbase/mat_tam.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/arith.h>
#include <hob3lbase/mat_gen_ext.h>
#include <hob3lbase/mat_gen_inl.h>
#include <hob3lbase/mat_is_rot.h>

#define CP_SINCOS_RAD(a) (&(cp_vec2_t){ .v={ sin(a), cos(a) }})
#define CP_SINCOS_DEG(a) (&(cp_vec2_t){ .v={ cp_sin_deg(a), cp_cos_deg(a) }})

/**
 * Cross product (which is right-handed, see above)
 */
extern void cp_vec3_cross(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *b);

/**
 * cross(a - o, b - o)
 */
extern void cp_vec3_right_cross3(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b);

/** unit(cross(a,b)), returns success (i.e., whether vector is not len0) */
extern bool cp_vec3_normal(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *b);

/** unit(cross3(a,o,b)), returns success (i.e., whether vector is not len0) */
extern bool cp_vec3_right_normal3(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b);

/** 4D matrix determinant */
extern cp_dim_t cp_mat4_det(
    cp_mat4_t const *m);

/** 4D matrix inverse, returns the determinant. */
extern double cp_mat4_inv(
    cp_mat4_t *r,
    cp_mat4_t const *m);

/** 3D matrix determinant */
extern cp_dim_t cp_mat3_det(
    cp_mat3_t const *m);

/** Determinant of 3D matrix with translation vector */
extern cp_dim_t cp_mat3w_det(
    cp_mat3w_t const *m);

/** 3D matrix inverse, returns the determinant. */
extern double cp_mat3_inv(
    cp_mat3_t *r,
    cp_mat3_t const *m);

/** 2D matrix determinant */
extern cp_dim_t cp_mat2_det(
    cp_mat2_t const *m);

/** Determinant of 2D matrix with translation vector */
extern cp_dim_t cp_mat2w_det(
    cp_mat2w_t const *m);

/** 3D matrix inverse, returns the determinant */
extern double cp_mat2_inv(
    cp_mat2_t *r,
    cp_mat2_t const *m);

/** Inverse of 3D matrix with translation vector, returns the determinant */
extern double cp_mat3w_inv(
    cp_mat3w_t *r,
    cp_mat3w_t const *m);

/** Inverse of 2D matrix with translation vector, returns the determinant */
extern double cp_mat2w_inv(
    cp_mat2w_t *r,
    cp_mat2w_t const *m);

/** Copy 4D matrix into 3D matrix with translation vector, if possible */
extern bool cp_mat3w_from_mat4(
    cp_mat3w_t *r,
    cp_mat4_t const *q);

/** Copy 3D matrix into 2D matrix with translation vector, if possible */
extern bool cp_mat2w_from_mat3(
    cp_mat2w_t *r,
    cp_mat3_t const *q);

/** Copy 3D matrix with translation vector into 4D matrix */
extern void cp_mat4_from_mat3w(
    cp_mat4_t *r,
    cp_mat3w_t const *q);

/** Copy 2D matrix with translation vector into 3D matrix */
extern void cp_mat3_from_mat2w(
    cp_mat3_t *r,
    cp_mat2w_t const *q);

/** Copy 4D matrix w/ inverse into 3D matrix w/ translation vector and inverse, if possible */
extern bool cp_mat3wi_from_mat4i(
    cp_mat3wi_t *r,
    cp_mat4i_t const *q);

/** Copy 3D matrix w/ inverse into 2D matrix w/ translation vector and inverse, if possible */
extern bool cp_mat2wi_from_mat3i(
    cp_mat2wi_t *r,
    cp_mat3i_t const *q);

/** Copy 3D matrix w/ inverse into 4D matrix w/ inverse */
extern void cp_mat4i_from_mat3wi(
    cp_mat4i_t *r,
    cp_mat3wi_t const *q);

/** Copy 2D matrix w/ inverse into 3D matrix w/ inverse */
extern void cp_mat3i_from_mat2wi(
    cp_mat3i_t *r,
    cp_mat2wi_t const *q);

/** Copy 2D matrix into 2D matrix w/ inverse, if possible */
extern bool cp_mat2i_from_mat2(
    cp_mat2i_t *r,
    cp_mat2_t const *q);

/** Copy 3D matrix into 3D matrix w/ inverse, if possible */
extern bool cp_mat3i_from_mat3(
    cp_mat3i_t *r,
    cp_mat3_t const *q);

/** Copy 4D matrix into 4D matrix w/ inverse, if possible */
extern bool cp_mat4i_from_mat4(
    cp_mat4i_t *r,
    cp_mat4_t const *q);

/**
 * Copy 2D matrix w/ translation vector into 2D matrix w/ inverse &
 * translation vector, if possible
 */
extern bool cp_mat2wi_from_mat2w(
    cp_mat2wi_t *r,
    cp_mat2w_t const *q);

/**
 * Copy 3D matrix w/ translation vector into 3D matrix w/ inverse &
 * translation vector, if possible
 */
extern bool cp_mat3wi_from_mat3w(
    cp_mat3wi_t *r,
    cp_mat3w_t const *q);

/**
 * Three points p1,p2,p3 are in one line, i.e. p1--p2 is collinear with p2--p3?
 *
 *  (x1,y1) = p1
 *  (x2,y2) = p2
 *  (x3,y3) = p3
 *
 *  Criterion: Slopes are equal.  (We already know that p2 is on both edges, so no
 *  parallelism is possible.)
 */
extern bool cp_vec2_in_line(
    cp_vec2_t const *p1,
    cp_vec2_t const *p2,
    cp_vec2_t const *p3);

/**
 * Whether tree points in 3D are in one line.
 * Criterion: slopes in XY, YZ and ZX planes.
 */
extern bool cp_vec3_in_line(
    cp_vec3_t const *p1,
    cp_vec3_t const *p2,
    cp_vec3_t const *p3);

/**
 * Port side direction of the vector a-->b (i.e., the non-normalised 'normal')
 */
static inline void cp_vec2_port(
    cp_vec2_t *r,
    cp_vec2_t const *a,
    cp_vec2_t const *b)
{
    CP_INIT(r, b->y - a->y, a->x - b->x);
}

/** unit(left(a,b)) */
static inline void cp_vec2_normal(
    cp_vec2_t *r,
    cp_vec2_t const *a,
    cp_vec2_t const *b)
{
    cp_vec2_port(r, a, b);
    cp_vec2_unit(r, r);
}

/**
 * Right handed (=standard) z coordinate of cross product
 */
static inline cp_f_t cp_cross_z(cp_f_t ax, cp_f_t ay, cp_f_t bx, cp_f_t by)
{
    return (ax * by) - (ay * bx);
}

/**
 * Sign of cp_right_z(), computed considering epsilon.
 * This returns -1, 0, +1.
 */
static inline int cp_normal_z(double ax, double ay, double bx, double by)
{
    return cp_sqr_cmp(ax * by, ay * bx);
}

/**
 * Cross product Z component of two vectors in Z=0 plane.
 * Note:
 * This is right-handed by the a=index, b=middle, r=thumb rule,
 * the standard cross product.
 */
static inline cp_f_t cp_vec2_cross_z(
    cp_vec2_t const *a,
    cp_vec2_t const *b)
{
    return cp_cross_z(a->x, a->y, b->x, b->y);
}

/**
 * Sign of cp_vec2_cross_z().
 * This returns -1, 0, +1.
 */
static inline int cp_vec2_normal_z(
    cp_vec2_t const *a,
    cp_vec2_t const *b)
{
    return cp_normal_z(a->x, a->y, b->x, b->y);
}

/**
 * = cross_z (a - o, b - o)
 * Note: o is the center vertex of a three point path: a-o-b
 *
 * This gives negative z values when vertices of a convex polygon in
 * the x-y plane are passed clockwise to this functions as a,o,b.
 * E.g. (1,0,0), (0,0,0), (0,1,0) gives (0,0,-1).  E.g., the cross3
 * product is right-handed.
 */
static inline cp_f_t cp_vec2_right_cross3_z(
    cp_vec2_t const *a,
    cp_vec2_t const *o,
    cp_vec2_t const *b)
{
    return cp_cross_z(a->x - o->x, a->y - o->y, b->x - o->x, b->y - o->y);
}

/**
 * Opposite of cp_vec2_right_cross3_z()
 */
static inline cp_f_t cp_vec2_left_cross3_z(
    cp_vec2_t const *a,
    cp_vec2_t const *o,
    cp_vec2_t const *b)
{
    return cp_vec2_right_cross3_z(b,o,a);
}

/**
 * Z component of normalised 3D cross product, right-handed.
 */
static inline int cp_vec2_right_normal3_z(
    cp_vec2_t const *a,
    cp_vec2_t const *o,
    cp_vec2_t const *b)
{
    return cp_normal_z(a->x - o->x, a->y - o->y, b->x - o->x, b->y - o->y);
}

/**
 * Z component of normalised 3D cross product, left-handed.
 */
static inline int cp_vec2_left_normal3_z(
    cp_vec2_t const *a,
    cp_vec2_t const *o,
    cp_vec2_t const *b)
{
    return cp_vec2_right_normal3_z(b,o,a);
}

/**
 * Opposite of cp_vec3_right_cross3()
 */
static inline void cp_vec3_left_cross3(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b)
{
    return cp_vec3_right_cross3(r,b,o,a);
}

/**
 * Opposite of cp_vec3_right_normal3()
 */
static inline bool cp_vec3_left_normal3(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b)
{
    return cp_vec3_right_normal3(r,b,o,a);
}

#endif /* __CP_MAT_H */
