/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/mat.h>
#include <hob3lbase/alloc.h>
#include <hob3l/gc.h>
#include <hob3l/obj.h>
#include <hob3l/csg.h>
#include <hob3l/csg2.h>
#include <hob3l/csg3.h>
#include <hob3l/scad.h>
#include "internal.h"

/* contexts for ctxt_t->context */
#define IN3D 0
#define IN2D 1

typedef struct {
    cp_mat3wi_t const *mat;
    cp_gc_t gc;
} mat_ctxt_t;

typedef struct {
    cp_csg3_tree_t *tree;
    cp_csg_opt_t const *opt;
    cp_err_t *err;
    unsigned context;
} ctxt_t;

__attribute__((format(printf,5,0)))
static bool vmsg(
    ctxt_t *c,
    unsigned ign,
    cp_loc_t loc,
    cp_loc_t loc2,
    char const *msg, va_list va)
{
    if (ign) {
        return true;
    }
    cp_vchar_vprintf(&c->err->msg, msg, va);
    if (loc != NULL) {
        c->err->loc = loc;
    }
    if (loc2 != NULL) {
        c->err->loc2 = loc2;
    }
    return false;
}

__attribute__((format(printf,5,6)))
static bool msg(
    ctxt_t *c,
    unsigned ign,
    cp_loc_t loc,
    cp_loc_t loc2,
    char const *msg, ...)
{
    va_list va;
    va_start(va, msg);
    bool r = vmsg(c, ign, loc, loc2, msg, va);
    va_end(va);
    return r;
}

/**
 * Returns a new unit matrix.
 */
static cp_mat3wi_t *mat_new(
    cp_csg3_tree_t *t)
{
    cp_mat3wi_t *m = CP_NEW(*m);
    cp_mat3wi_unit(m);
    cp_v_push(&t->mat, m);
    return m;
}

static cp_mat3wi_t const *the_unit(
    cp_csg3_tree_t *result)
{
    if (result->mat.size == 0) {
        return mat_new(result);
    }
    return cp_v_nth(&result->mat, 0);
}

static void mat_ctxt_init(
    mat_ctxt_t *m,
    cp_csg3_tree_t *t)
{
    CP_ZERO(m);
    m->mat = the_unit(t);
    m->gc.color.r = 220;
    m->gc.color.g = 220;
    m->gc.color.b = 64;
    m->gc.color.a = 255;
}

static bool csg3_from_scad(
    /**
     * 'non-empty object': set to true if a non-ignored object is
     * added to the result 'r'.  Never set to 'false' by the callee:
     * that's a responsibility of the caller.
     * Note that this conversion algorithm may not push anything to
     * \a r, but still set *no=true, because by scad's definition,
     * the input is non-empty.
     */
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *m,
    cp_scad_t const *s);

static bool csg3_from_v_scad(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *m,
    cp_v_scad_p_t const *ss)
{
    for (cp_v_each(i, ss)) {
        if (!csg3_from_scad(no, r, c, m, cp_v_nth(ss, i))) {
            return false;
        }
    }
    return true;
}

static bool csg3_from_union(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *m,
    cp_scad_union_t const *s)
{
    return csg3_from_v_scad(no, r, c, m, &s->child);
}

static bool csg3_from_difference(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *m,
    cp_scad_difference_t const *s)
{
    cp_v_obj_p_t f = CP_V_INIT;

    /* First child is positive.
     *
     * Actually, we need to add all children that are not ignored and
     * not empty (like 'group() {}').  Well, shapes that yield no
     * output are not ignored, like 'cylinder(h = -1, d = 1)' will
     * cause a difference to be empty.  We reject those by default
     * with an error, but in case we ever instead render them empty
     * like with a command line switch, we will have to generate an
     * empty shape in order to indicate that something is there to be
     * subtracted from.
     *
     * I find these rules quite offensive, because what is subtracted
     * from what should be a pure matter of syntax.  The semantics of
     * OpenSCAD is really weird.  Unsound even.  At least dirty
     * and informal.
     */
    bool add_no = false;
    size_t sub_i = 0;
    while ((sub_i < s->child.size) && !add_no) {
        if (!csg3_from_scad(&add_no, &f, c, m, cp_v_nth(&s->child, sub_i))) {
            return false;
        }
        sub_i++;
    }

    if (add_no) {
        *no = true;
    }

    if (f.size == 0) {
        /* empty, ignore */
        return true;
    }

    if ((f.size == 1) && (cp_v_nth(&f, 0)->type == CP_CSG3_SUB)) {
        cp_v_push(r, cp_v_nth(&f, 0));

        /* all others children are also negative */
        for (cp_v_each(i, &s->child, sub_i)) {
            if (!csg3_from_scad(
                no,
                &cp_csg_cast(cp_csg_sub_t, cp_v_nth(&f, 0))->sub->add,
                c, m, cp_v_nth(&s->child, i)))
            {
                return false;
            }
        }

        /* This does not change the bounding box of the cp_v_nth(&f, 0), as only
         * more stuff was subtracted, which we neglect for bb computation. */

        cp_v_fini(&f);
        return true;
    }

    cp_v_obj_p_t g = CP_V_INIT;

    /* all others children are negative */
    for (cp_v_each(i, &s->child, sub_i)) {
        if (!csg3_from_scad(no, &g, c, m, cp_v_nth(&s->child, i))) {
            return false;
        }
    }

    if (g.size == 0) {
        /* no more children => nothing to subtract => push f to output */
        cp_v_append(r, &f);
        cp_v_fini(&f);
        return true;
    }

    cp_csg_sub_t *o = cp_csg_new(*o, s->loc);
    cp_v_push(r, cp_obj(o));

    o->add = cp_csg_new(*o->add, s->loc);
    o->add->add = f;

    o->sub = cp_csg_new(*o->add, s->loc);
    o->sub->add = g;

    return true;
}

static void csg3_cut_push_add(
    cp_v_csg_add_p_t *cut,
    cp_v_obj_p_t *add)
{
    if (add->size > 0) {
        cp_csg_add_t *a = cp_csg_new(*a, cp_v_nth(add,0)->loc);

        a->add = *add;

        cp_v_push(cut, a);

        CP_ZERO(add);
    }
}

