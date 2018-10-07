/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <csg2plane/scad.h>
#include <cpmat/vchar.h>
#include <cpmat/mat.h>
#include <csg2plane/gc.h>
#include "internal.h"

static void v_scad_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_scad_p_t *r);

static void combine_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_combine_t *r,
    char const *which)
{
    cp_printf(s, "%s(){\n", which);
    v_scad_put_scad(s, d + IND, &r->child);
    cp_printf(s, "%*s}\n", d, "");
}

static void xyz_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_xyz_t *r,
    char const *which)
{
    cp_printf(s, "%s(v=["FF","FF","FF"]){\n", which, r->v.x, r->v.y, r->v.z);
    v_scad_put_scad(s, d + IND, &r->child);
    cp_printf(s, "%*s}\n", d, "");
}

static void rotate_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_rotate_t *r)
{
    if (r->around_n) {
        cp_printf(s, "rotate(a="FF",v=["FF","FF","FF"]){\n",
            r->a, r->n.x, r->n.y, r->n.z);
    }
    else {
        cp_printf(s, "rotate(a=["FF","FF","FF"]){\n",
            r->n.x, r->n.y, r->n.z);
    }
    v_scad_put_scad(s, d + IND, &r->child);
    cp_printf(s, "%*s}\n", d, "");
}

static void multmatrix_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_multmatrix_t *r)
{
    cp_mat3_t const *b = &r->m.b;
    cp_vec3_t const *w = &r->m.w;
    cp_printf(s, "multmatrix(m=["
        "["FF","FF","FF","FF"],"
        "["FF","FF","FF","FF"],"
        "["FF","FF","FF","FF"],"
        "[0,0,0,1]]) {\n",
        b->m[0][0], b->m[0][1], b->m[0][2], w->v[0],
        b->m[1][0], b->m[1][1], b->m[1][2], w->v[1],
        b->m[2][0], b->m[2][1], b->m[2][2], w->v[2]);
    v_scad_put_scad(s, d + IND, &r->child);
    cp_printf(s, "%*s}\n", d, "");
}

static void sphere_put_scad(
    cp_stream_t *s,
    int d __unused,
    cp_scad_sphere_t *r)
{
    cp_printf(s, "sphere(r="FF",$fa="FF",$fs="FF",$fn=%u);\n",
        r->r, r->_fa, r->_fs, r->_fn);
}

static void circle_put_scad(
    cp_stream_t *s,
    int d __unused,
    cp_scad_circle_t *r)
{
    cp_printf(s, "circle(r="FF",$fa="FF",$fs="FF",$fn=%u);\n",
        r->r, r->_fa, r->_fs, r->_fn);
}

static void cylinder_put_scad(
    cp_stream_t *s,
    int d __unused,
    cp_scad_cylinder_t *r)
{
    cp_printf(s, "cylinder(h="FF",r1="FF",r2="FF",center=%s,$fa="FF",$fs="FF",$fn=%u);\n",
        r->h, r->r1, r->r2,
        r->center ? "true" : "false",
        r->_fa, r->_fs, r->_fn);
}

static void cube_put_scad(
    cp_stream_t *s,
    int d __unused,
    cp_scad_cube_t *r)
{
    cp_printf(s, "cube(size=["FF","FF","FF"],center=%s);\n",
        r->size.x, r->size.y, r->size.z, r->center ? "true" : "false");
}

static void square_put_scad(
    cp_stream_t *s,
    int d __unused,
    cp_scad_square_t *r)
{
    cp_printf(s, "square(size=["FF","FF"],center=%s);\n",
        r->size.x, r->size.y, r->center ? "true" : "false");
}

