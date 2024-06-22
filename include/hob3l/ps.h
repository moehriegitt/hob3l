/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_PS_H_
#define CP_PS_H_

#include <hob3lbase/base-def.h>
#include <hob3lbase/stream_tam.h>
#include <hob3l/ps_tam.h>


/**
 * MM scale for PostScript output
 */
extern cp_ps_xform_t const cp_ps_mm;

/**
 * Inititalise xform from bounding box to get maximum view on paper.
 */
extern void cp_ps_xform_from_minmax(
    cp_ps_xform_t *d,
    cp_dim_t x_min,
    cp_dim_t y_min,
    cp_dim_t x_max,
    cp_dim_t y_max);

/**
 * Maps coordinates to PostScript center page.
 * If d is NULL, uses MM scale and centers on page.
 */
extern double cp_ps_x(cp_ps_xform_t const *d, double x);

/**
 * Maps coordinates to PostScript center page.
 * If d is NULL, uses MM scale and centers on page.
 */
extern double cp_ps_y(cp_ps_xform_t const *d, double y);

/**
 * Prints document header.
 *
 * If page_dnt is CP_SIZE_MAX, prints 'atend'.
 *
 * If x2 < x1, prints 'atend'.
 */
extern void cp_ps_doc_begin(
    cp_stream_t *s,
    cp_ps_opt_t const *opt CP_UNUSED,
    size_t page_cnt,
    long x1, long y1, long x2, long y2);

/**
 * Prints document trailer.
 *
 * If page_cnt is CP_SIZE_MAX, does not print any page count indicator,
 * assuming cp_ps_doc_begin() has.
 *
 * If x2 < x1, does not print any bounding box, assumeing
 * cp_ps_doc_begin() has.
 */
extern void cp_ps_doc_end(
    cp_stream_t *s,
    size_t page_cnt,
    long x1, long y1, long x2, long y2);

/**
 * Begin a PostScript page
 */
extern void cp_ps_page_begin(
    cp_stream_t *s,
    cp_ps_opt_t const *opt,
    size_t page);

/**
 * Restrict the PostScript clip box.
 */
extern void cp_ps_clip_box(
    cp_stream_t *s,
    double x1, double y1, double x2, double y2);

/**
 * End a PostScript page
 */
extern void cp_ps_page_end(
    cp_stream_t *s);

#endif /* CP_PS_H_ */
