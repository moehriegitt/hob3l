/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#define DEBUG 0

#include <cpmat/arith.h>
#include <cpmat/vec.h>
#include <cpmat/mat.h>
#include <cpmat/alloc.h>
#include <cpmat/pool.h>
#include <csg2plane/gc.h>
#include <csg2plane/csg2.h>
#include <csg2plane/csg3.h>
#include "internal.h"

/** Combine multiple comparisions into one value */
#define CMP_SHIFT(a,s) ((unsigned)(((a) & 3) << (s)))
#define CMP3(c,b,a)    (CMP_SHIFT(c,4) | CMP_SHIFT(b,2) | CMP_SHIFT(a,0))
#define CMP2(b,a)      CMP3(0,b,a)

/** Z plane edge categorisations */
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

    assert(!cp_equ(dst->z, src->z));
    double t01 = cp_t01(src->z, z, dst->z);
    if (cp_equ(t01, 0)) { t01 = 0; }
    if (cp_equ(t01, 1)) { t01 = 1; }
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
    assert(cp_equ(e->src->ref->coord.z, e->dst->ref->coord.z));
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
        assert(cp_equ(edge_src(f,e)->ref->coord.z, z));
        assert(cp_leq(e->src->ref->coord.z, z));

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
                if (cp_leq(edge_dst(f,edge_next(f,e))->ref->coord.z, q->z)) {
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

static void csg2_add_layer_poly(
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
    size_t *hea;
    CP_POOL_CALLOC_ARR(pool, hea, hea_size);
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
        CP_NYI("sphere");

    case CP_CSG3_CYL:
        CP_NYI("cylinder");

    case CP_CSG3_POLY:
        csg2_add_layer_poly(pool, z, &l->root.add, cp_csg3_poly_const(d));
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

extern cp_dim_t cp_csg2_layer_thickness(
    cp_csg2_tree_t *t,
    size_t zi __unused)
{
    /* Currently, all layers have uniform thickness. */
    return t->thick;
#if 0
    if (t->z.size <= 1) {
        return 1.0;
    }
    if (zi <= 0) {
       zi = 1;
    }
    if (zi >= t->z.size) {
       zi = t->z.size - 1;
    }
    double th = t->z.data[zi] - t->z.data[zi - 1];
    assert(th > 0);
    return th;
#endif
}
