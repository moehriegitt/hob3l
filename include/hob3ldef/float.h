/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_FLOAT_H_
#define CP_FLOAT_H_

typedef double cp_f_t;

#define CP_PI  3.1415926535897932384626433832795028841971694
#define CP_TAU 6.2831853071795864769252867665590057683943388

#define CP_PT_EPSILON_DEFAULT  0x1p-9
#define CP_EQ_EPSILON_DEFAULT  0x1p-15
#define CP_SQR_EPSILON_DEFAULT (CP_EQ_EPSILON_DEFAULT * CP_EQ_EPSILON_DEFAULT)

#define CP_F_MAX 0x1.fffffffffffffp+1023

#define CP_FF  "%.16g"
#define CP_FD  "%+9.5f"

#define cp_abs(x) fabs(x)

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
