/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

/*
 * Parses SVG files and converts to internal CSG2 data structures.  The only
 * actual structure is cp_csg2_poly_t, i.e., a polygon, so this produces a
 * sequence of those.
 *
 * # Units
 *
 *   1 inch = 96px
 *   1 inch = 72pt
 *   1 inch = 6pc
 *
 *   angle: deg (360), grad (400), turn (1), rad (2pi), (default = deg)
 *
 * # Matrix Order
 *
 *   translate(50 60) scale(2 3) == matrix(2 0 0 3 50 60)
 *
 * For constructing a multi-path polygon with unique points, this
 * uses aux1 for the point index and aux2 for the path index.
 *
 * # OpenSCAD
 *
 * OpenSCAD uses libxml for parsing and gets the CSS handler practically for
 * free.  We may want to implement this, too.
 *
 * OpenSCAD has problems in its libsvg that may need some fixing:
 *
 * (1) no recursion is applied, although the structure of SVG with <g>
 * and embedded <svg> elements is recursive.  This is why an embedded
 * <svg> messes up the whole import: it redefines global scaling
 * settings, because the shapes are traversed as a list.

 * (2) No matrix multiplication is used for the combination of the
 * transforms, as if the concept was unknown.  Instead, at each
 * element, the elements and all its parent's transforms are parsed
 * and put into a vector of matrices.  And this vector of matrices is
 * then sequentially applied to each coordinate of each of the
 * computed paths.

 * If (1) and (2) were applied, a single matrix on the stack of the recursion
 * would always be the current transformation and only one matrix*vector
 * mul would have to be done.  (This is what is done here.)
 */

#include <hob3lbase/base-mat.h>
#include <hob3lbase/pool.h>
#include <hob3lbase/utf8.h>
#include <hob3lbase/arith.h>
#include <hob3l/syn-msg.h>
#include <hob3l/syn.h>
#include <hob3l/xml-parse.h>
#include <hob3l/svg-parse.h>
#include <hob3l/csg3.h>

typedef cp_csg3_ctxt_t ctxt_t;

typedef struct {
    cp_csg3_local_t u;
    cp_mat2wi_t mat;
    cp_detail_t detail;
    double width;
    double height;
    double dim;
    double dpi;
    bool toplevel;
} local_t;

typedef struct {
    cp_v_obj_p_t *r;
    ctxt_t const *p;
    local_t const *c;
    cp_csg2_poly_t *poly;
    unsigned path_idx;
    unsigned point_idx;
    cp_loc_t cur_loc;
    double init_x, init_y;
    double cur_x, cur_y;
    double ctrl_x, ctrl_y;
    char prev_cmd; /* 0 or 'Q' or 'C' */
} path_t;

char const cp_svg_ns_[] = "http://www.w3.org/2000/svg";

typedef enum {
    ABS = 0,
    REL = 1,
} rel_t;

typedef enum {
    SPACE = 0,
    COMMA = 1,
} comma_t;

/* Base Units */
#define U_IN_MM (25.4)
#define U_CM_MM (10.0)
#define U_IN_PX (96.0)
#define U_IN_PT (72.0)
#define U_IN_PC (6.0)

#define U_IN_DPI(dpi) ((dpi) + 0.0)

/* For initial coordinate system */
#define U_PX_MM       (U_IN_MM / U_IN_PX)
#define U_DPI_MM(dpi) (U_IN_MM / U_IN_DPI(dpi))

/* For user coordinate system */
#define U_PT_PX  (U_IN_PX / U_IN_PT)
#define U_PC_PX  (U_IN_PX / U_IN_PC)
#define U_MM_PX  (U_IN_PX / U_IN_MM)
#define U_CM_PX  (U_CM_MM * U_MM_PX)

/* diagnostics and error handling */

#define msg(c, ...) \
        _msg_aux(CP_GENSYM(_c), (c), __VA_ARGS__)

#define _msg_aux(c, _c, ...) \
    ({ \
        ctxt_t const *c = _c; \
        cp_syn_msg(c->syn, c->err, __VA_ARGS__); \
    })

#define TRY_AUX3(_res, cmd) \
    ({ \
        __typeof__((cmd)) _res = (cmd); \
        if (!_res) { return 0; } \
        _res; \
    })

#define TRY_AUX2(_cnt, cmd) TRY_AUX3(_res##_cnt, cmd)
#define TRY_AUX1(_cnt, cmd) TRY_AUX2(_cnt, cmd)

#define TRY(cmd) TRY_AUX1(__COUNTER__, cmd)

#define ERR(p, l, ...) \
    do{ \
        ctxt_t const *p_ = (p); \
        p_->err->loc = l; \
        cp_vchar_printf(&(p_)->err->msg, __VA_ARGS__); \
        return 0; \
    }while(0)

#define WARN(p, l, ...) \
    do { \
        msg(p, CP_ERR_WARN, l, NULL, __VA_ARGS__); \
    }while(0)

/* auxiliary functions */

static path_t path_new(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c)
{
    return (path_t){
        .r = r,
        .p = p,
        .c = c,
    };
}

static int cmp_vec2_loc(
    cp_vec2_loc_t const *a,
    cp_vec2_loc_t const *b,
    void *user CP_UNUSED)
{
    return cp_vec2_lex_cmp(&a->coord, &b->coord);
}

static inline double d_mag2(
    cp_mat2wi_t const *m)
{
    return sqrt(fabs(m->d));
}

/**
 * Get the $fn count for a quadratic bezier curve.
 * This is the number of rendered approximated lines of
 * the curve.
 */
static inline unsigned quad_get_fn(
    path_t *q,
    cp_vec2_t const *v0,
    cp_vec2_t const *v1,
    cp_vec2_t const *v2)
{
    /* very rough path length (overestimate) */
    double len = cp_vec2_dist(v0, v1) + cp_vec2_dist(v1, v2);

    /* the angle: what's the difference in direction */
    double angle = cp_vec2_angle(&CP_VEC2_SUB(v1, v0), &CP_VEC2_SUB(v2, v1));

    /* have a different max_fn than for circles */
    unsigned max_fn = 2 + ((q->p->opt->max_fn + 3) / 4);

    /* get $fn from CSG3 module */
    return cp_csg3_get_fn(
        &q->c->detail,
        2, max_fn, false,
        angle, len, d_mag2(&q->c->mat));
}

/**
 * Get the $fn count for a cubic bezier curve.
 * This is the number of rendered approximated lines of
 * the curve.
 */
