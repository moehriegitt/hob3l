/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* print in PS format */

#include <hob3lbase/arith.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/base-mat.h>
#include <hob3lbase/stream.h>
#include <hob3lbase/panic.h>
#include <hob3l/gc.h>
#include <hob3l/csg.h>
#include <hob3l/csg2.h>
#include <hob3l/ps.h>
#include "internal.h"

#define RGB(x) ((x).r / 255.0), ((x).g / 255.0),  ((x).b / 255.0)

typedef struct {
    cp_stream_t *s;
    cp_ps_opt_t const *opt;
    cp_vec2_minmax_t bb;
} ctxt_t;

static void v_csg2_put_ps(
    ctxt_t *k,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_v_obj_p_t *r);

static void coord(cp_vec2_t *v, ctxt_t *k, cp_vec2_t *p, double z)
{
    cp_vec4_t w;
    w.b.b = *p;
    w.z = z;
    w.w = 1;
    cp_vec4_xform(&w, &k->opt->xform2, &w);
    v->x = cp_ps_x(k->opt->xform1, cp_div0(w.x, w.w));
    v->y = cp_ps_y(k->opt->xform1, cp_div0(w.y, w.w));
    if (k != NULL) {
        cp_vec2_minmax(&k->bb, v);
    }
}

static void triangle_put_ps(
    ctxt_t *k,
    cp_csg2_poly_t *o,
    cp_csg2_tri_t *t,
    double z)
{
    cp_vec2_t p1; coord(&p1, k, &cp_v_nth(&o->point, t->p[0]).coord, z);
    cp_vec2_t p2; coord(&p2, k, &cp_v_nth(&o->point, t->p[1]).coord, z);
    cp_vec2_t p3; coord(&p3, k, &cp_v_nth(&o->point, t->p[2]).coord, z);
    cp_printf(k->s,
        "newpath "
        "%g %g moveto "
        "%g %g lineto "
        "%g %g lineto "
        "closepath ",
        p1.x, p1.y,
        p2.x, p2.y,
        p3.x, p3.y);

    if (k->opt->no_tri) {
        cp_printf(k->s,
            "%g %g %g setrgbcolor "
            "fill\n",
            RGB(k->opt->color_fill));
    }
    else {
        cp_printf(k->s,
            "gsave "
            "%g %g %g setrgbcolor "
            "fill "
            "grestore "
            "%g %g %g setrgbcolor "
            "stroke\n",
            RGB(k->opt->color_fill),
            RGB(k->opt->color_tri));
    }
}

static void mark_put_ps(
    ctxt_t *k,
    cp_vec2_t const *p0,
    cp_vec2_t const *p1)
{
    cp_vec2_t r;
    cp_vec2_normal(&r, p0, p1);
    cp_vec2_mul(&r, &r, 2);

    cp_vec2_t m = *p0;
    cp_vec2_add(&m, &m, p1);
    cp_vec2_mul(&m, &m, 0.5);

    cp_printf(k->s, "newpath %g %g moveto %g %g lineto %g %g %g setrgbcolor stroke\n",
        m.x, m.y, m.x + r.x, m.y + r.y,
        RGB(k->opt->color_mark));
}

static void path_put_ps(
    ctxt_t *k,
    cp_csg2_poly_t *o,
    cp_csg2_path_t *t,
    double z)
{
    cp_printf(k->s,
        "newpath ");

    char const *cmd = "moveto";
    for (cp_v_each(i, &t->point_idx)) {
        cp_vec2_t p;
        coord(&p, k, &cp_v_nth(&o->point, t->point_idx.data[i]).coord, z);
        cp_printf(k->s,
            "%g %g %s ", p.x, p.y, cmd);
        cmd = "lineto";
    }

    cp_printf(k->s,
        "closepath "
        "%g %g %g setrgbcolor "
        "stroke\n",
        RGB(k->opt->color_path));

    if (!k->opt->no_mark) {
        if (t->point_idx.size >= 2) {
            cp_vec2_t p0;
            coord(&p0, k, &cp_v_nth(&o->point, t->point_idx.data[0]).coord, z);
            cp_vec2_t p1;
            coord(&p1, k, &cp_v_nth(&o->point, t->point_idx.data[1]).coord, z);
            mark_put_ps(k, &p0, &p1);
        }
    }
}

static void point_put_ps(
    ctxt_t *k,
    cp_vec2_loc_t *p,
    double z)
{
    cp_vec2_t c;
    coord(&c, k, &p->coord, z);

    cp_printf(k->s, "newpath %g %g 0.4 0 360 arc closepath %g %g %g setrgbcolor fill\n",
        c.x, c.y,
        RGB(k->opt->color_vertex));
}

static void poly_put_ps(
    ctxt_t *k,
    cp_csg2_poly_t *r,
    double z)
{
    for (cp_v_each(i, &r->tri)) {
        triangle_put_ps(k, r, &cp_v_nth(&r->tri, i), z);
    }
    if (!k->opt->no_path) {
        for (cp_v_each(i, &r->path)) {
            path_put_ps(k, r, &cp_v_nth(&r->path, i), z);
        }
        for (cp_v_each(i, &r->point)) {
            point_put_ps(k, &cp_v_nth(&r->point, i), z);
        }
    }
}

