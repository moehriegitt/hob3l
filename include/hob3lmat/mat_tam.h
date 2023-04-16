/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * Some matrix and vector type/macro definitions.
 *
 * We have many matrix types here to be able to use the minimum amount
 * of float computations, and to exploit precision where it exists.  E.g.
 * there are 2, 3, and 4 dimensional vectors and matrices.  For the
 * usual SCAD transformations, to support translations, there are
 * extendend matrixes suffixed with 'w' which contain the translation
 * vector.  These transformations do not need perspective corrections,
 * so this is faster than going up to full 4 dimensions.  And to improve
 * precision, there is a class of matrices that have their inverse
 * matrix plus their determinant, so we do not need to compute this later.
 * Computing those is usually very expensive and computationally unstable,
 * but for rotations, translations, and scaling transformations, it is
 * very easy to compute the inverse alongside the normal matrix.
 */

#ifndef CP_MAT_TAM_H_
#define CP_MAT_TAM_H_

#include <hob3lmat/mat_def.h>
#include <hob3lmat/mat_gen_tam.h>

#define CP_VEC2_MINMAX_EMPTY { \
    {{ +CP_F_MAX, +CP_F_MAX }}, \
    {{ -CP_F_MAX, -CP_F_MAX }} }

#define CP_VEC3_MINMAX_EMPTY { \
    {{ +CP_F_MAX, +CP_F_MAX, +CP_F_MAX }}, \
    {{ -CP_F_MAX, -CP_F_MAX, -CP_F_MAX }} }

#define CP_VEC4_MINMAX_EMPTY { \
    {{ +CP_F_MAX, +CP_F_MAX, +CP_F_MAX, +CP_F_MAX }}, \
    {{ -CP_F_MAX, -CP_F_MAX, -CP_F_MAX, -CP_F_MAX }} }

#define CP_VEC2_MINMAX_FULL { \
    {{ -CP_F_MAX, -CP_F_MAX }}, \
    {{ +CP_F_MAX, +CP_F_MAX }} }

#define CP_VEC3_MINMAX_FULL { \
    {{ -CP_F_MAX, -CP_F_MAX, -CP_F_MAX }}, \
    {{ +CP_F_MAX, +CP_F_MAX, +CP_F_MAX }} }

#define CP_VEC4_MINMAX_FULL { \
    {{ -CP_F_MAX, -CP_F_MAX, -CP_F_MAX, -CP_F_MAX }}, \
    {{ +CP_F_MAX, +CP_F_MAX, +CP_F_MAX, +CP_F_MAX }} }

#define CP_V01(p)   (p).v[0], (p).v[1]
#define CP_V012(p)  (p).v[0], (p).v[1], (p).v[2]
#define CP_V0123(p) (p).v[0], (p).v[1], (p).v[2], (p).v[3]

#define CP_VEC2(a,b) \
    ((cp_vec2_t){ .v={ a,b } })

#define CP_VEC3(a,b,c) \
    ((cp_vec3_t){ .v={ a,b,c } })

#define CP_VEC4(a,b,c,d) \
    ((cp_vec4_t){ .v={ a,b,c,d } })

#define CP_MAT2(a,b, c,d) \
    ((cp_mat2_t){ .v={ a,b, c,d } })

#define CP_MAT3(a,b,c, d,e,f, g,h,i) \
    ((cp_mat3_t){ .v={ a,b,c, d,e,f, g,h,i } })

#define CP_MAT4(a,b,c,d, e,f,g,h, i,j,k,l, m,n,o,p) \
    ((cp_mat4_t){ .v={ a,b,c,d, e,f,g,h, i,j,k,l, m,n,o,p } })

#define CP_MAT2W(_a,_b,_c, _d,_e,_f) \
    ((cp_mat2w_t){ .b={.v={ _a,_b, _d,_e }}, .w={.v={_c,_f}} })

#define CP_MAT3W(_a,_b,_c,_d, _e,_f,_g,_h, _i,_j,_k,_l) \
    ((cp_mat3w_t){ .b={.v={ _a,_b,_c, _e,_f,_g, _i,_j,_k }}, .w={.v={_d,_h,_l}} })

static inline void cp_mat4_init3(
    cp_mat4_t *m,
    double a, double b, double c,
    double d, double e, double f,
    double g, double h, double i)
{
    *m = CP_MAT4(
        a, b, c, 0,
        d, e, f, 0,
        g, h, i, 0,
        0, 0, 0, 1);
}

#endif /* CP_MAT_H_ */