static inline unsigned cub_get_fn(
    path_t *q,
    cp_vec2_t const *v0,
    cp_vec2_t const *v1,
    cp_vec2_t const *v2,
    cp_vec2_t const *v3)
{
    /* very rough path length (overestimate) */
    double len = cp_vec2_dist(v0, v1) + cp_vec2_dist(v1, v2) + cp_vec2_dist(v2, v3);

    /* have a different max_fn than for circles */
    unsigned max_fn = 2 + ((q->p->opt->max_fn + 1) / 2);

    /* the angle: what's the difference in direction */
    double angle = fmax(
        cp_vec2_angle(&CP_VEC2_SUB(v0, v1), &CP_VEC2_SUB(v1, v2)),
        cp_vec2_angle(&CP_VEC2_SUB(v1, v2), &CP_VEC2_SUB(v2, v3)));

    /* get $fn from CSG3 module */
    return cp_csg3_get_fn(
        &q->c->detail,
        2, max_fn, false,
        angle, len, d_mag2(&q->c->mat));
}

/**
 * Get the $fn count for an arc.
 * This is the number of rendered approximated lines of
 * the curve.
 * `angle` is in radians, not degrees.
 */
static unsigned arc_get_fn(
    path_t *q,
    double rx, double ry,
    double angle)
{
    /* very rough arc length (overestimate) */
    double flen = (rx > ry ? rx : ry) * CP_TAU;
    double len = flen / (angle / CP_TAU);

    /* get $fn from CSG3 module */
    return cp_csg3_get_fn(
        &q->c->detail,
        2, q->p->opt->max_fn, false,
        angle, len, d_mag2(&q->c->mat));
}

static inline void path_cur(
    path_t *p,
    cp_loc_t loc,
    double x,
    double y,
    char cmd,
    double ctrl_x,
    double ctrl_y)
{
    p->cur_loc = loc;
    p->cur_x = x;
    p->cur_y = y;
    p->prev_cmd = cmd;
    p->ctrl_x = ctrl_x;
    p->ctrl_y = ctrl_y;
}

static void path_close(
    path_t *p,
    cp_loc_t loc)
{
    path_cur(p, loc, p->init_x, p->init_y, 0,0,0);
    if (p->poly == NULL) {
        return;
    }
    if (p->point_idx == 0) {
        return;
    }
    p->point_idx = 0;
    p->path_idx++;
}

static void path_end(
    path_t *p,
    cp_loc_t loc)
{
    /* Our paths are always closed, so there is no difference here.
     * This is different from OpenSCAD, which does strokes for open
     * paths.
     * FIXME: possibly make this compatible with OpenSCAD, i.e.,
     * stroke this path (we may need better info on the end
     * direction for arcs and beziers, as the ends are currently
     * approximates, thus computed angles are imprecise).
     */
    path_close(p, loc);
}

static void path_uniquify(
    path_t *p)
{
    assert(p->poly != NULL);

    /* xform all points */
    cp_v_vec2_loc_t *q = &p->poly->point;
    for (cp_v_each(j, q)) {
        cp_vec2w_xform(&cp_v_nth(q, j).coord, &p->c->mat.n, &cp_v_nth(q,j).coord);
    }

    /* qsort points */
    cp_v_qsort(q, 0, CP_SIZE_MAX, cmp_vec2_loc, NULL);

    /* set indices of path, remove duplicates from point */
    size_t k = 0;
    cp_vec2_loc_t *qp = NULL;
    for (cp_v_each(j, q)) {
        cp_vec2_loc_t *qj = &cp_v_nth(q,j);
        size_t ref = k - 1;
        if ((qp == NULL) || (cp_vec2_lex_cmp(&qj->coord, &qp->coord) != 0)) {
            ref = k;
            cp_v_nth(q, k) = *qj;
            qp = qj;
            k++;
        }

        /* set path point */
        cp_v_ensure_size(&p->poly->path, qj->aux2 + 1);
        cp_v_size_t *u = &cp_v_nth(&p->poly->path, qj->aux2).point_idx;
        cp_v_ensure_size(u, qj->aux1 + 1);
        cp_v_nth(u, qj->aux1) = ref;
    }
    cp_v_set_size(q, k);
}

static void path_fini(
    path_t *p,
    cp_loc_t loc)
{
    path_end(p, loc);
    path_uniquify(p);

    if (p->poly->point.size < 3) {
        cp_v_clear(&p->poly->point, 0);
        cp_v_clear(&p->poly->path,  0);
        return;
    }

    p->poly = NULL;
}

static inline void path_push(
    path_t *p,
    cp_loc_t loc,
    double x,
    double y)
{
    /* ensure path */
    if (p->poly == NULL) {
        /* new polygon */
        p->poly = cp_csg2_new(*p->poly, loc);
        cp_v_push(p->r, cp_obj(p->poly));
        assert(p->point_idx == 0);
        assert(p->path_idx == 0);
    }

    /* push with current path/point index */
    cp_v_push(&p->poly->point,
        ((cp_vec2_loc_t){
            .coord = CP_VEC2(x, y),
            .loc = loc,
            .aux1 = p->point_idx,
            .aux2 = p->path_idx,
        }));

    /* increment point index */
    p->point_idx++;
}

static inline void path_start(
    path_t *p,
    cp_loc_t loc)
{
    if (p->point_idx == 0) {
        p->init_x = p->cur_x;
        p->init_y = p->cur_y;
        path_push(p, p->cur_loc ? p->cur_loc : loc, p->cur_x, p->cur_y);
    }
}

static inline void path_line_abs(
    path_t *p,
    cp_loc_t loc,
    double x,
    double y)
{
    if (cp_eq(p->cur_x, x) && cp_eq(p->cur_y, y)) {
        return;
    }
    path_start(p, loc);
    path_push(p, loc, x, y);
    path_cur(p, loc, x, y, 0,0,0);
}

static void path_line(
    path_t *p,
    cp_loc_t loc,
    rel_t rel,
    double x,
    double y)
{
    if (rel) {
        x += p->cur_x;
        y += p->cur_y;
    }
    path_line_abs(p, loc, x, y);
}

static void path_horiz(
    path_t *p,
    cp_loc_t loc,
    rel_t rel,
    double x)
{
    if (rel) {
        x += p->cur_x;
    }
    path_line_abs(p, loc, x, p->cur_y);
}

static void path_vert(
    path_t *p,
    cp_loc_t loc,
    rel_t rel,
    double y)
{
    if (rel) {
        y += p->cur_y;
    }
    path_line_abs(p, loc, p->cur_x, y);
}

static void path_move(
    path_t *p,
    cp_loc_t loc,
    rel_t rel,
    double x,
    double y)
{
    if (rel) {
        x += p->cur_x;
        y += p->cur_y;
    }
    path_end(p, loc);
    path_cur(p, loc, x, y, 0,0,0);
}

