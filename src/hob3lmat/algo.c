/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lmat/algo.h>
#include <hob3lmat/mat.h>

double cp_eq_epsilon  = CP_EQ_EPSILON_DEFAULT;
double cp_sqr_epsilon = CP_SQR_EPSILON_DEFAULT;

/**
 * Take a step on the circle iterator
 */
extern void cp_circle_iter_step(
    cp_circle_iter_t *iter)
{
    size_t i = ++iter->_i;
    if (i & 1) {
        size_t i1 = i + 1;
        iter->idx = i1 / 2;
        cp_angle_t a = iter->_a * cp_angle(iter->idx);
        if (i1 == iter->_n) {
            iter->cos = -1;
            iter->sin = 0;
        }
        else {
            iter->cos = cp_cos_deg(a);
            iter->sin = cp_sin_deg(a);
        }
    }
    else {
        iter->sin = -iter->sin;
        iter->idx = iter->_n - iter->idx;
    }
}

/**
 * Comparison using cp_eq_epsilon
 *
 * This should be the default way to compare cp_dim_t, cp_scale_t, and double.
 */
extern int cp_lex_cmp(double const *a, double const *b, size_t size)
{
    for (cp_size_each(i, size)) {
        if (!cp_eq(a[i], b[i])) {
            return a[i] < b[i] ? -1 : +1;
        }
    }
    return 0;
}

/**
 * Rotate around u by an angle given as sin+cos components.
 *
 * This is THE generic rotation matrix generator for this libray which
 * is used to fill in both mat3 and mat4 matrix structures.
 *
 * Asserts that u and sc are both unit or [0,0].
 *
 * Returns a rotation matrix as three row vectors r0,r1,r2.
 */
extern void cp_dim3_rot_unit(
    cp_vec3_t *r0,
    cp_vec3_t *r1,
    cp_vec3_t *r2,
    cp_vec3_t const *u,
    cp_vec2_t const *sc)
{
    cp_scale_t s = sc->v[0];
    cp_scale_t c = sc->v[1];
    assert(cp_vec2_has_len0_or_1(sc));
    assert(cp_vec3_has_len0_or_1(u));
    cp_dim_t x = u->x;
    cp_dim_t y = u->y;
    cp_dim_t z = u->z;
    cp_sqrdim_t x_s = x*s;
    cp_sqrdim_t y_s = y*s;
    cp_sqrdim_t z_s = z*s;
    cp_dim_t d = 1-c;
    cp_sqrdim_t x_d = x*d;
    cp_sqrdim_t y_d = y*d;
    cp_sqrdim_t z_d = z*d;
    cp_sqrdim_t x_y_d = x*y_d;
    cp_sqrdim_t x_z_d = x*z_d;
    cp_sqrdim_t y_z_d = y*z_d;
    *r0 = CP_VEC3(x*x_d + c,   x_y_d - z_s, x_z_d + y_s);
    *r1 = CP_VEC3(x_y_d + z_s, y*y_d + c,   y_z_d - x_s);
    *r2 = CP_VEC3(x_z_d - y_s, y_z_d + x_s, z*z_d + c);
}

/**
 * Mirror matrix.
 *
 * Asserts that u is unit or [0,0].
 */
extern void cp_dim2_mirror_unit(
    cp_vec2_t *r0,
    cp_vec2_t *r1,
    cp_vec2_t const *u)
{
    assert(cp_vec2_has_len0_or_1(u));
    cp_dim_t x = u->x;
    cp_dim_t y = u->y;
    cp_dim_t m2x = -2*x;
    cp_dim_t m2y = -2*y;
    cp_sqrdim_t m2xy = m2x*y;
    *r0 = CP_VEC2(1+m2x*x, m2xy);
    *r1 = CP_VEC2(m2xy,    1+m2y*y);
}

/**
 * Mirror matrix.
 *
 * Asserts that u is unit or [0,0].
 */
