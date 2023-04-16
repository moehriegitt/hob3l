/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef HOB3LOP_DEF_H_
#define HOB3LOP_DEF_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <hob3lbase/base-def.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/dict.h>
#include <hob3lbase/alloc.h>
#include <hob3lop/def_tam.h>

CP_NORETURN
extern void cq_fail(
    char const *msg);

static inline void cq_dim_minmax(
    cq_dim_t *min,
    cq_dim_t *max,
    cq_dim_t v)
{
    if (v < *min) { *min = v; }
    if (v > *max) { *max = v; }
}

#endif /* HOB3LOP_DEF_H_ */
