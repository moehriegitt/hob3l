/* -*- Mode: C -*- */

#define DEBUG 0

#include <cpmat/arith.h>
#include <cpmat/vec.h>
#include <cpmat/mat.h>
#include <cpmat/alloc.h>
#include <csg2plane/gc.h>
#include <csg2plane/csg2.h>
#include <csg2plane/csg3.h>
#include "internal.h"

static void csg2_point_from_csg3_edge(
    cp_vec2_loc_t *p,
    double z,
    cp_csg3_edge_t const *e)
{
    cp_vec3_t const *src = &e->src->ref->coord;
    cp_vec3_t const *dst = &e->dst->ref->coord;

    assert(!cp_equ(dst->z, src->z));
    double t01 = cp_t01(src->z, z, dst->z);
    assert(t01 >= 0);
    assert(t01 <= 1);
    p->loc = e->src->loc;
    p->coord.x = cp_lerp(src->x, dst->x, t01);
    p->coord.y = cp_lerp(src->y, dst->y, t01);
}

static inline bool edge_is_back(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    return f == e->back;
}

static inline cp_csg3_face_t const *edge_get_face(
    cp_csg3_edge_t const *e,
    bool back)
{
    return back ? e->back : e->fore;
}

static inline cp_csg3_face_t const *edge_get_buddy_face(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    return edge_get_face(e, !edge_is_back(f,e));
}

static inline cp_vec3_loc_ref_t const *edge_get_src(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    if (edge_is_back(f,e)) {
        return e->dst;
    }
    return e->src;
}

static inline cp_vec3_loc_ref_t const *edge_get_dst(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    if (edge_is_back(f,e)) {
        return e->src;
    }
    return e->dst;
}

static inline size_t edge_get_idx(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    return cp_v_idx(&f->point, edge_get_src(f, e));
}

#ifdef DEBUG
__unused
static char const *__coord_str(char *s, size_t n, cp_vec3_t const *x)
{
    if (x == NULL) {
        return "NULL";
    }
    snprintf(s, n, FD3, CP_V012(*x));
    s[n-1] = 0;
    return s;
}

#define coord_str(p) __coord_str((char[50]){0}, 50, p)

__unused
static const char *__edge_str(
    char *s, size_t n, cp_csg3_face_t const *f, cp_csg3_edge_t const *e)
{
    cp_vec3_loc_ref_t const *src = edge_get_src(f, e);
    cp_vec3_loc_ref_t const *dst = edge_get_dst(f, e);
    snprintf(s, n, "["FD3" -- "FD3">",
        CP_V012(src->ref->coord), CP_V012(dst->ref->coord));
    s[n-1] = 0;
    return s;
}

#define edge_str(f,e)  __edge_str((char[100]){0}, 100, f, e)

#endif

static void csg2_mark(
    cp_a_size_t *have_edge,
    cp_csg3_poly_t const *d,
    cp_csg3_edge_t const *e)
{
    cp_v_bit_set(have_edge, cp_v_idx(&d->edge, e), 1);
}

/**
 * Find edge that is below z, with its predecessor above z.
 * This searches around a single point in the plane to find
 * the right face for the next edge, because in a singleton
 * point, the adjacent face is ambiguous: many faces may share
 * a point (in contrast to an edge, which is handled more
 * easily in csg2_find_dst).
 */
