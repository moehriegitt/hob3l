/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/mat.h>
#include <hob3lbase/arith.h>
#include <hob3lbase/mat_gen_tam.h>
#include <hob3lbase/mat_gen_ext.h>
#include <hob3lbase/mat_gen_inl.h>

#include <stdio.h>

/**
 * Cross product (which is right-handed, see above)
 */
extern void cp_vec3_cross(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *b)
{
    cp_vec3_t q[1];

#   define STEP(i,j,k) \
        q->v[i] = (a->v[j] * b->v[k]) - (a->v[k] * b->v[j])
    STEP(0,1,2);
    STEP(1,2,0);
    STEP(2,0,1);
#   undef STEP

    *r = *q;
}

/**
 * cross(a - o, b - o)
 */
extern void cp_vec3_right_cross3(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b)
{
    cp_vec3_t q[1];

#   define STEP(i,j,k) \
        q->v[i] = \
            ((a->v[j] - o->v[j]) * (b->v[k] - o->v[k])) - \
            ((a->v[k] - o->v[k]) * (b->v[j] - o->v[j]))
    STEP(0,1,2);
    STEP(1,2,0);
    STEP(2,0,1);
#   undef STEP

    *r = *q;
}

/** unit(cross(a,b)), returns success (i.e., whether vector is not len0) */
extern bool cp_vec3_normal(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *b)
{
    cp_vec3_cross(r, a, b);
    if (cp_vec3_unit(r,r)) {
        return true;
    }

    /* Hmm, maybe the vectors where just too short: roughly normalise the input
     * vectors first (roughly means: divide by the largest absolute value) */
    cp_vec3_t ra;
    cp_vec3_div(&ra, a, cp_vec3_max_abs_coord(a));
    cp_vec3_t rb;
    cp_vec3_div(&rb, b, cp_vec3_max_abs_coord(b));
    cp_vec3_cross(r, &ra, &rb);
    return cp_vec3_unit(r,r);
}

/** unit(cross3(a,o,b)), returns success (i.e., whether vector is not len0) */
extern bool cp_vec3_right_normal3(
    cp_vec3_t *r,
    cp_vec3_t const *a,
    cp_vec3_t const *o,
    cp_vec3_t const *b)
{
    cp_vec3_t ao;
    cp_vec3_sub(&ao, a, o);
    cp_vec3_t bo;
    cp_vec3_sub(&bo, b, o);
    return cp_vec3_normal(r, &ao, &bo);
}

/**
 * Determinant of 4D matrix
 */
extern cp_dim_t cp_mat4_det(
    cp_mat4_t const *m)
{
    cp_sqrdim_t b00 = (m->m[0][0] * m->m[1][1]) - (m->m[1][0] * m->m[0][1]);
    cp_sqrdim_t b01 = (m->m[0][0] * m->m[2][1]) - (m->m[2][0] * m->m[0][1]);
    cp_sqrdim_t b02 = (m->m[0][0] * m->m[3][1]) - (m->m[3][0] * m->m[0][1]);
    cp_sqrdim_t b03 = (m->m[1][0] * m->m[2][1]) - (m->m[2][0] * m->m[1][1]);
    cp_sqrdim_t b04 = (m->m[1][0] * m->m[3][1]) - (m->m[3][0] * m->m[1][1]);
    cp_sqrdim_t b05 = (m->m[2][0] * m->m[3][1]) - (m->m[3][0] * m->m[2][1]);
    cp_sqrdim_t b06 = (m->m[0][2] * m->m[1][3]) - (m->m[1][2] * m->m[0][3]);
    cp_sqrdim_t b07 = (m->m[0][2] * m->m[2][3]) - (m->m[2][2] * m->m[0][3]);
    cp_sqrdim_t b08 = (m->m[0][2] * m->m[3][3]) - (m->m[3][2] * m->m[0][3]);
    cp_sqrdim_t b09 = (m->m[1][2] * m->m[2][3]) - (m->m[2][2] * m->m[1][3]);
    cp_sqrdim_t b10 = (m->m[1][2] * m->m[3][3]) - (m->m[3][2] * m->m[1][3]);
    cp_sqrdim_t b11 = (m->m[2][2] * m->m[3][3]) - (m->m[3][2] * m->m[2][3]);

    cp_dim_t d = (b00 * b11) - (b01 * b10) + (b02 * b09) + (b03 * b08) - (b04 * b07) + (b05 * b06);
    return d;
}

