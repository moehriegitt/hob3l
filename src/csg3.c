/* -*- Mode: C -*- */

#include <cpmat/mat.h>
#include <cpmat/alloc.h>
#include <csg2plane/gc.h>
#include <csg2plane/csg3.h>
#include "internal.h"

typedef struct {
    cp_mat3wi_t const *mat;
    cp_gc_t gc;
} mat_ctxt_t;

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

static void csg3_init(
    cp_csg3_t *r,
    cp_csg3_type_t type,
    cp_loc_t loc)
{
    r->type = type;
    r->loc = loc;
}

#define csg3_new(t, l) __csg3_new(CP_FILE, CP_LINE, t, l)

static cp_csg3_t *__csg3_new(
    char const *file,
    int line,
    cp_csg3_type_t type,
    cp_loc_t loc)
{
    static const size_t size[] = {
        [CP_CSG3_SPHERE]   = sizeof(cp_csg3_sphere_t),
        [CP_CSG3_CYL]      = sizeof(cp_csg3_cyl_t),
        [CP_CSG3_POLY]     = sizeof(cp_csg3_poly_t),
        [CP_CSG3_ADD]      = sizeof(cp_csg3_add_t),
        [CP_CSG3_SUB]      = sizeof(cp_csg3_sub_t),
        [CP_CSG3_CUT]      = sizeof(cp_csg3_cut_t),
        [CP_CSG2_CIRCLE]   = sizeof(cp_csg2_circle_t),
        [CP_CSG2_POLY]     = sizeof(cp_csg2_poly_t),
    };
    assert(type < cp_countof(size));
    assert(size[type] != 0);
    cp_csg3_t *r = cp_calloc(file, line, 1, size[type]);
    csg3_init(r, type, loc);
    return r;
}

static cp_csg3_t *csg3_new_push(
    cp_v_csg3_p_t *arr,
    cp_csg3_type_t type,
    cp_loc_t loc)
{
    cp_csg3_t *o = csg3_new(type, loc);
    cp_v_push(arr, o);
    return o;
}

#define csg3_new_push(a, t, l) \
    ({ \
        cp_v_csg3_p_t *__arr = (a); \
        assert(__arr != NULL); \
        csg3_new_push(__arr, t, l); \
    })

static cp_mat3wi_t const *the_unit(
    cp_csg3_tree_t *result)
{
    if (result->mat.size == 0) {
        return mat_new(result);
    }
    return cp_v_nth(&result->mat, 0);
}

static void csg3_add_minmax(
    cp_csg3_add_t *r)
{
    r->non_empty = false;
    for (cp_v_each(i, &r->add)) {
        cp_csg3_t *c = cp_v_nth(&r->add, i);
        if (c->non_empty) {
            if (r->non_empty) {
                cp_vec3_min(&r->bb.min, &r->bb.min, &c->bb.min);
                cp_vec3_max(&r->bb.max, &r->bb.max, &c->bb.max);
            }
            else {
                r->bb = c->bb;
                r->non_empty = true;
            }
        }
    }
}

static void csg3_poly_minmax(
    cp_csg3_poly_t *r)
{
    r->non_empty = (r->face.size >= 4);
    if (r->non_empty) {
        r->bb.min = r->bb.max = cp_v_nth(&r->point, 0).coord;
        for (cp_v_each(i, &r->point, 1)) {
            cp_vec3_minmax(&r->bb, &cp_v_nth(&r->point, i).coord);
        }
    }
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
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *m,
    cp_scad_t *s);

static bool csg3_from_v_scad(
    bool *no,
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *m,
    cp_v_scad_p_t *ss)
{
    for (cp_v_each(i, ss)) {
        if (!csg3_from_scad(no, r, t, e, m, cp_v_nth(ss, i))) {
            return false;
        }
    }
    return true;
}

static bool csg3_from_union(
    bool *no,
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *m,
    cp_scad_combine_t *s)
{
    return csg3_from_v_scad(no, r, t, e, m, &s->child);
}

/**
 * Returns false if the result becomes empty
 */
static bool try_subtract_cube(
    cp_vec3_minmax_t *bb,
    cp_csg3_t *_c)
{
    (void)bb;
    (void)_c;
#if 0
    /* FIXME: later, cut off cubes to improve prediction of
     * bounding box. */
    assert(_c != NULL);
    if (_c->type != CP_CSG3_POLY) {
        return;
    }
    cp_csg3_poly_t *c = &_c->poly;
    if (!c->is_cube) {
        return;
    }

    bool allx = (bb->min.x <= c->bb.min.x) && (bb->max.x >= c->bb.max.x);
    bool ally = (bb->min.y <= c->bb.min.y) && (bb->max.y >= c->bb.max.y);
    bool allz = (bb->min.z <= c->bb.min.z) && (bb->max.z >= c->bb.max.z);

    if (allx && ally) {
        if (allz) {
            return false;
        }
    }
#endif /*0*/
    return true;
}