static inline void path_quad_abs(
    path_t *p,
    cp_loc_t loc,
    double x1,
    double y1,
    double x2,
    double y2)
{
    path_start(p, loc);
    unsigned n = quad_get_fn(p,
        &CP_VEC2(p->cur_x, p->cur_y), &CP_VEC2(x1,y1), &CP_VEC2(x2,y2));
    double nn = (double)n;
    for (unsigned i = 1; i < n; i++) {
        double t = i / nn;
        double x = cp_interpol2(p->cur_x, x1, x2, t);
        double y = cp_interpol2(p->cur_y, y1, y2, t);
        path_push(p, loc, x, y);
    }
    path_push(p, loc, x2, y2);
    path_cur(p, loc, x2, y2, 'Q', x1, y1);
}

static void path_quad(
    path_t *p,
    cp_loc_t loc,
    rel_t rel,
    double x1,
    double y1,
    double x2,
    double y2)
{
    if (rel) {
        x1 += p->cur_x;
        y1 += p->cur_y;
        x2 += p->cur_x;
        y2 += p->cur_y;
    }
    path_quad_abs(p, loc, x1, y1, x2, y2);
}

static void path_squad_abs(
    path_t *p,
    cp_loc_t loc,
    double x2,
    double y2)
{
    double x1 = p->cur_x;
    double y1 = p->cur_y;
    if (p->prev_cmd == 'Q') {
        x1 = (2 * p->cur_x) - p->ctrl_x;
        y1 = (2 * p->cur_y) - p->ctrl_y;
    }
    path_quad_abs(p, loc, x1, y1, x2, y2);
}

static void path_squad(
    path_t *p,
    cp_loc_t loc,
    rel_t rel,
    double x2,
    double y2)
{
    if (rel) {
        x2 += p->cur_x;
        y2 += p->cur_y;
    }
    path_squad_abs(p, loc, x2, y2);
}

static inline void path_cub_abs(
    path_t *p,
    cp_loc_t loc,
    double x1,
    double y1,
    double x2,
    double y2,
    double x3,
    double y3)
{
    path_start(p, loc);
    unsigned n = cub_get_fn(p,
        &CP_VEC2(p->cur_x, p->cur_y), &CP_VEC2(x1,y1), &CP_VEC2(x2,y2), &CP_VEC2(x3,y3));
    double nn = (double)n;
    for (unsigned i = 1; i < n; i++) {
        double t = i / nn;
        double x = cp_interpol3(p->cur_x, x1, x2, x3, t);
        double y = cp_interpol3(p->cur_y, y1, y2, y3, t);
        path_push(p, loc, x, y);
    }
    path_push(p, loc, x3, y3);
    path_cur(p, loc, x3, y3, 'C', x2, y2);
}

static void path_cub(
    path_t *p,
    cp_loc_t loc,
    rel_t rel,
    double x1,
    double y1,
    double x2,
    double y2,
    double x3,
    double y3)
{
    if (rel) {
        x1 += p->cur_x;
        y1 += p->cur_y;
        x2 += p->cur_x;
        y2 += p->cur_y;
        x3 += p->cur_x;
        y3 += p->cur_y;
    }
    path_cub_abs(p, loc, x1, y1, x2, y2, x3, y3);
}

static void path_scub_abs(
    path_t *p,
    cp_loc_t loc,
    double x2,
    double y2,
    double x3,
    double y3)
{
    double x1 = p->cur_x;
    double y1 = p->cur_y;
    if (p->prev_cmd == 'C') {
        x1 = (2 * p->cur_x) - p->ctrl_x;
        y1 = (2 * p->cur_y) - p->ctrl_y;
    }
    path_cub_abs(p, loc, x1, y1, x2, y2, x3, y3);
}

static void path_scub(
    path_t *p,
    cp_loc_t loc,
    rel_t rel,
    double x2,
    double y2,
    double x3,
    double y3)
{
    if (rel) {
        x2 += p->cur_x;
        y2 += p->cur_y;
        x3 += p->cur_x;
        y3 += p->cur_y;
    }
    path_scub_abs(p, loc, x2, y2, x3, y3);
}

static inline void path_arc2_abs(
    path_t *p,
    cp_loc_t loc,
    double rx,
    double ry,
    double th1,
    double dth,
    double cx,
    double cy,
    cp_mat2_t const *rot)
{
    if (cp_eq(rx,0) || cp_eq(ry,0) || cp_eq(dth,0)) {
        return;
    }
    path_start(p, loc);
    unsigned n = arc_get_fn(p, rx, ry, dth);

    double nn = (double)n;
    for (unsigned i = 1; i < n; i++) {
        double t = i / nn;

        /* B.2.2: eq.3.1 */
        double th = th1 + (dth * t);
        cp_vec2_t v = CP_VEC2_MULC(&CP_COSSIN_RAD(th), &CP_VEC2(rx, ry));
        if (rot) {
            cp_vec2_xform(&v, rot, &v);
        }
        cp_vec2_add(&v, &v, &CP_VEC2(cx, cy));
        path_push(p, loc, v.x, v.y);
    }
}

static inline void path_arc_abs(
    path_t *p,
    cp_loc_t loc,
    double rx,
    double ry,
    double ph, /* in degrees [0..360] */
    bool fa,
    bool fs,
    double x2,
    double y2)
{
    /* Implemented according to:
     * https://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes
     * B.2.4 Conversion From Endpoint To Center Parameterization
     * B.2.5 Corrections of Radii
     */

    /* 9.5.1 */
    if (cp_eq(p->cur_x, x2) && cp_eq(p->cur_y, y2)) {
        return;
    }

    /* B.2.5 step 1, also 9.5.1 */
    if (cp_eq(rx,0) || cp_eq(ry,0)) {
        path_line_abs(p, loc, x2, y2);
        return;
    }

    /* B.2.5 step 2: eq.6.1, also 9.5.1 */
    rx = fabs(rx);
    ry = fabs(ry);

    /* B.2.4 step 1: eq.5.1 */
    double x1 = p->cur_x;
    double y1 = p->cur_y;
    cp_vec2_t sc = CP_SINCOS_DEG(ph);
    cp_vec2_t v1p;
    cp_vec2_xform(&v1p, &CP_MAT2(sc.y, sc.x, -sc.x, sc.y), &CP_VEC2((x1 - x2)/2, (y1 - y2)/2));
    double x1p2 = cp_sqr(v1p.x);
    double y1p2 = cp_sqr(v1p.y);

    /* B.2.5 step 3: eq.6.2, also 9.5.1 */
    double la = (x1p2 / cp_sqr(rx)) + (y1p2 / cp_sqr(ry));
    if (la > 1.0) {
        double sqrtla = sqrt(la);
        rx *= sqrtla;
        ry *= sqrtla;
    }

    /* B.2.4 step 2: eq.5.2 */
    double rx2 = cp_sqr(rx);
    double ry2 = cp_sqr(ry);
    double num = fmax(0, (rx2 * ry2) - (rx2 * y1p2) - (ry2 * x1p2));
    double den = (rx2 * y1p2) + (ry2 * x1p2);
    double coef = sqrt(cp_div0(num, den));
    if (fa == fs) { coef = -coef; }
    double cxp = +coef * cp_div0(rx * v1p.y, ry);
    double cyp = -coef * cp_div0(ry * v1p.x, rx);

    /* B.2.4 step 3: eq.5.3 */
    cp_mat2_t rot = CP_MAT2(sc.y, -sc.x, sc.x, sc.y);
    cp_vec2_t c;
    cp_vec2_xform(&c, &rot, &CP_VEC2(cxp, cyp));
    cp_vec2_add(&c, &c, &CP_VEC2((x1 + x2)/2, (y1 + y2)/2));

    /* B.2.4 step 4 */
    /* eq.5.5 */
    cp_vec2_t g = CP_VEC2(cp_div0(v1p.x - cxp, rx), cp_div0(v1p.y - cyp, ry));
    double th1 = cp_vec2_angle(&CP_VEC2(1,0), &g);

    /* eq.5.6 */
    cp_vec2_t h = CP_VEC2(cp_div0(-v1p.x - cxp, rx), cp_div0(-v1p.y - cyp, ry));
    double dth = cp_vec2_angle(&g, &h);
    if (!fs && (dth > 0)) { dth -= CP_TAU; }
    if (fs  && (dth < 0)) { dth += CP_TAU; }

    path_arc2_abs(p, loc, rx, ry, th1, dth, c.x, c.y, &rot);
    path_push(p, loc, x2, y2);
    path_cur(p, loc, x2,y2, 0,0,0);
}