/**
 * Invserse of 4D matrix inverse.
 *
 * Returns the determinant.
 */
extern double cp_mat4_inv(
    cp_mat4_t *r,
    cp_mat4_t const *m)
{
    cp_sqrdim_t b00 = (m->m[0][0] * m->m[1][1]) - (m->m[1][0] * m->m[0][1]);
    cp_sqrdim_t b01 = (m->m[0][0] * m->m[2][1]) - (m->m[2][0] * m->m[0][1]);
    cp_sqrdim_t b02 = (m->m[0][0] * m->m[3][1]) - (m->m[3][0] * m->m[0][1]);
    cp_sqrdim_t b03 = (m->m[1][0] * m->m[2][1]) - (m->m[2][0] * m->m[1][1]);
    cp_sqrdim_t b04 = (m->m[1][0] * m->m[3][1]) - (m->m[3][0] * m->m[1][1]);
    cp_sqrdim_t b05 = (m->m[2][0] * m->m[3][1]) - (m->m[3][0] * m->m[2][1]);
    cp_sqrdim_t b06 = (m->m[0][2] * m->m[1][3]) - (m->m[1][2] * m->m[0][3]);
    cp_sqrdim_t b07 = (m->m[0][2] * m->m[2][3]) - (m->m[2][2] * m->m[0][3]);
    cp_sqrdim_t b08 = (m->m[0][2] * m->m[3][3]) - (m->m[3][2] * m->m[0][3]);
    cp_sqrdim_t b09 = (m->m[1][2] * m->m[2][3]) - (m->m[2][2] * m->m[1][3]);
    cp_sqrdim_t b10 = (m->m[1][2] * m->m[3][3]) - (m->m[3][2] * m->m[1][3]);
    cp_sqrdim_t b11 = (m->m[2][2] * m->m[3][3]) - (m->m[3][2] * m->m[2][3]);

    cp_dim_t d = (b00 * b11) - (b01 * b10) + (b02 * b09) + (b03 * b08) - (b04 * b07) + (b05 * b06);
    if (cp_sqr_eq(d,0)) {
        CP_ZERO(r);
        return 0;
    }

    cp_dim_t oa = ((m->m[1][1] * b11) - (m->m[2][1] * b10) + (m->m[3][1] * b09)) / d;
    cp_dim_t ob = ((m->m[2][0] * b10) - (m->m[1][0] * b11) - (m->m[3][0] * b09)) / d;
    cp_dim_t oc = ((m->m[1][3] * b05) - (m->m[2][3] * b04) + (m->m[3][3] * b03)) / d;
    cp_dim_t od = ((m->m[2][2] * b04) - (m->m[1][2] * b05) - (m->m[3][2] * b03)) / d;
    cp_dim_t oe = ((m->m[2][1] * b08) - (m->m[0][1] * b11) - (m->m[3][1] * b07)) / d;
    cp_dim_t of = ((m->m[0][0] * b11) - (m->m[2][0] * b08) + (m->m[3][0] * b07)) / d;
    cp_dim_t og = ((m->m[2][3] * b02) - (m->m[0][3] * b05) - (m->m[3][3] * b01)) / d;
    cp_dim_t oh = ((m->m[0][2] * b05) - (m->m[2][2] * b02) + (m->m[3][2] * b01)) / d;
    cp_dim_t oi = ((m->m[0][1] * b10) - (m->m[1][1] * b08) + (m->m[3][1] * b06)) / d;
    cp_dim_t oj = ((m->m[1][0] * b08) - (m->m[0][0] * b10) - (m->m[3][0] * b06)) / d;
    cp_dim_t ok = ((m->m[0][3] * b04) - (m->m[1][3] * b02) + (m->m[3][3] * b00)) / d;
    cp_dim_t ol = ((m->m[1][2] * b02) - (m->m[0][2] * b04) - (m->m[3][2] * b00)) / d;
    cp_dim_t om = ((m->m[1][1] * b07) - (m->m[0][1] * b09) - (m->m[2][1] * b06)) / d;
    cp_dim_t on = ((m->m[0][0] * b09) - (m->m[1][0] * b07) + (m->m[2][0] * b06)) / d;
    cp_dim_t oo = ((m->m[1][3] * b01) - (m->m[0][3] * b03) - (m->m[2][3] * b00)) / d;
    cp_dim_t op = ((m->m[0][2] * b03) - (m->m[1][2] * b01) + (m->m[2][2] * b00)) / d;

    r->m[0][0] = oa;
    r->m[0][1] = oe;
    r->m[0][2] = oi;
    r->m[0][3] = om;
    r->m[1][0] = ob;
    r->m[1][1] = of;
    r->m[1][2] = oj;
    r->m[1][3] = on;
    r->m[2][0] = oc;
    r->m[2][1] = og;
    r->m[2][2] = ok;
    r->m[2][3] = oo;
    r->m[3][0] = od;
    r->m[3][1] = oh;
    r->m[3][2] = ol;
    r->m[3][3] = op;

    return d;
}