static void polyhedron_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_polyhedron_t *r)
{
    cp_printf(s, "polyhedron(\n");
    cp_printf(s, "%*spoints=[", d+IND,"");
    for (cp_v_each(i, &r->points)) {
        cp_vec3_t const *v = &r->points.data[i].coord;
        cp_printf(s,"%s["FF","FF","FF"]",
            i == 0 ? "" : ",",
            v->x, v->y, v->z);
    }
    cp_printf(s, "],\n");
    cp_printf(s, "%*sfaces=[", d+IND,"" );
    for (cp_v_each(i, &r->faces)) {
        cp_scad_face_t const *f = &r->faces.data[i];
        cp_printf(s, "%s[", i == 0 ? "" : ",");
        for (cp_v_each(j, &f->points)) {
            cp_printf(s, "%s%"_Pz"u",
                j == 0 ? "" : ",",
                cp_v_idx(&r->points, f->points.data[j].ref));
        }
        cp_printf(s, "]");
    }
    cp_printf(s, "]);\n");
}

static void polygon_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_polygon_t *r)
{
    cp_printf(s, "polygon(\n");
    cp_printf(s, "%*spoints=[", d+IND,"");
    for (cp_v_each(i, &r->points)) {
        cp_vec2_t const *v = &r->points.data[i].coord;
        cp_printf(s,"%s["FF","FF"]",
            i == 0 ? "" : ",",
            v->x, v->y);
    }
    cp_printf(s, "],\n");
    cp_printf(s, "%*spaths=[", d+IND,"" );
    for (cp_v_each(i, &r->paths)) {
        cp_scad_path_t const *f = &r->paths.data[i];
        cp_printf(s, "%s[", i == 0 ? "" : ",");
        for (cp_v_each(j, &f->points)) {
            cp_printf(s, "%s%"_Pz"u",
                j == 0 ? "" : ",",
                cp_v_idx(&r->points, f->points.data[j].ref));
        }
        cp_printf(s, "]");
    }
    cp_printf(s, "]);\n");
}

static void scad_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_t *r)
{
    cp_printf(s, "%*s", d, "");
    cp_gc_modifier_put_scad(s, r->modifier);
    switch (r->type) {
    case CP_SCAD_UNION:
        combine_put_scad(s, d, &r->combine, "union");
        break;

    case CP_SCAD_DIFFERENCE:
        combine_put_scad(s, d, &r->combine, "difference");
        break;

    case CP_SCAD_INTERSECTION:
        combine_put_scad(s, d, &r->combine, "intersection");
        break;

    case CP_SCAD_TRANSLATE:
        xyz_put_scad(s, d, &r->xyz, "translate");
        break;

    case CP_SCAD_MIRROR:
        xyz_put_scad(s, d, &r->xyz, "mirror");
        break;

    case CP_SCAD_SCALE:
        xyz_put_scad(s, d, &r->xyz, "scale");
        break;

    case CP_SCAD_ROTATE:
        rotate_put_scad(s, d, &r->rotate);
        break;

    case CP_SCAD_MULTMATRIX:
        multmatrix_put_scad(s, d, &r->multmatrix);
        break;

    case CP_SCAD_SPHERE:
        sphere_put_scad(s, d, &r->sphere);
        break;

    case CP_SCAD_CUBE:
        cube_put_scad(s, d, &r->cube);
        break;

    case CP_SCAD_CYLINDER:
        cylinder_put_scad(s, d, &r->cylinder);
        break;

    case CP_SCAD_POLYHEDRON:
        polyhedron_put_scad(s, d, &r->polyhedron);
        break;

    case CP_SCAD_CIRCLE:
        circle_put_scad(s, d, &r->circle);
        break;

    case CP_SCAD_SQUARE:
        square_put_scad(s, d, &r->square);
        break;

    case CP_SCAD_POLYGON:
        polygon_put_scad(s, d, &r->polygon);
        break;

    default:
        assert(0 && "Unrecognized SCAD object type");
    }
}

static void v_scad_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_scad_p_t *r)
{
    for (cp_v_each(i, r)) {
        scad_put_scad(s, d, r->data[i]);
    }
}

/**
 * Dump in SCAD format.
 */
extern void cp_scad_tree_put_scad(
    cp_stream_t *s,
    cp_scad_tree_t *r)
{
    v_scad_put_scad(s, 0, &r->toplevel);
}
