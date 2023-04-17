/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_FLOAT_H_
#define CP_FLOAT_H_

#include <stdbool.h>
#include <math.h>

typedef double cp_f_t;

#define CP_PI  3.1415926535897932384626433832795028841971694
#define CP_TAU 6.2831853071795864769252867665590057683943388

/**
 * By which factor to scale to convert int<->double.
 *
 * On 32-bit floats, we have 23 mantissa bits.
 *
 * We want at least +-1m max. size on 32-bit floats, or 1024mm =
 * 10 bits integer.
 *
 * So this is set to 2^13, which means the smallest difference
 * is 1/8192mm.
 *
 * The integer part can do 30 unsigned bits, so the coordinate
 * range at integer precision is +-2^17mm or +-128m.
 */
#define CP_DIM_SCALE_DEFAULT  8192

#define CP_EQ_EPSILON_DEFAULT  0x1p-15
#define CP_SQR_EPSILON_DEFAULT (CP_EQ_EPSILON_DEFAULT * CP_EQ_EPSILON_DEFAULT)

#define CP_FF  "%.16g"
#define CP_FD  "%+9.5f"

#define cp_abs(x) fabs(x)

/* handle older mingw compiler */
#ifndef cp_isfinite
#ifdef __MINGW32__
#if __GNUC__ <= 6
static inline bool cp_finite_aux(double x)
{
    return fabs(x) < INFINITY;
}
#define cp_isfinite cp_finite_aux
#endif
#endif
#endif

#ifndef cp_isfinite
#define cp_isfinite isfinite
#endif

static inline cp_f_t cp_double(size_t u)
{
#if __SIZEOF_POINTER__ == 8
    assert(u < (1ULL << 53));
#endif
    return (cp_f_t)u;
}

#define cp_f cp_double

typedef cp_f_t cp_dim_t;
typedef cp_f_t cp_sqrdim_t;
typedef cp_f_t cp_scale_t;
typedef cp_f_t cp_det_t;
typedef cp_f_t cp_angle_t;

#define cp_dim    cp_f
#define cp_sqrdim cp_f
#define cp_scale  cp_f
#define cp_det    cp_f
#define cp_angle  cp_f

#endif /* CP_FLOAT_H_ */