static void path_arc(
    path_t *p,
    cp_loc_t loc,
    rel_t rel,
    double rx,
    double ry,
    double ph,
    bool fa,
    bool fs,
    double x,
    double y)
{
    if (rel) {
        x += p->cur_x;
        y += p->cur_y;
    }
    path_arc_abs(p, loc, rx, ry, ph, fa, fs, x, y);
}

static double match_align(
    char const *haystack,
    char const *needle_min,
    char const *needle_mid,
    char const *needle_max)
{
    if (strstr(haystack, needle_min)) { return 0; }
    if (strstr(haystack, needle_mid)) { return 0.5; }
    if (strstr(haystack, needle_max)) { return 1; }
    return -1;
}

static char const *s_sep(
    char const **sp,
    comma_t comma)
{
    for (;;) {
        char const *s = *sp;
        unsigned u = cp_utf8_take(&s);
        if (comma && (u == ',')) {
            comma = false;
        }
        else
        if (!cp_xml_is_space(u)) {
            return *sp;
        }
        *sp = s;
    }
}

static bool s_end(
    char const **sp)
{
    s_sep(sp, SPACE);
    return **sp == 0;
}

static bool s_expect_raw(
    char const **sp,
    char const *n)
{
    *sp += TRY(strpref(*sp, n));
    return true;
}

static inline bool s_expect(
    char const **sp,
    char const *n)
{
    s_sep(sp, SPACE);
    return s_expect_raw(sp, n);
}

/**
 * This parses a unit, without skipping space.
 *
 * This returns 1.0 if no unit was found, which is equal to
 * the unit 'px'.  To distinguish from an explicit 'px' unit,
 * `*sp` can be checked before and after the call.
 */
static inline double s_unit_dim(
    char const **sp,
    double perc100,
    double unit1)
{
    switch (**sp) {
    case '%':
        (*sp)++;
        return perc100 / 100.0;

    case 'i':
        if (s_expect_raw(sp, "in")) {
            return U_IN_PX;
        }
        break;

    case 'm':
        if (s_expect_raw(sp, "mm")) {
            return U_MM_PX;
        }
        break;

    case 'c':
        if (s_expect_raw(sp, "cm")) {
            return U_CM_PX;
        }
        break;

    case 'p':
        if (s_expect_raw(sp, "px")) {
            return 1.0;
        }
        if (s_expect_raw(sp, "pt")) {
            return U_PT_PX;
        }
        if (s_expect_raw(sp, "pc")) {
            return U_PC_PX;
        }
        break;
    }

    return unit1;
}

/** A coordinate (or a comma) follows */
static bool looking_at_float(
    char const **sp)
{
    s_sep(sp, SPACE);
    char c = **sp;
    return
        (c == ',') ||
        (c == '.') || (c == '-') || (c == '+') ||
        ((c >= '0') && (c <= '9'));
}

/**
 * Parses a potential floating point number.
 *
 * Updates *sp to skip space.
 * Returns NULL on failure.
 * Otherwise, returns the pointer to the char past the end of the number.
 */
static char const *s_str_float(
    char const **sp,
    comma_t comma)
{
    /* We are very forgiving with commas */
    s_sep(sp, comma);

    /* mark old begin to be able to abort */
    char const *s = *sp;

    /* parse sign */
    (void)(s_expect_raw(&s, "-") || s_expect_raw(&s, "+"));

    /* parse zero or one dot and one or more digits
     * I found no format specification whether "1." is a
     * valid coordinate.  We assume it is.  For ".5", OTOH,
     * there are examples in the docs I found.
     */
    bool have_dot = false;
    bool have_digit = false;
    for (;;) {
        if ((*s >= '0') && (*s <= '9')) {
            have_digit = true;
            s++;
        }
        else
        if (!have_dot && (*s == '.')) {
            have_dot = true;
            s++;
        }
        else {
            break;
        }
    }
    TRY(have_digit);

    /* Parse optional exponent.
     *
     * This does not seem to be strict SVG 2 syntax, but faulty or
     * sloppy SVG writers may produce numbers like '-1e+3', and we
     * want to parse them.  There is no conflict with current path
     * commands, because 'e' and 'E' are no command letters.  Hex
     * floats (e.g., '0x1p2') are not parsed.
     *
     * Also, SVG1.1 has the exponent syntax, so we also support it here.
     */
    if ((*s == 'e') || (*s == 'E')) {
        char const *t = s;
        t++;
        (void)(s_expect_raw(&s, "-") || s_expect_raw(&s, "+"));
        bool have_exp = false;
        while ((*t >= '0') && (*t <= '9')) {
            have_exp = true;
            t++;
        }
        if (have_exp) {
            s = t; /* add exp to string */
        }
    }

    /* success */
    return s;
}

static bool s_double(
    double *rp,
    char const **sp,
    comma_t comma)
{
    char const *se = TRY(s_str_float(sp, comma));
    char *ep = NULL;
    double r = strtod(*sp, &ep);
    TRY(ep == se);
    *rp = r;
    *sp = se;
    return true;
}

static bool s_bool(
    bool *rp,
    char const **sp,
    comma_t comma)
{
    s_sep(sp, comma);
    if (**sp == '0') {
        (*sp)++;
        *rp = false;
        return true;
    }
    if (**sp == '1') {
        (*sp)++;
        *rp = true;
        return true;
    }
    return true;
}

