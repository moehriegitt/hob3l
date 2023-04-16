/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_PS_TAM_H_
#define CP_PS_TAM_H_

#include <hob3lmat/mat_gen_tam.h>
#include <hob3l/gc_tam.h>

#ifndef CP_PS_PAPER_NAME
/**
 * Paper size name */
#  define CP_PS_PAPER_NAME "a4"
/**
 * Paper width in PostScript points */
#  define CP_PS_PAPER_X 595
/**
 * Paper height in PostScript points */
#  define CP_PS_PAPER_Y 842
/**
 * Paper margin in PostScript points */
#  define CP_PS_PAPER_MARGIN 56
#endif

#define CP_PS_XFORM_MM \
    ((cp_ps_xform_t){ \
        .mul_x = 72.0 / 25.4, \
        .mul_y = 72.0 / 25.4, \
        .add_x = CP_PS_PAPER_X/2, \
        .add_y = CP_PS_PAPER_Y/2, \
    })

typedef struct {
    cp_scale_t mul_x;
    cp_scale_t mul_y;
    cp_dim_t add_x;
    cp_dim_t add_y;
} cp_ps_xform_t;

/**
 * Options for ps output */
typedef struct {
    cp_ps_xform_t const *xform1;
    cp_mat4_t xform2;
    cp_color_rgb_t color_path;
    cp_color_rgb_t color_tri;
    cp_color_rgb_t color_fill;
    cp_color_rgb_t color_vertex;
    cp_color_rgb_t color_mark;
    double line_width;
    bool single_page;
    bool no_tri;
    bool no_path;
    bool no_mark;
} cp_ps_opt_t;

#endif /* CP_PS_TAM_H_ */
