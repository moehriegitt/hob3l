/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#define DEBUG 0

#include <hob3lbase/arith.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/pool.h>
#include <hob3l/gc.h>
#include <hob3l/csg2.h>
#include <hob3l/csg3.h>
#include "internal.h"

/** Combine multiple comparisions into one value */
#define CMP_SHIFT(a,s) ((unsigned)(((a) & 3) << (s)))
#define CMP3(c,b,a)    (CMP_SHIFT(c,4) | CMP_SHIFT(b,2) | CMP_SHIFT(a,0))
#define CMP2(b,a)      CMP3(0,b,a)

/* Z plane edge categorisations */

/** Fore is Above, back is below */
#define FA 1
/** Fore is Below, back is above */
#define FB -1
/** Fore is Equal to back wrt. above/below */
#define FE 2

typedef struct {
    cp_a_size_t *have_edge;
    cp_v_vec2_loc_t *point;
    cp_v_csg2_path_t *path;
    cp_csg3_poly_t const *poly;
    double z;
} ctxt_t;

static inline bool edge_is_back(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    return f == e->back;
}

static inline cp_csg3_face_t const *edge_face(
    cp_csg3_edge_t const *e,
    bool back)
{
    return back ? e->back : e->fore;
}

static inline cp_csg3_face_t const *edge_buddy_face(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    return edge_face(e, !edge_is_back(f,e));
}

static inline cp_vec3_loc_ref_t const *edge_src(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    if (edge_is_back(f,e)) {
        return e->dst;
    }
    return e->src;
}

static inline cp_vec3_loc_ref_t const *edge_dst(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    if (edge_is_back(f,e)) {
        return e->src;
    }
    return e->dst;
}

static inline size_t edge_idx(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    return cp_v_idx(&f->point, edge_src(f, e));
}

static inline cp_csg3_edge_t const *edge_prev(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    size_t i = edge_idx(f,e);
    size_t i2 = cp_wrap_sub1(i, f->point.size);
    return cp_v_nth(&f->edge, i2);
}

static inline cp_csg3_edge_t const *edge_next(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    size_t i = edge_idx(f,e);
    size_t i2 = cp_wrap_add1(i, f->point.size);
    return cp_v_nth(&f->edge, i2);
}

static void point_on_edge(
    cp_vec2_loc_t *p,
    cp_csg3_edge_t const *e,
    double z)
{
    cp_vec3_t const *src = &e->src->ref->coord;
    cp_vec3_t const *dst = &e->dst->ref->coord;

    assert(!cp_eq(dst->z, src->z));
    double t01 = cp_t01(src->z, z, dst->z);
    if (cp_eq(t01, 0)) { t01 = 0; }
    if (cp_eq(t01, 1)) { t01 = 1; }
    assert(t01 >= 0);
    assert(t01 <= 1);
    p->loc = e->src->loc;
    p->coord.x = cp_lerp(src->x, dst->x, t01);
    p->coord.y = cp_lerp(src->y, dst->y, t01);
}

static void src_on_edge(
    cp_vec2_loc_t *p,
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e)
{
    cp_vec3_loc_ref_t const *q = edge_src(f,e);
    p->loc = q->loc;
    p->coord.x = q->ref->coord.x;
    p->coord.y = q->ref->coord.y;
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
    cp_vec3_loc_ref_t const *src = edge_src(f, e);
    cp_vec3_loc_ref_t const *dst = edge_dst(f, e);
    snprintf(s, n, "["FD3" -- "FD3">",
        CP_V012(src->ref->coord), CP_V012(dst->ref->coord));
    s[n-1] = 0;
    return s;
}

#define edge_str(f,e)  __edge_str((char[100]){0}, 100, f, e)

#endif

static cp_vec2_loc_t *path_push0(
    ctxt_t *q,
    cp_csg2_path_t **h)
{
    if (*h == NULL) {
        *h = cp_v_push0(q->path);
    }

    cp_vec2_loc_t *p = cp_v_push0(q->point);
    assert(cp_v_idx(q->point, p) == (q->point->size - 1));
    cp_v_push(&(*h)->point_idx, q->point->size - 1);

    return p;
}