extern void cp_dim3_mirror_unit(
    cp_vec3_t *r0,
    cp_vec3_t *r1,
    cp_vec3_t *r2,
    cp_vec3_t const *u)
{
    assert(cp_vec3_has_len0_or_1(u));
    cp_dim_t x = u->x;
    cp_dim_t y = u->y;
    cp_dim_t m2x = -2*x;
    cp_dim_t m2y = -2*y;
    cp_sqrdim_t m2xy = m2x*y;
    cp_dim_t z = u->z;
    cp_dim_t m2z = -2*z;
    cp_sqrdim_t m2xz = m2x*z;
    cp_sqrdim_t m2yz = m2y*z;
    *r0 = CP_VEC3(1+m2x*x, m2xy,    m2xz);
    *r1 = CP_VEC3(m2xy,    1+m2y*y, m2yz);
    *r2 = CP_VEC3(m2xz,    m2yz,    1+m2z*z);
}

/**
 * Rotate the unit vector u into the [0,0,1] axis.
 *
 * The rotation is around the ([0,0,1] x u) axis.
 *
 * Asserts that u is unit or [0,0].
 *
 * Returns the rotation matrix as three row vectors r0,r1,r2.
 */
extern void cp_dim3_rot_unit_into_z(
    cp_vec3_t *r0,
    cp_vec3_t *r1,
    cp_vec3_t *r2,
    cp_vec3_t const *u)
{
    assert(cp_vec3_has_len0_or_1(u));
    double x = u->x;
    double y = u->y;
    double z = u->z;
    if (cp_eq(x,0) && cp_eq(y,0)) {
        *r0 = CP_VEC3(1, 0, 0);
        *r1 = CP_VEC3(0, 1, 0);
        *r2 = CP_VEC3(0, 0, z);
        return;
    }

    double k = sqrt((x*x) + (y*y));
    *r0 = CP_VEC3(y/k,   -x/k,   0);
    *r1 = CP_VEC3(x*z/k, y*z/k, -k);
    *r2 = CP_VEC3(x,     y,      z);
}

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
 * Compute point on line that is closest to a given point.
 */
extern void cp_vec2_nearest(
    /** resulting point on line */
    cp_vec2_t *r,
    /** some point on the line */
    cp_vec2_t const *a,
    /** direction unit vector of line (must be unit) */
    cp_vec2_t const *ud,
    /** point somewhere for which to find the closest point on the line */
    cp_vec2_t const *p)
{
    assert(cp_eq(cp_vec2_sqr_len(ud), 1));
    cp_vec2_t w;
    cp_vec2_sub(&w, p, a);
    double d = cp_vec2_dot(&w, ud);
    cp_vec2_mul(&w, ud, d);
    cp_vec2_add(r, a, &w);
}

/**
 * Make a matrix to rotate and translate into a different coordinate system.
 *
 * This returns a matrix 'into_z' that:
 *    - rotates the vector (a-o) into  the Z axis (using csg_mat4_rot_into_z),
 *    - moves o to (0,0,0), and
 *    - rotates (b-o) around Z into the X-Z plane; if (b-o) is
 *      perpendicular to (a-o), it rotates (b-o) into the X axis.
 *
 * This does not scale, i.e., the length of (a-o) and of (b-o) is irrelevant,
 * i.e., this function does not try to map (a-o) to (0,0,1), but it only
 * rotates and translates, mapping (a-o) to (0,0,k) for some k.  The same
 * holds for the secondary rotation of (b-o) into the X-Z plane.
 *
 * This function also returns the inverse 'from_z' of the matrix
 * described above.  The function can compute the inverse less numerically
 * instably than running a matrix inversion on 'into_z'.
 *
 * Both 'into_z' and 'from_z' may be NULL if the matrix and/or its inverse is
 * not needed.
 *
 * If o is NULL, it will be assumed to be equal to (0,0,0).
 *
 * If (a-o) has length 0, the rotation into the Z axis will be skipped,
 * and the function will return false.
 *
 * If b is NULL, the rotation around the Z axis will be skipped and the
 * function will return true.
 *
 * If (b-o) has length 0 or is collinear to (0,0,1), the rotation around
 * the Z axis will be skipped and the function will return false.
 *
 * Otherwise, the function returns true.
 */