static inline bool s_coord(
    double *xp, double *yp,
    char const **sp,
    comma_t comma)
{
    return
       s_double(xp, sp, comma) && s_double(yp, sp, COMMA);
}

static inline bool s_coord2(
    double *x1p, double *y1p,
    double *x2p, double *y2p,
    char const **sp,
    comma_t comma)
{
    return
       s_double(x1p, sp, comma) && s_double(y1p, sp, COMMA) &&
       s_double(x2p, sp, COMMA) && s_double(y2p, sp, COMMA);
}

static inline bool s_coord3(
    double *x1p, double *y1p,
    double *x2p, double *y2p,
    double *x3p, double *y3p,
    char const **sp,
    comma_t comma)
{
    return
       s_double(x1p, sp, comma) && s_double(y1p, sp, COMMA) &&
       s_double(x2p, sp, COMMA) && s_double(y2p, sp, COMMA) &&
       s_double(x3p, sp, COMMA) && s_double(y3p, sp, COMMA);
}

static inline bool s_arc(
    double *rxp, double *ryp,
    double *php,
    bool *fap, bool *fsp,
    double *xp, double *yp,
    char const **sp,
    comma_t comma)
{
    return
       s_double(rxp, sp, comma) && s_double(ryp, sp, COMMA) &&
       s_double(php, sp, COMMA) &&
       s_bool(fap, sp, COMMA) && s_bool(fsp, sp, COMMA) &&
       s_double(xp, sp, COMMA) && s_double(yp, sp, COMMA);
}

static void svg_xform(
    cp_mat2wi_t *m,
    double vx, double vy, double vw, double vh,
    double ex, double ey, double ew, double eh,
    char const *pas)
{
    if (pas == NULL) { pas = ""; }

    s_expect(&pas, "defer");

    /* 0 = min, 0.5 = mid, 1 = max, <0 = none */
    double align_x = match_align(pas, "xMin", "xMid", "xMax");
    double align_y = match_align(pas, "YMin", "YMid", "YMax");
    bool do_align = (align_x < 0) && (align_y < 0);
    bool slice = (strstr(pas, "slice") != NULL);

    double scale_x = cp_div0(ew, vw);
    double scale_y = cp_div0(eh, vh);

    if (do_align) {
        if ((scale_x < scale_y) ^ slice) {
            scale_y = scale_x;
        }
        else {
            scale_x = scale_y;
        }
    }

    double xlat_x = ex - (vx * scale_x);
    double xlat_y = ey - (vy * scale_y);
    if (do_align) {
        xlat_x += (ew - (vw * scale_x)) * align_x;
        xlat_y += (eh - (vh * scale_y)) * align_y;
    }

    cp_mat2wi_scale(m, scale_x, scale_y);
    m->n.w.x = xlat_x;
    m->n.w.y = xlat_y;
    m->i.w.x = cp_div0(xlat_x, scale_x);
    m->i.w.y = cp_div0(xlat_y, scale_y);
}

static char const *s_attr_str(
    cp_xml_t *x,
    char const *name)
{
    x = TRY(cp_xml_find(x, CP_XML_ATTR, name, NULL));
    char const *s = TRY(TRY(x->child)->data);
    return s;
}

static bool s_dim(
    double *rp,
    char const **sp,
    double perc100,
    double unit1,
    comma_t comma)
{
    s_sep(sp, comma);
    char const *s = *sp;
    double r;
    TRY(s_double(&r, &s, SPACE));

    double u = s_unit_dim(&s, perc100, unit1);

    *sp = s;
    *rp = r * u;
    return true;
}

static bool s_attr_dim(
    double *rp,
    cp_xml_t *x,
    char const *name,
    double perc100,
    double unit1)
{
    char const *s = TRY(s_attr_str(x, name));
    double r;
    TRY(s_dim(&r, &s, perc100, unit1, SPACE) && s_end(&s));
    *rp = r;
    return true;
}

static bool s_attr_coord(
    double *xp, double *yp,
    local_t const *c,
    cp_xml_t *xml,
    char const *namex, char const *namey)
{
    *xp = 0.0;
    *yp = 0.0;
    return
        s_attr_dim(xp, xml, namex, c->width,  1) &&
        s_attr_dim(yp, xml, namey, c->height, 1);
}

static bool s_attr_size(
    double *wp, double *hp,
    local_t const *c,
    cp_xml_t *xml,
    char const *namew, char const *nameh)
{
    *wp = 0.0;
    *hp = 0.0;

    char const *sw = s_attr_str(xml, namew);
    if (strequ0(sw, "auto")) { sw = NULL; }

    char const *sh = s_attr_str(xml, nameh);
    if (strequ0(sh, "auto")) { sh = NULL; }

    /* both are auto => both are assumed 0 */
    if (!sw && !sh) {
        return true;
    }

    /* both are given: try to get explicit values */
    if (sw && sh) {
        double w,h;
        TRY(s_dim(&w, &sw, c->width,  1, SPACE) && s_end(&sw));
        TRY(s_dim(&h, &sh, c->height, 1, SPACE) && s_end(&sh));
        *wp = w;
        *hp = h;
    }
    else if (!sw) {
        /* width is auto */
        double h;
        TRY(s_dim(&h, &sh, c->height, 1, SPACE) && s_end(&sh));
        *wp = h;
        *hp = h;
    }
    else {
        /* height is auto */
        assert(!sh);
        double w;
        TRY(s_dim(&w, &sw, c->width,  1, SPACE) && s_end(&sw));
        *wp = w;
        *hp = w;
    }

    return true;
}

static bool str_path_val1(
    path_t *q,
    char const **sp,
    rel_t rel,
    void (*render)(path_t *, cp_loc_t, rel_t, double))
{
    char const *loc = s_sep(sp, SPACE);
    double v;
    if (!s_double(&v, sp, SPACE)) {
        WARN(q->p, *sp, "Coordinate expected.");
        return false;
    }
    render(q, loc, rel, v);
    while (looking_at_float(sp)) {
        loc = *sp;
        if (!s_double(&v, sp, COMMA)) {
            WARN(q->p, *sp, "Coordinate expected.");
            return false;
        }
        render(q, loc, rel, v);
    }
    return true;
}

static bool str_path_coord1(
    path_t *q,
    char const **sp,
    rel_t rel,
    void (*first)(path_t *, cp_loc_t, rel_t, double, double),
    void (*other)(path_t *, cp_loc_t, rel_t, double, double))
{
    char const *loc = s_sep(sp, SPACE);
    double x, y;
    if (!s_coord(&x, &y, sp, SPACE)) {
        WARN(q->p, *sp, "Coordinates expected.");
        return false;
    }
    first(q, loc, rel, x, y);
    while (looking_at_float(sp)) {
        loc = *sp;
        if (!s_coord(&x, &y, sp, COMMA)) {
            WARN(q->p, *sp, "Coordinates expected.");
            return false;
        }
        other(q, loc, rel, x, y);
    }
    return true;
}