static cp_csg3_edge_t const *csg2_find_buddy_edge(
    cp_csg3_face_t const **f2_p,
    double z,
    cp_csg3_face_t const *f_start,
    cp_csg3_edge_t const *p_start)
{
    cp_csg3_face_t const *f = f_start;
    cp_csg3_edge_t const *p = p_start;
    for (;;) {
        assert(cp_leq(p->src->ref->coord.z, z));
        assert(cp_leq(p->dst->ref->coord.z, z));
        cp_csg3_face_t const *f2 = edge_get_buddy_face(f, p);
        size_t i2 = edge_get_idx(f2, p);
        size_t j2 = cp_wrap_sub1(i2, f2->point.size);
        cp_csg3_edge_t const *q = cp_v_nth(&f2->edge, j2);
        assert(q != p_start);
        /* ugly case: q is parallel to plane */
        if (cp_equ(q->src->ref->coord.z, z) &&
            cp_equ(q->dst->ref->coord.z, z))
        {
            LOG("found parallel edge: %s\n", edge_str(f2,q));
            /* return q itself */
            assert(edge_get_dst(f_start, p_start)->ref == edge_get_dst(f2, q)->ref);
            *f2_p = f2;
            return q;
        }

        /* non-degenerated case: q is above, p is below */
        if (cp_geq(q->src->ref->coord.z, z) &&
            cp_geq(q->dst->ref->coord.z, z))
        {
            LOG("find_buddy_edge: found diving edge %s\n", edge_str(f2,p));
            assert(edge_get_dst(f_start, p_start)->ref == edge_get_src(f2, p)->ref);
            *f2_p = f2;
            return p;
        }

        /* q is still below: search next buddy plane */
        f = f2;
        p = q;
    }
}

static cp_csg3_edge_t const *csg2_find_dst(
    cp_csg3_face_t const **f2_p,
    cp_a_size_t *have_edge,
    cp_csg3_poly_t const *d,
    double z,
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e_start)
{
    cp_csg3_edge_t const *e = e_start;

    /* Cannot be exactly on z plane as such edge should have been skipped.
     * This functions does not handle faces coplanar with z plane. */
    assert(!cp_equ(e->src->ref->coord.z, e->dst->ref->coord.z));

    /* Should be either completely below z or cutting into z.  This cannot
     * deal with edges above z or cutting from below to above z. */
    assert(edge_get_dst(f,e)->ref->coord.z < z);

    /* Get the face where e cut z plane top-to-bottom. */
    size_t start_i = edge_get_idx(f, e);

    /* Now find an edge in the polyhedron that
     * defines the next point.  This edge, again, should be top-to-bottom,
     * i.e., this function will skip to the buddy face of the edge so that
     * the caller only sees top-to-bottom edges.
     */
    LOG("find_dst: BEGIN: %2zu: %s (%p back=%u)\n", start_i, edge_str(f,e), f, edge_is_back(f,e));
    size_t i = start_i;
    size_t next_i = cp_wrap_add1(i, f->point.size);
    for (;;) {
        cp_csg3_edge_t const *p = e;
        assert((p->fore == f) || (p->back == f));

        i = next_i;
        next_i = cp_wrap_add1(i, f->point.size);

        e = cp_v_nth(&f->edge, i);
        LOG("find_dst: STEP:  %2zu: %s\n", i, edge_str(f,e));

        assert(e != e_start); /* we missed the edge */
        assert((e->fore == f) || (e->back == f));
        cp_vec3_t const *e_src = &edge_get_src(f, e)->ref->coord;
        cp_vec3_t const *e_dst = &edge_get_dst(f, e)->ref->coord;
        assert(edge_get_dst(f, p)->ref == edge_get_src(f, e)->ref);

        if ((next_i != start_i) && cp_equ(e_dst->z, z)) {
            /* We hit right on the z plane.
             *
             * We continue until dst->z is clearly above so that we skip edges
             * parallel to the plane (must be collinear, since the face is not
             * z-coplanar).
             *
             * Further more, if this is a singleton point in the z edge, i.e.,
             * a corner of the face touches z, then the next edge's z will be
             * below z again and the singleton point will be skipped, too.
             */
            /* handle this in the next step; just continue iterating... */
            LOG("same z: skip\n");
        }
        else
        if (cp_equ(e_src->z, z)) {
            /* Now we know that dst.z != src.z or that we hit the initial edge, so
             * this edge must be above
             * z now.  We can now search for the buddy edge.  This is not
             * as trivial as with a non-trivially plane cutting edge, because
             * any edge containing e_src could be the one. */
            assert((next_i == start_i) || (e_dst->z > z));
            LOG("find_dst: END: single point\n");
            csg2_mark(have_edge, d, e);

            return csg2_find_buddy_edge(f2_p, z, f, p);
        }
        else
        if (e_dst->z > z) {
            /* We found the edge that cuts cleanly through z plane. */
            assert(!cp_equ(e_src->z, z)); /* previous edge should have matched */
            assert(e_src->z < z);         /* previous edge should have matched */
            *f2_p = edge_get_buddy_face(f,e);
            LOG("find_dst: END: cutting edge\n");
            return e;
        }
    }
}

