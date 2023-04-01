/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/algo.h>
#include <hob3lbase/arith.h>
#include <hob3lbase/mat.h>

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
