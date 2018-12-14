/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/arith.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/panic.h>
#include <hob3l/csg.h>
#include <hob3l/csg2.h>
#include <hob3l/gc.h>
#include "internal.h"

typedef struct {
    cp_stream_t *stream;
    cp_csg2_tree_t *tree;
    bool bin;
    unsigned tri_count;
} ctxt_t;

static void v_csg2_put_stl(
    ctxt_t *c,
    size_t zi,
    cp_v_obj_p_t *r);

static void write_u32(
    cp_stream_t *s,
    unsigned u)
{
    unsigned char c[4] = {
        u & 0xff,
        (u >> 8) & 0xff,
        (u >> 16) & 0xff,
        /* -Wconversion bug in gcc requires cast, unfortunately */
        (unsigned char)(u >> 24)
    };
    cp_write(s, c, sizeof(c));
}

static void write_u32p(cp_stream_t *s, unsigned const *p)
{
    write_u32(s, *p);
}

static void write_f32(cp_stream_t *s, float f)
{
    write_u32p(s, (void const *)&f);
}

static void write_f64_32(cp_stream_t *s, double f)
{
    write_f32(s, (float)f);
}

static void write_3f64_32(cp_stream_t *s, double fx, double fy, double fz)
{
    write_f64_32(s, fx);
    write_f64_32(s, fy);
    write_f64_32(s, fz);
}

static inline void triangle_put_stl(
    ctxt_t *c,
    double xn, double yn, double zn,
    cp_vec2_loc_t const *xy1, double z1,
    cp_vec2_loc_t const *xy2, double z2,
    cp_vec2_loc_t const *xy3, double z3)
{
    c->tri_count++;
    if (c->bin) {
        if (c->stream == NULL) {
            return;
        }
        write_3f64_32(c->stream, xn, yn, zn);
        write_3f64_32(c->stream, xy1->coord.x, xy1->coord.y, z1);
        write_3f64_32(c->stream, xy2->coord.x, xy2->coord.y, z2);
        write_3f64_32(c->stream, xy3->coord.x, xy3->coord.y, z3);

        char u16[2] = {0};
        cp_write(c->stream, u16, 2);
    }
    else {
        cp_printf(c->stream,
            "  facet normal "FF" "FF" "FF"\n"
            "    outer loop\n"
            "      vertex "FF" "FF" "FF"\n"
            "      vertex "FF" "FF" "FF"\n"
            "      vertex "FF" "FF" "FF"\n"
            "    endloop\n"
            "  endfacet\n",
            xn, yn, zn,
            xy1->coord.x, xy1->coord.y, z1,
            xy2->coord.x, xy2->coord.y, z2,
            xy3->coord.x, xy3->coord.y, z3);
    }
}

static inline cp_dim_t layer_gap(cp_dim_t x)
{
    return cp_eq(x,-1) ? 0.01 : x;
}

static void poly_put_stl(
    ctxt_t *c,
    size_t zi,
    cp_csg2_poly_t *r)
{
    cp_csg2_tree_t *t = c->tree;
    double z0 = cp_v_nth(&t->z, zi);
    double z1 = z0 + cp_monus(cp_csg2_layer_thickness(t, zi), layer_gap(t->opt->layer_gap));

    cp_v_vec2_loc_t const *point = &r->point;

    /* top */
    if (!cp_eq(z0, z1)) {
        for (cp_v_each(i, &r->triangle)) {
            size_t const *p = cp_v_nth(&r->triangle, i).p;
            triangle_put_stl(c,
                0., 0., 1.,
                &cp_v_nth(point, p[1]), z1,
                &cp_v_nth(point, p[0]), z1,
                &cp_v_nth(point, p[2]), z1);
        }
    }

    /* bottom */
    for (cp_v_each(i, &r->triangle)) {
        size_t const *p = cp_v_nth(&r->triangle, i).p;
        triangle_put_stl(c,
            0., 0., -1.,
            &cp_v_nth(point, p[0]), z0,
            &cp_v_nth(point, p[1]), z0,
            &cp_v_nth(point, p[2]), z0);
    }

    /* sides */
    if (!cp_eq(z0, z1)) {
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
                 *     (pk,z[1])-------(pj,z[1])
                 *     |                   |
                 *     (pk,z[0])-------(pj,z[0])
                 *
                 * Triangles in STL are CCW, so:
                 *     (pk,z[0])--(pj,z[1])--(pk,z[1])
                 * and (pk,z[0])..(pj,z[0])--(pj,z[1])
                 */
                cp_vec3_t n;
                cp_vec3_left_normal3(&n,
                    &(cp_vec3_t){{ pk->coord.x, pk->coord.y, z0 }},
                    &(cp_vec3_t){{ pj->coord.x, pj->coord.y, z1 }},
                    &(cp_vec3_t){{ pk->coord.x, pk->coord.y, z1 }});

                triangle_put_stl(c,
                    n.x, n.y, n.z,
                    pk, z0,
                    pj, z1,
                    pk, z1);
                triangle_put_stl(c,
                    n.x, n.y, n.z,
                    pk, z0,
                    pj, z0,
                    pj, z1);
            }
        }
    }
}