/** Determinant of 3D matrix. */
extern cp_dim_t cp_mat3_det(
    cp_mat3_t const *m)
{
    cp_sqrdim_t b00 = (m->m[0][0] * m->m[1][1]) - (m->m[1][0] * m->m[0][1]);
    cp_sqrdim_t b01 = (m->m[0][0] * m->m[2][1]) - (m->m[2][0] * m->m[0][1]);
    cp_sqrdim_t b03 = (m->m[1][0] * m->m[2][1]) - (m->m[2][0] * m->m[1][1]);
    cp_dim_t b08 = m->m[0][2];
    cp_dim_t b10 = m->m[1][2];
    cp_dim_t b11 = m->m[2][2];

    cp_dim_t d = (b00 * b11) - (b01 * b10) + (b03 * b08);
    return d;
}

/**
 * Determinant of 3D matrix with translation vector */
extern cp_dim_t cp_mat3w_det(
    cp_mat3w_t const *m)
{
    return cp_mat3_det(&m->b);
}

/**
 * Inverse of 3D matrix.
 *
 * Returns the determinant.
 */
extern double cp_mat3_inv(
    cp_mat3_t *r,
    cp_mat3_t const *m)
{
    cp_sqrdim_t b00 = (m->m[0][0] * m->m[1][1]) - (m->m[1][0] * m->m[0][1]);
    cp_sqrdim_t b01 = (m->m[0][0] * m->m[2][1]) - (m->m[2][0] * m->m[0][1]);
    cp_sqrdim_t b03 = (m->m[1][0] * m->m[2][1]) - (m->m[2][0] * m->m[1][1]);
    cp_dim_t b08 = m->m[0][2];
    cp_dim_t b10 = m->m[1][2];
    cp_dim_t b11 = m->m[2][2];

    cp_dim_t d = (b00 * b11) - (b01 * b10) + (b03 * b08);

    if (cp_sqr_eq(d, 0)) {
        CP_ZERO(r);
        return 0;
    }

    cp_dim_t oa = ((m->m[1][1] * b11) - (m->m[2][1] * b10)) / d;
    cp_dim_t ob = ((m->m[2][0] * b10) - (m->m[1][0] * b11)) / d;
    cp_dim_t oc = b03 / d;
    cp_dim_t oe = ((m->m[2][1] * b08) - (m->m[0][1] * b11)) / d;
    cp_dim_t of = ((m->m[0][0] * b11) - (m->m[2][0] * b08)) / d;
    cp_dim_t og = (-b01) / d;
    cp_dim_t oi = ((m->m[0][1] * b10) - (m->m[1][1] * b08)) / d;
    cp_dim_t oj = ((m->m[1][0] * b08) - (m->m[0][0] * b10)) / d;
    cp_dim_t ok = b00 / d;

    r->m[0][0] = oa;
    r->m[0][1] = oe;
    r->m[0][2] = oi;
    r->m[1][0] = ob;
    r->m[1][1] = of;
    r->m[1][2] = oj;
    r->m[2][0] = oc;
    r->m[2][1] = og;
    r->m[2][2] = ok;

    return d;
}

