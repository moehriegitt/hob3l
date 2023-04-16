/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3l/scad.h>
#include <hob3lbase/vchar.h>
#include <hob3lbase/base-mat.h>
#include <hob3lbase/panic.h>
#include <hob3l/gc.h>
#include "internal.h"

static void v_scad_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_scad_p_t const *r);

static void combine_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_scad_p_t const *child,
    char const *which)
{
    cp_printf(s, "%s(){\n", which);
    v_scad_put_scad(s, d + IND, child);
    cp_printf(s, "%*s}\n", d, "");
}

static void xyz_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_scad_p_t const *child,
    cp_vec3_t const *v,
    char const *which)
{
    cp_printf(s, "%s(v=["FF","FF","FF"]){\n", which, CP_V012(*v));
    v_scad_put_scad(s, d + IND, child);
    cp_printf(s, "%*s}\n", d, "");
}

static void translate_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_translate_t const *r)
{
    xyz_put_scad(s, d, &r->child, &r->v, "translate");
}

static void mirror_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_mirror_t const *r)
{
    xyz_put_scad(s, d, &r->child, &r->v, "mirror");
}

static void scale_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_scale_t const *r)
{
    xyz_put_scad(s, d, &r->child, &r->v, "scale");
}

static void linext_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_linext_t const *r)
{
    cp_printf(s, "linear_extrude("
        "height="FF", scale=["FF","FF"], twist="FF", slices=%u, center=%s"
        ", $fn=%u){\n",
        r->height, r->scale.x, r->scale.y, r->twist,
        r->slices, r->center ? "true" : "false",
        r->_fn);
    v_scad_put_scad(s, d + IND, &r->child);
    cp_printf(s,"%*s}\n", d, "");
}

static void rotext_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_rotext_t const *r)
{
    cp_printf(s, "rotate_extrude("
        "angle=%g, $fn=%u){\n", r->angle, r->_fn);
    v_scad_put_scad(s, d + IND, &r->child);
    cp_printf(s,"%*s}\n", d, "");
}

static void hull_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_hull_t const *r)
{
    cp_printf(s, "hull(){\n");
    v_scad_put_scad(s, d + IND, &r->child);
    cp_printf(s,"%*s}\n", d, "");
}

static void color_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_color_t const *r)
{
    if (r->valid) {
        cp_printf(s, "color(c=[%.3g,%.3g,%.3g,%.3g]){\n",
            r->rgba.r/255.0,
            r->rgba.g/255.0,
            r->rgba.b/255.0,
            r->rgba.a/255.0);
    }
    else {
        cp_printf(s, "color(alpha=%.3g){\n",
            r->rgba.a/255.0);
    }
    v_scad_put_scad(s, d + IND, &r->child);
    cp_printf(s,"%*s}\n", d, "");
}

static void import_put_scad(
    cp_stream_t *s,
    cp_scad_import_t const *r)
{
    cp_printf(s, "import(s=\"%s\");\n", r->file_tok);
}

static void surface_put_scad(
    cp_stream_t *s,
    cp_scad_surface_t const *r)
{
    cp_printf(s, "surface(s=\"%s\",center=%s);\n",
        r->file_tok, r->center?"true":"false");
}

static void text_put_scad(
    cp_stream_t *s,
    cp_scad_text_t const *r)
{
    cp_printf(s,
        "text(text=\"%s\",size=%g,font=\"%s\",halign=\"%s\",valign=\"%s\","
        "spacing=%g,direction=\"%s\",language=\"%s\",script=\"%s\",$fn=%u);\n",
        r->text, r->size, r->font, r->halign, r->valign,
        r->spacing, r->direction, r->language, r->script, r->_fn);
}

static void projection_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_projection_t const *r)
{
    cp_printf(s, "projection(cut=%s){\n", r->cut?"true":"false");
    v_scad_put_scad(s, d + IND, &r->child);
    cp_printf(s,"%*s}\n", d, "");
}