static bool csg3_from_difference(
    bool *no,
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *m,
    cp_scad_combine_t *s)
{
    cp_v_csg3_p_t f = CP_V_INIT;

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
        if (!csg3_from_scad(&add_no, &f, t, e, m, cp_v_nth(&s->child, sub_i))) {
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
                no, &cp_v_nth(&f, 0)->sub.sub.add, t, e, m, cp_v_nth(&s->child, i)))
            {
                return false;
            }
        }

        /* This does not change the bounding box of the cp_v_nth(&f, 0), as only
         * more stuff was subtracted, which we neglect for bb computation. */

        cp_v_fini(&f);
        return true;
    }

    cp_v_csg3_p_t g = CP_V_INIT;

    /* all others children are negative */
    for (cp_v_each(i, &s->child, sub_i)) {
        if (!csg3_from_scad(no, &g, t, e, m, cp_v_nth(&s->child, i))) {
            return false;
        }
    }

    if (g.size == 0) {
        /* no more children => nothing to subtract => push f to output */
        cp_v_append(r, &f);
        cp_v_fini(&f);
        return true;
    }

    cp_csg3_t *_o = csg3_new_push(r, CP_CSG3_SUB, s->loc);
    cp_csg3_sub_t *o = &_o->sub;

    csg3_init((cp_csg3_t*)&o->add, CP_CSG3_ADD, s->loc);
    o->add.add = f;
    csg3_add_minmax(&o->add);

    csg3_init((cp_csg3_t*)&o->sub, CP_CSG3_ADD, s->loc);
    o->sub.add = g;

    /* o->sub only restricts the bounding box, but it is too complicated to
     * know how.  So we basically ignore it.  Since often, large cubes  are used to
     * cut off stuff, we do search for those to restrict bb. */
    o->bb = o->add.bb;
    o->non_empty = o->add.non_empty;

    for (cp_v_each(i, &o->sub.add)) {
        if (!try_subtract_cube(&o->bb, cp_v_nth(&o->sub.add, i))) {
            o->non_empty = false;
            break;
        }
    }

    return true;
}

static void csg3_cut_push_add(
    cp_v_csg3_add_p_t *cut,
    cp_v_csg3_p_t *add)
{
    if (add->size > 0) {
        cp_csg3_t *_a = csg3_new(CP_CSG3_ADD, cp_v_nth(add,0)->loc);
        cp_csg3_add_t *a = &_a->add;
        a->add = *add;
        csg3_add_minmax(a);

        cp_v_push(cut, a);

        CP_ZERO(add);
    }
}

static bool bb_valid(cp_vec3_minmax_t const *a)
{
    return
        (a->min.x < a->max.x) &&
        (a->min.y < a->max.y) &&
        (a->min.z < a->max.z);
}