static bool str_path_coord2(
    path_t *q,
    char const **sp,
    rel_t rel,
    void (*render)(path_t *, cp_loc_t, rel_t, double, double, double, double))
{
    char const *loc = s_sep(sp, SPACE);
    double x1, y1, x2, y2;
    bool ok = s_coord2(&x1, &y1, &x2, &y2, sp, SPACE);
    for (;;) {
        if (!ok) {
            WARN(q->p, *sp, "Coordinate pair expected.");
            return false;
        }
        render(q, loc, rel, x1, y1, x2, y2);
        if (!looking_at_float(sp)) {
            return true;
        }
        loc = *sp;
        ok = s_coord2(&x1, &y1, &x2, &y2, sp, COMMA);
    }
}

static bool str_path_coord3(
    path_t *q,
    char const **sp,
    rel_t rel,
    void (*render)(path_t *, cp_loc_t, rel_t, double, double, double, double, double, double))
{
    char const *loc = s_sep(sp, SPACE);
    double x1, y1, x2, y2, x3, y3;
    bool ok = s_coord3(&x1, &y1, &x2, &y2, &x3, &y3, sp, SPACE);
    for (;;) {
        if (!ok) {
            WARN(q->p, *sp, "Coordinate triplet expected.");
            return false;
        }
        render(q, loc, rel, x1, y1, x2, y2, x3, y3);
        if (!looking_at_float(sp)) {
            return true;
        }
        loc = *sp;
        ok = s_coord3(&x1, &y1, &x2, &y2, &x3, &y3, sp, COMMA);
    }
}

static bool str_path_arc(
    path_t *q,
    char const **sp,
    rel_t rel)
{
    char const *loc = s_sep(sp, SPACE);
    double rx, ry, ph, x, y;
    bool fa = false, fs = false;
    bool ok = s_arc(&rx, &ry, &ph, &fa, &fs, &x, &y, sp, SPACE);
    for (;;) {
        if (!ok) {
            WARN(q->p, *sp, "Arc parameters expected.");
            return false;
        }
        path_arc(q, loc, rel, rx, ry, ph, fa, fs, x, y);
        if (!looking_at_float(sp)) {
            return true;
        }
        loc = *sp;
        ok = s_arc(&rx, &ry, &ph, &fa, &fs, &x, &y, sp, COMMA);
    }
}

static bool str_path(
    path_t *q,
    char const *s)
{
    while (!s_end(&s)) {
        char const *loc = s;
        rel_t rel = (*s >= 'a') && (*s <= 'z');
        switch (*(s++)) {
        case 'Z': case 'z': path_close(q, loc); break;
        case 'M': case 'm': TRY(str_path_coord1(q, &s, rel, path_move, path_line));  break;
        case 'L': case 'l': TRY(str_path_coord1(q, &s, rel, path_line, path_line));  break;
        case 'H': case 'h': TRY(str_path_val1  (q, &s, rel, path_horiz)); break;
        case 'V': case 'v': TRY(str_path_val1  (q, &s, rel, path_vert));  break;
        case 'Q': case 'q': TRY(str_path_coord2(q, &s, rel, path_quad));  break;
        case 'C': case 'c': TRY(str_path_coord3(q, &s, rel, path_cub));   break;
        case 'T': case 't': TRY(str_path_coord1(q, &s, rel, path_squad, path_squad)); break;
        case 'S': case 's': TRY(str_path_coord2(q, &s, rel, path_scub)); break;
        case 'A': case 'a': TRY(str_path_arc   (q, &s, rel)); break;
        default:
            WARN(q->p, loc, "Unrecognised path command '%c'", *loc);
            return false;
        }
    }
    return true;
}

static void xform_apply(
    local_t const **c,
    local_t *n,
    cp_mat2wi_t const *m1)
{
    if (n != *c) {
        *n = **c;
        *c = n;
    }
    cp_mat2wi_mul(&n->mat, &n->mat, m1);
}

static bool xform_cmd(
    local_t const **c,
    local_t *n,
    ctxt_t const *p,
    char const **sp)
{
    char const *s = *sp;

    char const *cmd = s;
    while (syn_is_alpha(*s)) {
        s++;
    }
    char const *cmd_end = s;
    size_t cmd_len = CP_MONUS(cmd_end, cmd);

    /* parse params (always numbers) */
    TRY(s_expect(&s, "("));
    double val[8] = {0};
    TRY(s_double(&val[0], &s, SPACE));
    size_t val_n = 1;
    while ((val_n < cp_countof(val)) && s_double(&val[val_n], &s, COMMA)) {
        val_n++;
    }
    TRY(s_expect(&s, ")"));

    /* successfully parsed */
    *sp = s;

    cp_mat2wi_t m1;
    switch (*cmd) {
    case 't':
        if (strnequ(cmd, "translate", cmd_len)) {
            if (val_n > 2) {
                WARN(p, cmd, "Too many arguments for 'translate': %zu.", val_n);
                return false;
            }
            cp_mat2wi_xlat(&m1, val[0], val[1]);
            xform_apply(c, n, &m1);
            return true;
        }
        break;

    case 's':
        if (strnequ(cmd, "scale", cmd_len)) {
            if (val_n > 2) {
                WARN(p, cmd, "Too many arguments for 'scale': %zu.", val_n);
                return false;
            }
            if (val_n < 2) {
                val[1] = val[0];
            }
            cp_mat2wi_scale(&m1, val[0], val[1]);
            xform_apply(c, n, &m1);
            return true;
        }
        if (strnequ(cmd, "skewX", cmd_len)) {
            if (val_n > 1) {
                WARN(p, cmd, "Too many arguments for 'skewX': %zu.", val_n);
                return false;
            }
            cp_mat2wi_from_mat2w(&m1,
                &CP_MAT2W(
                    1, cp_tan_deg(val[0]), 0,
                    0, 1,                  0));
            xform_apply(c, n, &m1);
            return true;
        }
        if (strnequ(cmd, "skewY", cmd_len)) {
            if (val_n > 1) {
                WARN(p, cmd, "Too many arguments for 'skewY': %zu.", val_n);
                return false;
            }
            cp_mat2wi_from_mat2w(&m1,
                &CP_MAT2W(
                    1,                  0, 0,
                    cp_tan_deg(val[0]), 1, 0));
            xform_apply(c, n, &m1);
            return true;
        }
        break;

    case 'r':
        if (strnequ(cmd, "rotate", cmd_len)) {
            if (val_n > 3) {
                WARN(p, cmd, "Too many arguments for 'rotate': %zu.", val_n);
                return false;
            }
            if (val_n >= 3) {
                cp_mat2wi_xlat(&m1, val[1], val[2]);
                xform_apply(c, n, &m1);
            }
            cp_mat2wi_rot(&m1, &CP_SINCOS_DEG(val[0]));
            xform_apply(c, n, &m1);
            if (val_n >= 3) {
                cp_mat2wi_xlat(&m1, -val[1], -val[2]);
                xform_apply(c, n, &m1);
            }
            return true;
        }
        break;

    case 'm':
        if (strnequ(cmd, "matrix", cmd_len)) {
            if (val_n > 6) {
                WARN(p, cmd, "Too many arguments for 'matrix': %zu.", val_n);
                return false;
            }
            if (!cp_mat2wi_from_mat2w(&m1,
                &CP_MAT2W(
                    val[0], val[2], val[4],
                    val[1], val[3], val[5])))
            {
                WARN(p, cmd, "Matrix is invalid as determinant is 0.");
            }
            xform_apply(c, n, &m1);
            return true;
        }
        break;
    }

    WARN(p, cmd, "Unrecognised transform command: '%.*s'", (int)cmd_len, cmd);
    return false;
}