/** Determinant of 2D matrix. */
extern cp_dim_t cp_mat2_det(
    cp_mat2_t const *m)
{
    cp_dim_t d = (m->m[0][0] * m->m[1][1]) - (m->m[1][0] * m->m[0][1]);
    return d;
}

/** Determinant of 2D matrix with translation vector */
extern cp_dim_t cp_mat2w_det(
    cp_mat2w_t const *m)
{
    return cp_mat2_det(&m->b);
}

/**
 * Inverse of 2D matrix.
 *
 * Returns the determinant.
 */
extern double cp_mat2_inv(
    cp_mat2_t *r,
    cp_mat2_t const *m)
{
    cp_sqrdim_t d = (m->m[0][0] * m->m[1][1]) - (m->m[1][0] * m->m[0][1]);

    if (cp_sqr_eq(d, 0)) {
        CP_ZERO(r);
        return 0;
    }

    cp_dim_t oa = m->m[1][1] / d;
    cp_dim_t ob = (-m->m[1][0]) / d;
    cp_dim_t oe = (-m->m[0][1]) / d;
    cp_dim_t of = m->m[0][0] / d;

    r->m[0][0] = oa;
    r->m[0][1] = oe;
    r->m[1][0] = ob;
    r->m[1][1] = of;

    return d;
}

/**
 * Inverse of 3D matrix with translation vector.
 *
 * Returns the determinant.
 */
extern double cp_mat3w_inv(
    cp_mat3w_t *r,
    cp_mat3w_t const *m)
{
    cp_sqrdim_t b00 = (m->b.m[0][0] * m->b.m[1][1]) - (m->b.m[1][0] * m->b.m[0][1]);
    cp_sqrdim_t b01 = (m->b.m[0][0] * m->b.m[2][1]) - (m->b.m[2][0] * m->b.m[0][1]);
    cp_sqrdim_t b03 = (m->b.m[1][0] * m->b.m[2][1]) - (m->b.m[2][0] * m->b.m[1][1]);
    cp_sqrdim_t b06 = (m->b.m[0][2] * m->w.v[1]) - (m->b.m[1][2] * m->w.v[0]);
    cp_sqrdim_t b07 = (m->b.m[0][2] * m->w.v[2]) - (m->b.m[2][2] * m->w.v[0]);
    cp_dim_t b08 = m->b.m[0][2];
    cp_sqrdim_t b09 = (m->b.m[1][2] * m->w.v[2]) - (m->b.m[2][2] * m->w.v[1]);
    cp_dim_t b10 = m->b.m[1][2];
    cp_dim_t b11 = m->b.m[2][2];

    cp_dim_t d = (b00 * b11) - (b01 * b10) + (b03 * b08);

    if (cp_sqr_eq(d, 0)) {
        CP_ZERO(r);
        return 0;
    }

    cp_dim_t oa = ((m->b.m[1][1] * b11) - (m->b.m[2][1] * b10)) / d;
    cp_dim_t ob = ((m->b.m[2][0] * b10) - (m->b.m[1][0] * b11)) / d;
    cp_dim_t oc = b03 / d;
    cp_dim_t oe = ((m->b.m[2][1] * b08) - (m->b.m[0][1] * b11)) / d;
    cp_dim_t of = ((m->b.m[0][0] * b11) - (m->b.m[2][0] * b08)) / d;
    cp_dim_t og = (-b01) / d;
    cp_dim_t oi = ((m->b.m[0][1] * b10) - (m->b.m[1][1] * b08)) / d;
    cp_dim_t oj = ((m->b.m[1][0] * b08) - (m->b.m[0][0] * b10)) / d;
    cp_dim_t ok = b00 / d;
    cp_dim_t om = ((m->b.m[1][1] * b07) - (m->b.m[0][1] * b09) - (m->b.m[2][1] * b06)) / d;
    cp_dim_t on = ((m->b.m[0][0] * b09) - (m->b.m[1][0] * b07) + (m->b.m[2][0] * b06)) / d;
    cp_dim_t oo = ((m->w.v[1] * b01) - (m->w.v[0] * b03) - (m->w.v[2] * b00)) / d;

    r->b.m[0][0] = oa;
    r->b.m[0][1] = oe;
    r->b.m[0][2] = oi;
    r->w.v[0] = om;
    r->b.m[1][0] = ob;
    r->b.m[1][1] = of;
    r->b.m[1][2] = oj;
    r->w.v[1] = on;
    r->b.m[2][0] = oc;
    r->b.m[2][1] = og;
    r->b.m[2][2] = ok;
    r->w.v[2] = oo;

    return d;
}