static bool csg3_from_intersection(
    bool *no,
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *m,
    cp_scad_combine_t *s)
{
    cp_v_csg3_add_p_t cut = CP_V_INIT;

    /* each child is a union */
    cp_v_csg3_p_t add = CP_V_INIT;
    for (cp_v_each(i, &s->child)) {
        csg3_cut_push_add(&cut, &add);
        if (!csg3_from_scad(no, &add, t, e, m, cp_v_nth(&s->child, i))) {
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

    cp_csg3_t *_o = csg3_new_push(r, CP_CSG3_CUT, s->loc);
    cp_csg3_cut_t *o = &_o->cut;
    o->cut = cut;

    /* Intersecting the bounding boxes means using the largest minimum
     * and the smallest maximum, which may mean we may already know
     * here that something is empty.  But we will not discard anything
     * now. */
    o->non_empty = cp_v_nth(&cut, 0)->non_empty;
    o->bb = cp_v_nth(&cut, 0)->bb;
    for (cp_v_each(i, &cut, 1)) {
        cp_vec3_max(&o->bb.min, &o->bb.min, &cp_v_nth(&cut, i)->bb.min);
        cp_vec3_min(&o->bb.max, &o->bb.max, &cp_v_nth(&cut, i)->bb.max);
        if (!cp_v_nth(&cut, i)->non_empty) {
            o->non_empty = false;
        }
    }
    if (!bb_valid(&o->bb)) {
        o->non_empty = false;
    }

    return true;
}

static bool csg3_from_translate(
    bool *no,
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *mo,
    cp_scad_xyz_t *s)
{
    if (cp_vec3_has_len0(&s->v)) {
        /* Avoid math ops unless necessary: for 0 length xlat,
         * it is not necessary */
        return csg3_from_v_scad(no, r, t, e, mo, &s->child);
    }

    cp_mat3wi_t *m1 = mat_new(t);
    cp_mat3wi_xlat_v(m1, &s->v);
    cp_mat3wi_mul(m1, mo->mat, m1);

    mat_ctxt_t mn = *mo;
    mn.mat = m1;
    return csg3_from_v_scad(no, r, t, e, &mn, &s->child);
}

static bool csg3_from_mirror(
    bool *no,
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *mo,
    cp_scad_xyz_t *s)
{
    if (cp_vec3_has_len0(&s->v)) {
        cp_vchar_printf(&e->msg, "Mirror plane normal has length zero.\n");
        e->loc = s->loc;
        return false;
    }

    cp_mat3wi_t *m1 = mat_new(t);
    cp_mat3wi_mirror_v(m1, &s->v);
    cp_mat3wi_mul(m1, mo->mat, m1);

    mat_ctxt_t mn = *mo;
    mn.mat = m1;
    return csg3_from_v_scad(no, r, t, e, &mn, &s->child);
}

static bool good_scale(
    cp_vec3_t const *v)
{
    return
        !cp_equ(v->x, 0) &&
        !cp_equ(v->y, 0) &&
        !cp_equ(v->z, 0);
}

static bool good_scale2(
    cp_vec2_t const *v)
{
    return
        !cp_equ(v->x, 0) &&
        !cp_equ(v->y, 0);
}

static bool csg3_from_scale(
    bool *no,
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *mo,
    cp_scad_xyz_t *s)
{
    if (!good_scale(&s->v)) {
        cp_vchar_printf(&e->msg, "Scale is zero.\n");
        e->loc = s->loc;
        return false;
    }
    cp_mat3wi_t *m1 = mat_new(t);
    cp_mat3wi_scale_v(m1, &s->v);
    cp_mat3wi_mul(m1, mo->mat, m1);

    mat_ctxt_t mn = *mo;
    mn.mat = m1;
    return csg3_from_v_scad(no, r, t, e, &mn, &s->child);
}

static bool csg3_from_multmatrix(
    bool *no,
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *mo,
    cp_scad_multmatrix_t *s)
{
    cp_mat3wi_t *m1 = mat_new(t);
    if (!cp_mat3wi_from_mat3w(m1, &s->m)) {
        cp_vchar_printf(&e->msg, "Non-invertible matrix.\n");
        e->loc = s->loc;
        return false;
    }
    cp_mat3wi_mul(m1, mo->mat, m1);

    mat_ctxt_t mn = *mo;
    mn.mat = m1;
    return csg3_from_v_scad(no, r, t, e, &mn, &s->child);
}

static bool csg3_from_rotate(
    bool *no,
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *mo,
    cp_scad_rotate_t *s)
{
    cp_mat3wi_t *m1 = mat_new(t);
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
    return csg3_from_v_scad(no, r, t, e, &mn, &s->child);
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

    /* Compute normal.  Spread the indices to get a more stable value for
     * fine structures. */
    size_t u = face->point.size / 3; /* >= 1 */
    size_t v = u * 2;                /* < face->point.size */
    bool normal_ok __unused = cp_vec3_right_normal3(&face->normal,
        &cp_v_nth(&face->point, 0).ref->coord,
        &cp_v_nth(&face->point, u).ref->coord,
        &cp_v_nth(&face->point, v).ref->coord);
    assert(normal_ok);

#ifndef NDEBUG
    for (cp_v_each(i, &face->point)) {
        size_t j = cp_wrap_sub1(i, face->point.size);
        size_t k = cp_wrap_sub1(j, face->point.size);
        cp_vec3_t n;
        normal_ok = cp_vec3_right_normal3(&n,
            &cp_v_nth(&face->point, k).ref->coord,
            &cp_v_nth(&face->point, j).ref->coord,
            &cp_v_nth(&face->point, i).ref->coord);
        assert(normal_ok);
        assert(cp_vec3_equ(&n, &face->normal));
    }
#endif
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
    cp_csg3_tree_t *t __unused,
    cp_err_t *e)
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
            cp_vchar_printf(&e->msg,
                "Identical edge occurs more than once in polyhedron.\n");
            e->loc = a->src->loc;
            e->loc2 = b->src->loc;
            return false;
        }
    }

    /* Step 3:
     * Assign edges for each polygon; find back edges; report errors */
    size_t max_idx = 0;
    for (cp_v_each(i, &r->face)) {
        cp_csg3_face_t *f = &cp_v_nth(&r->face, i);
        if (f->point.size != f->edge.size) {
            cp_vchar_printf(&e->msg,
                "Face edge array should be preallocated, but point.size=%"_Pz"u, edge.size=%"_Pz"u\n"
                " Internal Error.\n",
                f->point.size, f->edge.size);
            e->loc = f->loc;
            return false;
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
                cp_vchar_printf(&e->msg, "Edge has no adjacent reverse edge in polyhedron.\n");
                e->loc = cp_v_nth(&f->point, j1).loc;
                return false;
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
                    cp_vchar_printf(&e->msg, "Edge occurs multiple times in polyhedron.\n");
                    e->loc = k.src->loc;
                    e->loc2 = edge->src->loc;
                    return false;
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
                    cp_vchar_printf(&e->msg, "Edge occurs multiple times in polyhedron.\n");
                    e->loc = k.dst->loc;
                    e->loc2 = edge->dst->loc;
                    return false;
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
            cp_vchar_printf(&e->msg,
                "Edge has no adjacent reverse edge in polyhedron.\n");
            e->loc = b->src->loc;
            return false;
        }
    }
    if (max_idx >= point_cnt/2) {
        cp_csg3_edge_t const *b = &cp_v_nth(&r->edge, point_cnt/2);
        cp_vchar_printf(&e->msg,
            "Edge has no adjacent reverse edge in polyhedron.\n");
        e->loc = b->src->loc;
        return false;
    }
    r->edge.size = point_cnt/2;

    /* If we had no error by now, so something went wrong. */
    assert((point_cnt & 1) == 0);
    return true;
}