extern bool cp_mat3w_xform_into_zx_2(
    cp_mat3w_t *into_z2,
    cp_mat3w_t *from_z,
    cp_vec3_t const *o,
    cp_vec3_t const *a,
    cp_vec3_t const *b)
{
    static cp_vec3_t o0 = CP_VEC3(0,0,0);
    if (o == NULL) {
        o = &o0;
    }

    cp_vec3_t z[1];
    cp_vec3_sub(z, a, o);

    /* rotate into z */
    cp_mat3w_t into_z[1];
    bool res = cp_mat3w_rot_into_z(into_z, z);

    /* map o */
    cp_vec3_t on[1];
    cp_vec3w_xform(on, into_z, o);

    /* update matrices with offset compensation */
    into_z->w.v[0] = -on->x;
    into_z->w.v[1] = -on->y;
    into_z->w.v[2] = -on->z;

    /* compute inverse so far */
    if (from_z != NULL) {
        cp_mat3w_trans(from_z, into_z);

        from_z->w.v[0] = o->x;
        from_z->w.v[1] = o->y;
        from_z->w.v[2] = o->z;
    }

    /* map b */
    if (b == NULL) {
    }
    else
    if (cp_eq(b->x, o->x) && cp_eq(b->y, o->y)) {
        res = false;
    }
    else {
        cp_vec3_t bn[1];
        cp_vec3w_xform(bn, into_z, b);

        /* rotate bn into x axis */
        if (!cp_eq(bn->y, 0)) {
            double l = sqrt((bn->x*bn->x) + (bn->y*bn->y));
            double x = bn->x / l;
            double y = bn->y / l;

            cp_mat3w_t into_x[1] = {
                CP_MAT3W(
                    x,  y,  0, 0,
                   -y,  x,  0, 0,
                    0,  0,  1, 0) };

            /* add the Z axis rotation */
            cp_mat3w_mul(into_z, into_x, into_z);
            if (from_z != NULL) {
                cp_mat3w_trans(into_x, into_x);
                cp_mat3w_mul(from_z, from_z, into_x);
            }
        }
    }

    /* try to return matrix */
    if (into_z2 != NULL) {
        *into_z2 = *into_z;
    }

    return res;
}

typedef struct {
    unsigned long long mant;
    int exp;
    bool neg;
    int width;
} cp_f_split_t;

/* Returns whether the number is real or not.
 *
 * width is set to the numbers of significant bits
 * extracted from the float.
 *
 * The original mantissa is always shifted so that its
 * highest represented bit in the float is at the highest
 * bit in the unsigned integer.
 *
 * The exponent is made integer so that 0 means no shift of
 * the matissa.  The mantissa's highest bit is assumed to have
 * value 1 (=2^0).  I.e., '1' is represented by the highest bit
 * in the mantissa plus an exponent of 0.
 */
static bool split_float(
    cp_f_split_t *s,
    double f)
{
    union {
        unsigned long long i;
        double f;
    } u = { .f = f };
    assert(sizeof(u.i) == sizeof(u.f));

    /* raw decode */
    int exp_raw = (u.i >> 52) & 0x7ff;
    s->exp  = exp_raw - 1023;
    s->mant = (u.i & 0x000fffffffffffffULL) << 12;
    s->neg  = (u.i >> 63);
    s->width = 52;

    /* non-numbers are not modified further */
    if (exp_raw == 0x7ff) {
        return false;
    }

    /* for normals, add implicit highest bit.  subnormals need no care. */
    if (exp_raw != 0) {
        s->mant = (s->mant >> 1) | 0x8000000000000000ULL;
        s->width++;
    }
    return true;
}

/**
 * This returns true if \p f is an integers, and
 * then returns that integer in \p i.
 */
