/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_MAT_H
#define __CP_MAT_H

#include <cpmat/mat_tam.h>
#include <cpmat/vec.h>
#include <cpmat/arith.h>
#include <cpmat/mat_gen_ext.h>
#include <cpmat/mat_gen_inl.h>

#define CP_SINCOS_RAD(a) (&(cp_vec2_t){ .v={ sin(a), cos(a) }})
#define CP_SINCOS_DEG(a) (&(cp_vec2_t){ .v={ cp_sin_deg(a), cp_cos_deg(a) }})

extern bool cp_mat3_is_rect_rot(
    cp_mat3_t const *m);

/* ** "D special stuff ** */

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
    return cp_cmp(ax * by, ay * bx);
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

/** = cross_z (a - o, b - o)
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

static inline cp_f_t cp_vec2_left_cross3_z(
    cp_vec2_t const *a,
    cp_vec2_t const *o,
    cp_vec2_t const *b)
{
    return cp_vec2_right_cross3_z(b,o,a);
}

static inline int cp_vec2_right_normal3_z(
    cp_vec2_t const *a,
    cp_vec2_t const *o,
    cp_vec2_t const *b)
{
    return cp_normal_z(a->x - o->x, a->y - o->y, b->x - o->x, b->y - o->y);
}

static inline int cp_vec2_left_normal3_z(
    cp_vec2_t const *a,
    cp_vec2_t const *o,
    cp_vec2_t const *b)
{
    return cp_vec2_right_normal3_z(b,o,a);
}

/* ** 3D special stuff ** */

/** cross product (which is right-handed, see above) */
extern void cp_vec3_cross(
    cp_vec3_t *result,
    cp_vec3_t const *a,
    cp_vec3_t const *b);

/** cross(a - o, b - o) */
extern void cp_vec3_right_cross3(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b);

static inline void cp_vec3_left_cross3(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b)
{
    return cp_vec3_right_cross3(r,b,o,a);
}

/** unit(cross(a,b)), returns success (i.e., whether vector is not len0) */
extern __wur bool cp_vec3_normal(
    cp_vec3_t *result,
    cp_vec3_t const *a,
    cp_vec3_t const *b);

/** unit(cross3(a,o,b)), returns success (i.e., whether vector is not len0) */
extern __wur bool cp_vec3_right_normal3(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b);

static inline __wur bool cp_vec3_left_normal3(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b)
{
    return cp_vec3_right_normal3(r,b,o,a);
}

/**
 * Whether three points are in one line, i.e., whether p1--p2 and
 * p2--p3 are collinear.  2D variant.
 */
extern bool cp_vec2_in_line(
    cp_vec2_t const *p1,
    cp_vec2_t const *p2,
    cp_vec2_t const *p3);

/**
 * Whether three points are in one line, i.e., whether p1--p2 and
 * p2--p3 are collinear.  3D variant.
 */
extern bool cp_vec3_in_line(
    cp_vec3_t const *p1,
    cp_vec3_t const *p2,
    cp_vec3_t const *p3);

/**
 * Matrix copy */
extern bool cp_mat3w_from_mat4(
    cp_mat3w_t *r,
    cp_mat4_t const *q);

extern bool cp_mat2w_from_mat3(
    cp_mat2w_t *r,
    cp_mat3_t const *q);

extern void cp_mat4_from_mat3w(
    cp_mat4_t *r,
    cp_mat3w_t const *q);

extern void cp_mat3_from_mat2w(
    cp_mat3_t *r,
    cp_mat2w_t const *q);

extern bool cp_mat3wi_from_mat4i(
    cp_mat3wi_t *r,
    cp_mat4i_t const *q);

extern bool cp_mat2wi_from_mat3i(
    cp_mat2wi_t *r,
    cp_mat3i_t const *q);

extern void cp_mat4i_from_mat3wi(
    cp_mat4i_t *r,
    cp_mat3wi_t const *q);

extern void cp_mat3i_from_mat2wi(
    cp_mat3i_t *r,
    cp_mat2wi_t const *q);

extern bool cp_mat2i_from_mat2(
    cp_mat2i_t *r,
    cp_mat2_t const *q);

extern bool cp_mat3i_from_mat3(
    cp_mat3i_t *r,
    cp_mat3_t const *q);

extern bool cp_mat4i_from_mat4(
    cp_mat4i_t *r,
    cp_mat4_t const *q);

extern bool cp_mat2wi_from_mat2w(
    cp_mat2wi_t *r,
    cp_mat2w_t const *q);

extern bool cp_mat3wi_from_mat3w(
    cp_mat3wi_t *r,
    cp_mat3w_t const *q);

#endif /* __CP_MAT_H */