static bool csg3_from_sphere(
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *mo,
    cp_scad_sphere_t *s)
{
    if (cp_leq(s->r, 0)) {
        cp_vchar_printf(&e->msg, "Sphere scale is zero or negative.\n");
        e->loc = s->loc;
        return false;
    }
    cp_mat3wi_t const *m = mo->mat;
    if (!cp_equ(s->r, 1)) {
        cp_mat3wi_t *m1 = mat_new(t);
        cp_mat3wi_scale1(m1, s->r);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }
    cp_csg3_t *_o = csg3_new_push(r, CP_CSG3_SPHERE, s->loc);
    cp_csg3_sphere_t *o = cp_csg3_sphere(_o);
    o->non_empty = true;
    o->mat = m;
    o->gc = mo->gc;
    o->_fa = s->_fa;
    o->_fs = s->_fs;
    o->_fn = s->_fn;
    return true;
}

#if 0
/* FIXME: continue */
static bool csg3_from_circle(
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *mo,
    cp_scad_circle_t *s)
{
    if (cp_leq(s->r, 0)) {
        cp_vchar_printf(&e->msg, "Circle scale is zero or negative.\n");
        e->loc = s->loc;
        return false;
    }
    cp_mat3wi_t const *m = mo->mat;
    if (!cp_equ(s->r, 1)) {
        cp_mat3wi_t *m1 = mat_new(t);
        cp_mat3wi_scale1(m1, s->r);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }
    cp_csg3_t *_o = csg3_new_push(r, CP_CSG2_CIRCLE, s->loc);
    cp_csg2_circle_t *o = &_o->circle;
    o->mat.n = CP_MAT2W(
        m->n.b.m[0][0], m->n.b.m[0][1], m->n.w.v[0],
        m->n.b.m[1][0], m->n.b.m[1][1], m->n.w.v[1]);
    o->mat.i = CP_MAT2W(
        m->i.b.m[0][0], m->i.b.m[0][1], m->i.w.v[0],
        m->i.b.m[1][0], m->i.b.m[1][1], m->i.w.v[1]);
    o->gc = mo->gc;
    o->_fa = s->_fa;
    o->_fs = s->_fs;
    o->_fn = s->_fn;
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
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *m,
    cp_scad_polyhedron_t *s)
{
    if (s->points.size < 4) {
        cp_vchar_printf(&e->msg, "Polyhedron needs at least 4 points, but found only %"_Pz"u.\n",
            s->points.size);
        e->loc = s->loc;
        return false;
    }
    if (s->faces.size < 4) {
        cp_vchar_printf(&e->msg, "Polyhedron needs at least 4 faces, but found only %"_Pz"u.\n",
            s->faces.size);
        e->loc = s->loc;
        return false;
    }

    cp_csg3_t *_o = csg3_new_push(r, CP_CSG3_POLY, s->loc);
    cp_csg3_poly_t *o = &_o->poly;
    o->gc = m->gc;

    /* check that no point is duplicate: abuse the array we'll use in
     * the end, too, for temporarily sorting the points */
    /* copy points (same data type, just copy the array) */
    cp_v_init_with(&o->point, s->points.data, s->points.size);

    cp_v_qsort(&o->point, 0, CP_SIZE_MAX, cmp_vec3_loc, NULL);
    for (cp_v_each(i, &o->point, 1)) {
        cp_vec3_loc_t const *a = &cp_v_nth(&o->point, i-1);
        cp_vec3_loc_t const *b = &cp_v_nth(&o->point, i);
        if (cp_vec3_equ(&a->coord, &b->coord)) {
            cp_vchar_printf(&e->msg, "Duplicate point in polyhedron.\n");
            e->loc = a->loc;
            e->loc2 = b->loc;
            return false;
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

        /* only convex faces on polyhedron */
        /* compute normal.
         * FIXME: This only works for convex faces.  Use the cross product sum for
         * arbitrary polygons instead (we are not doing that here yet because its
         * naive implementation may be unstable due to large amounts of summands).
         */
        bool have_normal = false;
        for (cp_v_each(j, &cf->point, 2)) {
            if (cp_vec3_right_normal3(&cf->normal,
                &cp_v_nth(&cf->point, j-2).ref->coord,
                &cp_v_nth(&cf->point, j-1).ref->coord,
                &cp_v_nth(&cf->point, j).ref->coord))
            {
                have_normal = true;
                break;
            }
        }
        if (!have_normal) {
            cp_vchar_printf(&e->msg, "No normal can be computed at any vertex of face.\n");
            e->loc = cp_v_nth(&cf->point, 0).loc;
            return false;
        }

        cp_vec3_t normal2;
        for (cp_v_each(j, &cf->point)) {
            size_t k = cp_wrap_sub1(j, cf->point.size);
            size_t l = cp_wrap_sub1(k, cf->point.size);
            if (!cp_vec3_right_normal3(&normal2,
                &cp_v_nth(&cf->point, l).ref->coord,
                &cp_v_nth(&cf->point, k).ref->coord,
                &cp_v_nth(&cf->point, j).ref->coord))
            {
                continue;
            }

            if (!cp_vec3_equ(&cf->normal, &normal2)) {
                cp_vec3_neg(&normal2, &normal2);
                if (cp_vec3_equ(&cf->normal, &normal2)) {
                    /* FIXME: csg2-layer needs convex faces, but we could just make them
                     * convex (e.g. by triangulating them and re-assembling them). */
                    cp_vchar_printf(&e->msg,
                        "Not yet implemented: convex face expected, but found concave corner.\n");
                }
                else {
                    cp_vchar_printf(&e->msg,
                        "Face points are not inside a plane.  Normals are:\n"
                        " n1=("FD3") vs.\n"
                        " n2=("FD3")\n",
                        CP_V012(cf->normal),
                        CP_V012(normal2));
                    e->loc2 = cp_v_nth(&cf->point, 1).loc;
                }
                e->loc = cp_v_nth(&cf->point, k).loc;
                return false;
            }
        }
    }

    csg3_poly_minmax(o);
    return poly_make_edges(o, t, e);
}

static bool csg3_from_polygon(
    cp_v_csg3_p_t *r,
    cp_err_t *e,
    mat_ctxt_t const *m,
    cp_scad_polygon_t *s)
{
    if (s->points.size < 3) {
        cp_vchar_printf(&e->msg, "Polygons needs at least 3 points, but found only %"_Pz"u.\n",
            s->points.size);
        e->loc = s->loc;
        return false;
    }

    cp_csg3_t *_o = csg3_new_push(r, CP_CSG2_POLY, s->loc);
    cp_csg2_poly_t *o = &_o->poly2;

    /* check that no point is duplicate: abuse the array we'll use in
     * the end, too, for temporarily sorting the points */
    /* copy points (same data type, just copy the array) */
    cp_v_init_with(&o->point, s->points.data, s->points.size);

    cp_v_qsort(&o->point, 0, CP_SIZE_MAX, cmp_vec2_loc, NULL);
    for (cp_v_each(i, &o->point, 1)) {
        cp_vec2_loc_t const *a = &cp_v_nth(&o->point, i-1);
        cp_vec2_loc_t const *b = &cp_v_nth(&o->point, i);
        if (cp_vec2_equ(&a->coord, &b->coord)) {
            cp_vchar_printf(&e->msg, "Duplicate point in polygon.\n");
            e->loc = a->loc;
            e->loc2 = b->loc;
            return false;
        }
    }

    /* copy points (same data type, just copy the array) */
    cp_v_init_with(&o->point, s->points.data, s->points.size);

    /* in-place xform */
    for (cp_v_each(i, &o->point)) {
        cp_vec3_t v = { .z = 0 };
        v.b = cp_v_nth(&o->point, i).coord;
        cp_vec3w_xform(&v, &m->mat->n, &v);
        cp_v_nth(&o->point, i).coord = v.b;
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
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *mo,
    cp_scad_cube_t *s)
{
    /* size 0 in any direction is an error */
    if (!good_scale(&s->size)) {
        cp_vchar_printf(&e->msg, "Cube scale is zero.\n");
        e->loc = s->loc;
        return false;
    }

    cp_mat3wi_t const *m = mo->mat;

    /* possibly scale */
    if (!cp_equ(s->size.x, 1) ||
        !cp_equ(s->size.y, 1) ||
        !cp_equ(s->size.z, 1))
    {
        cp_mat3wi_t *m1 = mat_new(t);
        cp_mat3wi_scale_v(m1, &s->size);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* possibly translate to center */
    if (s->center) {
        cp_mat3wi_t *m1 = mat_new(t);
        cp_mat3wi_xlat(m1, -.5, -.5, -.5);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* make cube shape */
    cp_csg3_t *_o = csg3_new_push(r, CP_CSG3_POLY, s->loc);
    cp_csg3_poly_t *o = &_o->poly;
    o->gc = mo->gc;

    o->is_cube = cp_mat3_is_rect_rot(&m->n.b);

    // /=y (topright is pos)        1----0     0 is at (1,1,1)
    // -=x (right is pos)  d       /|   /|
    // |=z (top is pos)           3----2 |
    //                            | 5--|-4
    //                            |/   |/
    //                            7----6       7 is at (0,0,0)
    cp_v_init0(&o->point, 8);
    for (cp_size_each(i, 8)) {
        cp_v_nth(&o->point, i) = (cp_vec3_loc_t){
            .coord = { .v={ !(i&1), !(i&2), !(i&4) }},
            .loc = s->loc,
        };
    }

    /* in-place xform */
    for (cp_v_each(i, &o->point)) {
        cp_vec3w_xform(&cp_v_nth(&o->point, i).coord, &m->n, &cp_v_nth(&o->point, i).coord);
    }

    /* make faces; depending on determinant, have the face forward or backward */
    static size_t const face[6][4] = {
        { 0, 4, 6, 2 },
        { 2, 6, 7, 3 },
        { 3, 7, 5, 1 },
        { 1, 5, 4, 0 },
        { 0, 2, 3, 1 },
        { 4, 5, 7, 6 },
    };
    cp_static_assert(cp_countof(face) == 6);
    cp_static_assert(cp_countof(face[0]) == 4);
    cp_v_init0(&o->face, cp_countof(face));
    for (cp_arr_each(i, face)) {
        face_init_from_point_ref(
            &cp_v_nth(&o->face, i), o, face[i], cp_countof(face[i]), (m->d < 0), s->loc);
    }
    assert(o->face.size == 6);
    assert(cp_v_nth(&o->face, 0).point.size == 4);

    /* compute bounding box */
    csg3_poly_minmax(o);

    /* make edges */
    if (!poly_make_edges(o, t, e)) {
        cp_vchar_printf(&e->msg,
            " Internal Error: Cube polyhedron construction algorithm is broken.\n");
        return false;
    }
    return true;
}

static bool csg3_from_square(
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *mo,
    cp_scad_square_t *s)
{
    /* size 0 in any direction is an error */
    if (!good_scale2(&s->size)) {
        cp_vchar_printf(&e->msg, "Square scale is zero.\n");
        e->loc = s->loc;
        return false;
    }

    cp_mat3wi_t const *m = mo->mat;

    /* possibly scale */
    if (!cp_equ(s->size.x, 1) ||
        !cp_equ(s->size.y, 1))
    {
        cp_mat3wi_t *m1 = mat_new(t);
        cp_mat3wi_scale(m1, s->size.x, s->size.y, 1);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* possibly translate to center */
    if (s->center) {
        cp_mat3wi_t *m1 = mat_new(t);
        cp_mat3wi_xlat(m1, -.5, -.5, 0);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* make cube shape */
    cp_csg3_t *_o = csg3_new_push(r, CP_CSG2_POLY, s->loc);
    cp_csg2_poly_t *o = &_o->poly2;

    cp_csg2_path_t *path = cp_v_push0(&o->path);
    for (cp_size_each(i, 4)) {
        cp_vec2_loc_t *p = cp_v_push0(&o->point);
        p->coord.x = cp_dim(i & 1);
        p->coord.y = cp_dim(i & 2);
        p->loc = s->loc;
    }

    cp_v_push(&path->point_idx, 0);
    cp_v_push(&path->point_idx, 1);
    cp_v_push(&path->point_idx, 3);
    cp_v_push(&path->point_idx, 2);

    return true;
}

static void csg3_poly_make_cone(
    cp_csg3_poly_t *o,
    cp_mat3wi_t const *m,
    cp_scad_cylinder_t *s,
    size_t fn)
{
    /* generate the points */
    cp_v_init0(&o->point, fn + 1);
    for (cp_size_each(i, fn)) {
        cp_angle_t a = cp_angle(i) * (CP_TAU / cp_angle(fn));
        cp_scale_t ss = sin(a);
        cp_scale_t cc = cos(a);
        cp_v_nth(&o->point, i) = (cp_vec3_loc_t){
            .coord = { .v={ cc, ss, -.5 }},
            .loc = s->loc,
        };
    }
    cp_v_nth(&o->point, fn) = (cp_vec3_loc_t){
        .coord = { .v={ 0, 0, +.5 }},
        .loc = s->loc,
    };

    /* in-place xform */
    for (cp_v_each(i, &o->point)) {
        cp_vec3w_xform(&cp_v_nth(&o->point, i).coord, &m->n, &cp_v_nth(&o->point, i).coord);
    }

    /* generate the surrounding faces */
    bool rev = (m->d < 0);
    cp_v_init0(&o->face, fn + 1);
    for (cp_size_each(i, fn)) {
        size_t j = cp_wrap_add1(i, fn);
        face_init_from_point_ref(
            &cp_v_nth(&o->face, i), o, (size_t[4]){ j, i, fn }, 3, rev, s->loc);
    }

    /* bottom face */
    cp_csg3_face_t *f = &cp_v_nth(&o->face, fn);
    cp_v_init0(&f->point, fn);
    for (cp_v_each(i, &f->point)) {
        cp_v_nth(&f->point, i).ref = &cp_v_nth(&o->point, i);
        cp_v_nth(&f->point, i).loc = s->loc;
    }
    face_basics(f, rev, s->loc);
}

static void csg3_poly_make_cylinder(
    cp_csg3_poly_t *o,
    cp_mat3wi_t const *m,
    cp_scad_cylinder_t *s,
    cp_scale_t r2,
    size_t fn)
{
    /* generate the points */
    cp_v_init0(&o->point, 2*fn);
    for (cp_size_each(i, fn)) {
        cp_angle_t a = cp_angle(i) * (CP_TAU / cp_angle(fn));
        cp_scale_t ss = sin(a);
        cp_scale_t cc = cos(a);
        cp_v_nth(&o->point, i) = (cp_vec3_loc_t){
            .coord = { .v={ cc, ss, -.5 }},
            .loc = s->loc,
        };
        cp_v_nth(&o->point, i + fn) = (cp_vec3_loc_t){
            .coord = { .v={ r2*cc, r2*ss, +.5 }},
            .loc = s->loc,
        };
    }

    /* in-place xform */
    for (cp_v_each(i, &o->point)) {
        cp_vec3w_xform(&cp_v_nth(&o->point, i).coord, &m->n, &cp_v_nth(&o->point, i).coord);
    }

    /* generate the surrounding faces */
    bool rev = (m->d < 0);
    cp_v_init0(&o->face, fn + 2);
    for (cp_size_each(i, fn)) {
        size_t j = cp_wrap_add1(i, fn);
        face_init_from_point_ref(
            &cp_v_nth(&o->face, i), o, (size_t[4]){ i+fn, j+fn, j, i }, 4, rev, s->loc);
    }

    /* bottom face */
    cp_csg3_face_t *f = &cp_v_nth(&o->face, fn);
    cp_v_init0(&f->point, fn);
    for (cp_v_each(i, &f->point)) {
        cp_v_nth(&f->point, i).ref = &cp_v_nth(&o->point, i);
        cp_v_nth(&f->point, i).loc = s->loc;
    }
    face_basics(f, rev, s->loc);

    /* top face */
    f = &cp_v_nth(&o->face, fn + 1);
    cp_v_init0(&f->point, fn);
    for (cp_v_each(i, &f->point)) {
        cp_v_nth(&f->point, i).ref = &cp_v_nth(&o->point, fn + (fn - i - 1));
        cp_v_nth(&f->point, i).loc = s->loc;
    }
    face_basics(f, rev, s->loc);
}

static bool csg3_poly_cylinder(
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    cp_mat3wi_t const *m,
    cp_scad_cylinder_t *s,
    mat_ctxt_t const *mo,
    cp_scale_t r2,
    size_t fn)
{
    cp_csg3_t *_o = csg3_new_push(r, CP_CSG3_POLY, s->loc);
    cp_csg3_poly_t *o = &_o->poly;
    o->gc = mo->gc;

    if (cp_equ(r2, 0)) {
        csg3_poly_make_cone(o, m, s, fn);
    }
    else {
        csg3_poly_make_cylinder(o, m, s, r2, fn);
    }

    /* bouding box */
    csg3_poly_minmax(o);

    /* make edges */
    if (!poly_make_edges(o, t, e)) {
        cp_vchar_printf(&e->msg,
            " Internal Error: Cylinder polyhedron construction algorithm is broken.\n");
        return false;
    }
    return true;
}

static bool csg3_from_cylinder(
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *mo,
    cp_scad_cylinder_t *s)
{
    cp_scale_t r1 = s->r1;
    cp_scale_t r2 = s->r2;

    if (cp_leq(s->h, 0)) {
        cp_vchar_printf(&e->msg, "Cylinder length is zero or negative.\n");
        e->loc = s->loc;
        return false;
    }
    if (cp_leq(r1, 0) && cp_leq(r2, 0)) {
        cp_vchar_printf(&e->msg, "Cylinder scale is zero or negative.\n");
        e->loc = s->loc;
        return false;
    }

    cp_mat3wi_t const *m = mo->mat;

    /* normalise height */
    if (!cp_equ(s->h, 1)) {
        cp_mat3wi_t *m1 = mat_new(t);
        cp_mat3wi_scale(m1, 1, 1, s->h);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* normalise center */
    if (!s->center) {
        cp_mat3wi_t *m1 = mat_new(t);
        cp_mat3wi_xlat(m1, 0, 0, +.5);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
    }

    /* normalise radii */
    if (r1 < r2) {
        /* want smaller diameter (especially 0) on top */
        cp_mat3wi_t *m1 = mat_new(t);
        cp_mat3wi_scale(m1, 1, 1, -1);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
        CP_SWAP(&r1, &r2);
    }

    if (!cp_equ(r1, 1)) {
        cp_mat3wi_t *m1 = mat_new(t);
        cp_mat3wi_scale(m1, r1, r1, 1);
        cp_mat3wi_mul(m1, m, m1);
        m = m1;
        r2 /= r1;
    }

    /* possibly generate a polyhedron */
    if ((s->_fn >= 3) && (s->_fn <= t->opt.max_fn)) {
        return csg3_poly_cylinder(r, t, e, m, s, mo, r2, s->_fn);
    }

    /* create a real cylinder */
    cp_csg3_t *_o = csg3_new_push(r, CP_CSG3_CYL, s->loc);
    cp_csg3_cyl_t *o = &_o->cyl;
    o->non_empty = true;
    o->mat = m;
    o->gc = mo->gc;
    o->r2 = r2;
    o->_fa = s->_fa;
    o->_fs = s->_fs;
    o->_fn = s->_fn;
    return true;
}

static void object(
    bool *no)
{
    *no = true;
}

static bool csg3_from_scad(
    bool *no,
    cp_v_csg3_p_t *r,
    cp_csg3_tree_t *t,
    cp_err_t *e,
    mat_ctxt_t const *m,
    cp_scad_t *s)
{
    assert(r != NULL);
    assert(t != NULL);
    assert(e != NULL);
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
        return csg3_from_union(no, r, t, e, m, &s->combine);

    case CP_SCAD_DIFFERENCE:
        return csg3_from_difference(no, r, t, e, m, &s->combine);

    case CP_SCAD_INTERSECTION:
        return csg3_from_intersection(no, r, t, e, m, &s->combine);

    /* transformations */
    case CP_SCAD_TRANSLATE:
        return csg3_from_translate(no, r, t, e, m, &s->xyz);

    case CP_SCAD_MIRROR:
        return csg3_from_mirror(no, r, t, e, m, &s->xyz);

    case CP_SCAD_SCALE:
        return csg3_from_scale(no, r, t, e, m, &s->xyz);

    case CP_SCAD_ROTATE:
        return csg3_from_rotate(no, r, t, e, m, &s->rotate);

    case CP_SCAD_MULTMATRIX:
        return csg3_from_multmatrix(no, r, t, e, m, &s->multmatrix);

    /* 3D objects */
    case CP_SCAD_SPHERE:
        object(no);
        return csg3_from_sphere(r, t, e, m, &s->sphere);

    case CP_SCAD_CUBE:
        object(no);
        return csg3_from_cube(r, t, e, m, &s->cube);

    case CP_SCAD_CYLINDER:
        object(no);
        return csg3_from_cylinder(r, t, e, m, &s->cylinder);

    case CP_SCAD_POLYHEDRON:
        object(no);
        return csg3_from_polyhedron(r, t, e, m, &s->polyhedron);

    /* 2D objects */
    case CP_SCAD_CIRCLE:
        object(no);
        return true;
#if 0
        /* FIXME: continue */
        return csg3_from_circle(r, t, e, m, &s->circle);
#endif

    case CP_SCAD_SQUARE:
        object(no);
        return csg3_from_square(r, t, e, m, &s->square);

    case CP_SCAD_POLYGON:
        object(no);
        return csg3_from_polygon(r, e, m, &s->polygon);
    }

    CP_DIE("SCAD object type");
}

static void csg3_init_tree(
    cp_csg3_tree_t *t,
    cp_loc_t loc)
{
    if (t->root == NULL) {
        cp_csg3_t *_o = csg3_new(CP_CSG3_ADD, loc);
        t->root = &_o->add;
    }
}

static void csg3_fini_tree(
    cp_csg3_tree_t *t)
{
    csg3_add_minmax(t->root);
}

extern bool cp_csg3_from_scad(
    cp_csg3_tree_t *t,
    cp_err_t *e,
    cp_scad_t *s)
{
    assert(t != NULL);
    assert(e != NULL);
    assert(s != NULL);

    csg3_init_tree(t, s->loc);

    bool no = false;

    mat_ctxt_t m;
    CP_ZERO(&m);
    m.mat = the_unit(t);

    if (!csg3_from_scad(&no, &t->root->add, t, e, &m, s)) {
        return false;
    }
    csg3_fini_tree(t);
    return true;
}

extern bool cp_csg3_from_v_scad(
    cp_csg3_tree_t *t,
    cp_err_t *e,
    cp_v_scad_p_t *ss)
{
    assert(t != NULL);
    assert(e != NULL);
    assert(ss != NULL);
    if (ss->size == 0) {
        return true;
    }

    csg3_init_tree(t, cp_v_nth(ss,0)->loc);

    bool no = false;

    mat_ctxt_t m;
    CP_ZERO(&m);
    m.mat = the_unit(t);

    if (!csg3_from_v_scad(&no, &t->root->add, t, e, &m, ss)) {
        return false;
    }

    csg3_fini_tree(t);
    return true;
}

extern bool cp_csg3_from_scad_tree(
    cp_csg3_tree_t *r,
    cp_err_t *t,
    cp_scad_tree_t *scad)
{
    assert(scad != NULL);
    if (scad->root != NULL) {
        return cp_csg3_from_scad(r, t, scad->root);
    }
    return cp_csg3_from_v_scad(r, t, &scad->toplevel);
}

static void get_max_bb_csg3(
    cp_vec3_minmax_t *bb,
    cp_csg3_t const *r);

static void get_max_bb_v_csg3(
    cp_vec3_minmax_t *bb,
    cp_v_csg3_p_t const *r)
{
    for (cp_v_each(i, r)) {
        get_max_bb_csg3(bb, cp_v_nth(r, i));
    }
}

static void get_max_bb_add(
    cp_vec3_minmax_t *bb,
    cp_csg3_add_t const *r)
{
    get_max_bb_v_csg3(bb, &r->add);
}

static void get_max_bb_sub(
    cp_vec3_minmax_t *bb,
    cp_csg3_sub_t const *r)
{
    get_max_bb_add(bb, &r->add);
    get_max_bb_add(bb, &r->sub);
}

static void get_max_bb_cut(
    cp_vec3_minmax_t *bb,
    cp_csg3_cut_t const *r)
{
    for (cp_v_each(i, &r->cut)) {
        get_max_bb_add(bb, cp_v_nth(&r->cut, i));
    }
}

static void get_max_bb_poly(
    cp_vec3_minmax_t *bb,
    cp_csg3_poly_t const *r)
{
    for (cp_v_each(i, &r->point)) {
        cp_vec3_minmax(bb, &cp_v_nth(&r->point, i).coord);
    }
}

static void get_max_bb_csg3(
    cp_vec3_minmax_t *bb,
    cp_csg3_t const *r)
{
    switch (r->type) {
    case CP_CSG3_ADD:
        get_max_bb_add(bb, cp_csg3_add_const(r));
        return;

    case CP_CSG3_SUB:
        get_max_bb_sub(bb, cp_csg3_sub_const(r));
        return;

    case CP_CSG3_CUT:
        get_max_bb_cut(bb, cp_csg3_cut_const(r));
        return;

    case CP_CSG3_SPHERE:
        /* get_max_bb_sphere(bb, &r->sphere); FIXME: continue */
        return;

    case CP_CSG3_CYL:
        /* get_max_bb_cyl(bb, &r->cyl); FIXME: continue */
        return;

    case CP_CSG3_POLY:
        get_max_bb_poly(bb, cp_csg3_poly_const(r));
        return;
    }
}

extern void cp_csg3_tree_max_bb(
    cp_vec3_minmax_t *bb,
    cp_csg3_tree_t const *r)
{
    if (r->root) {
        get_max_bb_add(bb, r->root);
    }
}
