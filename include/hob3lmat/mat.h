/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_MAT_H_
#define CP_MAT_H_

#include <hob3lmat/mat_tam.h>
#include <hob3lmat/algo.h>
#include <hob3lmat/mat_gen_ext.h>
#include <hob3lmat/mat_gen_inl.h>
#include <hob3lmat/mat_is_rot.h>

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
 * This gives positive z values when vertices of a convex polygon in
 * the x-y plane are passed clockwise to this functions as a,o,b.
 * E.g. (1,0), (0,0), (0,1) gives +1: (1-0)*(1-0) - (0-0)*(0-0).
 * E.g., the cross3_z product is right-handed (the normal cross
 * product).
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

static inline void cp_vec3_div_x(
    cp_vec3_t * r,
    cp_vec3_t const* a,
    double b)
{
    r->v[0] = a->v[0] / b;
    r->v[1] = a->v[1] / b;
    r->v[2] = a->v[2] / b;
}

static inline bool cp_vec3_unit_x(
    cp_vec3_t * r,
    cp_vec3_t const* a)
{
    double l = cp_vec3_len(a);
    cp_vec3_div_x(r, a, l);
    return cp_isfinite(r->x) && cp_isfinite(r->y) && cp_isfinite(r->z);
}

/** unit(cross(a,b)), returns success (i.e., whether vector is not len0)
 * Like cp_vec3_normal() with the same difference as between cp_vec3_right_normal3()
 * and cp_vec3_right_normal3_x(): this is pure floating point.  Prefer if
 * precision is more important than corner cases.
 */
static inline bool cp_vec3_normal_x(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *b)
{
    cp_vec3_cross(r, a, b);
    return cp_vec3_unit_x(r,r);
}

/**
 * unit(cross3(a,o,b)), returns success (i.e., whether vector is not len0)
 * This uses no rounding to check for unity, i.e., this is pure floatinng
 * point arith.  Use this if max. precision is more important than
 * comparing to 0, i.e., than finding corner cases.
 *
 * This returns whether all coordinates are finite.
 */
static inline bool cp_vec3_right_normal3_x(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b)
{
    cp_vec3_t ao;
    cp_vec3_sub(&ao, a, o);
    cp_vec3_t bo;
    cp_vec3_sub(&bo, b, o);
    return cp_vec3_normal_x(r, &ao, &bo);
}

static inline bool cp_vec3_left_normal3_x(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b)
{
    return cp_vec3_right_normal3_x(r, b, o, a);
}

#endif /* CP_MAT_H_ */
