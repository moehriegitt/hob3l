/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef HOB3LOP_HEDRON_TAM_H_
#define HOB3LOP_HEDRON_TAM_H_

#include <hob3lop/op-def.h>
#include <hob3lop/gon.h>

/**
 * 3-dimensional vector
 */
typedef union {
    cq_dim_t v[3];
    cq_vec2_t xy;
    struct {
        cq_dim_t x, y, z;
    };
} cq_vec3_t;

/**
 * Compound literal constructor for a cq_vec3_t */
#define CQ_VEC3(x,y,z) ((cq_vec3_t){ .v = { (x), (y), (z) } })

#define CQ_VEC3_MAX CQ_VEC3(CQ_DIM_MAX, CQ_DIM_MAX, CQ_DIM_MAX)
#define CQ_VEC3_MIN CQ_VEC3(CQ_DIM_MIN, CQ_DIM_MIN, CQ_DIM_MIN)

typedef CP_VEC_T(cq_vec3_t) cq_v_vec3_t;

/**
 * A vector or vectors of vec3 is essentially also a polygon,
 * with each v_vec3_t a face.
 *
 * Note that the vectors are stored inline, too.  The basic
 * structure is quite small (three pointers).
 */
typedef CP_VEC_T(cq_v_vec3_t) cq_v_v_vec3_t;

/**
 * minmax values */
typedef struct {
    cq_vec3_t min;
    cq_vec3_t max;
} cq_vec3_minmax_t;

#define CQ_BB3(lo_,hi_) ((cq_vec3_minmax_t){ .min = lo_, .max = hi_ })
#define CQ_VEC3_MINMAX_INIT     CQ_BB3(CQ_VEC3_MAX, CQ_VEC3_MIN)

#endif /* HOB3LOP_HEDRON_TAM_H_ */
