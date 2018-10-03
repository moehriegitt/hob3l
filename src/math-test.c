/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <cpmat/def.h>
#include <cpmat/arith.h>
#include <cpmat/mat.h>
#include "test.h"

static cp_f_t simple_sin_deg(cp_f_t a)
{
    return sin(cp_deg(a));
}

static cp_f_t simple_cos_deg(cp_f_t a)
{
    return cos(cp_deg(a));
}

#define TEST_EQ_SIN(a,b) \
    TEST_EQ(cp_sin_deg(a), b); \
    TEST_EQ(cp_sin_deg((a) + 360), b); \
    TEST_EQ(cp_sin_deg((a) - 360), b); \
    TEST_EQ(cp_cos_deg((a) - 90), b); \
    TEST_FEQ(cp_sin_deg(a), simple_sin_deg(a));

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
        double old_epsilon = cp_equ_epsilon;
        cp_equ_epsilon = 0.02;

        cp_vec3_t n = {{ 0.70675, -0.70746, 0 }};
        cp_vec3_t a = {{ 1000, 0, 0 }};
        cp_vec3_t b = {{ 0, -1000, 0 }};
        cp_vec3_t c = {{ 0, -999, -52 }};

        cp_vec3_t p;
        cp_vec3_left_normal3(&p, &a, &b, &c);
        TEST_FEQ(n.x, p.x);
        TEST_FEQ(n.y, p.y);
        TEST_FEQ(n.z, p.z);

        cp_equ_epsilon = old_epsilon;
    }
}
