/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lop/hedron.h>

extern void cq_vec3_minmax(
    cq_vec3_minmax_t *r,
    cq_vec3_t const *v)
{
    cq_dim_minmax(&r->min.x, &r->max.x, v->x);
    cq_dim_minmax(&r->min.y, &r->max.y, v->y);
    cq_dim_minmax(&r->min.z, &r->max.z, v->z);
}

extern void cq_vec3_minmax_minmax2(
    cq_vec2_minmax_t *r,
    cq_vec3_minmax_t const *v,
    unsigned coord0,
    unsigned coord1)
{
    cq_dim_minmax(&r->min.x, &r->max.x, v->min.v[coord0]);
    cq_dim_minmax(&r->min.y, &r->max.y, v->min.v[coord1]);
}

extern void cq_v_vec3_minmax(
    cq_vec3_minmax_t *r,
    cq_v_vec3_t const *v)
{
    for (cp_v_eachp(i, v)) {
        cq_vec3_minmax(r, i);
    }
}

extern void cq_v_v_vec3_minmax(
    cq_vec3_minmax_t *r,
    cq_v_v_vec3_t const *v)
{
    for (cp_v_eachp(i, v)) {
        cq_v_vec3_minmax(r, i);
    }
}