/**
 * Inverse of 2D matrix with translation vector.
 *
 * Returns the determinant.
 */
extern double cp_mat2w_inv(
    cp_mat2w_t *r,
    cp_mat2w_t const *m)
{
    cp_dim_t d = (m->b.m[0][0] * m->b.m[1][1]) - (m->b.m[1][0] * m->b.m[0][1]);

    cp_dim_t b08 = m->w.v[0];
    cp_dim_t b10 = m->w.v[1];

    if (cp_sqr_eq(d, 0)) {
        CP_ZERO(r);
        return 0;
    }

    cp_dim_t oa = m->b.m[1][1] / d;
    cp_dim_t ob = (-m->b.m[1][0]) / d;
    cp_dim_t oe = (-m->b.m[0][1]) / d;
    cp_dim_t of = m->b.m[0][0] / d;
    cp_sqrdim_t oi = ((m->b.m[0][1] * b10) - (m->b.m[1][1] * b08)) / d;
    cp_sqrdim_t oj = ((m->b.m[1][0] * b08) - (m->b.m[0][0] * b10)) / d;

    r->b.m[0][0] = oa;
    r->b.m[0][1] = oe;
    r->w.v[0] = oi;
    r->b.m[1][0] = ob;
    r->b.m[1][1] = of;
    r->w.v[1] = oj;

    return d;
}

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
    cp_mat4_t const *q)
{
    for (int y = 0; y < 3; y++) {
        r->b.row[y] = q->row[y].b;
        r->w.v[y] = q->m[y][3];
    }

    return
        cp_eq(q->m[3][0], 0) &&
        cp_eq(q->m[3][1], 0) &&
        cp_eq(q->m[3][2], 0) &&
        cp_eq(q->m[3][3], 1);
}

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
    cp_mat3_t const *q)
{
    for (int y = 0; y < 2; y++) {
        r->b.row[y] = q->row[y].b;
        r->w.v[y] = q->m[y][2];
    }

    return
        cp_eq(q->m[2][0], 0) &&
        cp_eq(q->m[2][1], 0) &&
        cp_eq(q->m[2][2], 1);
}

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
    cp_mat3w_t const *q)
{
    for (int y = 0; y < 2; y++) {
        r->b.row[y] = q->b.row[y].b;
        r->w.v[y] = q->w.v[y];
    }

    return
        cp_eq(q->w.v[2], 0) &&
        cp_eq(q->b.m[0][2], 0) &&
        cp_eq(q->b.m[1][2], 0) &&
        cp_eq(q->b.m[2][0], 0) &&
        cp_eq(q->b.m[2][1], 0) &&
        cp_eq(q->b.m[2][2], 1);
}

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
    cp_mat3w_t const *q)
{
    for (int y = 0; y < 3; y++) {
        r->row[y].b = q->b.row[y];
        r->m[y][3] = q->w.v[y];
    }
    r->m[3][0] = r->m[3][1] = r->m[3][2] = 0;
    r->m[3][3] = 1;
}

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
    cp_mat2w_t const *q)
{
    for (int y = 0; y < 2; y++) {
        r->row[y].b = q->b.row[y];
        r->m[y][2] = q->w.v[y];
    }
    r->m[2][0] = r->m[2][1] = 0;
    r->m[2][2] = 1;
}

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
    cp_mat2w_t const *q)
{
    for (int y = 0; y < 2; y++) {
        r->b.row[y].b = q->b.row[y];
        r->w.v[y] = q->w.v[y];
    }
    r->w.v[2] = 0;
    r->b.m[2][0] = r->b.m[0][2] = 0;
    r->b.m[2][1] = r->b.m[1][2] = 0;
    r->b.m[2][2] = 1;
}

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
    cp_mat4i_t const *q)
{
    bool ok =
        cp_mat3w_from_mat4(&r->n, &q->n) &&
        cp_mat3w_from_mat4(&r->i, &q->i);
    r->d = cp_mat3_det(&r->n.b);
    return ok;
}

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
    cp_mat3i_t const *q)
{
    bool ok =
        cp_mat2w_from_mat3(&r->n, &q->n) &&
        cp_mat2w_from_mat3(&r->i, &q->i);
    r->d = cp_mat2_det(&r->n.b);
    return ok;
}

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
    cp_mat3wi_t const *q)
{
    bool ok =
        cp_mat2w_from_mat3w(&r->n, &q->n) &&
        cp_mat2w_from_mat3w(&r->i, &q->i);
    r->d = cp_mat2_det(&r->n.b);
    return ok;
}

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
    cp_mat3wi_t const *q)
{
    cp_mat4_from_mat3w(&r->n, &q->n);
    cp_mat4_from_mat3w(&r->i, &q->i);
    r->d = q->d;
}

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
    cp_mat2wi_t const *q)
{
    cp_mat3_from_mat2w(&r->n, &q->n);
    cp_mat3_from_mat2w(&r->i, &q->i);
    r->d = q->d;
}

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
    cp_mat2wi_t const *q)
{
    cp_mat3w_from_mat2w(&r->n, &q->n);
    cp_mat3w_from_mat2w(&r->i, &q->i);
    r->d = q->d;
}