static cp_vec2_loc_t *csg2_path_push0(
    cp_v_vec2_loc_t *point,
    cp_csg2_path_t *h)
{
    cp_vec2_loc_t *p = cp_v_push0(point);
    assert(cp_v_idx(point, p) == (point->size - 1));
    cp_v_push(&h->point_idx, point->size - 1);

    return p;
}

static void csg2_add_coplanar(
    cp_v_vec2_loc_t *point,
    cp_v_csg2_path_t *path,
    cp_csg3_face_t const *f)
{
    LOG("DEBUG: add coplanar face %p\n", f);
    cp_csg2_path_t *h = cp_v_push0(path);

    /* add whole 3D path as 2D face */
    for (cp_v_each(i, &f->edge)) {
        cp_csg3_edge_t const *e = cp_v_nth(&f->edge, i);
        cp_vec3_loc_ref_t const *src = edge_get_src(f, e);

        /* add point to path */
        cp_vec2_loc_t *p = csg2_path_push0(point, h);
        p->loc = src->loc;
        p->coord = src->ref->coord.b;
        LOG("add point A: "FD2"\n", p->coord.x, p->coord.y);
    }
}

static void csg2_find_coplanar_face(
    cp_a_size_t *have_edge,
    cp_v_vec2_loc_t *point,
    cp_v_csg2_path_t *path,
    cp_csg3_poly_t const *d,
    double z,
    cp_csg3_face_t const *f)
{
    assert(f->edge.size > 0);
    if (cp_equ(f->edge.data[0]->src->ref->coord.z, z) &&
        cp_equ(f->normal.x, 0) &&
        cp_equ(f->normal.y, 0))
    {
        assert(!cp_equ(f->normal.z, 0));

        /* Face is coplanar.  All edges are marked as 'done'. */
        for (cp_v_each(i, &f->edge)) {
            cp_csg3_edge_t const *e = cp_v_nth(&f->edge, i);
            csg2_mark(have_edge, d, e);
        }

        /* Bottom faces are added to output, top faces are not. */
        if (f->normal.z < 0) {
            csg2_add_coplanar(point, path, f);
        }
    }
}

/**
 * Return whether the face is above (+1) or below (-1) or in z (0).
 */
static int csg2_face_cmp_z(
    cp_csg3_face_t const *f,
    double z)
{
    for (cp_v_each(i, &f->point)) {
        int c = cp_cmp(cp_v_nth(&f->point,i).ref->coord.z, z);
        if (c != 0) {
            return c;
        }
    }
    return 0;
}

