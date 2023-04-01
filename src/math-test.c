/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/def.h>
#include <hob3lbase/arith.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/stream.h>
#include "test.h"
#include "math-test.h"

static cp_f_t simple_sin_deg(cp_f_t a)
{
    return sin(cp_deg(a));
}

static cp_f_t simple_cos_deg(cp_f_t a)
{
    return cos(cp_deg(a));
}

static void rot_math_test(void)
{
    cp_mat3wi_t m[1];
    cp_vec3_t v[1];
    bool b;
    cp_mat3w_t i[1];
    cp_stream_t *cerr = CP_STREAM_FROM_FILE(stderr);
    cp_vec3_t const *mo;
    cp_vec3_t const *ma;
    cp_vec3_t const *mb;

    /* triviality test */
    b = cp_mat3wi_xform_into_zx(m,
        &CP_VEC3(0,0,0),
        &CP_VEC3(0,0,1),
        &CP_VEC3(1,0,0));
    TEST_EQ(b, true);
    TEST_EQ(cp_mat3w_eq(&m->n,
        &CP_MAT3W(1,0,0,0, 0,1,0,0, 0,0,1,0)), true);
    TEST_EQ(cp_mat3w_eq(&m->i,
        &CP_MAT3W(1,0,0,0, 0,1,0,0, 0,0,1,0)), true);

    /* triviality test and NULL test */
    b = cp_mat3wi_xform_into_zx(m,
        NULL,
        &CP_VEC3(0,0,1),
        NULL);
    TEST_EQ(b, true);
    TEST_EQ(cp_mat3w_eq(&m->n,
        &CP_MAT3W(1,0,0,0, 0,1,0,0, 0,0,1,0)), true);
    TEST_EQ(cp_mat3w_eq(&m->i,
        &CP_MAT3W(1,0,0,0, 0,1,0,0, 0,0,1,0)), true);

    /* triviality test and NULL test */
    b = cp_mat3wi_xform_into_zx(m,
        NULL,
        &CP_VEC3(0,0,0),
        NULL);
    TEST_EQ(b, false);
    TEST_EQ(cp_mat3w_eq(&m->n,
        &CP_MAT3W(1,0,0,0, 0,1,0,0, 0,0,1,0)), true);
    TEST_EQ(cp_mat3w_eq(&m->i,
        &CP_MAT3W(1,0,0,0, 0,1,0,0, 0,0,1,0)), true);

    /* triviality test */
    b = cp_mat3wi_xform_into_zx(m,
        &CP_VEC3(0,0,0),
        &CP_VEC3(0,0,2),
        &CP_VEC3(2,0,0));
    TEST_EQ(b, true);
    TEST_EQ(cp_mat3w_eq(&m->n,
        &CP_MAT3W(1,0,0,0, 0,1,0,0, 0,0,1,0)), true);
    TEST_EQ(cp_mat3w_eq(&m->i,
        &CP_MAT3W(1,0,0,0, 0,1,0,0, 0,0,1,0)), true);

    /* translation test */
    b = cp_mat3wi_xform_into_zx(m,
        &CP_VEC3(3,4,5),
        &CP_VEC3(3,4,7),
        &CP_VEC3(5,4,5));
    TEST_EQ(b, true);
    TEST_EQ(cp_mat3w_eq(&m->n,
        &CP_MAT3W(1,0,0,-3, 0,1,0,-4, 0,0,1,-5)), true);
    TEST_EQ(cp_mat3w_eq(&m->i,
        &CP_MAT3W(1,0,0,3, 0,1,0,4, 0,0,1,5)), true);

    /* translation test */
    b = cp_mat3wi_xform_into_zx(m,
        &CP_VEC3(3,4,5),
        &CP_VEC3(3,4,5),
        &CP_VEC3(5,4,5));
    TEST_EQ(b, false);
    TEST_EQ(cp_mat3w_eq(&m->n,
        &CP_MAT3W(1,0,0,-3, 0,1,0,-4, 0,0,1,-5)), true);
    TEST_EQ(cp_mat3w_eq(&m->i,
        &CP_MAT3W(1,0,0,3, 0,1,0,4, 0,0,1,5)), true);

    /* rotation test */
    mo = &CP_VEC3(3,4,5);
    ma = &CP_VEC3(7,8,2);
    b = cp_mat3wi_xform_into_zx(m, mo, ma, NULL);
    TEST_EQ(b, true);
    cp_vec3w_xform(v, &m->n, mo);
    fprintf(stderr, "DEBUG: %g %g %g\n", v->x, v->y, v->z);
    TEST_EQ(cp_vec3_eq(v, &CP_VEC3(0,0,0)), true);

    cp_vec3w_xform(v, &m->n, ma);
    fprintf(stderr, "DEBUG: %g %g %g\n", v->x, v->y, v->z);
    TEST_EQ(cp_vec2_eq(&v->b, &CP_VEC2(0,0)), true);

    /* rotation test */
    mo = &CP_VEC3(3,4,5);
    ma = &CP_VEC3(7,8,2);
    mb = &CP_VEC3(17,2,3);
    b = cp_mat3wi_xform_into_zx(m, mo, ma, mb);
    TEST_EQ(b, true);
    cp_vec3w_xform(v, &m->n, mo);
    fprintf(stderr, "DEBUG: %g %g %g\n", v->x, v->y, v->z);
    TEST_EQ(cp_vec3_eq(v, &CP_VEC3(0,0,0)), true);

    cp_vec3w_xform(v, &m->n, ma);
    fprintf(stderr, "DEBUG: %g %g %g\n", v->x, v->y, v->z);
    TEST_EQ(cp_vec2_eq(&v->b, &CP_VEC2(0,0)), true);

    cp_vec3w_xform(v, &m->n, mb);
    fprintf(stderr, "DEBUG: %g %g %g\n", v->x, v->y, v->z);
    TEST_EQ(cp_eq(v->y, 0), true);

    cp_mat3w_inv(i, &m->n);
    fprintf(stderr, "i=\n");
    cp_mat3w_put(cerr, i);
    fprintf(stderr, "i'=\n");
    cp_mat3w_put(cerr, &m->i);
    TEST_EQ(cp_mat3w_eq(i, &m->i), true);

    /* rotation test */
    mo = &CP_VEC3(3,   4,   5);
    ma = &CP_VEC3(7,   9,   5); /* 4, 5, 0 */
    mb = &CP_VEC3(3-5, 4+4, 5);
    b = cp_mat3wi_xform_into_zx(m, mo, ma, mb);
    TEST_EQ(b, true);
    cp_vec3w_xform(v, &m->n, mo);
    fprintf(stderr, "DEBUG: %g %g %g\n", v->x, v->y, v->z);
    TEST_EQ(cp_vec3_eq(v, &CP_VEC3(0,0,0)), true);

    cp_vec3w_xform(v, &m->n, ma);
    fprintf(stderr, "DEBUG: %g %g %g\n", v->x, v->y, v->z);
    TEST_EQ(cp_vec2_eq(&v->b, &CP_VEC2(0,0)), true);

    cp_vec3w_xform(v, &m->n, mb);
    fprintf(stderr, "DEBUG: %g %g %g\n", v->x, v->y, v->z);
    TEST_EQ(cp_eq(v->y, 0), true);
    TEST_EQ(cp_eq(v->z, 0), true);

    cp_mat3w_inv(i, &m->n);
    fprintf(stderr, "i=\n");
    cp_mat3w_put(cerr, i);
    fprintf(stderr, "i'=\n");
    cp_mat3w_put(cerr, &m->i);
    TEST_EQ(cp_mat3w_eq(i, &m->i), true);
}

