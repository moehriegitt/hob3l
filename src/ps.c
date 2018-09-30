/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <cpmat/arith.h>
#include <cpmat/stream.h>
#include <csg2plane/ps.h>

cp_ps_xform_t const ps_mm = CP_PS_XFORM_MM;

extern void cp_ps_xform_from_bb(
    cp_ps_xform_t *d,
    cp_dim_t x_min,
    cp_dim_t y_min,
    cp_dim_t x_max,
    cp_dim_t y_max)
{
    *d = CP_PS_XFORM_MM;
    if (cp_gt(x_max, x_min) && cp_gt(y_max, x_min)) {
        d->mul = cp_min(
            (CP_PS_PAPER_X - CP_PS_PAPER_MARGIN) / (x_max - x_min),
            (CP_PS_PAPER_Y - CP_PS_PAPER_MARGIN) / (y_max - y_min));

        d->add_x = CP_PS_PAPER_X/2 -
            d->mul * (x_max + x_min)/2;

        d->add_y = CP_PS_PAPER_Y/2 -
            d->mul * (y_max + y_min)/2;
    }
}

extern double cp_ps_x(cp_ps_xform_t const *d, double x)
{
    if (d == NULL) {
        d = &ps_mm;
    }
    return d->add_x + (x * d->mul);
}

extern double cp_ps_y(cp_ps_xform_t const *d, double y)
{
    if (d == NULL) {
        d = &ps_mm;
    }
    return d->add_y + (y * d->mul);
}

extern void cp_ps_doc_begin(
    cp_stream_t *s,
    size_t page_cnt,
    long x1, long y1, long x2, long y2)
{
    cp_printf(s,
        "%%!PS-Adobe-3.0\n"
        "%%%%Title: csg2plane\n"
        "%%%%Creator: csg2plane\n"
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

extern void cp_ps_page_begin(
    cp_stream_t *s,
    size_t page)
{
    cp_printf(s,
        "%%%%Page: %"_Pz"u %"_Pz"u\n"
        "save\n"
        "1 setlinecap\n"
        "1 setlinejoin\n"
        "0.4 setlinewidth\n"
        "0 setgray\n"
        "/Helvetica findfont 18 scalefont setfont\n",
        page, page);
}

extern void cp_ps_page_end(
    cp_stream_t *s)
{
    cp_printf(s,
        "restore\n"
        "showpage\n");
}