static bool csg3_from_intersection(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *m,
    cp_scad_intersection_t const *s)
{
    cp_v_csg_add_p_t cut = CP_V_INIT;

    /* each child is a union */
    cp_v_obj_p_t add = CP_V_INIT;
    for (cp_v_each(i, &s->child)) {
        csg3_cut_push_add(&cut, &add);
        if (!csg3_from_scad(no, &add, c, m, cp_v_nth(&s->child, i))) {
            return false;
        }
    }

    if (cut.size == 0) {
        cp_v_append(r, &add);
        cp_v_fini(&add);
        return true;
    }

    csg3_cut_push_add(&cut, &add);
    assert(cut.size >= 2);

    cp_csg_cut_t *o = cp_csg_new(*o, s->loc);
    cp_v_push(r, cp_obj(o));

    o->cut = cut;

    return true;
}

static bool csg3_from_translate(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_translate_t const *s)
{
    if (cp_vec3_has_len0(&s->v)) {
        /* Avoid math ops unless necessary: for 0 length xlat,
         * it is not necessary */
        return csg3_from_v_scad(no, r, c, mo, &s->child);
    }

    cp_mat3wi_t *m1 = mat_new(c->tree);
    cp_mat3wi_xlat_v(m1, &s->v);
    cp_mat3wi_mul(m1, mo->mat, m1);

    mat_ctxt_t mn = *mo;
    mn.mat = m1;
    return csg3_from_v_scad(no, r, c, &mn, &s->child);
}

static bool csg3_from_mirror(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_mirror_t const *s)
{
    if (cp_vec3_has_len0(&s->v)) {
        return msg(c, CP_ERR_FAIL, s->loc, NULL, "Mirror plane vector has length zero.\n");
    }

    cp_mat3wi_t *m1 = mat_new(c->tree);
    cp_mat3wi_mirror_v(m1, &s->v);
    cp_mat3wi_mul(m1, mo->mat, m1);

    mat_ctxt_t mn = *mo;
    mn.mat = m1;
    return csg3_from_v_scad(no, r, c, &mn, &s->child);
}

static bool good_scale(
    cp_vec3_t const *v)
{
    return
        !cp_eq(v->x, 0) &&
        !cp_eq(v->y, 0) &&
        !cp_eq(v->z, 0);
}

static bool good_scale2(
    cp_vec2_t const *v)
{
    return
        !cp_eq(v->x, 0) &&
        !cp_eq(v->y, 0);
}

static bool csg3_from_scale(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_scale_t const *s)
{
    if (!good_scale(&s->v)) {
        return msg(c, c->opt->err_collapse, s->loc, NULL,
           "Expected non-zero scale, but v=["FF3"].\n", CP_V012(s->v));
    }
    cp_mat3wi_t *m1 = mat_new(c->tree);
    cp_mat3wi_scale_v(m1, &s->v);
    cp_mat3wi_mul(m1, mo->mat, m1);

    mat_ctxt_t mn = *mo;
    mn.mat = m1;
    return csg3_from_v_scad(no, r, c, &mn, &s->child);
}

static bool csg3_from_multmatrix(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_multmatrix_t const *s)
{
    cp_mat3wi_t *m1 = mat_new(c->tree);
    if (!cp_mat3wi_from_mat3w(m1, &s->m)) {
        return msg(c, c->opt->err_collapse, s->loc, NULL, "Non-invertible matrix.\n");
    }
    cp_mat3wi_mul(m1, mo->mat, m1);

    mat_ctxt_t mn = *mo;
    mn.mat = m1;
    return csg3_from_v_scad(no, r, c, &mn, &s->child);
}

static bool csg3_from_color(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_color_t const *s)
{
    mat_ctxt_t mn  = *mo;
    mn.gc.color.a = s->rgba.a;
    if (s->valid) {
       mn.gc.color.rgb = s->rgba.rgb;
    }
    return csg3_from_v_scad(no, r, c, &mn, &s->child);
}

static bool csg3_from_rotate(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_rotate_t const *s)
{
    cp_mat3wi_t *m1 = mat_new(c->tree);
    if (s->around_n) {
        cp_mat3wi_rot_v(m1, &s->n, CP_SINCOS_DEG(s->a));
    }
    else {
        cp_mat3wi_rot_z(m1, CP_SINCOS_DEG(s->n.z));

        cp_mat3wi_t m2;
        cp_mat3wi_rot_y(&m2, CP_SINCOS_DEG(s->n.y));
        cp_mat3wi_mul(m1, m1, &m2);

        cp_mat3wi_rot_x(&m2, CP_SINCOS_DEG(s->n.x));
        cp_mat3wi_mul(m1, m1, &m2);
    }
    cp_mat3wi_mul(m1, mo->mat, m1);

    mat_ctxt_t mn = *mo;
    mn.mat = m1;
    return csg3_from_v_scad(no, r, c, &mn, &s->child);
}

/**
 * Requires a convex face to work properly */
static void face_basics(
    cp_csg3_face_t *face,
    bool rev,
    cp_loc_t loc)
{
    assert(face->point.size >= 3);

    /* set location */
    face->loc = loc;

    /* Allocate edge array already, but leave it zero. */
    cp_v_init0(&face->edge, face->point.size);

    /* Possibly reverse */
    if (rev) {
        cp_v_reverse(&face->point, 0, face->point.size);
    }
}

static cp_csg3_face_t *face_init_from_point_ref(
    cp_csg3_face_t *face,
    cp_csg3_poly_t const *poly,
    size_t const *data,
    size_t size,
    bool rev,
    cp_loc_t loc)
{
    assert(size >= 3);
    assert(face->point.size == 0);
    assert(face->point.data == NULL);
    assert(face->edge.size == 0);
    assert(face->edge.data == NULL);

    /* Instead, first add all the stuff to the point array,
     * which will be discarded later. */
    cp_v_init0(&face->point, size);
    for (cp_v_each(i, &face->point)) {
        assert(i < size);
        cp_v_nth(&face->point, i).ref = &cp_v_nth(&poly->point, data[i]);
        cp_v_nth(&face->point, i).loc = loc;
    }

    face_basics(face, rev, loc);

    return face;
}

static int cmp_edge(
    cp_csg3_edge_t const *a,
    cp_csg3_edge_t const *b,
    void *u __unused)
{
    /* by order: src < dst comes before src > dst. */
    if ((a->src->ref < a->dst->ref) != (b->src->ref < b->dst->ref)) {
        return a->src->ref < a->dst->ref ? -1 : +1;
    }
    /* by src */
    if (a->src->ref != b->src->ref) {
        return a->src->ref < b->src->ref ? -1 : +1;
    }
    /* by dst */
    if (a->dst->ref != b->dst->ref) {
        return a->dst->ref < b->dst->ref ? -1 : +1;
    }
    return 0;
}

