/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef HOB3LOP_PS_H_
#define HOB3LOP_PS_H_

#include <hob3lop/op-def.h>
#include <hob3lop/matq.h>

#if CQ_TRACE
#include <stdio.h>
typedef FILE cq_ps_info_file_t;
#else
typedef void cq_ps_info_file_t;
#endif

typedef struct {
    char fn[200];
    char fn2[200];
    cq_ps_info_file_t *f;
    cq_dim_t sub_x, sub_y;
    double   mul_x, mul_y;
    unsigned left;
    unsigned right;
    unsigned top;
    unsigned bottom;
    unsigned page;
} cq_ps_info_t;

#if CQ_TRACE

extern cq_ps_info_t cq_ps_info;

extern void cq_ps_init(
    cq_vec2_minmax_t const *bb);

extern void cq_ps_doc_begin(char const *psfn);
extern void cq_ps_doc_end(void);
extern void cq_ps_page_begin(void);
extern void cq_ps_page_end(void);

/*
 * The params to the following functions are in cq_dim_t coord system,
 * but in order to be able to output sub-integer coordinates, they are
 * passed as double. */

extern double cq_ps_coord_x(double x);
extern double cq_ps_coord_y(double y);
extern void cq_ps_line(double x1, double y1, double x2, double y2);
extern void cq_ps_box(double x1, double y1, double x2, double y2);
extern void cq_ps_dot(double x, double y, double radius);

static inline double cq_ps_left(void)
{
    return 0.0 + cq_ps_info.left;
}

static inline double cq_ps_bottom(void)
{
    return 0.0 + cq_ps_info.bottom;
}

static inline double cq_ps_line_y(int i)
{
    return (0.0 + cq_ps_info.top) - (i * 16.0);
}

static inline cq_ps_info_file_t *cq_ps_file(void)
{
    return cq_ps_info.f;
}

#else /* CQ_TRACE */

#define cq_ps_init(...)        ((void)0)
#define cq_ps_doc_begin(...)   ((void)0)
#define cq_ps_doc_end(...)     ((void)0)
#define cq_ps_page_begin(...)  ((void)0)
#define cq_ps_page_end(...)    ((void)0)
#define cq_ps_coord_x(...)     (0.0)
#define cq_ps_coord_y(...)     (0.0)
#define cq_ps_line(...)        ((void)0)
#define cq_ps_box(...)         ((void)0)
#define cq_ps_dot(...)         ((void)0)
#define cq_ps_left()           (0.0)
#define cq_ps_bottom()         (0.0)
#define cq_ps_line_y(...)      (0.0)
#define cq_ps_file()           (NULL)

#endif /* CQ_TRACE */

#endif /* HOB3LOP_PS_H_ */
