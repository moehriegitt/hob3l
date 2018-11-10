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

/**
 * Determinant of 4D matrix
 */
extern cp_dim_t cp_mat4_det(
    cp_mat4_t const *m);

/**
 * Invserse of 4D matrix inverse.
 *
 * Returns the determinant.
 */
extern double cp_mat4_inv(
    cp_mat4_t *r,
    cp_mat4_t const *m);

/** Determinant of 3D matrix. */
extern cp_dim_t cp_mat3_det(
    cp_mat3_t const *m);

/**
 * Determinant of 3D matrix with translation vector */
extern cp_dim_t cp_mat3w_det(
    cp_mat3w_t const *m);

/**
 * Inverse of 3D matrix.
 *
 * Returns the determinant.
 */
extern double cp_mat3_inv(
    cp_mat3_t *r,
    cp_mat3_t const *m);

/** Determinant of 2D matrix. */
extern cp_dim_t cp_mat2_det(
    cp_mat2_t const *m);

/** Determinant of 2D matrix with translation vector */
extern cp_dim_t cp_mat2w_det(
    cp_mat2w_t const *m);

/**
 * Inverse of 2D matrix.
 *
 * Returns the determinant.
 */
extern double cp_mat2_inv(
    cp_mat2_t *r,
    cp_mat2_t const *m);

/**
 * Inverse of 3D matrix with translation vector.
 *
 * Returns the determinant.
 */
extern double cp_mat3w_inv(
    cp_mat3w_t *r,
    cp_mat3w_t const *m);

/**
 * Inverse of 2D matrix with translation vector.
 *
 * Returns the determinant.
 */
extern double cp_mat2w_inv(
    cp_mat2w_t *r,
    cp_mat2w_t const *m);

/**
 * Copy 4D matrix into 3D matrix with translation vector.
 *
 * The translation is taken from the 4th column.
 *
 * Return whether the last row is {0,0,0,1}.
 *
 * IN:
 * | a b c d |
 * | e f g h |
 * | i j k l |
 * | m n o p |
 *
 * OUT:
 * | a b c | d |
 * | e f g | h |
 * | i j k | l |
 * return m,n,o=0 && p == 1
 */
extern bool cp_mat3w_from_mat4(
    cp_mat3w_t *r,
    cp_mat4_t const *q);

/**
 * Copy 3D matrix into 2D matrix with translation vector.
 *
 * The translation is taken from the 3rd column.
 *
 * Return whether the last row is {0,0,1}.
 *
 * IN:
 * | a b c |
 * | d e f |
 * | g h i |
 *
 * OUT:
 * | a b | c |
 * | d e | f |
 * return g,h=0 && i == 1
 */
extern bool cp_mat2w_from_mat3(
    cp_mat2w_t *r,
    cp_mat3_t const *q);

/**
 * Copy 3D matrix into 2D matrix with translation vector.
 *
 * Return whether the 3rd row and column are both {0,0,1} and
 * the z translation is 0.
 *
 * IN:
 * | a b c | d |
 * | e f g | h |
 * | i j k | l |
 *
 * OUT:
 * | a b | d |
 * | d e | h |
 * return i,j,c,g,l == 0 && k == 1
 */
extern bool cp_mat2w_from_mat3w(
    cp_mat2w_t *r,
    cp_mat3w_t const *q);

/**
 * Copy 3D matrix with translation vector into 4D matrix.
 *
 * This puts the translation into the 4th column.
 *
 * Fills the additional row trivially from the unit matrix.
 *
 * IN:
 * | a b c | d |
 * | e f g | h |
 * | i j k | l |
 *
 * OUT:
 * | a b c d |
 * | e f g h |
 * | i j k l |
 * | 0 0 0 1 |
 */
extern void cp_mat4_from_mat3w(
    cp_mat4_t *r,
    cp_mat3w_t const *q);

/**
 * Copy 2D matrix with translation vector into 3D matrix.
 *
 * This puts the translation into the 3rd column.
 *
 * Fills the additional row trivially from the unit matrix.
 *
 * IN:
 * | a b | c |
 * | d e | f |
 *
 * OUT:
 * | a b c |
 * | d e f |
 * | 0 0 1 |
 */
extern void cp_mat3_from_mat2w(
    cp_mat3_t *r,
    cp_mat2w_t const *q);

/**
 * Copy 2D matrix with translation vector into 3D matrix with translation.
 *
 * Fills the additional rows and columns trivially from the unit matrix.
 *
 * IN:
 * | a b | c |
 * | d e | f |
 *
 * OUT:
 * | a b 0 | c |
 * | d e 0 | f |
 * | 0 0 1 | 0 |
 */
