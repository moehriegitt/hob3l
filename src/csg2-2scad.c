/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* print in SCAD format */

#include <hob3lbase/arith.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/stream.h>
#include <hob3lbase/panic.h>
#include <hob3l/gc.h>
#include <hob3l/csg2.h>
#include "internal.h"

static void v_csg2_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    /** indentation level */
    int d,
    /** z index */
    size_t zi,
    cp_v_csg2_p_t *r);

static inline cp_dim_t layer_gap(cp_dim_t x)
{
    return cp_eq(x,-1) ? 0 : x;
}

static void poly_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    size_t zi,
    cp_csg2_poly_t *r)
{
    cp_printf(s, "%*s", d,"");
    cp_dim_t lt = cp_csg2_layer_thickness(t, zi);
    cp_printf(s, "linear_extrude(height="FF",center=0,convexity=2,twist=0)",
        lt - layer_gap(t->opt.layer_gap));
    cp_printf(s, "polygon(");
    cp_printf(s, "points=[");
    for (cp_v_each(i, &r->point)) {
        cp_vec2_t const *v = &cp_v_nth(&r->point, i).coord;
        cp_printf(s,"%s["FF","FF"]",
            i == 0 ? "" : ",",
            v->x, v->y);
    }
    cp_printf(s, "],");
    cp_printf(s, "paths=[");
    if (r->triangle.size > 0) {
        for (cp_v_each(i, &r->triangle)) {
            cp_size3_t const *f = &cp_v_nth(&r->triangle, i);
            cp_printf(s, "%s[%"_Pz"u,%"_Pz"u,%"_Pz"u]",
                i == 0 ? "" : ",",
                f->p[0], f->p[1], f->p[2]);
        }
    }
    else {
        for (cp_v_each(i, &r->path)) {
            cp_csg2_path_t const *f = &cp_v_nth(&r->path, i);
            cp_printf(s, "%s[", i == 0 ? "" : ",");
            for (cp_v_each(j, &f->point_idx)) {
                cp_printf(s, "%s%"_Pz"u", j == 0 ? "" : ",", cp_v_nth(&f->point_idx, j));
            }
            cp_printf(s, "]");
        }
    }
    cp_printf(s, "]);\n");
}

static void union_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    size_t zi,
    cp_v_csg2_p_t *r)
{
    if (r->size == 1) {
        v_csg2_put_scad(s, t, d, zi, r);
        return;
    }
    cp_printf(s, "%*sunion(){\n", d, "");
    v_csg2_put_scad(s, t, d + IND, zi, r);
    cp_printf(s, "%*s}\n", d, "");
}

static void add_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    size_t zi,
    cp_csg2_add_t *r)
{
    v_csg2_put_scad(s, t, d, zi, &r->add);
}

static void sub_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    size_t zi,
    cp_csg2_sub_t *r)
{
    cp_printf(s, "%*sdifference(){\n", d, "");
    cp_printf(s, "%*s// add\n", d+IND, "");
    union_put_scad (s, t, d + IND, zi, &r->add.add);
    cp_printf(s, "%*s// sub\n", d+IND, "");
    v_csg2_put_scad(s, t, d + IND, zi, &r->sub.add);
    cp_printf(s, "%*s}\n", d, "");
}

static void cut_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    size_t zi,
    cp_csg2_cut_t *r)
{
    cp_printf(s, "%*sintersection(){\n", d, "");
    for (cp_v_each(i, &r->cut)) {
        union_put_scad(s, t, d + IND, zi, &cp_v_nth(&r->cut, i)->add);
    }
    cp_printf(s, "%*s}\n", d, "");
}

static void layer_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    size_t zi __unused,
    cp_csg2_layer_t *r)
{
    if (r->root.add.size == 0) {
        return;
    }
    assert(zi == r->zi);
    double z = cp_v_nth(&t->z, r->zi);

    cp_printf(s, "%*s", d,"");
    cp_printf(s, "translate([0,0,"FF"]) {\n", z);

    v_csg2_put_scad(s, t, d + IND, r->zi, &r->root.add);

    cp_printf(s, "%*s", d,"");
    cp_printf(s, "}\n");
}

static void stack_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    cp_csg2_stack_t *r)
{
    if (r->layer.size == 0) {
        return;
    }
    cp_printf(s, "%*s", d,"");
    cp_printf(s, "group(){\n");
    for (cp_v_each(i, &r->layer)) {
        layer_put_scad(s, t, d + IND, r->idx0 + i, &cp_v_nth(&r->layer, i));
    }
    cp_printf(s, "%*s", d,"");
    cp_printf(s, "}\n");
}

static void csg2_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    size_t zi,
    cp_csg2_t *r)
{
    switch (r->type) {
    case CP_CSG2_ADD:
        add_put_scad(s, t, d, zi, &r->add);
        return;

    case CP_CSG2_SUB:
        sub_put_scad(s, t, d, zi, &r->sub);
        return;

    case CP_CSG2_CUT:
        cut_put_scad(s, t, d, zi, &r->cut);
        return;

    case CP_CSG2_POLY:
        poly_put_scad(s, t, d, zi, &r->poly);
        return;

    case CP_CSG2_STACK:
        stack_put_scad(s, t, d, &r->stack);
        return;

    case CP_CSG2_CIRCLE:
        CP_NYI("circle");
        return;
    }

    CP_DIE("2D object type %#x", r->type);
}

static void v_csg2_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    size_t zi,
    cp_v_csg2_p_t *r)
{
    for (cp_v_each(i, r)) {
        csg2_put_scad(s, t, d, zi, cp_v_nth(r, i));
    }
}

/* ********************************************************************** */

/**
 * Print as scad file.
 *
 * Note that SCAD is actually 3D and OpenSCAD does not really
 * honor Z translations for 2D objects.  The F5 view is OK, but
 * the F6 view maps everything to z==0.
 *
 * This prefers the triangle info.  If not present, it uses the
 * polygon path info.
 *
 * This prints in 1:1 scale, i.e., if the finput is in MM,
 * the SCAD output is in MM, too.
 */
extern void cp_csg2_tree_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t)
{
    if (t->root != NULL) {
        csg2_put_scad(s, t, 0, 0, t->root);
    }
}