/**
 * Convert the point-wise representation into an edge-wise
 * representation.  This also checks soundness of the
 * polyhedron, because an unsound polyhedron cannot be
 * converted into edge representation.
 *
 * The only thing we don't see here if we have an inside-out
 * polyhedron.  But I am not even sure where this is invalid -- I
 * suppose the subsequent algorithms should be working anyway.
 */
static bool poly_make_edges(
    cp_csg3_poly_t *r,
    ctxt_t *c)
{
    /* Number of edges is equal to number of total face point divided
     * by two, because each pair of consecutive points is translated to
     * one face, and each face must be defined exactly twice, once
     * foreward, once backward.
     */
    size_t point_cnt = 0;
    for (cp_v_each(i, &r->face)) {
        point_cnt += cp_v_nth(&r->face, i).point.size;
    }

    /* Ignore for now if point_cnt is odd.  This is wrong, but due to this,
     * we expect some problem with wrong edges, for which we will be able
     * to give a better error message than 'Odd number of vertices in
     * polyhedron'. */

    /* Step 1:
     * Insert all edges, i.e., the array will have double the required size.
     * This is done so that we can give good error message if some edges have
     * no buddy.  The array will be resized later, after checking that
     * everything is OK.
     */
    cp_v_init0(&r->edge, point_cnt);
    size_t edge_i = 0;
    for (cp_v_each(i, &r->face)) {
        cp_csg3_face_t *f = &cp_v_nth(&r->face, i);
        for (cp_v_each(j1, &f->point)) {
            size_t j2 = cp_wrap_add1(j1, f->point.size);
            assert(edge_i < r->edge.size);
            cp_v_nth(&r->edge, edge_i).src = &cp_v_nth(&f->point, j1);
            cp_v_nth(&r->edge, edge_i).dst = &cp_v_nth(&f->point, j2);
            edge_i++;
        }
    }

    /* Step 2:
     * Sort and find duplicates */
    cp_v_qsort(&r->edge, 0, CP_SIZE_MAX, cmp_edge, NULL);
    for (cp_v_each(i, &r->edge, 1)) {
        cp_csg3_edge_t const *a = &cp_v_nth(&r->edge, i-1);
        cp_csg3_edge_t const *b = &cp_v_nth(&r->edge, i);
        if ((a->src->ref == b->src->ref) && (a->dst->ref == b->dst->ref)) {
            return msg(c, CP_ERR_FAIL, a->src->loc, b->src->loc,
                "Identical edge occurs more than once in polyhedron.\n");
        }
    }

    /* Step 3:
     * Assign edges for each polygon; find back edges; report errors */
    size_t max_idx = 0;
    for (cp_v_each(i, &r->face)) {
        cp_csg3_face_t *f = &cp_v_nth(&r->face, i);
        if (f->point.size != f->edge.size) {
            return msg(c, CP_ERR_FAIL, f->loc, NULL,
                "Face edge array should be preallocated, "
                "but point.size=%"_Pz"u, edge.size=%"_Pz"u\n"
                " Internal Error.\n",
                f->point.size, f->edge.size);
        }

        assert((f->point.size == f->edge.size) && "edge should be preallocated");
        for (cp_v_each(j1, &f->point)) {
            size_t j2 = cp_wrap_add1(j1, f->point.size);

            cp_csg3_edge_t k;
            k.src = &cp_v_nth(&f->point, j1);
            k.dst = &cp_v_nth(&f->point, j2);
            if (k.src->ref > k.dst->ref) {
                CP_SWAP(&k.src, &k.dst);
            }

            size_t h = cp_v_bsearch(&k, &r->edge, cmp_edge, (void*)&r->point);
            if (!(h < r->edge.size)) {
                return msg(c, CP_ERR_FAIL, cp_v_nth(&f->point, j1).loc, NULL,
                    "Edge has no adjacent reverse edge in polyhedron.\n");
            }
            assert(h != CP_SIZE_MAX);
            assert(h < r->edge.size);
            if (h > max_idx) {
                max_idx = h;
            }
            cp_csg3_edge_t *edge = &cp_v_nth(&r->edge, h);
            if (k.src->ref == cp_v_nth(&f->point, j1).ref) {
                /* fore */
                if (edge->fore != NULL) {
                    assert(edge->src != k.src);
                    assert(edge->dst != k.dst);
                    return msg(c, CP_ERR_FAIL, k.src->loc, edge->src->loc,
                        "Edge occurs multiple times in polyhedron.\n");
                }
                assert(edge->fore == NULL);
                edge->fore = f;
                assert(cp_v_idx(&edge->fore->point, edge->src) == j1);
            }
            else {
                /* back */
                if (edge->back != NULL) {
                    assert(edge->src != k.src);
                    assert(edge->dst != k.dst);
                    return msg(c, CP_ERR_FAIL, k.dst->loc, edge->dst->loc,
                        "Edge occurs multiple times in polyhedron.\n");
                }
                assert(edge->back == NULL);
                edge->back = f;
                /* Reset dst so that edge->dst is the source of the back edge.
                 * This will allow to find the input file location of the backward
                 * edge in the above error message, like edge->src locates the
                 * foreward edge. */
                assert(edge->dst->ref == k.dst->ref);
                assert(edge->dst != k.dst);
                edge->dst = k.dst;
                assert(cp_v_idx(&edge->back->point, edge->dst) == j1);
            }

            /* store the edge in the face */
            cp_v_nth(&f->edge, j1) = edge;
        }
    }

    /* More checks that all edges have a buddy (may be redundant, I don't know,
     * the checks have complex dependencies). */
    for (cp_v_each(i, &r->edge)) {
        cp_csg3_edge_t const *b = &cp_v_nth(&r->edge, i);
        if ((b->src->ref < b->dst->ref) && (b->back == NULL)) {
            return msg(c, CP_ERR_FAIL, b->src->loc, NULL,
                "Edge has no adjacent reverse edge in polyhedron.\n");
        }
    }
    if (max_idx >= point_cnt/2) {
        cp_csg3_edge_t const *b = &cp_v_nth(&r->edge, point_cnt/2);
        return msg(c, CP_ERR_FAIL, b->src->loc, NULL,
            "Edge has no adjacent reverse edge in polyhedron.\n");
    }
    r->edge.size = point_cnt/2;

    /* If we had no error by now, so something went wrong. */
    assert((point_cnt & 1) == 0);
    return true;
}

