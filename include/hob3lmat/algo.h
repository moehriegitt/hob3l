/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_MAT_ALGO_H_
#define CP_MAT_ALGO_H_

#include <hob3lmat/mat_gen_tam.h>
#include <hob3lmat/mat_tam.h>

#define CP_SINCOS_RAD(a) ((cp_vec2_t){ .v={ sin(a), cos(a) }})
#define CP_SINCOS_DEG(a) ((cp_vec2_t){ .v={ cp_sin_deg(a), cp_cos_deg(a) }})

#define CP_COSSIN_RAD(a) ((cp_vec2_t){ .v={ cos(a), sin(a) }})
#define CP_COSSIN_DEG(a) ((cp_vec2_t){ .v={ cp_cos_deg(a), cp_sin_deg(a) }})

#define cp_deg(rad) ((rad) * (CP_PI / 180))

CP_STATIC_ASSERT(cp_offsetof(cp_mat3_t,  m[0][0]) == 0);
CP_STATIC_ASSERT(cp_offsetof(cp_mat3w_t, b.m[0][0]) == 0);

/**
 * Iterator for circles */
typedef struct {
    cp_dim_t cos;
    cp_dim_t sin;
    size_t idx;
    size_t _i;
    size_t _n;
    cp_angle_t _a;
} cp_circle_iter_t;

#define CP_CIRCLE_ITER_INIT(n) \
    { \
        .cos = 1, \
        .sin = 0, \
        .idx = 0, \
        ._i = 0, \
        ._n = n, \
        ._a = 360 / cp_angle(n), \
    }

#define cp_circle_each(iter, n) \
    cp_circle_iter_t iter = CP_CIRCLE_ITER_INIT(n); \
    iter._i < iter._n; \
    cp_circle_iter_step(&iter)

/**
 * Take a step on the circle iterator
 */
#define cp_circle_each(iter, n) \
    cp_circle_iter_t iter = CP_CIRCLE_ITER_INIT(n); \
    iter._i < iter._n; \
    cp_circle_iter_step(&iter)

/**
 * General epsilon for comparisons.
 *
 * Typically the square of cp_pt_epsilon.
 */
extern double cp_eq_epsilon;

/**
 * Epsilon for comparison of squared values, or two coordinates multiplied,
 * or determinants.
 *
 * Typically the square of cp_eq_epsilon.
 */
extern double cp_sqr_epsilon;

/**
 * Comparison using cp_eq_epsilon
 *
 * This should be the default way to compare cp_dim_t, cp_scale_t, and double.
 */
extern int cp_lex_cmp(double const *a, double const *b, size_t size);

/* comparisons using any epsilon */
static inline bool cp_e_eq (cp_f_t e, cp_f_t a, cp_f_t b) { return cp_abs(a - b) < e; }
static inline bool cp_e_le (cp_f_t e, cp_f_t a, cp_f_t b) { return (a - b) < e; }
static inline bool cp_e_lt (cp_f_t e, cp_f_t a, cp_f_t b) { return (a - b) < -e; }
static inline bool cp_e_ge (cp_f_t e, cp_f_t a, cp_f_t b) { return cp_e_le(e, b, a); }
static inline bool cp_e_gt (cp_f_t e, cp_f_t a, cp_f_t b) { return cp_e_lt(e, b, a); }
static inline int  cp_e_cmp(cp_f_t e, cp_f_t a, cp_f_t b)
{
    return cp_e_eq(e, a, b) ? 0 : a < b ? -1 : +1;
}

/* comparisons using cp_eq_epsilon */
static inline bool cp_eq (cp_f_t a, cp_f_t b) { return cp_e_eq (cp_eq_epsilon, a, b); }
static inline bool cp_le (cp_f_t a, cp_f_t b) { return cp_e_le (cp_eq_epsilon, a, b); }
static inline bool cp_lt (cp_f_t a, cp_f_t b) { return cp_e_lt (cp_eq_epsilon, a, b); }
static inline bool cp_ge (cp_f_t a, cp_f_t b) { return cp_e_ge (cp_eq_epsilon, a, b); }
static inline bool cp_gt (cp_f_t a, cp_f_t b) { return cp_e_gt (cp_eq_epsilon, a, b); }
static inline int  cp_cmp(cp_f_t a, cp_f_t b) { return cp_e_cmp(cp_eq_epsilon, a, b); }

