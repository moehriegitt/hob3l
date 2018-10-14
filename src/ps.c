/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/arith.h>
#include <hob3lbase/stream.h>
#include <hob3l/ps.h>

cp_ps_xform_t const ps_mm = CP_PS_XFORM_MM;

/**
 * Inititalise xform from bounding box to get maximum view on paper.
 */
extern void cp_ps_xform_from_bb(
    cp_ps_xform_t *d,
    cp_dim_t x_min,
    cp_dim_t y_min,
    cp_dim_t x_max,
    cp_dim_t y_max)
{
    *d = CP_PS_XFORM_MM;
    if (cp_gt(x_max, x_min) && cp_gt(y_max, x_min)) {
        d->mul_x = cp_min(
            (CP_PS_PAPER_X - CP_PS_PAPER_MARGIN) / (x_max - x_min),
            (CP_PS_PAPER_Y - CP_PS_PAPER_MARGIN) / (y_max - y_min));

        d->mul_y = d->mul_x;

        d->add_x = CP_PS_PAPER_X/2 -
            d->mul_x * (x_max + x_min)/2;

        d->add_y = CP_PS_PAPER_Y/2 -
            d->mul_y * (y_max + y_min)/2;
    }
}

/**
 * Maps coordinates to PostScript center page.
 * If d is NULL, uses MM scale and centers on page.
 */
extern double cp_ps_x(cp_ps_xform_t const *d, double x)
{
    if (d == NULL) {
        d = &ps_mm;
    }
    return d->add_x + (x * d->mul_x);
}

/**
 * Maps coordinates to PostScript center page.
 * If d is NULL, uses MM scale and centers on page.
 */
extern double cp_ps_y(cp_ps_xform_t const *d, double y)
{
    if (d == NULL) {
        d = &ps_mm;
    }
    return d->add_y + (y * d->mul_y);
}

/**
 * Prints document header.
 *
 * If page_dnt is CP_SIZE_MAX, prints 'atend'.
 *
 * If x2 < x1, prints 'atend'.
 */
extern void cp_ps_doc_begin(
    cp_stream_t *s,
    cp_ps_opt_t const *opt __unused,
    size_t page_cnt,
    long x1, long y1, long x2, long y2)
{
    cp_printf(s,
        "%%!PS-Adobe-3.0\n"
        "%%%%Title: hob3l\n"
        "%%%%Creator: hob3l\n"
        "%%%%Orientation: Portrait\n");

    if (page_cnt != ~(size_t)0) {
        cp_printf(s,
            "%%%%Pages: %"_Pz"u\n",
            page_cnt);
    }
    else {
        cp_printf(s,
            "%%%%Pages: atend\n");
    }

    if (x1 <= x2) {
        cp_printf(s,
            "%%%%BoundingBox: %ld %ld %ld %ld\n",
            x1, y1, x2, y2);
    }
    else {
        cp_printf(s,
            "%%%%BoundingBox: atend\n");
    }

    cp_printf(s,
        "%%%%DocumentPaperSizes: "CP_PS_PAPER_NAME"\n"
        "%%Magnification: 1.0000\n"
        "%%%%EndComments\n");
}

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
    long x1, long y1, long x2, long y2)
{
    cp_printf(s,
        "%%%%Trailer\n");

    if (page_cnt != ~(size_t)0) {
        cp_printf(s,
            "%%%%Pages: %"_Pz"u\n",
            page_cnt);
    }

    if (x1 <= x2) {
        cp_printf(s,
            "%%%%BoundingBox: %ld %ld %ld %ld\n",
            x1, y1, x2, y2);
    }

    cp_printf(s,
        "%%%%EOF\n");
}

/**
 * Begin a PostScript page
 */
extern void cp_ps_page_begin(
    cp_stream_t *s,
    cp_ps_opt_t const *opt,
    size_t page)
{
    cp_printf(s,
        "%%%%Page: %"_Pz"u %"_Pz"u\n"
        "save\n"
        "1 setlinecap\n"
        "1 setlinejoin\n"
        "%g setlinewidth\n"
        "0 setgray\n"
        "/Helvetica findfont 14 scalefont setfont\n",
        page, page,
        opt->line_width);
}

/**
 * Restrict the PostScript clip box.
 */
extern void cp_ps_clip_box(
    cp_stream_t *s,
    double x1, double y1, double x2, double y2)
{
    cp_printf(s,
        "newpath %g %g moveto %g %g lineto %g %g lineto %g %g lineto closepath clip\n",
        x1, y1,
        x1, y2,
        x2, y2,
        x2, y1);
}

/**
 * End a PostScript page
 */
extern void cp_ps_page_end(
    cp_stream_t *s)
{
    cp_printf(s,
        "restore\n"
        "showpage\n");
}
