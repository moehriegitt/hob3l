/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lmat/mat.h>
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
    cp_printf(s, " sphere(r=1,center=true,$fn=%"CP_Z"u);\n",
        r->_fn);
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

static void xor_put_scad(
    cp_stream_t *s,
    int d,
    cp_csg_xor_t *r)
{
    cp_printf(s, "%*shob3l_xor(){\n", d, "");
    for (cp_v_each(i, &r->xor)) {
        union_put_scad(s, d + IND, &r->xor.data[i]->add);
    }
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
            cp_printf(s, "%s%"CP_Z"u",
                j == 0 ? "" : ",",
                cp_v_idx(&r->point, f->point.data[j].ref));
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
    case CP_CSG_ADD:
        assert(0); /* add is passed via the 'add' vector, never as an object */
        break;

    case CP_CSG_XOR:
        xor_put_scad(s, d, cp_csg_cast(cp_csg_xor_t, r));
        break;

    case CP_CSG_SUB:
        sub_put_scad(s, d, cp_csg_cast(cp_csg_sub_t, r));
        break;

    case CP_CSG_CUT:
        cut_put_scad(s, d, cp_csg_cast(cp_csg_cut_t, r));
        break;

    case CP_CSG3_SPHERE:
        sphere_put_scad(s, d, cp_csg3_cast(cp_csg3_sphere_t, r));
        break;

    case CP_CSG3_POLY:
        poly_put_scad(s, d, cp_csg3_cast(cp_csg3_poly_t, r));
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
