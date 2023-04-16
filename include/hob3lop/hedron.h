/* -*- Mode: C -*- */

#ifndef HOB3LOP_HEDRON_H_
#define HOB3LOP_HEDRON_H_

#include <hob3lop/op-def.h>
#include <hob3lop/matq.h>
#include <hob3lop/gon.h>
#include <hob3lop/hedron_tam.h>

extern void cq_vec3_minmax_minmax2(
    cq_vec2_minmax_t *r,
    cq_vec3_minmax_t const *v,
    unsigned coord0,
    unsigned coord1);

extern void cq_vec3_minmax(
    cq_vec3_minmax_t *r,
    cq_vec3_t const *v);

extern void cq_v_vec3_minmax(
    cq_vec3_minmax_t *r,
    cq_v_vec3_t const *v);

extern void cq_v_v_vec3_minmax(
    cq_vec3_minmax_t *r,
    cq_v_v_vec3_t const *v);

#endif /* HOB3LOP_HEDRON_H_ */