/**
 * Copy 2D matrix into 2D matrix w/ inverse, if possible.
 *
 * Fills the additional columns trivially from the unit matrix.
 */
extern bool cp_mat2i_from_mat2(
    cp_mat2i_t *r,
    cp_mat2_t const *q)
{
    cp_mat2_t n = *q;
    r->n = n;
    r->d = cp_mat2_inv(&r->i, &r->n);
    return !cp_sqr_eq(r->d, 0);
}

/**
 * Copy 3D matrix into 3D matrix w/ inverse, if possible.
 *
 * This adds the inverse and returns whether the determinant is non-0.
 */
extern bool cp_mat3i_from_mat3(
    cp_mat3i_t *r,
    cp_mat3_t const *q)
{
    cp_mat3_t n = *q;
    r->n = n;
    r->d = cp_mat3_inv(&r->i, &r->n);
    return !cp_sqr_eq(r->d, 0);
}

/**
 * Copy 4D matrix into 4D matrix w/ inverse, if possible.
 *
 * This adds the inverse and returns whether the determinant is non-0.
 */
extern bool cp_mat4i_from_mat4(
    cp_mat4i_t *r,
    cp_mat4_t const *q)
{
    cp_mat4_t n = *q;
    r->n = n;
    r->d = cp_mat4_inv(&r->i, &r->n);
    return !cp_sqr_eq(r->d, 0);
}

/**
 * Copy 2D matrix w/ translation vector into 2D matrix w/ inverse &
 * translation vector, if possible
 *
 * This adds the inverse and returns whether the determinant is non-0.
 */
extern bool cp_mat2wi_from_mat2w(
    cp_mat2wi_t *r,
    cp_mat2w_t const *q)
{
    cp_mat2w_t n = *q;
    r->n = n;
    r->d = cp_mat2w_inv(&r->i, &r->n);
    return !cp_sqr_eq(r->d, 0);
}

/**
 * Copy 3D matrix w/ translation vector into 3D matrix w/ inverse &
 * translation vector, if possible
 *
 * This adds the inverse and returns whether the determinant is non-0.
 */