static void union_put_stl(
    ctxt_t *c,
    size_t zi,
    cp_v_obj_p_t *r)
{
    v_csg2_put_stl(c, zi, r);
}

static void add_put_stl(
    ctxt_t *c,
    size_t zi,
    cp_csg_add_t *r)
{
    union_put_stl (c, zi, &r->add);
}

static void sub_put_stl(
    ctxt_t *c,
    size_t zi,
    cp_csg_sub_t *r)
{
    /* This output format cannot do SUB, only UNION, so we ignore
     * the 'sub' part.  It is wrong, but you asked for it. */
    union_put_stl (c, zi, &r->add->add);
}

static void cut_put_stl(
    ctxt_t *c,
    size_t zi,
    cp_csg_cut_t *r)
{
    /* This output format cannot do CUT, only UNION, so just print
     * the first part.  It is wrong, but you asked for it. */
    if (r->cut.size > 0) {
        union_put_stl(c, zi, &cp_v_nth(&r->cut, 0)->add);
    }
}

static void xor_put_stl(
    ctxt_t *c,
    size_t zi,
    cp_csg_xor_t *r)
{
    /* This output format cannot do CUT, only UNION, so just print
     * the first part.  It is wrong, but you asked for it. */
    if (r->xor.size > 0) {
        union_put_stl(c, zi, &cp_v_nth(&r->xor, 0)->add);
    }
}

static void layer_put_stl(
    ctxt_t *c,
    size_t zi CP_UNUSED,
    cp_csg2_layer_t *r)
{
    if (cp_csg_add_size(r->root) == 0) {
        return;
    }
    assert(zi == r->zi);
    v_csg2_put_stl(c, r->zi, &r->root->add);
}

static void stack_put_stl(
    ctxt_t *c,
    cp_csg2_stack_t *r)
{
    for (cp_v_each(i, &r->layer)) {
        layer_put_stl(c, r->idx0 + i, &cp_v_nth(&r->layer, i));
    }
}

static void csg2_put_stl(
    ctxt_t *c,
    size_t zi,
    cp_csg2_t *r)
{
    if (r == NULL) {
        return;
    }

    switch (r->type) {
    case CP_CSG_ADD:
        add_put_stl(c, zi, cp_csg_cast(cp_csg_add_t, r));
        return;

    case CP_CSG_XOR:
        xor_put_stl(c, zi, cp_csg_cast(cp_csg_xor_t, r));
        return;

    case CP_CSG_SUB:
        sub_put_stl(c, zi, cp_csg_cast(cp_csg_sub_t, r));
        return;

    case CP_CSG_CUT:
        cut_put_stl(c, zi, cp_csg_cast(cp_csg_cut_t, r));
        return;

    case CP_CSG2_POLY:
        poly_put_stl(c, zi, cp_csg2_cast(cp_csg2_poly_t, r));
        return;

    case CP_CSG2_STACK:
        stack_put_stl(c, cp_csg2_cast(cp_csg2_stack_t, r));
        return;
    }

    CP_DIE();
}

static void v_csg2_put_stl(
    ctxt_t *c,
    size_t zi,
    cp_v_obj_p_t *r)
{
    for (cp_v_each(i, r)) {
        csg2_put_stl(c, zi, cp_csg2_cast(cp_csg2_t, cp_v_nth(r, i)));
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
    cp_csg2_tree_t *t,
    bool bin)
{
    ctxt_t c = {
        .stream = s,
        .tree = t,
        .bin = bin
    };

    if (bin) {
        /* This is unfortunate: we need the number of triangles in the header. */
        /* pass 1: count */
        c.stream = NULL;
        csg2_put_stl(&c, 0, t->root);

        /* pass 2: write */
        char header[80] = {0};
        cp_write(s, header, sizeof(header));

        unsigned cnt = c.tri_count;
        write_u32(s, cnt);

        c.stream = s;
        c.tri_count = 0;
        csg2_put_stl(&c, 0, t->root);

        assert(c.tri_count == cnt);
    }
    else {
        cp_printf(s, "solid model\n");
        csg2_put_stl(&c, 0, t->root);
        cp_printf(s, "endsolid model\n");
    }
}