__unused
static bool edge_is_marked(
    ctxt_t *q,
    cp_csg3_edge_t const *e)
{
    size_t i = cp_v_idx(&q->poly->edge, e);
    return cp_v_bit_get(q->have_edge, i);
}

static void edge_mark(
    ctxt_t *q,
    cp_csg3_edge_t const *e)
{
    size_t i = cp_v_idx(&q->poly->edge, e);
    assert(!cp_v_bit_get(q->have_edge, i));
    cp_v_bit_set(q->have_edge, i, 1);
}

static unsigned edge_cmp_z(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e,
    double z)
{
    cp_vec3_t const *s = &edge_src(f,e)->ref->coord;
    cp_vec3_t const *d = &edge_dst(f,e)->ref->coord;
    return CMP2(cp_cmp(s->z, z), cp_cmp(d->z, z));
}

static int face_cmp_z(
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

static unsigned edge_z_cat(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const *e,
    double z)
{
    assert(cp_eq(e->src->ref->coord.z, e->dst->ref->coord.z));
    switch (CMP2(face_cmp_z(f, z), face_cmp_z(edge_buddy_face(f,e), z))) {
    case CMP2(0,0):
    case CMP2(+1,+1):
    case CMP2(-1,-1):
    case CMP2(-1,0):
    case CMP2(0,-1):
        return CMP3(FE,0,0);

    case CMP2(+1,-1):
    case CMP2(+1,0):
        return CMP3(FA,0,0);

    case CMP2(-1,+1):
    case CMP2(0,+1):
        return CMP3(FB,0,0);
    }
    CP_DIE();
}

static unsigned edge_follow_path(
    cp_csg3_face_t const *f,
    cp_csg3_edge_t const **e_p,
    double z)
{
    LOG("follow_path\n");
    cp_csg3_edge_t const *e = *e_p;
    for (;;) {
        cp_csg3_edge_t const *p __unused = e;
        assert((p->fore == f) || (p->back == f));

        e = edge_next(f,e);

        assert(e != *e_p); /* we missed the opposite edge */
        assert((e->fore == f) || (e->back == f));
        unsigned c = edge_cmp_z(f, e, z);
        switch (c) {
        case CMP2(-1,+1): /* up crossing */
        case CMP2(-1,0):  /* up touching */
            *e_p = e;
            return c;

        case CMP2(-1,-1): /* below: continue search */
            break;

        default: /* above: we missed the transition */
            CP_DIE("missed transition: c=0x%x", c);
        }
    }
}

static unsigned src_cw_search(
    cp_csg3_face_t const **f_p,
    cp_csg3_edge_t const **e_p,
    double z)
{
    LOG("cw_search\n");
    cp_csg3_face_t const *f = *f_p;
    cp_csg3_edge_t const *e = *e_p;
    for (;;) {
        /* must be below z plane, with src in z plane */
        assert(cp_eq(edge_src(f,e)->ref->coord.z, z));
        assert(cp_le(e->src->ref->coord.z, z));

        /* get next edge in CW direction = buddy_of(previous_edge(e)) */
        cp_csg3_edge_t const *e2 = edge_prev(f,e);
        cp_csg3_face_t const *f2 = edge_buddy_face(f,e2);
        if (e2 == *e_p) {
            /* cannot find another edge: hopefully this only happens
             * for an initial edge, not if the path has already started */
            return CMP3(FE,0,0);
        }
        assert(edge_src(f, e)->ref == edge_src(f2, e2)->ref);

        /* classify */
        int cmp_e2_d = cp_cmp(edge_dst(f2, e2)->ref->coord.z, z);
        LOG("CW: e=%s, c=%+d\n", edge_str(f2, e2), cmp_e2_d);
        switch (cmp_e2_d) {
        case -1: /* dst is below */
            /* continue search */
            break;

        case +1: /* dst is strictly above */
            /* return prev edge with new classification */
            *f_p = f;
            *e_p = e;
            return CMP3(+1,0,-1); /* touching down, face extends up */

        case 0: { /* dst is in z plane */
            /* return next edge */
            unsigned c = edge_z_cat(f2, e2, z);
            if (c == CMP3(FE,0,0)) {
                /* continue search */
                break;
            }
            *f_p = f2;
            *e_p = e2;
            return c; /* in z plane */
        }

        default:
            CP_DIE();
        }

        /* iterate*/
        f = f2;
        e = e2;
    }
}

static void edge_find_path(
    ctxt_t *q,
    cp_csg3_edge_t const *e_start)
{
    TRACE("edge: %s -- %s",
        coord_str(&e_start->src->ref->coord),
        coord_str(&e_start->dst->ref->coord));
    cp_csg3_edge_t const *e = e_start;

    cp_csg2_path_t *h = NULL;
    cp_csg3_face_t const *f= e->fore;
    unsigned c = edge_cmp_z(f, e, q->z);
    for (;;) {
        cp_csg3_face_t const *fo __unused = f;
        cp_csg3_edge_t const *eo = e;

        LOG("FIND: e=%s, c=0x%x\n", edge_str(f, e), c);
        switch (c) {
        default:
            CP_DIE("edge category: 0x%x", c);
        case CMP2(-1,-1):   /* below */
        case CMP2(+1,+1):   /* above */
        case CMP2(+1,0):    /* down touching */
        case CMP2(0,+1):    /* touching up */
        case CMP3(FE,0,0):  /* in z plane, not in output polygon */
            assert(!edge_is_marked(q, e));
            edge_mark(q, e);
            assert(h == NULL);
            return;

        case CMP2(-1,+1):   /* up crossing */
            f = edge_buddy_face(f, e);
            /* fall-through */
        case CMP2(+1,-1):   /* down crossing */
            point_on_edge(path_push0(q, &h), e, q->z);
            assert(!edge_is_marked(q, e));
            edge_mark(q, e);
            c = edge_follow_path(f, &e, q->z);
            break;

        case CMP3(+1,0,-1): /* touching down, face extends up */
            src_on_edge(path_push0(q, &h), f, e);
            assert(!edge_is_marked(q, e));
            edge_mark(q, e);
            c = edge_follow_path(f, &e, q->z);
            break;

        case CMP3(FA,0,0):  /* in z plane, part of polygon, forward */
            src_on_edge(path_push0(q, &h), f, e);
            f = edge_buddy_face(f, e);
            /* fall-through */
        case CMP3(0,0,-1):  /* touching down, unknown face orientation */
        case CMP3(-1,0,-1): /* touching down, face is strictly below */
        case CMP3(FB,0,0):  /* in z plane, part of polygon, backward */
            if (h == NULL) {
                /* wait for another edge to start the path */
                return;
            }
            assert(!edge_is_marked(q, e));
            edge_mark(q, e);
            c = src_cw_search(&f, &e, q->z);
            break;

        case CMP2(-1,0):    /* up touching */
            if (h == NULL) {
                if (cp_le(edge_dst(f,edge_next(f,e))->ref->coord.z, q->z)) {
                    /* wait for another edge to start the path */
                    return;
                }
            }
            f = edge_buddy_face(f, e);
            assert(!edge_is_marked(q, e));
            c = src_cw_search(&f, &e, q->z);
            assert((e != eo) || (f != fo));
            if (e != eo) { /* could be same edge: only mark once */
                edge_mark(q, eo);
            }
            break;

        case CMP3(0,0,0):   /* in z plane, unknown face orientation */
            c = edge_z_cat(f, e, q->z);
            break;
        }

        if ((eo != e) && (e == e_start)) {
            assert(h != NULL);
            LOG("END: (%s)\n", edge_str(f,e));
            return;
        }
    }
}

static int rand_int(int max)
{
    return rand() % max;
}

static unsigned char rand_color(
    cp_csg2_tree_opt_t const *opt,
    unsigned char v)
{
    int q = opt->color_rand;
    if (q == 0) {
        return v;
    }
    int x = v;
    x += q - rand_int(2*q+1);
    if (x < 0) x = 0;
    if (x > 255) x = 255;
    unsigned u = x & 0xff;
    return u & 0xff;
}

static void rand_color3(
    cp_color_rgba_t *o,
    cp_csg2_tree_opt_t const *opt,
    cp_color_rgba_t const *i)
{
    o->r = rand_color(opt, i->r);
    o->g = rand_color(opt, i->g);
    o->b = rand_color(opt, i->b);
    o->a = i->a;
}

static void csg2_add_layer_poly(
    cp_csg2_tree_opt_t const*opt,
    cp_pool_t *pool,
    double z,
    cp_v_csg2_p_t *c,
    cp_csg3_poly_t const *d)
{
    /* try paths from all edges */
    cp_v_vec2_loc_t point;
    CP_ZERO(&point);

    cp_v_csg2_path_t path;
    CP_ZERO(&path);

    assert(d->edge.size > 0);
    size_t hea_size = CP_ROUNDUP_DIV(d->edge.size, 8*sizeof(size_t));
    size_t *hea = CP_POOL_NEW_ARR(pool, *hea, hea_size);
    cp_a_size_t have_edge = CP_A_INIT_WITH(hea, hea_size);

    ctxt_t q = {
        .have_edge = &have_edge,
        .point = &point,
        .path = &path,
        .poly = d,
        .z = z,
    };

    /* remaining edges */
    for (cp_v_each(i, &d->edge)) {
        if (!cp_v_bit_get(&have_edge, i)) {
            cp_csg3_edge_t const *e = &cp_v_nth(&d->edge, i);
            edge_find_path(&q, e);
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

        /* set a uniform color for all vertices */
        for (cp_v_each(i, &point)) {
            rand_color3(&cp_v_nth(&point, i).color, opt, &d->gc.color);
        }

        /* make a new 2D polygon */
        cp_csg2_t *_r = cp_csg2_new(CP_CSG2_POLY, d->loc);
        cp_csg2_poly_t *r = cp_csg2_poly(_r);

        r->point = point;
        r->path = path;

        cp_v_push(c, _r);
    }
}

static void csg2_add_layer_sphere(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool __unused,
    double z,
    cp_v_csg2_p_t *c,
    cp_csg3_sphere_t const *d)
{
    /* Mechanism for slicing an ellipse from the ellipsoid:
     * Problem: we want to cut the ellipsoid at the XZ plane defined by z.
     * The ellipsoid is given as a unit sphere mapped to the object coordinate
     * system using d->mat.
     *
     * Idea:
     *   * Map the cutting XZ plane into the unit sphere coordinate system using
     *     the inverse object coordinate matrix.  This can be done by mapping three
     *     points in the XY plane, e.g., [1,0,z], [0,0,z], and [0,1,z] and then
     *     reconstructing the plane normal from those points.
     *   * Rotate the unit sphere's coordinate system so that the mapped plane
     *     is parallel to z again, call this coordinate system Q.  This can be
     *     done using the *_rot_into_z() matrix.
     *   * In Q, compute the distance between the plane and the unit sphere center,
     *     which is easy because by the nature of this coordinate system, we just
     *     need to subtract the z coordinates.
     *   * Decide by distance whether the plane cuts the sphere, and compute the
     *     diameter of the circle in Q.  This can be done by basic trigonometry.
     *   * Nap the circle using the inverse matrix of Q back into the object
     *     coordinate system, then reduce the 3D mapping matrix to a 2D mapping
     *     matrix by eliminating the Z component.
     */

    /* step 1: compute the plane in d's coordinate system */
    cp_vec3_t oa = {{ 1, 0, z }};
    cp_vec3_t ob = {{ 0, 0, z }};
    cp_vec3_t oc = {{ 0, 1, z }};
    cp_vec3_t sa, sb, sc;
    cp_vec3w_xform(&sa, &d->mat->i, &oa);
    cp_vec3w_xform(&sb, &d->mat->i, &ob);
    cp_vec3w_xform(&sc, &d->mat->i, &oc);
    cp_vec3_t sn;
    if (!cp_vec3_right_normal3(&sn, &sa, &sb, &sc)) {
        /* coordinate system is too small/large: generate nothing */
        return;
    }

    /* step 2: compute coordinate system where sn points upright again */
    cp_mat3wi_t mq = {0};
    cp_mat3wi_rot_into_z(&mq, &sn);

    /* step 3: rotate plane base point around unit sphere */
    cp_vec3_t sq;
    cp_vec3w_xform(&sq, &mq.n, &sb);

    /* step 4: distance and radius of circle cutting the plane */
    cp_dim_t dist = fabs(sq.z);
    if (cp_ge(dist, 1)) {
        /* plane does not cut unit sphere */
        return;
    }
    cp_dim_t rad = sqrt(1 - (dist*dist));

    /* step 5: map 2D circle into so that we can compute it at [cos,sin,0] */

    /* (a) scale unit circle */
    cp_mat3wi_t mt;
    cp_mat3wi_scale(&mt, rad, rad, 1);

    /* (b) move circle into plane in Q */
    cp_mat3wi_t mm;
    cp_mat3wi_xlat(&mm, 0, 0, sq.z);
    cp_mat3wi_mul(&mt, &mm, &mt);

    /* (c) rotate into sphere coordinate system: apply mq inverse.
     * We do not need mq anymore, so we can invert in place */
    cp_mat3wi_inv(&mq, &mq);
    cp_mat3wi_mul(&mt, &mq, &mt);

    /* (d) map from unit sphere coordinate system into object coordinate system */
    cp_mat3wi_mul(&mt, d->mat, &mt);

    /* (e) extract 2D part of matrix */
    cp_mat2wi_t mt2;
    (void)cp_mat2wi_from_mat3wi(&mt2, &mt);

    /* add ellipse to output layer */
    if (CP_CSG2_HAVE_CIRCLE) {
        /* store real circle */
        cp_csg2_t *_r = cp_csg2_new(CP_CSG2_CIRCLE, d->loc);
        cp_csg2_circle_t *r = cp_csg2_circle(_r);
        r->color = d->gc.color;

        r->mat = mt2;
        r->_fa = d->_fa;
        r->_fs = d->_fs;
        r->_fn = d->_fn;
        cp_v_push(c, _r);
    }
    else {
        /* render ellipse polygon */
        cp_csg2_t *_r = cp_csg2_new(CP_CSG2_POLY, d->loc);
        cp_csg2_poly_t *r = cp_csg2_poly(_r);

        cp_csg2_path_t *o = cp_v_push0(&r->path);
        size_t fn = d->_fn;
        assert(fn >= 3);
        double a = CP_TAU/cp_f(fn);
        for (cp_size_each(i, fn)) {
            cp_vec2_loc_t *p = cp_v_push0(&r->point);
            p->coord.x = cos(a * cp_f(i));
            p->coord.y = sin(a * cp_f(i));
            p->loc = d->loc;
            p->color = d->gc.color;
            rand_color3(&p->color, opt, &d->gc.color);
            cp_vec2w_xform(&p->coord, &mt2.n, &p->coord);
            cp_v_push(&o->point_idx, i);
        }
        if (mt2.d > 0) {
            cp_v_reverse(&o->point_idx, 0, -(size_t)1);
        }
        cp_v_push(c, _r);
    }
}

static bool csg2_add_layer(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_csg2_t *c);

static bool csg2_add_layer_v(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_v_csg2_p_t *c)
{
    for (cp_v_each(i, c)) {
        if (!csg2_add_layer(no, pool, r, t, zi, cp_v_nth(c,i))) {
            return false;
        }
    }
    return true;
}

static bool csg2_add_layer_add(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_csg2_add_t *c)
{
    return csg2_add_layer_v(no, pool, r, t, zi, &c->add);
}

static bool csg2_add_layer_sub(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_csg2_sub_t *c)
{
    bool add_no = false;
    if (!csg2_add_layer_add(&add_no, pool, r, t, zi, &c->add)) {
        return false;
    }
    if (add_no) {
        *no = true;
        if (!csg2_add_layer_add(no, pool, r, t, zi, &c->sub)) {
            return false;
        }
    }
    return true;
}

static bool csg2_add_layer_cut(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_csg2_cut_t *c)
{
    for (cp_v_each(i, &c->cut)) {
        if (!csg2_add_layer_add(no, pool, r, t, zi, cp_v_nth(&c->cut, i))) {
            return false;
        }
    }
    return true;
}

static bool csg2_add_layer_stack(
    bool *no,
    cp_pool_t *pool,
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
        csg2_add_layer_sphere(&r->opt, pool, z, &l->root.add, cp_csg3_cast(_sphere, d));
        break;

    case CP_CSG3_CYL:
        CP_NYI("cylinder");

    case CP_CSG3_POLY:
        csg2_add_layer_poly(&r->opt, pool, z, &l->root.add, cp_csg3_cast(_poly, d));
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
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi,
    cp_csg2_t *c)
{
    switch (c->type) {
    case CP_CSG2_STACK:
        return csg2_add_layer_stack(no, pool, r, t, zi, cp_csg2_stack(c));

    case CP_CSG2_ADD:
        return csg2_add_layer_add(no, pool, r, t, zi, cp_csg2_add(c));

    case CP_CSG2_SUB:
        return csg2_add_layer_sub(no, pool, r, t, zi, cp_csg2_sub(c));

    case CP_CSG2_CUT:
        return csg2_add_layer_cut(no, pool, r, t, zi, cp_csg2_cut(c));

    case CP_CSG2_POLY:
    case CP_CSG2_CIRCLE:
        CP_DIE("unexpected tree structure: objects should be inside stack");
    }

    CP_DIE("3D object type: %#x", c->type);
}

/* ********************************************************************** */
/* extern */

/**
 * If the stack has the given layer, return it.
 * Otherwise, return NULL.
 */
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

/**
 * This generates stacks of polygon CSG2 trees.
 *
 * The tree must be either empty, or the root of the tree must have
 * type CP_CSG2_ADD.  If the result is non-empty, this will set the
 * root to a node of type CP_CSG2_ADD.
 *
 * In the polygons, only the 'path' entries are filled in, i.e.,
 * the 'triangle' entries are left empty.
 *
 * Uses \p pool for all temporary allocations (but not for constructing r).
 */
extern bool cp_csg2_tree_add_layer(
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi)
{
    assert(r->root != NULL);
    assert(r->root->type == CP_CSG2_ADD);
    assert(zi < r->z.size);
    bool no = false;
    return csg2_add_layer_add(&no, pool, r, t, zi, cp_csg2_add(r->root));
}

/**
 * Compute bounding box
 *
 * Runtime: O(n), n=size of vector
 */
extern void cp_v_vec2_loc_minmax(
    cp_vec2_minmax_t *m,
    cp_v_vec2_loc_t *o)
{
    for (cp_v_each(i, o)) {
        cp_vec2_minmax(m, &cp_v_nth(o,i).coord);
    }
}

/**
 * Append all paths from a into r, destroying a.
 *
 * This does no intersection test, but simply appends vectors
 * and adjusts indices.
 *
 * This moves both the paths and the triangulation.
 *
 * NOTE: If necessary for some future algorithm, this could be
 * rewritten to only clear a->path, but leave a->point and a->triangle
 * intact, because the latter vectors do not contain any allocated
 * substructures.  Currently, for consistency, the whole poly is
 * cleared.
 */
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

/**
 * Return the layer thickness of a given layer.
 */
extern cp_dim_t cp_csg2_layer_thickness(
    cp_csg2_tree_t *t,
    size_t zi __unused)
{
    /* Currently, all layers have uniform thickness. */
    return t->thick;
}