static void csg3_sphere_minmax1(
    cp_vec3_minmax_t *bb,
    cp_mat3wi_t const *mat,
    size_t i)
{
    /* Computing the bounding box of transformed unit sphere is not
     * trivial.  The following was summaries by Tavian Barnes
     * (www.tavianator.com). */
    double a = mat->n.w.v[i];
    double m0 = mat->n.b.m[i][0];
    double m1 = mat->n.b.m[i][1];
    double m2 = mat->n.b.m[i][2];
    double b = m0*m0 + m1*m1 + m2*m2;
    double c = sqrt(b);
    double l = a - c;
    double h = a + c;
    if (l < bb->min.v[i]) { bb->min.v[i] = l; }
    if (h > bb->min.v[i]) { bb->max.v[i] = h; }
}

static void csg3_sphere_minmax(
    cp_vec3_minmax_t *bb,
    cp_mat3wi_t const *mat)
{
    csg3_sphere_minmax1(bb, mat, 0);
    csg3_sphere_minmax1(bb, mat, 1);
    csg3_sphere_minmax1(bb, mat, 2);
}

static size_t get_fn(
    cp_csg_opt_t const *opt,
    size_t fn,
    bool have_circular)
{
    if (fn == 0) {
        return have_circular ? 0 : opt->max_fn;
    }
    if (fn > opt->max_fn) {
        return have_circular ? 0 : fn;
    }
    if (fn < 3) {
        return 3;
    }
    return fn;
}

/**
 * From an array of points in the rough shape of a tower,
 * make a polyhedron.  'Tower' means the shape consists
 * of layers of polygon points stack on each other.
 *
 * This function also handles the case of the top collapsing into a
 * single point.
 *
 * So this shape works for (polyhedronized) cylinders, cones,
 * spheres, cubes, and linear_extrudes.
 *
 * If the connecting quads are not planar, then tri_side can be
 * set to non-false to split them into triangles.  This shape is
 * probably not nice, but correct in that the faces are planar,
 * since every triangle is trivially planar).
 *
 * The top and bottom faces must be planar.
 *
 * rev^(m->d < 0) inverts face vertex order to allow managing
 * mirroring and negative determinants.  This also gives some freedom
 * for the construction: if top and bottom are swapped (i.e., the
 * points 0..fn-1 are the top, not the bottom), then rev be passed as
 * non-false.
 *
 * This also runs xform and minmax, but not make_edges.
 */
static void faces_from_tower(
    cp_csg3_poly_t *o,
    cp_mat3wi_t const *m,
    cp_loc_t loc,
    size_t fn,
    size_t fnz,
    bool rev,
    bool tri_side)
{
    /* reverse based on determinant */
    if (m->d < 0) {
        rev = !rev;
    }

    /* in-place xform */
    for (cp_v_each(i, &o->point)) {
        cp_vec3w_xform(&cp_v_nth(&o->point, i).coord, &m->n, &cp_v_nth(&o->point, i).coord);
    }

    bool has_top = (o->point.size == fn*fnz);
    assert(has_top || (o->point.size == 1 + (fn * (fnz - 1))));

    /* generate faces */
    size_t k = 0;
    cp_v_init0(&o->face, 1U + has_top + ((fnz - 1) * fn * (1U + tri_side)));;

    /* bottom */
    cp_csg3_face_t *f = &cp_v_nth(&o->face, k++);
    cp_v_init0(&f->point, fn);
    for (cp_size_each(j, fn)) {
        cp_vec3_loc_ref_t *v = &cp_v_nth(&f->point, j);
        v->ref = &cp_v_nth(&o->point, j);
        v->loc = loc;
    }
    face_basics(f, rev, loc);

    if (has_top) {
        /* top */
        f = &cp_v_nth(&o->face, k++);
        cp_v_init0(&f->point, fn);
        for (cp_size_each(j, fn)) {
            cp_vec3_loc_ref_t *v = &cp_v_nth(&f->point, j);
            v->ref = &cp_v_nth(&o->point, o->point.size - j - 1);
            v->loc = loc;
        }
        face_basics(f, rev, loc);
    }

    /* sides */
    for (cp_size_each(i, fnz, 1, !has_top)) {
        size_t k1 = i * fn;
        size_t k0 = k1 - fn;
        for (cp_size_each(j0, fn)) {
            size_t j1 = cp_wrap_add1(j0, fn);
            f = &cp_v_nth(&o->face, k++);
            if (tri_side) {
                face_init_from_point_ref(
                    f, o, (size_t[4]){ k0+j0, k0+j1, k1+j1 }, 3, !rev, loc);
                f = &cp_v_nth(&o->face, k++);
                face_init_from_point_ref(
                    f, o, (size_t[4]){ k1+j1, k1+j0, k0+j0 }, 3, !rev, loc);
            }
            else {
                face_init_from_point_ref(
                    f, o, (size_t[4]){ k0+j0, k0+j1, k1+j1, k1+j0 }, 4, !rev, loc);
            }
        }
    }

    if (!has_top) {
        /* roof */
        size_t kw = o->point.size - 1;
        size_t kv = kw - fn;
        for (cp_size_each(j0, fn)) {
            size_t j1 = cp_wrap_add1(j0, fn);
            f = &cp_v_nth(&o->face, k++);
            face_init_from_point_ref(
                f, o, (size_t[4]){ kv+j0, kv+j1, kw }, 3, !rev, loc);
        }
    }

    assert(o->face.size == k);
}

static void set_vec3_loc(
    cp_vec3_loc_t *p,
    double x, double y, double z,
    cp_loc_t loc)
{
    p->coord.x = x;
    p->coord.y = y;
    p->coord.z = z;
    p->loc = loc;
}

static void csg3_poly_make_sphere(
    cp_csg3_poly_t *o,
    cp_mat3wi_t const *m,
    cp_scad_sphere_t const *s,
    size_t fn)
{
    assert(fn >= 3);

    /* This is modelled after what OpenSCAD 2015.3 does */
    size_t fnz = (fn + 1) >> 1;
    assert(fnz >= 2);

    /* generate the points */
    cp_v_init0(&o->point, fn * fnz);
    double fnza = CP_PI / cp_f(fnz * 2);
    double fna = CP_TAU / cp_f(fn);
    cp_vec3_loc_t *p = o->point.data;
    for (cp_size_each(i, fnz)) {
        double w = cp_f(1 + (2*i)) * fnza;
        double z = cos(w);
        double r = sin(w);
        for (cp_size_each(j, fn)) {
            assert(p < (o->point.data + o->point.size));
            double t = cp_f(j) * fna;
            set_vec3_loc(p++, r*cos(t), r*sin(t), z, s->loc);
        }
    }

    /* make faces */
    faces_from_tower(o, m, s->loc, fn, fnz, true, false);
}

