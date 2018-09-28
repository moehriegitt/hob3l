/* -*- Mode: C -* */

#include <cpmat/algo.h>
#include <cpmat/arith.h>
#include <cpmat/mat.h>

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
    CP_INIT(r0, x*x_d + c,   x_y_d - z_s, x_z_d + y_s);
    CP_INIT(r1, x_y_d + z_s, y*y_d + c,   y_z_d - x_s);
    CP_INIT(r2, x_z_d - y_s, y_z_d + x_s, z*z_d + c);
}

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
    CP_INIT(r0, 1+m2x*x, m2xy);
    CP_INIT(r1, m2xy,    1+m2y*y);
}

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
    CP_INIT(r0, 1+m2x*x, m2xy,    m2xz);
    CP_INIT(r1, m2xy,    1+m2y*y, m2yz);
    CP_INIT(r2, m2xz,    m2yz,    1+m2z*z);
}

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
    double k = sqrt((x*x) + (y*y));
    CP_INIT(r0, y/k,   -x/k,   0);
    CP_INIT(r1, x*z/k, y*z/k, -k);
    CP_INIT(r2, x,     y,      z);
}