extern bool cp_f_get_int(
    long long *ip,
    cp_f_t f)
{
    cp_f_split_t s;
    if (!split_float(&s, f)) {
        return false;
    }
    /* check we are not obviously below 1 */
    if (s.exp < 0) {
        return false;
    }
    /* check that the unit bit (bit0 of the integer) can be represented in the mantissa */
    if (s.exp >= s.width) {
        return false;
    }
    /* check that there are no zeros after decimal point */
    if ((s.mant << (s.exp + 1)) != 0) {
        return false;
    }

    /* we're int! */
    long long i = (long long)(s.mant >> ((8 * sizeof(s.mant)) - (s.exp & 0xffffff) - 1));
    if (s.neg) {
        i = -i;
    }
    *ip = i;
    return true;
}

static cp_f_t const *exact_sin(long long ai)
{
    ai = ai % 360;
    if (ai < 0) {
        ai += 360;
    }
    assert((ai >= 0) && (ai < 360));

    static const cp_f_t f_0 = 0;
    static const cp_f_t f_1 = 1;
    static const cp_f_t f_m1 = -1;
    static const cp_f_t f_05 = 0.5;
    static const cp_f_t f_m05 = -0.5;
    switch (ai & 0x7fffffff) {
    case 0:
    case 180: return &f_0;
    case 90:  return &f_1;
    case 270: return &f_m1;
    case 30:
    case 150: return &f_05;
    case 210:
    case 330: return &f_m05;
    }
    return NULL;
}

/**
 * For angles that have exact rational results, this will return
 * exactly those results.  E.g. cp_sin_deg(180) == 0, not just close
 * to 0, but 0.
 */
extern cp_f_t cp_sin_deg(cp_f_t a)
{
    long long ai;
    if (cp_f_get_int(&ai, a)) {
        cp_f_t const *r = exact_sin(ai);
        if (r != NULL) {
            return *r;
        }
    }
    return sin(cp_deg(a));
}

/**
 * For angles that have exact rational results, this will return
 * exactly those results.  E.g. cp_cos_deg(180) == 1, not just close
 * to 1, but 1.
 */
extern cp_f_t cp_cos_deg(cp_f_t a)
{
    long long ai;
    if (cp_f_get_int(&ai, a)) {
        cp_f_t const *r = exact_sin(ai + 90);
        if (r != NULL) {
            return *r;
        }
    }
    return cos(cp_deg(a));
}

/**
 * Find the angle between two 2D vectors
 *
 * Returns a value in (-CP_PI, +CP_PI] (i.e., prefers +PI over -PI).
 */
extern cp_angle_t cp_vec2_angle(
    cp_vec2_t const *a,
    cp_vec2_t const *b)
{
#if 0
    /* This is what is given in SVG W3C Implementation Notes eq.5.4 */
    double c = cp_vec2_dot(a,b) / (cp_vec2_len(a) * cp_vec2_len(b));
    /* avoid +-1.00000000000000001 to avoud NaN from acos() */
    if (cp_eq(c, +1)) { c = +1; }
    if (cp_eq(c, -1)) { c = -1; }
    assert(c <= 1);
    assert(c >= -1);
    double r = acos(c);
    assert(r >= 0);
    double s = cp_vec2_cross_z(a,b);
    if (s < 0) {
        r = -r;
    }
    return r;
#elif 0
    /* There may be another possibility using the cross product and asin(). */
#else
    /* This is an implementation with atan2(), which should be equivalent. */
    double r = atan2(b->y, b->x) - atan2(a->y, a->x);
    if (r <= -CP_PI) {
        r += CP_TAU;
    }
    if (r > +CP_PI) {
        r -= CP_TAU;
    }
    return r;
#endif
}

/**
 * Find the angle between two 2D vectors with a given origin.
 *
 * This returns the angle between o-->a and o-->b using cp_vec2_angle().
 *
 * Returns a value in (-CP_PI, +CP_PI] (i.e., prefers +PI over -PI).
 */
extern cp_angle_t cp_vec2_angle3(
    cp_vec2_t const *a,
    cp_vec2_t const *o,
    cp_vec2_t const *b)
{
    cp_vec2_t ao;
    cp_vec2_sub(&ao, a, o);
    cp_vec2_t bo;
    cp_vec2_sub(&bo, b, o);
    return cp_vec2_angle(&ao, &bo);
}