static void sub_put_ps(
    ctxt_t *k,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg_sub_t *r)
{
    v_csg2_put_ps(k, t, zi, &r->add->add);
    v_csg2_put_ps(k, t, zi, &r->sub->add);
}

static void add_put_ps(
    ctxt_t *k,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg_add_t *r)
{
    v_csg2_put_ps(k, t, zi, &r->add);
}

static void cut_put_ps(
    ctxt_t *k,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg_cut_t *r)
{
    for (cp_v_each(i, &r->cut)) {
        v_csg2_put_ps(k, t, zi, &cp_v_nth(&r->cut, i)->add);
    }
}

static void xor_put_ps(
    ctxt_t *k,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg_xor_t *r)
{
    for (cp_v_each(i, &r->xor)) {
        v_csg2_put_ps(k, t, zi, &cp_v_nth(&r->xor, i)->add);
    }
}

static void layer_put_ps(
    ctxt_t *k,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg2_layer_t *r)
{
    if (zi != r->zi) {
        return;
    }
    v_csg2_put_ps(k, t, zi, &r->root->add);
}

static void stack_put_ps(
    ctxt_t *k,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg2_stack_t *r)
{
    for (cp_v_each(i, &r->layer)) {
        layer_put_ps(k, t, zi, &cp_v_nth(&r->layer, i));
    }
}

static void csg2_put_ps(
    ctxt_t *k,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg2_t *r)
{
    switch (r->type) {
    case CP_CSG_ADD:
        add_put_ps(k, t, zi, cp_csg_cast(cp_csg_add_t, r));
        return;

    case CP_CSG_XOR:
        xor_put_ps(k, t, zi, cp_csg_cast(cp_csg_xor_t, r));
        return;

    case CP_CSG_SUB:
        sub_put_ps(k, t, zi, cp_csg_cast(cp_csg_sub_t, r));
        return;

    case CP_CSG_CUT:
        cut_put_ps(k, t, zi, cp_csg_cast(cp_csg_cut_t, r));
        return;

    case CP_CSG2_STACK:
        stack_put_ps(k, t, zi, cp_csg2_cast(cp_csg2_stack_t, r));
        return;

    case CP_CSG2_POLY:
        poly_put_ps(k, cp_csg2_cast(cp_csg2_poly_t, r), cp_v_nth(&t->z, zi));
        return;

    case CP_CSG2_VLINE2:
        assert(0 && "no v_line2 support");
        return;

    case CP_CSG2_SWEEP:
        assert(0 && "no cq_sweep support");
        return;
    }

    CP_DIE("2D object type");
}

static void v_csg2_put_ps(
    ctxt_t *k,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_v_obj_p_t *r)
{
    for (cp_v_each(i, r)) {
        csg2_put_ps(k, t, zi, cp_csg2_cast(cp_csg2_t, cp_v_nth(r,i)));
    }
}

/* ********************************************************************** */

/**
 * Print as PS file.
 *
 * Each layer is printed on a separate page.
 *
 * This prints both the triangle and the path data in different
 * colours, overlayed so that the shape can be debugged.
 *
 * If xform is NULL, this assumes that the input is in MM.  Otherwise,
 * any other scaling transformation can be applied.
 */
extern void cp_csg2_tree_put_ps(
    cp_stream_t *s,
    cp_ps_opt_t const *opt,
    cp_csg2_tree_t *t)
{
    if (t->root != NULL) {
        ctxt_t k;
        CP_ZERO(&k);
        k.s = s;
        k.opt = opt;
        k.bb.min.x = +99999;
        k.bb.min.y = +99999;
        k.bb.max.x = -99999;
        k.bb.max.y = -99999;

        size_t page_cnt = 0;
        cp_ps_doc_begin(s, opt, CP_SIZE_MAX, 0, 0, -1, -1);
        if (opt->single_page) {
            page_cnt++;
            cp_ps_page_begin(s, opt, page_cnt);
        }

        for (cp_v_each(zi, &t->z)) {
            if (cp_v_nth(&t->flag, zi) & CP_CSG2_FLAG_NON_EMPTY) {
                if (!opt->single_page) {
                    page_cnt++;
                    cp_ps_page_begin(s, opt, page_cnt);
                    cp_printf(s, "10 10 moveto (z=%g zi=%"CP_Z"u) show\n", cp_v_nth(&t->z, zi), zi);
                }

                csg2_put_ps(&k, t, zi, t->root);

                if (!opt->single_page) {
                    cp_ps_page_end(s);
                }
            }
        }

        if ((k.bb.min.x > k.bb.max.x) || (k.bb.min.y > k.bb.max.y)) {
            k.bb.min.x = k.bb.min.y = k.bb.max.x = k.bb.max.y = 0.5;
        }

        if (opt->single_page) {
            cp_ps_page_end(s);
        }
        cp_ps_doc_end(s,
            page_cnt,
            lrint(floor(k.bb.min.x)),
            lrint(floor(k.bb.min.y)),
            lrint(ceil(k.bb.max.x)),
            lrint(ceil(k.bb.max.y)));
    }
}