static void csg2_edge_find_path(
    cp_a_size_t *have_edge,
    cp_v_vec2_loc_t *point,
    cp_v_csg2_path_t *path,
    cp_csg3_poly_t const *d,
    double z,
    cp_csg3_edge_t const *e_start)
{
    TRACE("edge: %s -- %s",
        coord_str(&e_start->src->ref->coord),
        coord_str(&e_start->dst->ref->coord));
    cp_csg3_edge_t const *e = e_start;

    cp_vec3_t const *v1 = &e->src->ref->coord;
    cp_vec3_t const *v2 = &e->dst->ref->coord;

    int cmp_z1 = cp_cmp(v1->z, z);
    int cmp_z2 = cp_cmp(v2->z, z);

    /* Ignore edges touching in one point. */
    if ((cmp_z1 == 0) != (cmp_z2 == 0)) {
        return;
    }

    cp_csg3_face_t const *f = NULL;

    /* Corner case 1: edge is in plane */
    if ((cmp_z1 == 0) && (cmp_z2 == 0)) {
        /* Corner case 1a: one of the adjacent faces is completely in the plane:
         * discarded if it is a top face, or copy as is if it is a bottom face. */

        /* Check fore face: completely inside the plane? */
        int cmp_fpz = csg2_face_cmp_z(e->fore, z);
        assert(cmp_fpz != 0); /* should have been handled in find_coplanar_face */

        /* Check back face: completely inside the plane? */
        int cmp_bpz = csg2_face_cmp_z(e->back, z);
        assert(cmp_bpz != 0); /* should have been handled in find_coplanar_face */

        /* Corner case 1c: fore face and back face are on the same side of the z
         * plane: discard, as this might be a single edge, not a polygon.  (It
         * may be a polygon that touches, if the 3D structure is hollow.  This
         * special case is not handled here, but will be treated consistently
         * like a single touching edge.)
         * (test30b.scad)
         */
        if (cmp_fpz == cmp_bpz) {
            return;
        }

        /* Corner case 1b: fore face and back face are on opposite sides of z plane
         * => do include this edge and search for path. (test30d.scad). */
        LOG("Touching crossing edge encountered: %d\n", cmp_fpz);
        f = e->fore;
        if (cmp_fpz > 0) {
            /* use face below z for iterating */
            f = e->back;
        }
    }
    else {
        /* Corner case 2: one point of path is a singleton point in plane, i.e.,
         * the other point is not in the plane: discard for now and wait for another
         * point of the face to decide the path's fate. (test30c.scad) */
        if ((cmp_z1 == 0) || (cmp_z2 == 0)) {
            /* FIXME: This could go wrong if all points of the polygon are in the
             * plane, but no edge is (corner case 1b would then trigger), and if we
             * need that polygon because the polyhedron is cut.
             * (test30e.scad)
             */
            return;
        }

        /* Only start at edges intersecting the plane */
        if ((cmp_z1 < 0) == (cmp_z2 < 0)) {
            return;
        }

        assert(cp_between(z, v1->z, v2->z));

        /* get face of edge where edge points from above plane to below plane */
        f = edge_get_face(e, e->src->ref->coord.z < e->dst->ref->coord.z);
        assert(edge_get_src(f,e)->ref->coord.z > z);
        assert(edge_get_dst(f,e)->ref->coord.z < z);
    }

    /* make new path */
    cp_csg2_path_t *h = cp_v_push0(path);

    LOG("find_path: %s\n", edge_str(f, e));
    /* iterate through faces to construct 2D path */
    for(;;) {
        /* edge(s) in plane: add them, too */
        if (cp_equ(e->src->ref->coord.z, e->dst->ref->coord.z)) {
            while (cp_equ(e->src->ref->coord.z, e->dst->ref->coord.z)) {
                size_t i2 = edge_get_idx(f, e);

                /* mark edge processed */
                csg2_mark(have_edge, d, e);

                cp_vec3_loc_t *dst = edge_get_dst(f, e)->ref;
                cp_vec2_loc_t *q = csg2_path_push0(point, h);
                q->loc = e->src->loc;
                q->coord.x = dst->coord.x;
                q->coord.y = dst->coord.y;
                LOG("add point B: "FD2"\n", q->coord.x, q->coord.y);

                e = cp_v_nth(&f->edge, cp_wrap_sub1(i2, f->edge.size));
            }
            /* switch to buddy face to continue search */
            assert(edge_get_src(f,e)->ref->coord.z < z);
            f = edge_get_buddy_face(f, e);
        }

        /* mark edge processed */
        csg2_mark(have_edge, d, e);

        /* add point to path */
        cp_vec2_loc_t *p = csg2_path_push0(point, h);
        csg2_point_from_csg3_edge(p, z, e);
        LOG("add point C: "FD2"\n", p->coord.x, p->coord.y);

        /* search end of 2D edge in face */
        cp_csg3_face_t const *f2 = NULL;
        cp_csg3_edge_t const *e2 = csg2_find_dst(&f2, have_edge, d, z, f, e);
        assert(f2 != NULL);
        if (e2 == e_start) {
            /* theoretically, the polygon should not be degenerated */
            assert(h->point_idx.size >= 3);
            return; /* poly is complete */
        }

        /* continue with next edge */
        e = e2;
        f = f2;
    }
}