static void xform_try(
    local_t const **c,
    local_t *n,
    ctxt_t const *p,
    cp_xml_t *xml)
{
    char const *s = s_attr_str(xml, "transform");
    if (s == NULL) {
        return;
    }
    s_sep(&s, SPACE);
    while (*s != 0) {
        if (!xform_cmd(c, n, p, &s)) {
            return;
        }
        s_sep(&s, COMMA);
    }
}

static bool csg2_from_elem(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *xml);

static bool csg2_from_svg(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c0,
    cp_xml_t *x)
{
    // FIXME: implement centering, but not by coordinates, but by centering
    // the viewBox.

    /* The 'transform' on <svg> seems to be a bit tricky.  OpenSCAD seems to
     * ignore it at toplevel (always?) and embedded <svg> does not really seem
     * to work either.
     * Also, I am not sure about the correct behaviour at top-level, because in
     * Firefox, the transform seems to be influenced by what the SVG is embedded
     * into -- it's never completely absolute and independent.
     */
    double unit1 = 96.0 / c0->dpi;

    double ew = c0->width;
    bool have_ew = s_attr_dim(&ew, x, "width", c0->width, unit1) || (c0->width > 0);
    if (ew <= 0) { have_ew = false; }

    double eh = c0->height;
    bool have_eh = s_attr_dim(&eh, x, "height", c0->height, unit1) || (c0->height > 0);
    if (eh <= 0) { have_eh = false; }
    bool have_size = have_ew && have_eh;

    char const *vs = s_attr_str(x, "viewBox");
    double vx, vy, vw, vh;
    bool have_viewBox = (vs != NULL) &&
        s_dim(&vx, &vs, c0->width,  1, SPACE) &&
        s_dim(&vy, &vs, c0->height, 1, COMMA) &&
        s_dim(&vw, &vs, c0->width,  1, COMMA) &&
        s_dim(&vh, &vs, c0->height, 1, COMMA) &&
        s_end(&vs) &&
        (vw > 0) && (vh > 0);

    char const *pas = s_attr_str(x, "preserveAspectRatio");

    /* open a new context */
    local_t c1 = *c0;
    c1.toplevel = false;
    c1.dpi = 96; /* in non-toplevel contexts, DPI is again 96 as in SVG */
    local_t const *c = &c1;

    /* do the shift if this is not top-level */
    double px, py;
    (void)s_attr_coord(&px, &py, c, x, "x", "y");
    if (!c0->toplevel) {
        cp_mat2wi_t m1;
        cp_mat2wi_xlat(&m1, px, py);
        xform_apply(&c, &c1, &m1);
    }

    /* do the transform */
    xform_try(&c, &c1, p, x);

    /* if we have a viewBox, we'll apply the DPI setting unless
     * there is a size (which takes the unit into account) */
    if (have_viewBox && c0->toplevel) {
        cp_mat2wi_t m1;
        cp_mat2wi_scale1(&m1, have_size ? U_PX_MM : U_DPI_MM(c0->dpi));
        cp_mat2wi_mul(&c1.mat, &c1.mat, &m1);
    }

    /* scale only if we have all params: we have no external size limit */
    if (have_size && have_viewBox) {
        c1.width = vw;
        c1.height = vh;
        c1.dim = sqrt((vw*vw + vh*vh) / 2);

        cp_mat2wi_t m1;
        svg_xform(&m1, vx, vy, vw, vh, 0, 0, ew, eh, pas);
        cp_mat2wi_mul(&c1.mat, &c1.mat, &m1);
    }

    /* At top level, have a coordinate shift to put the SVG in
     * positive coordinates */
    if (c0->toplevel) {
        if (have_viewBox) {
            cp_mat2wi_t m1;
            cp_mat2wi_xlat(&m1, 0, -vh);
            cp_mat2wi_mul(&c1.mat, &c1.mat, &m1);
        }
    }

    /* recurse */
    return csg2_from_elem(r, p, c, x);
}

static bool csg2_from_g(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *x)
{
    local_t c1;
    xform_try(&c, &c1, p, x);

    return csg2_from_elem(r, p, c, x);
}

static bool csg2_from_path(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *x)
{
    local_t c1;
    xform_try(&c, &c1, p, x);

    char const *s = TRY(s_attr_str(x, "d"));
    path_t q = path_new(r, p, c);
    (void)str_path(&q, s);
    path_fini(&q, s);
    return true;
}

static bool csg2_from_rect(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *x)
{
    local_t c1;
    xform_try(&c, &c1, p, x);

    double px, py;
    (void)s_attr_coord(&px, &py, c, x, "x", "y");

    double width, height;
    /* no 'auto' for width/height for <rect>, at least not in my web browser */
    (void)s_attr_coord(&width, &height, c, x, "width", "height");
    if ((width <= 0) || (height <= 0)) {
        return true;
    }

    double rx, ry;
    (void)s_attr_size(&rx, &ry, c, x, "rx", "ry");
    if ((rx <= 0) || (ry <= 0)) {
        rx = 0;
        ry = 0;
    }
    else {
        if (rx > (width / 2)) {
            rx = width / 2;
        }
        if (ry > (height / 2)) {
            ry = height / 2;
        }
        width  -= 2*rx;
        height -= 2*ry;
    }

    path_t q = path_new(r, p, c);
    path_move (&q, x->loc, ABS, px + rx, py);
    path_line (&q, x->loc, REL, width, 0);
    path_arc  (&q, x->loc, REL, rx, ry, 0,0,1, rx, ry);
    path_line (&q, x->loc, REL, 0, height);
    path_arc  (&q, x->loc, REL, rx, ry, 0,0,1, -rx, ry);
    path_line (&q, x->loc, REL, -width, 0);
    path_arc  (&q, x->loc, REL, rx, ry, 0,0,1, -rx, -ry);
    path_line (&q, x->loc, REL, 0, -height);
    path_arc  (&q, x->loc, REL, rx, ry, 0,0,1, rx, -ry);
    path_close(&q, x->loc);
    path_fini (&q, x->loc);
    return true;
}