extern void cp_mat3w_from_mat2w(
    cp_mat3w_t *r,
    cp_mat2w_t const *q);

/**
 * Copy 4D matrix w/ inverse into 3D matrix w/ translation vector and inverse.
 *
 * The translation is taken from the 4th column.
 *
 * This recomputes the determinant from the resulting matrix, because
 * a column significnat for the determinant is reinterpreted as a
 * translation.
 *
 * Returns whether the last row is {0,0,0,1}.
 *
 * IN:
 * n=| a b c d |    i=| A B C D |    d=Q
 *   | e f g h |      | E F G H |
 *   | i j k l |      | I J K L |
 *   | m n o p |      | M N O P |
 *
 * OUT:
 * n=| a b c | d |  i=| A B C | D |  d=(recomputed)
 *   | e f g | h |    | E F G | H |
 *   | i j k | l |    | I J K | L |
 * return m,n,o=0 && p == 1 && M,N,O=0 && P == 1
 */
extern bool cp_mat3wi_from_mat4i(
    cp_mat3wi_t *r,
    cp_mat4i_t const *q);

/**
 * Copy 3D matrix w/ inverse into 2D matrix w/ translation vector and inverse, if possible.
 *
 * The translation is taken from the 3rd column.
 *
 * This recomputes the determinant from the resulting matrix, because
 * a column significnat for the determinant is reinterpreted as a
 * translation.
 *
 * Returns whether the last row is {0,0,1}.
 *
 * IN:
 * n=| a b c |    i=| A B C |    d=Q
 *   | e f g |      | E F G |
 *   | i j k |      | I J K |
 *
 * OUT:
 * n=| a b | c |  i=| A B | C |  d=(recomputed)
 *   | e f | g |    | E F | G |
 * return i,j=0 && k == 1 && I,J=0 && K == 1
 */
extern bool cp_mat2wi_from_mat3i(
    cp_mat2wi_t *r,
    cp_mat3i_t const *q);

/**
 * Copy 3D matrix w/ translation and inverse into 2D matrix w/ translation and inverse.
 *
 * Return whether the 3rd row and column are both {0,0,1} and
 * the z translation is 0.
 *
 * IN:
 * n=| a b c | d |  i=| A B C | D |  d=Q
 *   | e f g | h |    | E F G | H |
 *   | i j k | l |    | I J K | L |
 *
 * OUT:
 * n=| a b | d |    i=| A B | D |    d=(recomputed)
 *   | d e | h |      | D E | H |
 * return i,j,c,g,l == 0 && k == 1 && I,J,C,G,L == 0 && K == 1
 */
extern bool cp_mat2wi_from_mat3wi(
    cp_mat2wi_t *r,
    cp_mat3wi_t const *q);

/**
 * Copy 3D matrix w/ inverse into 4D matrix w/ inverse.
 *
 * Fills the additional columns trivially from the unit matrix.
 *
 * IN:
 * n=| a b c | d |  i=| A B C | D |  d=Q
 *   | e f g | h |    | E F G | H |
 *   | i j k | l |    | I J K | L |
 *
 * OUT:
 * n=| a b c d |    i=| A B C D |    d=Q
 *   | e f g h |      | E F G H |
 *   | i j k l |      | I J K L |
 *   | 0 0 0 1 |      | 0 0 0 1 |
 */
extern void cp_mat4i_from_mat3wi(
    cp_mat4i_t *r,
    cp_mat3wi_t const *q);

/**
 * Copy 2D matrix w/ inverse into 3D matrix w/ inverse.
 *
 * Fills the additional columns trivially from the unit matrix.
 *
 * IN:
 * n=| a b | d |  i=| A B | D |  d=Q
 *   | e f | h |    | E F | H |
 *
 * OUT:
 * n=| a b d |    i=| A B D |    d=Q
 *   | e f h |      | E F H |
 *   | 0 0 1 |      | 0 0 1 |
 */
extern void cp_mat3i_from_mat2wi(
    cp_mat3i_t *r,
    cp_mat2wi_t const *q);

/**
 * Copy 2D matrix with translation vector into 3D matrix with translation.
 *
 * Fills the additional rows and columns trivially from the unit matrix.
 *
 * IN:
 * n=| a b | c |    i=| A B | C |    d=Q
 *   | d e | f |      | D E | F |
 *
 * OUT:
 * n=| a b 0 | c |  i=| A B 0 | C |  d=Q
 *   | d e 0 | f |    | D E 0 | F |
 *   | 0 0 1 | 0 |    | 0 0 1 | 0 |
 */