static void csg2_add_layer_poly(
    double z,
    cp_v_csg2_p_t *c,
    cp_csg3_poly_t const *d)
{
    /* try paths from all edges */
    cp_v_vec2_loc_t point;
    CP_ZERO(&point);

    cp_v_csg2_path_t path;
    CP_ZERO(&path);

    /* FIXME: use pool */
    assert(d->edge.size > 0);
    size_t hea[CP_ROUNDUP_DIV(d->edge.size, 8*sizeof(size_t))];
    CP_ZERO(hea);
    cp_a_size_t have_edge = CP_A_INIT_WITH_ARR(hea);

    /* check faces for inside z plane */
    for (cp_v_each(i, &d->face)) {
        csg2_find_coplanar_face(&have_edge, &point, &path, d, z, &cp_v_nth(&d->face, i));
    }

    /* remaining edges */
    for (cp_v_each(i, &d->edge)) {
        if (!cp_v_bit_get(&have_edge, i)) {
            cp_csg3_edge_t const *e = &cp_v_nth(&d->edge, i);
            csg2_edge_find_path(&have_edge, &point, &path, d, z, e);
        }
    }

#if DEBUG
    LOG("POLY: #point=%zu, #path=%zu\n", point.size, path.size);
    for (cp_v_each(i, &point)) {
        LOG("  POINT %zu: "FD2"\n", i, CP_V01(point.data[i].coord));
    }
    for (cp_v_each(i, &path)) {
        LOG("  PATH %zu: #point=%zu\n", i, path.data[i].point_idx.size);
        for (cp_v_each(j, &path.data[i].point_idx)) {
            LOG("    POINT %zu.%zu: %zu\n", i, j, path.data[i].point_idx.data[j]);
        }
    }
#endif

    if (point.size > 0) {
        assert(path.size > 0);
        /* make a new 2D polygon */
        cp_csg2_t *_r = cp_csg2_new(CP_CSG2_POLY, d->loc);
        cp_csg2_poly_t *r = cp_csg2_poly(_r);

        r->point = point;
        r->path = path;

        cp_v_push(c, _r);
    }
}

static bool csg2_add_layer(
    bool *no,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_csg2_t *c);

static bool csg2_add_layer_v(
    bool *no,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_v_csg2_p_t *c)
{
    for (cp_v_each(i, c)) {
        if (!csg2_add_layer(no, r, t, zi, cp_v_nth(c,i))) {
            return false;
        }
    }
    return true;
}

static bool csg2_add_layer_add(
    bool *no,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_csg2_add_t *c)
{
    return csg2_add_layer_v(no, r, t, zi, &c->add);
}

static bool csg2_add_layer_sub(
    bool *no,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_csg2_sub_t *c)
{
    bool add_no = false;
    if (!csg2_add_layer_add(&add_no, r, t, zi, &c->add)) {
        return false;
    }
    if (add_no) {
        *no = true;
        if (!csg2_add_layer_add(no, r, t, zi, &c->sub)) {
            return false;
        }
    }
    return true;
}

static bool csg2_add_layer_cut(
    bool *no,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_csg2_cut_t *c)
{
    for (cp_v_each(i, &c->cut)) {
        if (!csg2_add_layer_add(no, r, t, zi, cp_v_nth(&c->cut, i))) {
            return false;
        }
    }
    return true;
}

