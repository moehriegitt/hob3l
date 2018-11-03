/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/mat.h>
#include <hob3lbase/panic.h>
#include <hob3l/csg.h>
#include <hob3l/csg2.h>
#include <hob3l/csg3.h>
#include <hob3l/gc.h>
#include "internal.h"

static void v_csg3_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_obj_p_t *r);

static void mat3wi_put_scad(
    cp_stream_t *s,
    cp_mat3wi_t const *m)
{
    cp_mat3_t const *b = &m->n.b;
    cp_vec3_t const *w = &m->n.w;
    cp_printf(s, "multmatrix(m=["
        "["FF","FF","FF","FF"],"
        "["FF","FF","FF","FF"],"
        "["FF","FF","FF","FF"],"
        "[0,0,0,1]])",
        b->m[0][0], b->m[0][1], b->m[0][2], w->v[0],
        b->m[1][0], b->m[1][1], b->m[1][2], w->v[1],
        b->m[2][0], b->m[2][1], b->m[2][2], w->v[2]);
}

static void sphere_put_scad(
    cp_stream_t *s,
    int d,
    cp_csg3_sphere_t *r)
{
    cp_printf(s, "%*s", d,"");
    cp_gc_modifier_put_scad(s, r->gc.modifier);
    mat3wi_put_scad(s, r->mat);
    cp_printf(s, " sphere(r=1,center=true,$fa="FF",$fs="FF",$fn=%"_Pz"u);\n",
        r->_fa, r->_fs, r->_fn);
}

#if 0
/* FIXME: continue */
static void circle_put_scad(
    cp_stream_t *s,
    int d,
    cp_csg2_circle_t *r)
{
    cp_printf(s, "%*s", d,"");
    mat3wi_put_scad(s, r->mat);
    cp_printf(s, " circle(r=1,center=true,$fa="FF",$fs="FF",$fn=%u);\n",
        r->_fa, r->_fs, r->_fn);
}
#endif

static void cyl_put_scad(
    cp_stream_t *s,
    int d,
    cp_csg3_cyl_t *r)
{
    cp_printf(s, "%*s", d,"");
    cp_gc_modifier_put_scad(s, r->gc.modifier);
    mat3wi_put_scad(s, r->mat);
    cp_printf(s, " cylinder(h=1,r1=1,r2=%g,center=true,$fa="FF",$fs="FF",$fn=%"_Pz"u);\n",
        r->r2,
        r->_fa, r->_fs, r->_fn);
}

static void union_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_obj_p_t *r)
{
    if (r->size == 1) {
        v_csg3_put_scad(s, d, r);
        return;
    }
    cp_printf(s, "%*sunion(){\n", d, "");
    v_csg3_put_scad(s, d + IND, r);
    cp_printf(s, "%*s}\n", d, "");
}

static void sub_put_scad(
    cp_stream_t *s,
    int d,
    cp_csg_sub_t *r)
{
    cp_printf(s, "%*sdifference(){\n", d, "");
    cp_printf(s, "%*s// add\n", d+IND, "");
    union_put_scad (s, d + IND, &r->add->add);
    cp_printf(s, "%*s// sub\n", d+IND, "");
    v_csg3_put_scad(s, d + IND, &r->sub->add);
    cp_printf(s, "%*s}\n", d, "");
}

static void cut_put_scad(
    cp_stream_t *s,
    int d,
    cp_csg_cut_t *r)
{
    cp_printf(s, "%*sintersection(){\n", d, "");
    for (cp_v_each(i, &r->cut)) {
        union_put_scad(s, d + IND, &r->cut.data[i]->add);
    }
    cp_printf(s, "%*s}\n", d, "");
}

static void poly_put_scad(
    cp_stream_t *s,
    int d,
    cp_csg3_poly_t *r)
{
    cp_printf(s, "%*s", d,"");
    cp_gc_modifier_put_scad(s, r->gc.modifier);
    cp_printf(s, "polyhedron(");
    cp_printf(s, "points=[");
    for (cp_v_each(i, &r->point)) {
        cp_vec3_t const *v = &r->point.data[i].coord;
        cp_printf(s,"%s["FF","FF","FF"]",
            i == 0 ? "" : ",",
            v->x, v->y, v->z);
    }
    cp_printf(s, "],");
    cp_printf(s, "faces=[");
    for (cp_v_each(i, &r->face)) {
        cp_csg3_face_t const *f = &r->face.data[i];
        cp_printf(s, "%s[", i == 0 ? "" : ",");
        for (cp_v_each(j, &f->point)) {
            cp_printf(s, "%s%"_Pz"u",
                j == 0 ? "" : ",",
                cp_v_idx(&r->point, f->point.data[j].ref));
        }
        cp_printf(s, "]");
    }
    cp_printf(s, "]);\n");
}

static void poly2_put_scad(
    cp_stream_t *s,
    int d,
    cp_csg2_poly_t *r)
{
    cp_printf(s, "%*s", d,"");
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
    for (cp_v_each(i, &r->path)) {
        cp_csg2_path_t const *f = &r->path.data[i];
        cp_printf(s, "%s[", i == 0 ? "" : ",");
        for (cp_v_each(j, &f->point_idx)) {
            cp_printf(s, "%s%"_Pz"u",
                j == 0 ? "" : ",",
                f->point_idx.data[j]);
        }
        cp_printf(s, "]");
    }
    cp_printf(s, "]);\n");
}

static void csg3_put_scad(
    cp_stream_t *s,
    int d,
    cp_csg3_t *r)
{
    switch (r->type) {
    case CP_CSG3_ADD:
        assert(0); /* add is passed via the 'add' vector, never as an object */
        break;

    case CP_CSG3_SUB:
        sub_put_scad(s, d, cp_csg_cast(cp_csg_sub_t, r));
        break;

    case CP_CSG3_CUT:
        cut_put_scad(s, d, cp_csg_cast(cp_csg_cut_t, r));
        break;

    case CP_CSG3_SPHERE:
        sphere_put_scad(s, d, cp_csg3_cast(cp_csg3_sphere_t, r));
        break;

    case CP_CSG3_CYL:
        cyl_put_scad(s, d, cp_csg3_cast(cp_csg3_cyl_t, r));
        break;

    case CP_CSG3_POLY:
        poly_put_scad(s, d, cp_csg3_cast(cp_csg3_poly_t, r));
        break;

    case CP_CSG2_POLY:
        poly2_put_scad(s, d, cp_csg2_cast(cp_csg2_poly_t, r));
        break;

    default:
        CP_DIE("Unrecognized CSG3 object type");
    }
}

static void v_csg3_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_obj_p_t *r)
{
    for (cp_v_each(i, r)) {
        csg3_put_scad(s, d, cp_csg3_cast(cp_csg3_t, cp_v_nth(r,i)));
    }
}

/**
 * Dump a CSG3 tree in SCAD format
 */
extern void cp_csg3_tree_put_scad(
    cp_stream_t *s,
    cp_csg3_tree_t *r)
{
    if (r->root != NULL) {
        v_csg3_put_scad(s, 0, &r->root->add);
    }
}
