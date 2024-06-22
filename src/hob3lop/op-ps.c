/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <stdlib.h>

#include <hob3lop/op-ps.h>

#ifdef PSTRACE

#define PS_MM2PT (72 / 25.4)
#define PS_MM(mm) ((mm) * PS_MM2PT)

/* a4 dimensions */
#define PS_MARGIN ((unsigned)PS_MM(20))

#define PS_WIDTH 596
#define PS_HEIGHT 843

#define PS_OVER PS_MM(10)

cq_ps_info_t cq_ps_info;

static unsigned const ps_left   = PS_MARGIN;
static unsigned const ps_right  = PS_WIDTH - PS_MARGIN;
static unsigned const ps_bottom = PS_MARGIN;
static unsigned const ps_top    = PS_HEIGHT - PS_MARGIN;

static char const *ps_doc_begin =
"%%!PS-Adobe-3.0\n"
"%%%%Title: test\n"
"%%%%Creator: hob3lop\n"
"%%%%Orientation: Portrait\n"
"%%%%Pages: atend\n"
"%%%%BoundingBox: 0 0 595 842\n"
"%%%%DocumentPaperSizes: a4\n"
"%%Magnification: 1.0000\n"
"%%%%EndComments\n";

static char const *ps_page_begin =
"%%%%Page: %u %u\n"
"save\n"
"1 setlinecap\n"
"1 setlinejoin\n"
"0.4 setlinewidth\n"
"0 setgray\n"
"/Helvetica findfont 14 scalefont setfont\n"
"newpath 0 0 moveto 0 842 lineto 595 842 lineto 595 0 lineto closepath clip\n";

#if 0
"30 30 moveto (TRI: bend bot) show\n"
"30 55 moveto (+19.48438 +21.51172) show\n"
"0 0.9 0.9 setrgbcolor 1 setlinewidth\n";
#endif

static char const *ps_page_end =
"restore\n"
"showpage\n";

static char const *ps_doc_end =
"%%%%Trailer\n"
"%%%%Pages: %u\n"
"%%%%EOF\n";

extern void cq_ps_init(
    cq_vec2_minmax_t const *bb)
{
    cq_ps_info.sub_x = bb->min.x;
    cq_ps_info.sub_y = bb->min.y;

    cq_dim_t sx = (bb->max.x - bb->min.x);
    cq_dim_t sy = (bb->max.y - bb->min.y);

    double px = ps_right - ps_left;
    double py = ps_top - ps_bottom;

    double mx = px / sx;
    double my = py / sy;
    double m = mx < my ? mx : my;

    cq_ps_info.mul_x = m;
    cq_ps_info.mul_y = m;
    cq_ps_info.left = ps_left;
    cq_ps_info.right = ps_right;
    cq_ps_info.bottom = ps_bottom;
    cq_ps_info.top = ps_top;
}

extern void cq_ps_doc_begin(char const *psfn_)
{
    snprintf(cq_ps_info.fn,  sizeof(cq_ps_info.fn),  "%s",     psfn_);
    snprintf(cq_ps_info.fn2, sizeof(cq_ps_info.fn2), "%s.new", psfn_);

    FILE *psf = fopen(cq_ps_info.fn2, "wt");
    if (psf == NULL) {
        fprintf(stderr, "%s: ERROR: unable to open: %m\n", cq_ps_info.fn2);
        exit(1);
    }
    fprintf(psf, ps_doc_begin);
    cq_ps_info.f = psf;
    cq_ps_info.page = 0;
}

extern void cq_ps_doc_end(void)
{
    if (cq_ps_info.f == NULL) {
        return;
    }
    fprintf(cq_ps_info.f, ps_doc_end, cq_ps_info.page);
    fclose(cq_ps_info.f);
    cq_ps_info.f = NULL;

    rename(cq_ps_info.fn2, cq_ps_info.fn);
}

extern void cq_ps_page_begin(void)
{
    if (cq_ps_info.f == NULL) {
        return;
    }
    unsigned page = ++cq_ps_info.page;
    fprintf(cq_ps_info.f, ps_page_begin, page, page);
}

extern void cq_ps_page_end(void)
{
    if (cq_ps_info.f == NULL) {
        return;
    }
    fprintf(cq_ps_info.f, ps_page_end);
}

extern double cq_ps_coord_x(
    double x)
{
    if ((long)x == CQ_DIM_MIN) {
        return ps_left - PS_OVER;
    }
    if ((long)x == CQ_DIM_MAX) {
        return ps_right + PS_OVER;
    }
    return cq_ps_info.left + ((x - cq_ps_info.sub_x) * cq_ps_info.mul_x);
}

extern double cq_ps_coord_y(
    double y)
{
    if ((long)y == CQ_DIM_MIN) {
        return ps_bottom - PS_OVER;
    }
    if ((long)y == CQ_DIM_MAX) {
        return ps_top + PS_OVER;
    }
    return cq_ps_info.bottom + ((y - cq_ps_info.sub_y) * cq_ps_info.mul_y);
}

static char const *ps_line_stroke =
"newpath %g %g moveto %g %g lineto stroke\n";

extern void cq_ps_line(double x1, double y1, double x2, double y2)
{
    if (cq_ps_info.f == NULL) {
        return;
    }
    fprintf(cq_ps_info.f, ps_line_stroke,
        cq_ps_coord_x(x1), cq_ps_coord_y(y1),
        cq_ps_coord_x(x2), cq_ps_coord_y(y2));
}

static char const *ps_box_stroke =
"newpath %g %g moveto %g %g lineto %g %g lineto %g %g lineto closepath stroke\n";



extern void cq_ps_box(double x1, double y1, double x2, double y2)
{
    if (cq_ps_info.f == NULL) {
        return;
    }
    fprintf(cq_ps_info.f, ps_box_stroke,
        cq_ps_coord_x(x1), cq_ps_coord_y(y1),
        cq_ps_coord_x(x1), cq_ps_coord_y(y2),
        cq_ps_coord_x(x2), cq_ps_coord_y(y2),
        cq_ps_coord_x(x2), cq_ps_coord_y(y1));
}

static char const *ps_dot_fill =
"newpath %g %g %g 0 360 arc closepath fill\n";

extern void cq_ps_dot(double x, double y, double radius)
{
    if (cq_ps_info.f == NULL) {
        return;
    }
    fprintf(cq_ps_info.f, ps_dot_fill,
        cq_ps_coord_x(x), cq_ps_coord_y(y), 3.0 * radius);
}

#endif /* PSTRACE */