extern cp_csg2_layer_t *cp_csg2_stack_get_layer(
    cp_csg2_stack_t *c,
    size_t zi)
{
    if (! (zi >= c->idx0)) {
        return NULL;
    }
    size_t i = zi - c->idx0;
    if (! (i < c->layer.size)) {
        return NULL;
    }
    return &cp_v_nth(&c->layer, i);
}

static bool csg2_add_layer_stack(
    bool *no,
    cp_csg2_tree_t *r,
    cp_err_t *t __unused,
    size_t zi,
    cp_csg2_stack_t *c)
{
    double z = cp_v_nth(&r->z, zi);

    cp_csg3_t const *d = c->csg3;

    cp_csg2_layer_t *l = cp_csg2_stack_get_layer(c, zi);
    if (l == NULL) {
        return true;
    }

    cp_csg2_add_init_perhaps(&l->root, d->loc);
    l->zi = zi;

    switch (d->type) {
    case CP_CSG3_SPHERE:
        CP_NYI("sphere");

    case CP_CSG3_CYL:
        CP_NYI("cylinder");

    case CP_CSG3_POLY:
        csg2_add_layer_poly(z, &l->root.add, cp_csg3_poly_const(d));
        break;

    case CP_CSG2_POLY:
    case CP_CSG2_CIRCLE:
        CP_DIE("2D object");

    default:
        CP_DIE("3D object");
    }

    if (l->root.add.size > 0) {
        *no = true;
    }

    return true;
}

static bool csg2_add_layer(
    bool *no,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_csg2_t *c)
{
    switch (c->type) {
    case CP_CSG2_STACK:
        return csg2_add_layer_stack(no, r, t, zi, cp_csg2_stack(c));

    case CP_CSG2_ADD:
        return csg2_add_layer_add(no, r, t, zi, cp_csg2_add(c));

    case CP_CSG2_SUB:
        return csg2_add_layer_sub(no, r, t, zi, cp_csg2_sub(c));

    case CP_CSG2_CUT:
        return csg2_add_layer_cut(no, r, t, zi, cp_csg2_cut(c));

    case CP_CSG2_POLY:
    case CP_CSG2_CIRCLE:
        CP_DIE("unexpected tree structure: objects should be inside stack");
    }

    CP_DIE("3D object type: %#x", c->type);
}

extern bool cp_csg2_tree_add_layer(
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi)
{
    assert(r->root != NULL);
    assert(r->root->type == CP_CSG2_ADD);
    assert(zi < r->z.size);
    bool no = false;
    return csg2_add_layer_add(&no, r, t, zi, cp_csg2_add(r->root));
}

extern void cp_v_vec2_loc_minmax(
    cp_vec2_minmax_t *m,
    cp_v_vec2_loc_t *o)
{
    for (cp_v_each(i, o)) {
        cp_vec2_minmax(m, &cp_v_nth(o,i).coord);
    }
}

extern void cp_csg2_poly_merge(
    cp_csg2_poly_t *r,
    cp_csg2_poly_t *a)
{
    size_t po = r->point.size;

    /* reenumerate point_idx */
    for (cp_v_each(i, &a->path)) {
        cp_csg2_path_t *p = &cp_v_nth(&a->path, i);
        for (cp_v_each(j, &p->point_idx)) {
            cp_v_nth(&p->point_idx, j) += po;
        }
    }
    for (cp_v_each(i, &a->triangle)) {
        cp_size3_t *p = &cp_v_nth(&a->triangle, i);
        p->p[0] += po;
        p->p[1] += po;
        p->p[2] += po;
    }

    /* append */
    cp_v_append(&r->point,    &a->point);
    cp_v_append(&r->path,     &a->path);
    cp_v_append(&r->triangle, &a->triangle);

    /* clear a so there are no duplicate references */
    CP_ZERO(a);
}