#define TEST_EQ_SIN(a,b) \
    TEST_EQ(cp_sin_deg(a), b); \
    TEST_EQ(cp_sin_deg((a) + 360), b); \
    TEST_EQ(cp_sin_deg((a) - 360), b); \
    TEST_EQ(cp_cos_deg((a) - 90), b); \
    TEST_FEQ(cp_sin_deg(a), simple_sin_deg(a));

/**
 * Unit tests of basic math functionality.
 */
extern void cp_math_test(void)
{
    long long ll;
    TEST_EQ(cp_f_get_int(&ll, +1.0), true);
    TEST_EQ(ll, 1);
    TEST_EQ(cp_f_get_int(&ll, -1.0), true);
    TEST_EQ(ll, -1);
    TEST_EQ(cp_f_get_int(&ll, +2.0), true);
    TEST_EQ(ll, +2);
    TEST_EQ(cp_f_get_int(&ll, -2.0), true);
    TEST_EQ(ll, -2);
    TEST_EQ(cp_f_get_int(&ll, +1267650600228229401496703205376e0), false);
    TEST_EQ(cp_f_get_int(&ll, -1267650600228229401496703205376e0), false);
    TEST_EQ(cp_f_get_int(&ll, 0.5),  false);
    TEST_EQ(cp_f_get_int(&ll, 0.25), false);
    TEST_EQ(cp_f_get_int(&ll, 0.3),  false);
    TEST_EQ(cp_f_get_int(&ll, 0.1),  false);
    TEST_EQ(cp_f_get_int(&ll, 3.0),  true);
    TEST_EQ(ll, 3);
    TEST_EQ(cp_f_get_int(&ll, 4503599627370494e0), true);
    TEST_EQ(ll, 4503599627370494);
    TEST_EQ(cp_f_get_int(&ll, 4503599627370494.5e0), false);
    TEST_EQ(cp_f_get_int(&ll, 4503599627370495e0), true);
    TEST_EQ(ll, 4503599627370495);
    TEST_EQ(cp_f_get_int(&ll, 4503599627370495.5e0), false);
    TEST_EQ(cp_f_get_int(&ll, 4503599627370496e0), true);
    TEST_EQ(ll, 4503599627370496);
    TEST_EQ(cp_f_get_int(&ll, 4503599627370497e0), true);
    TEST_EQ(ll, 4503599627370497);
    TEST_EQ(cp_f_get_int(&ll, +9007199254740991e0), true);
    TEST_EQ(ll, 9007199254740991);
    TEST_EQ(cp_f_get_int(&ll, -9007199254740991e0), true);
    TEST_EQ(ll, -9007199254740991);
    TEST_EQ(cp_f_get_int(&ll, 9007199254740992e0), false);
    TEST_EQ(cp_f_get_int(&ll, 9007199254740993e0), false);
    TEST_EQ(cp_f_get_int(&ll, 1e-319), false);
    TEST_EQ(cp_f_get_int(&ll, 1.0/0.0), false);
    TEST_EQ(cp_f_get_int(&ll, 0.0/0.0), false);

    /* Float precision tests for special cases of sin and cos: I want no
     * precision loss here, it must be spot on if the input values are
     * exact.  So we use '==' on double here, something that should normally
     * not be done. */
    TEST_EQ_SIN(360,    0);
    TEST_EQ_SIN(-7200,  0);
    TEST_EQ_SIN(180,    0);
    TEST_EQ_SIN(-180,   0);
    TEST_EQ_SIN(90,     1);
    TEST_EQ_SIN(-270,   1);
    TEST_EQ_SIN(270,   -1);
    TEST_EQ_SIN(-90,   -1);
    TEST_EQ_SIN(30,     0.5);
    TEST_EQ_SIN(150,    0.5);
    TEST_EQ_SIN(210,   -0.5);
    TEST_EQ_SIN(330,   -0.5);

    TEST_FEQ(cp_sin_deg(0),     simple_sin_deg(0));
    TEST_FEQ(cp_sin_deg(360),   simple_sin_deg(360));
    TEST_FEQ(cp_sin_deg(-360),  simple_sin_deg(-360));
    TEST_FEQ(cp_sin_deg(-720),  simple_sin_deg(-720));
    TEST_FEQ(cp_sin_deg(+7200), simple_sin_deg(+7200));
    TEST_FEQ(cp_sin_deg(-7200), simple_sin_deg(-7200));
    TEST_FEQ(cp_sin_deg(+7201), simple_sin_deg(+7201));
    TEST_FEQ(cp_sin_deg(-7201), simple_sin_deg(-7201));
    TEST_FEQ(cp_sin_deg(90),    simple_sin_deg(90));
    TEST_FEQ(cp_sin_deg(-270),  simple_sin_deg(-270));
    TEST_FEQ(cp_sin_deg(180),   simple_sin_deg(180));
    TEST_FEQ(cp_sin_deg(-180),  simple_sin_deg(-180));
    TEST_FEQ(cp_sin_deg(270),   simple_sin_deg(270));
    TEST_FEQ(cp_sin_deg(-270),  simple_sin_deg(-270));
    TEST_FEQ(cp_sin_deg(300),   simple_sin_deg(300));
    TEST_FEQ(cp_sin_deg(-300),  simple_sin_deg(-300));
    TEST_FEQ(cp_sin_deg(-90),   simple_sin_deg(-90));
    TEST_FEQ(cp_sin_deg(30),    simple_sin_deg(30));
    TEST_FEQ(cp_sin_deg(60),    simple_sin_deg(60));
    TEST_FEQ(cp_sin_deg(40),    simple_sin_deg(40));
    TEST_FEQ(cp_sin_deg(-40),   simple_sin_deg(-40));
    TEST_FEQ(cp_sin_deg(80),    simple_sin_deg(80));
    TEST_FEQ(cp_sin_deg(-80),   simple_sin_deg(-80));
    TEST_FEQ(cp_sin_deg(810),   simple_sin_deg(810));
    TEST_FEQ(cp_sin_deg(-81),   simple_sin_deg(-81));
    TEST_FEQ(cp_sin_deg(-810),  simple_sin_deg(-810));

    TEST_FEQ(cp_cos_deg(0),     simple_cos_deg(0));
    TEST_FEQ(cp_cos_deg(360),   simple_cos_deg(360));
    TEST_FEQ(cp_cos_deg(-360),  simple_cos_deg(-360));
    TEST_FEQ(cp_cos_deg(-720),  simple_cos_deg(-720));
    TEST_FEQ(cp_cos_deg(+7200), simple_cos_deg(+7200));
    TEST_FEQ(cp_cos_deg(-7200), simple_cos_deg(-7200));
    TEST_FEQ(cp_cos_deg(+7201), simple_cos_deg(+7201));
    TEST_FEQ(cp_cos_deg(-7201), simple_cos_deg(-7201));
    TEST_FEQ(cp_cos_deg(90),    simple_cos_deg(90));
    TEST_FEQ(cp_cos_deg(-270),  simple_cos_deg(-270));
    TEST_FEQ(cp_cos_deg(180),   simple_cos_deg(180));
    TEST_FEQ(cp_cos_deg(-180),  simple_cos_deg(-180));
    TEST_FEQ(cp_cos_deg(270),   simple_cos_deg(270));
    TEST_FEQ(cp_cos_deg(-270),  simple_cos_deg(-270));
    TEST_FEQ(cp_cos_deg(300),   simple_cos_deg(300));
    TEST_FEQ(cp_cos_deg(-300),  simple_cos_deg(-300));
    TEST_FEQ(cp_cos_deg(-90),   simple_cos_deg(-90));
    TEST_FEQ(cp_cos_deg(30),    simple_cos_deg(30));
    TEST_FEQ(cp_cos_deg(60),    simple_cos_deg(60));
    TEST_FEQ(cp_cos_deg(40),    simple_cos_deg(40));
    TEST_FEQ(cp_cos_deg(-40),   simple_cos_deg(-40));
    TEST_FEQ(cp_cos_deg(80),    simple_cos_deg(80));
    TEST_FEQ(cp_cos_deg(-80),   simple_cos_deg(-80));
    TEST_FEQ(cp_cos_deg(810),   simple_cos_deg(810));
    TEST_FEQ(cp_cos_deg(-81),   simple_cos_deg(-81));
    TEST_FEQ(cp_cos_deg(-810),  simple_cos_deg(-810));

    /* rotation matrices should also be exact now */
    cp_mat3_t m;
    cp_mat3_rot_unit(&m, &(cp_vec3_t){{ 1, 0, 0 }}, CP_SINCOS_DEG(0));
    TEST_EQ(m.m[0][0], 1); TEST_EQ(m.m[0][1], 0); TEST_EQ(m.m[0][2], 0);
    TEST_EQ(m.m[1][0], 0); TEST_EQ(m.m[1][1], 1); TEST_EQ(m.m[1][2], 0);
    TEST_EQ(m.m[2][0], 0); TEST_EQ(m.m[2][1], 0); TEST_EQ(m.m[2][2], 1);

    cp_mat3_rot_unit(&m, &(cp_vec3_t){{ 1, 0, 0 }}, CP_SINCOS_DEG(90));
    TEST_EQ(m.m[0][0], 1); TEST_EQ(m.m[0][1], 0); TEST_EQ(m.m[0][2], 0);
    TEST_EQ(m.m[1][0], 0); TEST_EQ(m.m[1][1], 0); TEST_EQ(m.m[1][2], -1);
    TEST_EQ(m.m[2][0], 0); TEST_EQ(m.m[2][1],+1); TEST_EQ(m.m[2][2], 0);

    cp_mat3_rot_unit(&m, &(cp_vec3_t){{ 1, 0, 0 }}, CP_SINCOS_DEG(-90));
    TEST_EQ(m.m[0][0], 1); TEST_EQ(m.m[0][1], 0); TEST_EQ(m.m[0][2], 0);
    TEST_EQ(m.m[1][0], 0); TEST_EQ(m.m[1][1], 0); TEST_EQ(m.m[1][2], +1);
    TEST_EQ(m.m[2][0], 0); TEST_EQ(m.m[2][1],-1); TEST_EQ(m.m[2][2], 0);

    {
        /* Test that CCW normal computation is really called 'left'. */
        cp_vec3_t n = {{ 0, 0, 1 }};
        cp_vec3_t a = {{ 0, 1, 0 }};
        cp_vec3_t b = {{ 0, 0, 0 }};
        cp_vec3_t c = {{ 1, 0, 0 }};

        cp_vec3_t p;
        cp_vec3_left_normal3(&p, &a, &b, &c);
        TEST_FEQ(n.x, p.x);
        TEST_FEQ(n.y, p.y);
        TEST_FEQ(n.z, p.z);
    }
    {
        /* Test that CW normal computation is really called 'right'. */
        cp_vec3_t n = {{ 0, 0, 1 }};
        cp_vec3_t a = {{ 1, 0, 0 }};
        cp_vec3_t b = {{ 0, 0, 0 }};
        cp_vec3_t c = {{ 0, 1, 0 }};

        cp_vec3_t p;
        cp_vec3_right_normal3(&p, &a, &b, &c);
        TEST_FEQ(n.x, p.x);
        TEST_FEQ(n.y, p.y);
        TEST_FEQ(n.z, p.z);
    }
    {
        /* STL facet from OpenSCAD: uses 'left normal' */
        cp_vec3_t n = {{ -6.62557e-18, -1, 2.86288e-16 }};
        cp_vec3_t a = {{ -107, -6, 50.5 }};
        cp_vec3_t b = {{ -107, -6, 51.4711 }};
        cp_vec3_t c = {{ -109.7, -6, 44.6289 }};

        cp_vec3_t p;
        cp_vec3_left_normal3(&p, &a, &b, &c);
        TEST_FEQ(n.x, p.x);
        TEST_FEQ(n.y, p.y);
        TEST_FEQ(n.z, p.z);
    }
    {
        /* STL facet from Wikipedia: also uses 'left normal' */
        /* This requires an epsilon of 0.02 to work, maybe this is an artistic
         * error or done for some shaping fanciness. */
        double old_epsilon = cp_eq_epsilon;
        cp_eq_epsilon = 0.02;

        cp_vec3_t n = {{ 0.70675, -0.70746, 0 }};
        cp_vec3_t a = {{ 1000, 0, 0 }};
        cp_vec3_t b = {{ 0, -1000, 0 }};
        cp_vec3_t c = {{ 0, -999, -52 }};

        cp_vec3_t p;
        cp_vec3_left_normal3(&p, &a, &b, &c);
        TEST_FEQ(n.x, p.x);
        TEST_FEQ(n.y, p.y);
        TEST_FEQ(n.z, p.z);

        cp_eq_epsilon = old_epsilon;
    }
    {
        /* Test of own STL output */
        cp_vec3_t n = {{ 0, 0, 1 }};
        cp_vec3_t a = {{ 0, 0, 2.19}};
        cp_vec3_t b = {{ 10, 0, 2.19}};
        cp_vec3_t c = {{ 0, 10, 2.19}};

        cp_vec3_t p;
        cp_vec3_left_normal3(&p, &a, &b, &c);
        TEST_FEQ(n.x, p.x);
        TEST_FEQ(n.y, p.y);
        TEST_FEQ(n.z, p.z);
    }
    {
        /* Test of STL face */
        cp_vec3_t n = {{ 0.682114, 0.186335, -0.707107 }};
        cp_vec3_t a = {{ 8.24063, 27.0101, -73.3779 }};
        cp_vec3_t b = {{ 6.10409, 30.573, -74.5 }};
        cp_vec3_t c = {{ 7.16564, 30.9453, -73.3779 }};

        cp_vec3_t p;
        cp_vec3_left_normal3(&p, &a, &b, &c);
        TEST_FEQ(n.x, p.x);
        TEST_FEQ(n.y, p.y);
        TEST_FEQ(n.z, p.z);
    }
    {
        /* Test of STL face */
        cp_vec3_t n = {{ -0.682114, -0.186335, 0.707107 }};
        cp_vec3_t a = {{ 6.08316, 30.6496, -74.5 }};
        cp_vec3_t b = {{ 6.10409, 30.573, -74.5 }};
        cp_vec3_t c = {{ 7.16564, 30.9453, -73.3779 }};

        double old_epsilon = cp_eq_epsilon;
        cp_eq_epsilon = 0.02;

        cp_vec3_t p;
        cp_vec3_left_normal3(&p, &a, &b, &c);
        TEST_FEQ(n.x, p.x);
        TEST_FEQ(n.y, p.y);
        TEST_FEQ(n.z, p.z);

        cp_eq_epsilon = old_epsilon;
    }

    TEST_EQ(1, 0x1p0);
    TEST_EQ(2, 0x1p1);
    TEST_EQ(3, 0x1.8p1);
    TEST_EQ(0.125, 0x1p-3);

    /* rotation matrix test */
    rot_math_test();
}