static bool csg2_from_circle(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *x)
{
    local_t c1;
    xform_try(&c, &c1, p, x);

    double cx, cy;
    (void)s_attr_coord(&cx, &cy, c, x, "cx", "cy");
    double rr = 0.0;
    (void)s_attr_dim(&rr, x, "r", c->dim, 1);
    if (rr <= 0) {
        char const *str = s_attr_str(x, "r");
        WARN(p, str ? str : x->loc, "Illegal radius for circle.");
        return false;
    }

    path_t q = path_new(r, p, c);
    path_move(&q, x->loc, ABS, cx + rr, cy);
    path_arc2_abs(&q, x->loc, rr, rr, 0, CP_TAU, cx, cy, NULL);
    path_close(&q, x->loc);
    path_fini(&q, x->loc);
    return true;
}

static bool csg2_from_ellipse(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *x)
{
    local_t c1;
    xform_try(&c, &c1, p, x);

    double cx, cy;
    (void)s_attr_coord(&cx, &cy, c, x, "cx", "cy");
    double rx, ry;
    (void)s_attr_size(&rx, &ry, c, x, "rx", "ry");
    if (rx <= 0) {
        char const *str = s_attr_str(x, "rx");
        WARN(p, str ? str : x->loc, "Illegal x radius for ellipse.");
        return false;
    }
    if (ry <= 0) {
        char const *str = s_attr_str(x, "ry");
        WARN(p, str ? str : x->loc, "Illegal y radius for ellipse.");
        return false;
    }

    path_t q = path_new(r, p, c);
    path_move(&q, x->loc, ABS, cx + rx, cy);
    path_arc2_abs(&q, x->loc, rx, ry, 0, CP_TAU, cx, cy, NULL);
    path_close(&q, x->loc);
    path_fini(&q, x->loc);
    return true;
}

static bool csg2_from_line(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *x)
{
    local_t c1;
    xform_try(&c, &c1, p, x);

    /* This is implemented correctly as a two point line, but since we never stroke,
     * this always ends up being discarded.  Maybe some day, we will implement stroke. */
    double x1, y1;
    TRY(s_attr_coord(&x1, &y1, c, x, "x1", "y1"));

    double x2, y2;
    TRY(s_attr_coord(&x2, &y2, c, x, "x2", "y2"));

    path_t q = path_new(r, p, c);
    path_move(&q, x->loc, ABS, x1, y1);
    path_line(&q, x->loc, ABS, x2, y2);
    path_fini(&q, x->loc);
    return true;
}

static bool csg2_from_polyline(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *x)
{
    local_t c1;
    xform_try(&c, &c1, p, x);

    char const *s = TRY(s_attr_str(x, "points"));
    path_t q = path_new(r, p, c);
    TRY(str_path_coord1(&q, &s, ABS, path_move, path_line));
    TRY(s_end(&s));
    path_fini(&q, s);
    return true;
}

static bool csg2_from_polygon(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *x)
{
    local_t c1;
    xform_try(&c, &c1, p, x);

    char const *s = TRY(s_attr_str(x, "points"));
    path_t q = path_new(r, p, c);
    TRY(str_path_coord1(&q, &s, ABS, path_move, path_line));
    TRY(s_end(&s));
    path_close(&q, s);
    path_fini(&q, s);
    return true;
}

static bool csg2_from_ignore(
    cp_v_obj_p_t *r CP_UNUSED,
    ctxt_t const *p CP_UNUSED,
    local_t const *c CP_UNUSED,
    cp_xml_t *x CP_UNUSED)
{
    return true;
}

#include "svg-elem.inc"

static bool csg2_from_elem_rec(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *x)
{
    assert(x->data != NULL);
    elem_value_t const *cm = elem_find(x->data, strlen(x->data));
    if (cm != NULL) {
        return cm->callback(r, p, c, x->child);
    }

    WARN(p, x->loc, "Unsupported SVG element '%s'.", x->data);
    return csg2_from_g(r, p, c, x->child);
}

static bool csg2_from_elem(
    cp_v_obj_p_t *r,
    ctxt_t const *p,
    local_t const *c,
    cp_xml_t *x)
{
    x = cp_xml_skip_attr(x);
    for (; x != NULL; x = x->next) {
        if (x->ns != CP_SVG_NS) {
            continue;
        }
        if (!csg2_from_elem_rec(r, p, c, x)) {
            /* Without an error message, we simply ignore the problem, i.e.,
             * disable rendering for this element. */
            if (p->err->msg.size > 0) {
                return false;
            }
        }
    }
    return true;
}

/**
 * Parse an SVG file and pushes the objects into 'r'.
 *
 * This also gets an initial transformation matrix.  It is
 * compatible with functions from csg3.c, only reads the
 * stuff from SVG instead of from SCAD syntax.  The source
 * is an XML tree.
 *
 * This ignores all nodes that have a different namespace than
 * CP_SVG_NS (must be token-identical => use cp_xml_set_ns()).
 */
extern bool cp_svg_parse(
    cp_v_obj_p_t *r,
    cp_csg3_ctxt_t const *p,
    cp_csg3_local_t const *local,
    cp_detail_t const *detail,
    double dpi,
    cp_xml_t *xml)
{
    local_t c[1] = {{
        .u = *local,
        .detail = *detail,
        .width = 0,
        .height = 0,
        .dim = 0,
        .toplevel = true,
        .dpi = dpi,
    }};

    /* Import the outer transform matrix.
     *
     * We use mat2wi instead of mat2w, because we
     * need the determinant.  Yes, it could be computed,
     * but we have the wi matrixes everywhere, so it is
     * less thinking...
     */
    cp_mat2wi_from_mat3wi(&c->mat, c->u.mat);

    /* Set 1px = 1/96in resolution for initial coord system.
     * Use the 'dpi' setting for 'import' to adjust the
     * DPI setting.  OpenSCAD uses 72dpi by default.
     */

    /* The Y axis is reversed in SCAD compared to SVG.
     * We need to consider this for alignment, too, as
     * OpenSCAD shifts the shapes into positive Y
     * coordinates (if it thinks it knows how).
     */
    cp_mat2wi_t m1;
    cp_mat2wi_scale(&m1, 1, -1);
    cp_mat2wi_mul(&c->mat, &c->mat, &m1);

    return csg2_from_elem(r, p, c, xml);
}