extern void cp_mat3wi_from_mat2wi(
    cp_mat3wi_t *r,
    cp_mat2wi_t const *q);

/**
 * Copy 2D matrix into 2D matrix w/ inverse, if possible.
 *
 * Fills the additional columns trivially from the unit matrix.
 */
extern bool cp_mat2i_from_mat2(
    cp_mat2i_t *r,
    cp_mat2_t const *q);

/**
 * Copy 3D matrix into 3D matrix w/ inverse, if possible.
 *
 * This adds the inverse and returns whether the determinant is non-0.
 */
extern bool cp_mat3i_from_mat3(
    cp_mat3i_t *r,
    cp_mat3_t const *q);

/**
 * Copy 4D matrix into 4D matrix w/ inverse, if possible.
 *
 * This adds the inverse and returns whether the determinant is non-0.
 */
extern bool cp_mat4i_from_mat4(
    cp_mat4i_t *r,
    cp_mat4_t const *q);

/**
 * Copy 2D matrix w/ translation vector into 2D matrix w/ inverse &
 * translation vector, if possible
 *
 * This adds the inverse and returns whether the determinant is non-0.
 */
extern bool cp_mat2wi_from_mat2w(
    cp_mat2wi_t *r,
    cp_mat2w_t const *q);

/**
 * Copy 3D matrix w/ translation vector into 3D matrix w/ inverse &
 * translation vector, if possible
 *
 * This adds the inverse and returns whether the determinant is non-0.
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
 * cp_v_vec2_loc_t nth function for cp_vec2_arr_ref_t
 */
extern cp_vec2_t *_cp_v_vec2_loc_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i);

/**
 * cp_v_vec2_loc_t idx function for cp_vec2_arr_ref_t
 */
extern size_t _cp_v_vec2_loc_idx(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a);

/**
 * cp_v_vec3_loc_t nth function for cp_vec2_arr_ref_t, XY plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_xy_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i);

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, XY plane
 */
extern size_t _cp_v_vec3_loc_xy_idx(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a);

/**
 * cp_v_vec3_loc_ref_t nth function for cp_vec2_arr_ref_t, XY plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_ref_xy_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i);

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, XY plane
 */
extern size_t _cp_v_vec3_loc_ref_xy_idx(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a);

/**
 * cp_v_vec3_loc_ref_t nth function for cp_vec2_arr_ref_t, YZ plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_ref_yz_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i);

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, YZ plane
 */
extern size_t _cp_v_vec3_loc_ref_yz_idx(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a);

/**
 * Port side direction of the vector a-->b (i.e., the non-normalised 'normal')
 */
static inline void cp_vec2_port(
    cp_vec2_t *r,
    cp_vec2_t const *a,
    cp_vec2_t const *b)
{
    *r = CP_VEC2(b->y - a->y, a->x - b->x);
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

static inline cp_vec2_t *cp_vec2_arr_ref(
    cp_vec2_arr_ref_t const *a,
    size_t i)
{
    return a->nth(a, i);
}

static inline size_t cp_vec2_arr_idx(
    cp_vec2_arr_ref_t const *a,
    cp_vec2_t const *p)
{
    return a->idx(a, p);
}

/**
 * Convert to vec2 array.
 */
static inline void cp_vec2_arr_ref_from_v_vec2_loc(
    cp_vec2_arr_ref_t *a,
    cp_v_vec2_loc_t const *v)
{
    a->nth = _cp_v_vec2_loc_nth;
    a->idx = _cp_v_vec2_loc_idx;
    a->user1 = v;
    a->user2 = NULL;
}

/**
 * Convert to vec2 array.
 */
static inline void cp_vec2_arr_ref_from_a_vec3_loc_xy(
    cp_vec2_arr_ref_t *a,
    cp_a_vec3_loc_t const *v)
{
    a->nth = _cp_v_vec3_loc_xy_nth;
    a->idx = _cp_v_vec3_loc_xy_idx;
    a->user1 = v;
    a->user2 = NULL;
}

/**
 * Convert to vec2 array.
 */
static inline void cp_vec2_arr_ref_from_a_vec3_loc_ref(
    cp_vec2_arr_ref_t *a,
    cp_a_vec3_loc_t const *v,
    cp_a_vec3_loc_ref_t const *w,
    bool yz_plane)
{
    a->nth = yz_plane ? _cp_v_vec3_loc_ref_yz_nth : _cp_v_vec3_loc_ref_xy_nth;
    a->idx = yz_plane ? _cp_v_vec3_loc_ref_yz_idx : _cp_v_vec3_loc_ref_xy_idx;
    a->user1 = v;
    a->user2 = w;
}

#endif /* __CP_MAT_H */
