/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* print in SCAD format */

#include <cpmat/arith.h>
#include <cpmat/vec.h>
#include <cpmat/mat.h>
#include <cpmat/stream.h>
#include <cpmat/panic.h>
#include <csg2plane/gc.h>
#include <csg2plane/csg2.h>
#include "internal.h"

static void v_csg2_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    /** indentation level */
    int d,
    /** z index */
    size_t zi,
    cp_v_csg2_p_t *r);

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
        union_put_scad(s, t, d + IND, zi, &r->cut.data[i]->add);
    }
    cp_printf(s, "%*s}\n", d, "");
}

static cp_dim_t layer_thickness(
    cp_csg2_tree_t *t,
    size_t zi)
{
    if (t->z.size <= 1) {
        return 1.0;
    }
    if (zi <= 0) {
       zi = 1;
    }
    if (zi >= t->z.size) {
       zi = t->z.size - 1;
    }
    double th = t->z.data[zi] - t->z.data[zi - 1];
    assert(th > 0);
    return th;
}

static void poly_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    size_t zi,
    cp_csg2_poly_t *r)
{
    cp_printf(s, "%*s", d,"");
    cp_dim_t lt = layer_thickness(t, zi);
    cp_printf(s, "linear_extrude(height=%g,center=0,convexity=2,twist=0)", lt - 0.01);
    cp_printf(s, "polygon(");
    cp_printf(s, "points=[");
    for (cp_v_each(i, &r->point)) {
        cp_vec2_t const *v = &r->point.data[i].coord;
        cp_printf(s,"%s["FF","FF"]",
            i == 0 ? "" : ",",
            v->x, v->y);
    }
    cp_printf(s, "],");
    cp_printf(s, "paths=[");
    if (r->triangle.size > 0) {
        for (cp_v_each(i, &r->triangle)) {
            cp_size3_t const *f = &r->triangle.data[i];
            cp_printf(s, "%s[%"_Pz"u,%"_Pz"u,%"_Pz"u]",
                i == 0 ? "" : ",",
                f->p[0], f->p[1], f->p[2]);
        }
    }
    else {
        for (cp_v_each(i, &r->path)) {
            cp_csg2_path_t const *f = &r->path.data[i];
            cp_printf(s, "%s[", i == 0 ? "" : ",");
            for (cp_v_each(j, &f->point_idx)) {
                cp_printf(s, "%s%"_Pz"u", j == 0 ? "" : ",", f->point_idx.data[j]);
            }
            cp_printf(s, "]");
        }
    }
    cp_printf(s, "]);\n");
}

static void layer_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    size_t zi,
    cp_csg2_layer_t *r)
{
    double z = t->z.data[r->zi];

    cp_printf(s, "%*s", d,"");
    cp_printf(s, "translate([0,0,"FF"]) {\n", z);

    v_csg2_put_scad(s, t, d + IND, zi, &r->root.add);

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
        layer_put_scad(s, t, d + IND, i, &r->layer.data[i]);
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
        csg2_put_scad(s, t, d, zi, r->data[i]);
    }
}

extern void cp_csg2_tree_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *t)
{
    if (t->root != NULL) {
        csg2_put_scad(s, t, 0, 0, t->root);
    }
}
