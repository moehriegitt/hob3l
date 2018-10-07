/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* print in STL format */

#include <cpmat/arith.h>
#include <cpmat/vec.h>
#include <cpmat/mat.h>
#include <cpmat/panic.h>
#include <csg2plane/csg2.h>
#include <csg2plane/gc.h>
#include "internal.h"

static void v_csg2_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_v_csg2_p_t *r);

static inline void triangle_put_stl(
    cp_stream_t *s,
    double xn, double yn, double zn,
    double x1, double y1, double z1,
    double x2, double y2, double z2,
    double x3, double y3, double z3)
{
    cp_printf(s,
        "  facet normal "FF" "FF" "FF"\n"
        "    outer loop\n"
        "      vertex "FF" "FF" "FF"\n"
        "      vertex "FF" "FF" "FF"\n"
        "      vertex "FF" "FF" "FF"\n"
        "    endloop\n"
        "  endfacet\n",
        xn, yn, zn,
        x1, y1, z1,
        x2, y2, z2,
        x3, y3, z3);
}

static void poly_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg2_poly_t *r)
{
    double z1 = cp_v_nth(&t->z, zi);
    double z2 = z1 + cp_csg2_layer_thickness(t, zi) - t->opt.layer_gap;

    cp_v_vec2_loc_t const *point = &r->point;

    /* top */
    for (cp_v_each(i, &r->triangle)) {
        size_t const *p = cp_v_nth(&r->triangle, i).p;
        triangle_put_stl(s,
            0., 0., 1.,
            cp_v_nth(point, p[1]).coord.x, cp_v_nth(point, p[1]).coord.y, z2,
            cp_v_nth(point, p[0]).coord.x, cp_v_nth(point, p[0]).coord.y, z2,
            cp_v_nth(point, p[2]).coord.x, cp_v_nth(point, p[2]).coord.y, z2);
    }

    /* bottom */
    for (cp_v_each(i, &r->triangle)) {
        size_t const *p = cp_v_nth(&r->triangle, i).p;
        triangle_put_stl(s,
            0., 0., -1.,
            cp_v_nth(point, p[0]).coord.x, cp_v_nth(point, p[0]).coord.y, z1,
            cp_v_nth(point, p[1]).coord.x, cp_v_nth(point, p[1]).coord.y, z1,
            cp_v_nth(point, p[2]).coord.x, cp_v_nth(point, p[2]).coord.y, z1);
    }

    /* sides */
    for (cp_v_each(i, &r->path)) {
        cp_csg2_path_t const *p = &cp_v_nth(&r->path, i);
        for (cp_v_each(j, &p->point_idx)) {
            size_t k = cp_wrap_add1(j, p->point_idx.size);
            size_t ij = cp_v_nth(&p->point_idx, j);
            size_t ik = cp_v_nth(&p->point_idx, k);
            cp_vec2_loc_t const *pj = &cp_v_nth(point, ij);
            cp_vec2_loc_t const *pk = &cp_v_nth(point, ik);

            /**
             * All paths are viewed from above, and pj, pk are in CW order =>
             * Side view from outside:
             *
             *     (pk,z2)-------(pj,z2)
             *     |                   |
             *     (pk,z1)-------(pj,z1)
             *
             * Triangles in STL are CCW, so:
             *     (pk,z1)--(pj,z2)--(pk,z2)
             * and (pk,z1)..(pj,z1)--(pj,z2)
             */
            cp_vec3_t n;
            cp_vec3_left_normal3(&n,
                &(cp_vec3_t){{ pk->coord.x, pk->coord.y, z1 }},
                &(cp_vec3_t){{ pj->coord.x, pj->coord.y, z2 }},
                &(cp_vec3_t){{ pk->coord.x, pk->coord.y, z2 }});

            triangle_put_stl(s,
                n.x, n.y, n.z,
                pk->coord.x, pk->coord.y, z1,
                pj->coord.x, pj->coord.y, z2,
                pk->coord.x, pk->coord.y, z2);
            triangle_put_stl(s,
                n.x, n.y, n.z,
                pk->coord.x, pk->coord.y, z1,
                pj->coord.x, pj->coord.y, z1,
                pj->coord.x, pj->coord.y, z2);
        }
    }
}

static void union_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_v_csg2_p_t *r)
{
    v_csg2_put_stl(s, t, zi, r);
}

static void add_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg2_add_t *r)
{
    union_put_stl (s, t, zi, &r->add);
}

static void sub_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg2_sub_t *r)
{
    /* This output format cannot do SUB, only UNION, so we ignore
     * the 'sub' part.  It is wrong, but you asked for it. */
    union_put_stl (s, t, zi, &r->add.add);
}

static void cut_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg2_cut_t *r)
{
    /* This output format cannot do CUT, only UNION, so just print
     * the first part.  It is wrong, but you asked for it. */
    if (r->cut.size > 0) {
        union_put_stl(s, t, zi, &cp_v_nth(&r->cut, 0)->add);
    }
}

static void layer_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    size_t zi __unused,
    cp_csg2_layer_t *r)
{
    if (r->root.add.size == 0) {
        return;
    }
    assert(zi == r->zi);
    v_csg2_put_stl(s, t, r->zi, &r->root.add);
}

static void stack_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    cp_csg2_stack_t *r)
{
    for (cp_v_each(i, &r->layer)) {
        layer_put_stl(s, t, r->idx0 + i, &cp_v_nth(&r->layer, i));
    }
}

static void csg2_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_csg2_t *r)
{
    switch (r->type) {
    case CP_CSG2_ADD:
        add_put_stl(s, t, zi, &r->add);
        return;

    case CP_CSG2_SUB:
        sub_put_stl(s, t, zi, &r->sub);
        return;

    case CP_CSG2_CUT:
        cut_put_stl(s, t, zi, &r->cut);
        return;

    case CP_CSG2_POLY:
        poly_put_stl(s, t, zi, &r->poly);
        return;

    case CP_CSG2_STACK:
        stack_put_stl(s, t, &r->stack);
        return;

    case CP_CSG2_CIRCLE:
        CP_NYI("circle in stl");
        return;
    }

    assert(0);
}

static void v_csg2_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    size_t zi,
    cp_v_csg2_p_t *r)
{
    for (cp_v_each(i, r)) {
        csg2_put_stl(s, t, zi, cp_v_nth(r, i));
    }
}

/* ********************************************************************** */

/**
 * Print as STL file.
 *
 * This generates one 3D solid for each layer.
 *
 * This uses both the triangle and the polygon data for printing.  The
 * triangles are used for the xy plane (top and bottom) and the path
 * for connecting top and bottom at the edges of the slice.
 *
 * This prints in 1:10 scale, i.e., if the input in MM,
 * the STL output is in CM.
 */
extern void cp_csg2_tree_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t)
{
    cp_printf(s, "solid model\n");
    if (t->root != NULL) {
        csg2_put_stl(s, t, 0, t->root);
    }
    cp_printf(s, "endsolid model\n");
}