extern bool cp_mat3wi_from_mat3w(
    cp_mat3wi_t *r,
    cp_mat3w_t const *q)
{
    cp_mat3w_t n = *q;
    r->n = n;
    r->d = cp_mat3w_inv(&r->i, &r->n);
    return !cp_sqr_eq(r->d, 0);
}

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
    cp_vec2_t const *p3)
{
    /*
     *
     *  Slopes:
     *
     *  a1 = (y2 - y1) / (x2 - x1)
     *  a2 = (y3 - y2) / (x3 - x2)
     *
     *  Collinear if a1 == a2.  Eliminate division by 0:
     *
     *       (y2 - y1) / (x2 - x1) == (y3 - y2) / (x3 - x2)
     *  <=>  (y2 - y1) * (x3 - x2) == (y3 - y2) * (x2 - x1)
     *
     */
    return cp_sqr_eq((p2->y - p1->y) * (p3->x - p2->x), (p3->y - p2->y) * (p2->x - p1->x));
}

/**
 * Whether tree points in 3D are in one line.
 * Criterion: slopes in XY, YZ and ZX planes.
 */
extern bool cp_vec3_in_line(
    cp_vec3_t const *p1,
    cp_vec3_t const *p2,
    cp_vec3_t const *p3)
{
    return
        cp_sqr_eq((p2->y - p1->y) * (p3->z - p2->z), (p3->y - p2->y) * (p2->z - p1->z)) &&
        cp_sqr_eq((p2->y - p1->y) * (p3->x - p2->x), (p3->y - p2->y) * (p2->x - p1->x)) &&
        cp_sqr_eq((p2->z - p1->z) * (p3->x - p2->x), (p3->z - p2->z) * (p2->x - p1->x));
}

/**
 * cp_v_vec2_loc_t nth function for cp_vec2_arr_ref_t
 */
extern cp_vec2_t *_cp_v_vec2_loc_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i)
{
    cp_v_vec2_loc_t const *v = u->user1;
    return &cp_v_nth(v, i).coord;
}

/**
 * cp_v_vec2_loc_t idx function for cp_vec2_arr_ref_t
 */
extern size_t _cp_v_vec2_loc_idx(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a)
{
    cp_v_vec2_loc_t const *v = u->user1;
    return cp_v_idx(v, CP_BOX_OF(a, cp_vec2_loc_t const, coord));
}

/**
 * cp_v_vec3_loc_t nth function for cp_vec2_arr_ref_t, XY plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_xy_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i)
{
    cp_v_vec3_loc_t const *v = u->user1;
    return &cp_v_nth(v, i).coord.b;
}

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, XY plane
 */
extern size_t _cp_v_vec3_loc_xy_idx(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a)
{
    cp_v_vec3_loc_t const *v = u->user1;
    return cp_v_idx(v, CP_BOX_OF(a, cp_vec3_loc_t const, coord.b));
}

/**
 * cp_v_vec3_loc_ref_t nth function for cp_vec2_arr_ref_t, XY plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_ref_xy_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i)
{
    cp_a_vec3_loc_ref_t const *v = u->user2;
    return &cp_v_nth(v, i).ref->coord.b;
}

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, XY plane
 */
extern size_t _cp_v_vec3_loc_ref_xy_idx(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a)
{
    cp_a_vec3_loc_t const *v = u->user1;
    return cp_v_idx(v, CP_BOX_OF(a, cp_vec3_loc_t const, coord.b));
}

/**
 * cp_v_vec3_loc_ref_t nth function for cp_vec2_arr_ref_t, YZ plane
 */
extern cp_vec2_t *_cp_v_vec3_loc_ref_yz_nth(
    cp_vec2_arr_ref_t const *u,
    size_t i)
{
    cp_a_vec3_loc_ref_t const *v = u->user2;
    return &cp_v_nth(v, i).ref->coord.be;
}

/**
 * cp_v_vec3_loc_t idx function for cp_vec2_arr_ref_t, YZ plane
 */
extern size_t _cp_v_vec3_loc_ref_yz_idx(
    cp_vec2_arr_ref_t const *u,
    cp_vec2_t const *a)
{
    cp_a_vec3_loc_t const *v = u->user1;
    return cp_v_idx(v, CP_BOX_OF(a, cp_vec3_loc_t const, coord.be));
}