/* comparisons using cp_sqr_epsilon */
static inline bool cp_sqr_eq (cp_f_t a, cp_f_t b) { return cp_e_eq (cp_sqr_epsilon, a, b); }
static inline bool cp_sqr_le (cp_f_t a, cp_f_t b) { return cp_e_le (cp_sqr_epsilon, a, b); }
static inline bool cp_sqr_lt (cp_f_t a, cp_f_t b) { return cp_e_lt (cp_sqr_epsilon, a, b); }
static inline bool cp_sqr_ge (cp_f_t a, cp_f_t b) { return cp_e_ge (cp_sqr_epsilon, a, b); }
static inline bool cp_sqr_gt (cp_f_t a, cp_f_t b) { return cp_e_gt (cp_sqr_epsilon, a, b); }
static inline int  cp_sqr_cmp(cp_f_t a, cp_f_t b) { return cp_e_cmp(cp_sqr_epsilon, a, b); }

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
    cp_vec2_t const *sc);

/**
 * Mirror matrix.
 *
 * Asserts that u is unit or [0,0].
 */
extern void cp_dim2_mirror_unit(
    cp_vec2_t *r0,
    cp_vec2_t *r1,
    cp_vec2_t const *u);

/**
 * Mirror matrix.
 *
 * Asserts that u is unit or [0,0].
 */
extern void cp_dim3_mirror_unit(
    cp_vec3_t *r0,
    cp_vec3_t *r1,
    cp_vec3_t *r2,
    cp_vec3_t const *u);

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
    cp_vec3_t const *u);

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
    cp_vec2_t const *p);

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
    cp_vec3_t const *b);

/**
 * This returns true if \p f is an integers, and
 * then returns that integer in \p i.
 */
extern bool cp_f_get_int(
    long long *ip,
    cp_f_t f);

/**
 * For angles that have exact rational results, this will return
 * exactly those results.  E.g. cp_sin_deg(180) == 0, not just close
 * to 0, but 0.
 */
extern cp_f_t cp_sin_deg(cp_f_t a);

/**
 * For angles that have exact rational results, this will return
 * exactly those results.  E.g. cp_cos_deg(180) == 1, not just close
 * to 1, but 1.
 */
extern cp_f_t cp_cos_deg(cp_f_t a);

/**
 * Take a step on the circle iterator
 */
extern void cp_circle_iter_step(
    cp_circle_iter_t *iter);

/**
 * Find the angle between two 2D vectors
 *
 * Returns a value in (-CP_PI, +CP_PI] (i.e., prefers +PI over -PI).
 */
extern cp_angle_t cp_vec2_angle(
    cp_vec2_t const *a,
    cp_vec2_t const *b);

/**
 * Find the angle between two 2D vectors with a given origin.
 *
 * This returns the angle between o--a and o--b using cp_vec2_angle().
 *
 * Returns a value in (-CP_PI, +CP_PI] (i.e., prefers +PI over -PI).
 */
extern cp_angle_t cp_vec2_angle3(
    cp_vec2_t const *a,
    cp_vec2_t const *o,
    cp_vec2_t const *b);

/**
 * Same as cp_mat4_xform_into_zx_2 with a cp_mat4i_t target type.
 */
static inline bool cp_mat3wi_xform_into_zx(
    cp_mat3wi_t *m,
    cp_vec3_t const *o,
    cp_vec3_t const *a,
    cp_vec3_t const *b)
{
    return cp_mat3w_xform_into_zx_2(&m->n, &m->i, o, a, b);
}

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

/**
 * Divide, avoiding division by zero by returning 0.  This is often a
 * sound error propagation with matrices, e.g., if a determinant gets
 * 0, the inverted matrix also has determinant 0.  If a vector has
 * length 0, then its unit is also length 0.  Because this happens to
 * be useful often, it is a service function here, and many utility
 * functions, instead of assert failing or raising div-by-0, they just
 * work by continuing with 0.
 */
static inline cp_f_t cp_div0(cp_f_t a, cp_f_t b)
{
    return cp_eq(b,0) ? 0 : a / b;
}

/**
 * Get lerp index 0..1 of val between src and dst.
 *
 * With t = cp_t01(a,x,b) it will hold that x = cp_lerp(a,b,t).
 */
static inline cp_f_t cp_t01(cp_f_t src, cp_f_t val, cp_f_t dst)
{
    return cp_div0(val - src, dst - src);
}

/**
 * Get lerp index -1..1 of val between src and dst.
 *
 * With t = cp_t_pm(a,x,b) it will hold that x = cp_lerp_pm(a,b,t).
 *
 * If you have a choice, prefer the pair cp_t01/cp_lerp over
 * cp_t_pm/cp_lerp_pm, because the former needs less floating point
 * operations.
 */
static inline cp_f_t cp_t_pm(cp_f_t src, cp_f_t val, cp_f_t dst)
{
    return (cp_t01(src, val, dst) * 2) - 1;
}

/**
 * For angles that have exact rational results, this will return
 * exactly those results.
 */
static inline cp_f_t cp_tan_deg(cp_f_t a)
{
    return cp_sin_deg(a) / cp_cos_deg(a);
}

#endif /* CP_MAT_ALGO_H_ */