static bool csg3_from_sphere(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_sphere_t const *s)
{
    if (c->context != IN3D) {
        return msg(c, c->opt->err_outside_3d, s->loc, NULL, "'sphere' found outside 3D context.");
    }
    *no = true;

    if (cp_le(s->r, 0)) {
        return msg(c, c->opt->err_empty, s->loc, NULL,
            "Expected non-empty sphere, found r="FF"\n", s->r);
    }

    cp_mat3wi_t const *m = mo->mat;
    if (!cp_eq(s->r, 1)) {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_scale1(m1, s->r);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    size_t fn = get_fn(c->opt, s->_fn, true);
    if (fn > 0) {
        cp_csg3_poly_t *o = cp_csg3_new_obj(*o, s->loc, mo->gc);
        cp_v_push(r, cp_obj(o));

        csg3_poly_make_sphere(o, m, s, fn);
        if (!poly_make_edges(o, c)) {
            return msg(c, CP_ERR_FAIL, NULL, NULL,
                " Internal Error: Sphere polyhedron construction algorithm is broken.\n");
        }
        return true;
    }

    cp_csg3_sphere_t *o = cp_csg3_new_obj(*o, s->loc, mo->gc);
    cp_v_push(r, cp_obj(o));

    o->mat = m;
    o->_fa = s->_fa;
    o->_fs = s->_fs;
    o->_fn = c->opt->max_fn;

    return true;
}

#if 0
/* FIXME: continue */
static bool csg3_from_circle(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_circle_t const *s)
{
    *no = true;
    if (c->context != IN2D) {
        if (!msg(c, c->opt->err_2d_in_3d, s->loc, NULL, "Circle found outside 2D context.")) {
            return false;
        }
    }

    if (cp_leq(s->r, 0)) {
        if (t->opt->err_empty) {
            return true;
        }
        cp_vchar_printf(&e->msg, "Expected non-empty circle, found r="FF"\n", s->r);
        e->loc = s->loc;
        return false;
    }
    cp_mat3wi_t const *m = mo->mat;
    if (!cp_equ(s->r, 1)) {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_scale1(m1, s->r);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    cp_csg3_2d_t *o = cp_csg3_new_obj(*o, s->loc, mo->gc);
    cp_v_push(r, cp_obj());

    o->csg3 = cp_csg2_new(*o->csg3, s->loc);

    o->mat.n = CP_MAT2W(
        m->n.b.m[0][0], m->n.b.m[0][1], m->n.w.v[0],
        m->n.b.m[1][0], m->n.b.m[1][1], m->n.w.v[1]);
    o->mat.i = CP_MAT2W(
        m->i.b.m[0][0], m->i.b.m[0][1], m->i.w.v[0],
        m->i.b.m[1][0], m->i.b.m[1][1], m->i.w.v[1]);
    o->_fa = s->_fa;
    o->_fs = s->_fs;
    o->_fn = get_fn(&t->opt, s->_fn, true);
    return true;
}
#endif

static int cmp_vec2_loc(
    cp_vec2_loc_t const *a,
    cp_vec2_loc_t const *b,
    void *user __unused)
{
    return cp_vec2_lex_cmp(&a->coord, &b->coord);
}

static int cmp_vec3_loc(
    cp_vec3_loc_t const *a,
    cp_vec3_loc_t const *b,
    void *user __unused)
{
    return cp_vec3_lex_cmp(&a->coord, &b->coord);
}

static bool csg3_from_polyhedron(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *m,
    cp_scad_polyhedron_t const *s)
{
    if (c->context != IN3D) {
        return msg(c, c->opt->err_outside_3d, s->loc, NULL,
            "'polyhedron' found outside 3D context.");
    }
    *no = true;

    if (s->points.size < 4) {
        return msg(c, c->opt->err_empty, s->loc, NULL,
            "Polyhedron needs at least 4 points, but found only %"_Pz"u.\n",
            s->points.size);
    }
    if (s->faces.size < 4) {
        return msg(c, c->opt->err_empty, s->loc, NULL,
            "Polyhedron needs at least 4 faces, but found only %"_Pz"u.\n",
            s->faces.size);
    }

    cp_csg3_poly_t *o = cp_csg3_new_obj(*o, s->loc, m->gc);
    cp_v_push(r, cp_obj(o));

    /* check that no point is duplicate: abuse the array we'll use in
     * the end, too, for temporarily sorting the points */
    /* copy points (same data type, just copy the array) */
    cp_v_init_with(&o->point, s->points.data, s->points.size);

    cp_v_qsort(&o->point, 0, CP_SIZE_MAX, cmp_vec3_loc, NULL);
    for (cp_v_each(i, &o->point, 1)) {
        cp_vec3_loc_t const *a = &cp_v_nth(&o->point, i-1);
        cp_vec3_loc_t const *b = &cp_v_nth(&o->point, i);
        if (cp_vec3_eq(&a->coord, &b->coord)) {
            return msg(c, CP_ERR_FAIL, a->loc, b->loc, "Duplicate point in polyhedron.\n");
        }
    }

    /* copy points (same data type, just copy the array) */
    cp_v_init_with(&o->point, s->points.data, s->points.size);

    /* in-place xform */
    for (cp_v_each(i, &o->point)) {
        cp_vec3w_xform(&cp_v_nth(&o->point, i).coord, &m->mat->n, &cp_v_nth(&o->point, i).coord);
    }

    /* copy faces */
    cp_v_init0(&o->face, s->faces.size);
    for (cp_v_each(i, &s->faces)) {
        cp_scad_face_t *sf = &cp_v_nth(&s->faces, i);
        cp_csg3_face_t *cf = &cp_v_nth(&o->face, i);
        cf->loc = sf->loc;

        /* copy the point indices (same data type, but references into different array) */
        cp_v_init0(&cf->point, sf->points.size);
        for (cp_v_each(j, &sf->points)) {
            size_t idx = cp_v_idx(&s->points, cp_v_nth(&sf->points, j).ref);
            cp_v_nth(&cf->point, j).ref = o->point.data + idx;
            cp_v_nth(&cf->point, j).loc = cp_v_nth(&sf->points, j).loc;
        }

        /* init edge to same size as point */
        cp_v_init0(&cf->edge,  cf->point.size);
    }

    return poly_make_edges(o, c);
}

static bool csg3_from_polygon(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *m,
    cp_scad_polygon_t const *s)
{
    if (c->context != IN2D) {
        return msg(c, c->opt->err_outside_2d, s->loc, NULL, "'polygon' found outside 2D context.");
    }
    *no = true;

    if (s->points.size < 3) {
        return msg(c, c->opt->err_empty, s->loc, NULL,
            "Polygons needs at least 3 points, but found only %"_Pz"u.\n",
             s->points.size);
    }

    cp_csg2_poly_t *o = cp_csg2_new(*o, s->loc);
    cp_v_push(r, cp_obj(o));

    /* check that no point is duplicate: abuse the array we'll use in
     * the end, too, for temporarily sorting the points */
    /* copy points (same data type, just copy the array) */
    cp_v_init_with(&o->point, s->points.data, s->points.size);

    cp_v_qsort(&o->point, 0, CP_SIZE_MAX, cmp_vec2_loc, NULL);
    for (cp_v_each(i, &o->point, 1)) {
        cp_vec2_loc_t const *a = &cp_v_nth(&o->point, i-1);
        cp_vec2_loc_t const *b = &cp_v_nth(&o->point, i);
        if (cp_vec2_eq(&a->coord, &b->coord)) {
            return msg(c, CP_ERR_FAIL, a->loc, b->loc,
                "Duplicate point in polygon.\n");
        }
    }

    /* copy points (same data type, just copy the array) */
    cp_v_init_with(&o->point, s->points.data, s->points.size);

    /* in-place xform + color */
    for (cp_v_each(i, &o->point)) {
        cp_vec3_t v = { .z = 0 };
        cp_vec2_loc_t *w = &cp_v_nth(&o->point, i);
        v.b = w->coord;
        cp_vec3w_xform(&v, &m->mat->n, &v);
        w->coord = v.b;
        w->color = m->gc.color;
    }

    /* copy faces */
    cp_v_init0(&o->path, s->paths.size);
    for (cp_v_each(i, &s->paths)) {
        cp_scad_path_t *sf = &cp_v_nth(&s->paths, i);
        cp_csg2_path_t *cf = &cp_v_nth(&o->path, i);

        /* copy the point indices (same data type, but references into different array) */
        cp_v_init0(&cf->point_idx, sf->points.size);
        for (cp_v_each(j, &sf->points)) {
            size_t idx = cp_v_idx(&s->points, cp_v_nth(&sf->points, j).ref);
            cp_v_nth(&cf->point_idx, j) = idx;
        }
    }

    return true;
}

static bool csg3_from_cube(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_cube_t const *s)
{
    if (c->context != IN3D) {
        return msg(c, c->opt->err_outside_3d, s->loc, NULL, "'cube' found outside 3D context.");
    }
    *no = true;

    /* size 0 in any direction is an error */
    if (!good_scale(&s->size)) {
        return msg(c, c->opt->err_empty, s->loc, NULL,
            "Expected non-empty cube, but size=["FF3"].\n",
            CP_V012(s->size));
    }

    cp_mat3wi_t const *m = mo->mat;

    /* possibly scale */
    if (!cp_eq(s->size.x, 1) ||
        !cp_eq(s->size.y, 1) ||
        !cp_eq(s->size.z, 1))
    {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_scale_v(m1, &s->size);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* possibly translate to center */
    if (s->center) {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_xlat(m1, -.5, -.5, -.5);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* make points */
    cp_csg3_poly_t *o = cp_csg3_new_obj(*o, s->loc, mo->gc);
    cp_v_push(r, cp_obj(o));

    o->is_cube = cp_mat3_is_rect_rot(&m->n.b);

    //   1----0
    //  /|   /|
    // 2----3 |
    // | 5--|-4
    // |/   |/
    // 6----7
    cp_v_init0(&o->point, 8);
    for (cp_size_each(i, 8)) {
        set_vec3_loc(&cp_v_nth(&o->point, i), !(i&1)^!(i&2), !(i&2), !(i&4), s->loc);
    }

    /* make faces */
    faces_from_tower(o, m, s->loc, 4, 2, false, false);

    /* make edges */
    if (!poly_make_edges(o, c)) {
        return msg(c, CP_ERR_FAIL, NULL, NULL,
            " Internal Error: Cube polyhedron construction algorithm is broken.\n");
    }
    return true;
}

static bool csg3_from_square(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_square_t const *s)
{
    if (c->context != IN2D) {
        return msg(c, c->opt->err_outside_2d, s->loc, NULL, "'square' found outside 2D context.");
    }
    *no = true;

    /* size 0 in any direction is an error */
    if (!good_scale2(&s->size)) {
        return msg(c, c->opt->err_empty, s->loc, NULL,
            "Expected non-empty square, but size=["FF2"].\n",
            CP_V01(s->size));
    }

    cp_mat3wi_t const *m = mo->mat;

    /* possibly scale */
    if (!cp_eq(s->size.x, 1) ||
        !cp_eq(s->size.y, 1))
    {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_scale(m1, s->size.x, s->size.y, 1);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* possibly translate to center */
    if (s->center) {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_xlat(m1, -.5, -.5, 0);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* make square shape */
    cp_csg2_poly_t *o = cp_csg2_new(*o, s->loc);
    cp_v_push(r, cp_obj(o));

    cp_csg2_path_t *path = cp_v_push0(&o->path);
    for (cp_size_each(i, 4)) {
        cp_vec2_loc_t *p = cp_v_push0(&o->point);
        p->coord.x = cp_dim(i & 1);
        p->coord.y = cp_dim(i & 2);
        p->loc = s->loc;
        p->color = mo->gc.color;
    }

    cp_v_push(&path->point_idx, 0);
    cp_v_push(&path->point_idx, 1);
    cp_v_push(&path->point_idx, 3);
    cp_v_push(&path->point_idx, 2);

    return true;
}

static bool csg3_poly_cylinder(
    cp_v_obj_p_t *r,
    ctxt_t *c,
    cp_mat3wi_t const *m,
    cp_scad_cylinder_t const *s,
    mat_ctxt_t const *mo,
    cp_scale_t r2,
    size_t fn)
{
    cp_csg3_poly_t *o = cp_csg3_new_obj(*o, s->loc, mo->gc);
    cp_v_push(r, cp_obj(o));

    /* make points */
    if (cp_eq(r2, 0)) {
        /* cone */
        cp_v_init0(&o->point, fn + 1);
        for (cp_size_each(i, fn)) {
            cp_angle_t a = cp_angle(i) * (CP_TAU / cp_angle(fn));
            set_vec3_loc(&cp_v_nth(&o->point, i), sin(a), cos(a), -.5, s->loc);
        }
        set_vec3_loc(&cp_v_nth(&o->point, fn), 0, 0, +.5, s->loc);
    }
    else {
        /* cylinder */
        cp_v_init0(&o->point, 2*fn);
        for (cp_size_each(i, fn)) {
            cp_angle_t a = cp_angle(i) * (CP_TAU / cp_angle(fn));
            cp_scale_t ss = sin(a);
            cp_scale_t cc = cos(a);
            set_vec3_loc(&cp_v_nth(&o->point, i),    cc,    ss,    -.5, s->loc);
            set_vec3_loc(&cp_v_nth(&o->point, i+fn), cc*r2, ss*r2, +.5, s->loc);
        }
    }

    /* make faces */
    faces_from_tower(o, m, s->loc, fn, 2, false, false);

    /* make edges */
    if (!poly_make_edges(o, c)) {
        return msg(c, CP_ERR_FAIL, NULL, NULL,
            " Internal Error: Cylinder polyhedron construction algorithm is broken.\n");
    }
    return true;
}

static bool csg3_from_cylinder(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_cylinder_t const *s)
{
    if (c->context != IN3D) {
        return msg(c, c->opt->err_outside_3d, s->loc, NULL,
            "'cylinder' found outside 3D context.");
    }
    *no = true;

    cp_scale_t r1 = s->r1;
    cp_scale_t r2 = s->r2;

    if (cp_le(s->h, 0)) {
        return msg(c, c->opt->err_empty, s->loc, NULL,
            "Expected non-empty cylinder, but h="FF".\n", s->h);
    }
    if (cp_le(r1, 0) && cp_le(r2, 0)) {
        return msg(c, c->opt->err_empty, s->loc, NULL,
            "Expected non-empty cylinder, but r1="FF", r2="FF".\n",
            r1, r2);
    }

    cp_mat3wi_t const *m = mo->mat;

    /* normalise height */
    if (!cp_eq(s->h, 1)) {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_scale(m1, 1, 1, s->h);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* normalise center */
    if (!s->center) {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_xlat(m1, 0, 0, +.5);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* normalise radii */
    if (r1 < r2) {
        /* want smaller diameter (especially 0) on top */
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_scale(m1, 1, 1, -1);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
        CP_SWAP(&r1, &r2);
    }

    if (!cp_eq(r1, 1)) {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_scale(m1, r1, r1, 1);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
        r2 /= r1;
    }

    /* cp_csg3_cylinder_t: for now, generate a polyhedron */
    size_t fn = get_fn(c->opt, s->_fn, false);
    return csg3_poly_cylinder(r, c, m, s, mo, r2, fn);
}

static bool csg3_from_linext(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *mo,
    cp_scad_linext_t const *s)
{
    return true; /* FIXME: continue */
    if (c->context != IN3D) {
        return msg(c, c->opt->err_outside_3d, s->loc, NULL,
            "'linear_extrude' found outside 3D context.");
    }
    *no = true;

    if (cp_le(s->height, 0)) {
        return msg(c, c->opt->err_empty, s->loc, NULL,
           "Expected non-empty linear_extrude, but height="FF".\n",
            s->height);
    }
    if (s->slices < 1) {
        return msg(c, c->opt->err_empty, s->loc, NULL,
            "Expected non-empty linear_extrude, but slices=%"_Pz"u.\n",
            s->slices);
    }

    cp_mat3wi_t const *m = mo->mat;

    /* normalise height */
    if (!cp_eq(s->height, 1)) {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_scale(m1, 1, 1, s->height);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* normalise center */
    if (!s->center) {
        cp_mat3wi_t *m1 = mat_new(c->tree);
        cp_mat3wi_xlat(m1, 0, 0, +.5);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* construct a separate tree for the children */
    ctxt_t c2 = *c;
    c2.context = IN2D;
    cp_v_obj_p_t rc = {0};
    if (!csg3_from_v_scad(no, &rc, &c2, mo, &s->child)) {
        return false;
    }

    /* FIXME: continue */
    (void)r;

    return true;
}

static void object(
    bool *no)
{
    *no = true;
}

static bool csg3_from_scad(
    bool *no,
    cp_v_obj_p_t *r,
    ctxt_t *c,
    mat_ctxt_t const *m,
    cp_scad_t const *s)
{
    assert(c != NULL);
    assert(r != NULL);
    assert(c->tree != NULL);
    assert(c->err != NULL);
    assert(m != NULL);
    assert(s != NULL);

    mat_ctxt_t mn;
    if (s->modifier != 0) {
        /* ignore sub-structure? */
        if (s->modifier & CP_GC_MOD_IGNORE) {
            return true;
        }

        mn = *m;
        mn.gc.modifier |= s->modifier;
        m = &mn;
    }

    switch (s->type) {
    /* operators */
    case CP_SCAD_UNION:
        return csg3_from_union(no, r, c, m, cp_scad_cast(cp_scad_union_t, s));

    case CP_SCAD_DIFFERENCE:
        return csg3_from_difference(no, r, c, m, cp_scad_cast(cp_scad_difference_t, s));

    case CP_SCAD_INTERSECTION:
        return csg3_from_intersection(no, r, c, m, cp_scad_cast(cp_scad_intersection_t, s));

    /* transformations */
    case CP_SCAD_TRANSLATE:
        return csg3_from_translate(no, r, c, m, cp_scad_cast(cp_scad_translate_t, s));

    case CP_SCAD_MIRROR:
        return csg3_from_mirror(no, r, c, m, cp_scad_cast(cp_scad_mirror_t, s));

    case CP_SCAD_SCALE:
        return csg3_from_scale(no, r, c, m, cp_scad_cast(cp_scad_scale_t, s));

    case CP_SCAD_ROTATE:
        return csg3_from_rotate(no, r, c, m, cp_scad_cast(cp_scad_rotate_t, s));

    case CP_SCAD_MULTMATRIX:
        return csg3_from_multmatrix(no, r, c, m, cp_scad_cast(cp_scad_multmatrix_t, s));

    /* graphics context manipulations */
    case CP_SCAD_COLOR:
        return csg3_from_color(no, r, c, m, cp_scad_cast(cp_scad_color_t, s));

    /* 2D->3D extruding */
    case CP_SCAD_LINEXT:
        return csg3_from_linext(no, r, c, m, cp_scad_cast(cp_scad_linext_t, s));

    /* 3D objects */
    case CP_SCAD_SPHERE:
        return csg3_from_sphere(no, r, c, m, cp_scad_cast(cp_scad_sphere_t, s));

    case CP_SCAD_CUBE:
        return csg3_from_cube(no, r, c, m, cp_scad_cast(cp_scad_cube_t, s));

    case CP_SCAD_CYLINDER:
        object(no);
        return csg3_from_cylinder(no, r, c, m, cp_scad_cast(cp_scad_cylinder_t, s));

    case CP_SCAD_POLYHEDRON:
        object(no);
        return csg3_from_polyhedron(no, r, c, m, cp_scad_cast(cp_scad_polyhedron_t, s));

    /* 2D objects */
    case CP_SCAD_CIRCLE:
        CP_NYI("circle");
        *no = true;
        return true;
#if 0
        /* FIXME: continue */
        return csg3_from_circle(r, c, m, cp_scad_cast(cp_scad_circle_t, s));
#endif

    case CP_SCAD_SQUARE:
        return csg3_from_square(no, r, c, m, cp_scad_cast(cp_scad_square_t, s));

    case CP_SCAD_POLYGON:
        return csg3_from_polygon(no, r, c, m, cp_scad_cast(cp_scad_polygon_t, s));
    }

    CP_DIE("SCAD object type");
}

static void csg3_init_tree(
    cp_csg3_tree_t *t,
    cp_loc_t loc)
{
    if (t->root == NULL) {
        t->root = cp_csg_new(*t->root, loc);
    }
}

static bool cp_csg3_from_scad(
    ctxt_t *c,
    cp_scad_t const *s)
{
    assert(c != NULL);
    assert(c->tree != NULL);
    assert(c->err != NULL);
    assert(s != NULL);

    csg3_init_tree(c->tree, s->loc);

    bool no = false;

    mat_ctxt_t m;
    mat_ctxt_init(&m, c->tree);

    if (!csg3_from_scad(&no, &c->tree->root->add, c, &m, s)) {
        return false;
    }
    return true;
}

static bool cp_csg3_from_v_scad(
    ctxt_t *c,
    cp_v_scad_p_t const *ss)
{
    assert(c != NULL);
    assert(c->tree != NULL);
    assert(c->err != NULL);
    assert(ss != NULL);
    if (ss->size == 0) {
        return true;
    }

    csg3_init_tree(c->tree, cp_v_nth(ss,0)->loc);

    bool no = false;

    mat_ctxt_t m;
    mat_ctxt_init(&m, c->tree);

    if (!csg3_from_v_scad(&no, &c->tree->root->add, c, &m, ss)) {
        return false;
    }

    return true;
}

static void get_bb_csg3(
    cp_vec3_minmax_t *bb,
    cp_csg3_t const *r,
    bool max);

static void get_bb_v_csg3(
    cp_vec3_minmax_t *bb,
    cp_v_obj_p_t const *r,
    bool max)
{
    for (cp_v_each(i, r)) {
        get_bb_csg3(bb, cp_csg3_cast(cp_csg3_t, cp_v_nth(r, i)), max);
    }
}

static void get_bb_add(
    cp_vec3_minmax_t *bb,
    cp_csg_add_t const *r,
    bool max)
{
    get_bb_v_csg3(bb, &r->add, max);
}

static void get_bb_sub(
    cp_vec3_minmax_t *bb,
    cp_csg_sub_t const *r,
    bool max)
{
    get_bb_add(bb, r->add, max);
    if (max) {
        get_bb_add(bb, r->sub, max);
    }
}

static void get_bb_cut(
    cp_vec3_minmax_t *bb,
    cp_csg_cut_t const *r,
    bool max)
{
    if (r->cut.size == 0) {
        return;
    }

    if (max) {
        for (cp_v_each(i, &r->cut)) {
            get_bb_add(bb, cp_v_nth(&r->cut, i), max);
        }
    }
    else {
        cp_vec3_minmax_t bb2 = CP_VEC3_MINMAX_FULL;
        for (cp_v_each(i, &r->cut, 1)) {
            cp_vec3_minmax_t bb3 = CP_VEC3_MINMAX_EMPTY;
            get_bb_add(&bb3, cp_v_nth(&r->cut,i), max);
            cp_vec3_minmax_and(&bb2, &bb2, &bb3);
            if (!cp_vec3_minmax_valid(&bb2)) {
                break;
            }
        }
        cp_vec3_minmax_or(bb, bb, &bb2);
    }
}

static void get_bb_poly(
    cp_vec3_minmax_t *bb,
    cp_csg3_poly_t const *r)
{
    if ((r->point.size == 0) || (r->face.size < 4)) {
        return;
    }
    for (cp_v_each(i, &r->point)) {
        cp_vec3_minmax(bb, &cp_v_nth(&r->point, i).coord);
    }
}

static void get_bb_sphere(
    cp_vec3_minmax_t *bb,
    cp_csg3_sphere_t const *r)
{
    csg3_sphere_minmax(bb, r->mat);
}

static void get_bb_csg3(
    cp_vec3_minmax_t *bb,
    cp_csg3_t const *r,
    bool max)
{
    switch (r->type) {
    case CP_CSG3_ADD:
        get_bb_add(bb, cp_csg_cast(cp_csg_add_t, r), max);
        return;

    case CP_CSG3_SUB:
        get_bb_sub(bb, cp_csg_cast(cp_csg_sub_t, r), max);
        return;

    case CP_CSG3_CUT:
        get_bb_cut(bb, cp_csg_cast(cp_csg_cut_t, r), max);
        return;

    case CP_CSG3_SPHERE:
        get_bb_sphere(bb, cp_csg3_cast(cp_csg3_sphere_t, r));
        return;

    case CP_CSG3_CYL:
        /* return get_bb_cyl(bb, &r->cyl); FIXME: continue */
        return;

    case CP_CSG3_POLY:
        get_bb_poly(bb, cp_csg3_cast(cp_csg3_poly_t, r));
        return;
    }
    CP_NYI();
}

/* ********************************************************************** */

/**
 * Get bounding box of all points, including those that are
 * in subtracted parts that will be outside of the final solid.
 *
 * If max is non-false, the bb will include structures that are
 * subtracted.
 *
 * bb will not be cleared, but only updated.
 *
 * Returns whether there the structure is non-empty, i.e.,
 * whether bb has been updated.
 */
extern void cp_csg3_tree_bb(
    cp_vec3_minmax_t *bb,
    cp_csg3_tree_t const *r,
    bool max)
{
    if (r->root == NULL) {
        return;
    }

    get_bb_add(bb, r->root, max);
}

/**
 * Convert a SCAD AST into a CSG3 tree.
 */
extern bool cp_csg3_from_scad_tree(
    cp_csg3_tree_t *r,
    cp_err_t *t,
    cp_scad_tree_t const *scad)
{
    assert(scad != NULL);
    assert(r != NULL);
    assert(t != NULL);
    assert(r->opt != NULL);
    ctxt_t c = {
        .tree = r,
        .opt = r->opt,
        .err = t,
        .context = IN3D,
    };
    if (scad->root != NULL) {
        return cp_csg3_from_scad(&c, scad->root);
    }
    return cp_csg3_from_v_scad(&c, &scad->toplevel);
}