static void rotate_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_rotate_t const *r)
{
    if (r->around_n) {
        cp_printf(s, "rotate(a="FF", v=["FF","FF","FF"]){\n",
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
    cp_scad_multmatrix_t const *r)
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
    cp_scad_sphere_t const *r)
{
    cp_printf(s, "sphere(r="FF", $fn=%u);\n",
        r->r, r->_fn);
}

static void circle_put_scad(
    cp_stream_t *s,
    cp_scad_circle_t const *r)
{
    cp_printf(s, "circle(r="FF", $fn=%u);\n",
        r->r, r->_fn);
}

static void cylinder_put_scad(
    cp_stream_t *s,
    cp_scad_cylinder_t const *r)
{
    cp_printf(s, "cylinder(h="FF", r1="FF", r2="FF", center=%s, $fn=%u);\n",
        r->h, r->r1, r->r2,
        r->center ? "true" : "false",
        r->_fn);
}

static void cube_put_scad(
    cp_stream_t *s,
    cp_scad_cube_t const *r)
{
    cp_printf(s, "cube(size=["FF","FF","FF"], center=%s);\n",
        r->size.x, r->size.y, r->size.z, r->center ? "true" : "false");
}

static void square_put_scad(
    cp_stream_t *s,
    cp_scad_square_t const *r)
{
    cp_printf(s, "square(size=["FF","FF"], center=%s);\n",
        r->size.x, r->size.y, r->center ? "true" : "false");
}

static void polyhedron_put_scad(
    cp_stream_t *s,
    int d,
    cp_scad_polyhedron_t const *r)
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
            cp_printf(s, "%s%"CP_Z"u",
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
    cp_scad_polygon_t const *r)
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
            cp_printf(s, "%s%"CP_Z"u",
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
    cp_scad_t const *r)
{
    cp_printf(s, "%*s", d, "");
    cp_gc_modifier_put_scad(s, r->modifier);
    switch (r->type) {
    case CP_SCAD_UNION:
        combine_put_scad(s, d, &cp_scad_cast(cp_scad_union_t, r)->child, "union");
        return;

    case CP_SCAD_DIFFERENCE:
        combine_put_scad(s, d, &cp_scad_cast(cp_scad_difference_t, r)->child, "difference");
        return;

    case CP_SCAD_INTERSECTION:
        combine_put_scad(s, d, &cp_scad_cast(cp_scad_intersection_t, r)->child, "intersection");
        return;

    case CP_SCAD_TRANSLATE:
        translate_put_scad(s, d, cp_scad_cast(cp_scad_translate_t, r));
        return;

    case CP_SCAD_MIRROR:
        mirror_put_scad(s, d, cp_scad_cast(cp_scad_mirror_t, r));
        return;

    case CP_SCAD_SCALE:
        scale_put_scad(s, d, cp_scad_cast(cp_scad_scale_t, r));
        return;

    case CP_SCAD_ROTATE:
        rotate_put_scad(s, d, cp_scad_cast(cp_scad_rotate_t, r));
        return;

    case CP_SCAD_MULTMATRIX:
        multmatrix_put_scad(s, d, cp_scad_cast(cp_scad_multmatrix_t, r));
        return;

    case CP_SCAD_SPHERE:
        sphere_put_scad(s, cp_scad_cast(cp_scad_sphere_t, r));
        return;

    case CP_SCAD_CUBE:
        cube_put_scad(s, cp_scad_cast(cp_scad_cube_t, r));
        return;

    case CP_SCAD_CYLINDER:
        cylinder_put_scad(s, cp_scad_cast(cp_scad_cylinder_t, r));
        return;

    case CP_SCAD_POLYHEDRON:
        polyhedron_put_scad(s, d, cp_scad_cast(cp_scad_polyhedron_t, r));
        return;

    case CP_SCAD_IMPORT:
        import_put_scad(s, cp_scad_cast(cp_scad_import_t, r));
        return;

    case CP_SCAD_SURFACE:
        surface_put_scad(s, cp_scad_cast(cp_scad_surface_t, r));
        return;

    case CP_SCAD_TEXT:
        text_put_scad(s, cp_scad_cast(cp_scad_text_t, r));
        return;

    case CP_SCAD_CIRCLE:
        circle_put_scad(s, cp_scad_cast(cp_scad_circle_t, r));
        return;

    case CP_SCAD_SQUARE:
        square_put_scad(s, cp_scad_cast(cp_scad_square_t, r));
        return;

    case CP_SCAD_POLYGON:
        polygon_put_scad(s, d, cp_scad_cast(cp_scad_polygon_t, r));
        return;

    case CP_SCAD_PROJECTION:
        projection_put_scad(s, d, cp_scad_cast(cp_scad_projection_t, r));
        return;

    case CP_SCAD_LINEXT:
        linext_put_scad(s, d, cp_scad_cast(cp_scad_linext_t, r));
        return;

    case CP_SCAD_ROTEXT:
        rotext_put_scad(s, d, cp_scad_cast(cp_scad_rotext_t, r));
        return;

    case CP_SCAD_HULL:
        hull_put_scad(s, d, cp_scad_cast(cp_scad_hull_t, r));
        return;

    case CP_SCAD_COLOR:
        color_put_scad(s, d, cp_scad_cast(cp_scad_color_t, r));
        return;
    }
    CP_NYI("type=0x%x", r->type);
}

static void v_scad_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_scad_p_t const *r)
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
    cp_scad_tree_t const *r)
{
    v_scad_put_scad(s, 0, &r->toplevel);
}
