/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/*
 * TODO:
 *
 *   - holes work, but be sure that coincident points work, because we
 *     may encounter those easily, e.g. from a boolean operation.
 *     If not, I think it may be as easy as extending the comparison
 *     function to consider the other side of an edge from comparing
 *     points.
 */

#define DEBUG 0

#include <stdio.h>
#include <cpmat/dict.h>
#include <cpmat/mat.h>
#include <cpmat/list.h>
#include <cpmat/panic.h>
#include <csg2plane/csg2.h>
#include <csg2plane/ps.h>
#include "internal.h"

typedef enum {
    INACTIVE,
    BOT,
    TOP
} type_t;

/**
 * The cases are order by processing order: lowest first
 */
typedef enum {
    CASE_END,
    CASE_BEND,
    CASE_START,
} case_t;

/**
 * A point and an edge that can be inserted into a set.
 *
 * In px, this represents the point 'p'.
 *
 * In ey, this represents the edge 'p--dst->p'.
 */
typedef cp_csg2_3node_t node_t;
typedef cp_csg2_3edge_t edge_t;
typedef cp_csg2_3list_t list_t;

typedef struct {
    cp_vec2_arr_ref_t *point_arr;
    cp_v_size3_t *tri;
    cp_err_t *t;
    cp_dict_t *nx;
    cp_dict_t *ey;
    list_t *list_data;
    size_t  list_size;
    size_t  list_end;
    list_t  list_free;
} ctxt_t;

static inline node_t *get_nx(
    cp_dict_t *d)
{
    return CP_BOX0_OF(d, node_t, node_nx);
}

static inline edge_t *get_ey(
    cp_dict_t *d)
{
    return CP_BOX0_OF(d, edge_t, node_ey);
}

static inline node_t *get_li(list_t *d)
{
    return d->node;
}

static list_t *list_alloc(ctxt_t *c, node_t *n)
{
    assert(n != NULL);
    list_t *r = c->list_free.next;
    if (r != &c->list_free) {
        cp_list_remove(r);
    }
    else {
        assert(c->list_end < c->list_size);
        r = &c->list_data[c->list_end++];
        cp_list_init(r);
    }
    assert(r != NULL);
    assert(r->prev == r);
    assert(r->next == r);
    assert(r->node == NULL);
    r->node = n;
    return r;
}

static inline void list_free(ctxt_t *c, list_t *d)
{
    assert(d->next == d);
    assert(d->prev == d);
    d->node = NULL;
    cp_list_insert(&c->list_free, d);
}

static void list_remove(ctxt_t *c, list_t *d)
{
    cp_list_remove(d);
    list_free(c, d);
}

static int coord_cmp(cp_vec2_t const *a, cp_vec2_t const *b)
{
    return cp_vec2_lex_cmp(a, b);
}

/**
 * To be consistent (for same x coordinate points), left/right must be defined by
 * means of coord_cmp().  We will not have identical end points on an edge.
 * Because we order the points for more than the coordinates, we need to return
 * the node_t, not the cp_vec2_t.
 */
static inline void left_right2(
    node_t **l,
    node_t **r,
    node_t *a,
    node_t *b)
{
    if (coord_cmp(a->coord, b->coord) <= 0) {
        *l = a;
        *r = b;
    }
    else {
        *l = b;
        *r = a;
    }
}

static inline void left_right(
    node_t **l,
    node_t **r,
    edge_t *a)
{
    left_right2(l, r, a->src, a->dst);
}

static node_t *left(
    edge_t *a)
{
    node_t *l, *r;
    left_right(&l, &r, a);
    return l;
}

__unused
static node_t *right(
    edge_t *a)
{
    node_t *l, *r;
    left_right(&l, &r, a);
    return r;
}

__unused
static char const *__coord_str(char *s, size_t n, cp_vec2_t const *x)
{
    if (x == NULL) {
        return "NULL";
    }
    snprintf(s, n, FD2, CP_V01(*x));
    s[n-1] = 0;
    return s;
}

#define coord_str(p) __coord_str((char[50]){0}, 50, p)

__unused
static const char *__node_str(char *s, size_t n, node_t *x)
{
    return __coord_str(s, n, x->coord);
}

#define node_str(p)  __node_str((char[50]){0}, 50,  p)

__unused
static const char *__edge_str(char *s, size_t n, edge_t *e)
{
    cp_vec2_t *a = e->src->coord;
    cp_vec2_t *b = e->dst->coord;
    char ac = '(';
    char bc = '>';
    if (a->x > b->x) {
        CP_SWAP(&a, &b);
        ac = '<';
        bc = ')';
    }
    snprintf(s, n, "%c"FD2":"FD2"%c %c",
        ac, CP_V01(*a), CP_V01(*b), bc,
        e->type == BOT ? 'A' : e->type == TOP ? 'V' : '-');
    s[n-1] = 0;
    return s;
}

#define edge_str(e)  __edge_str((char[100]){0}, 100, e)

#if DEBUG

static size_t list_len(
    list_t *start)
{
    assert(start != NULL);
    assert(get_li(start) == NULL);
    bool back = (start->next->node == NULL);
    size_t i = 0;
    for (list_t *n = start->step[back]; get_li(n) != NULL; n = n->step[back]) {
        assert(n != start);
        i++;
    }
    return i;
}

static void dump_list_fore(list_t *start, list_t *rm)
{
    assert(start != NULL);
    assert(get_li(start) == NULL);
    if (get_li(start->prev) != NULL) {
        LOG(" 1HEAD");
    }
    for (list_t *n = start->next; get_li(n) != NULL; n = n->next) {
        assert(n != start);
        assert(n->next->prev == n);
        assert(n->prev->next == n);
        LOG(" ");
        if (n == rm) {
            LOG("*");
        }
        LOG("%s", node_str(get_li(n)));
    }
}

static void dump_list_back_rec(list_t *_n, list_t *rm)
{
    assert(_n->next->prev == _n);
    assert(_n->prev->next == _n);
    node_t *n = get_li(_n);
    if (n == NULL) {
        return;
    }

    dump_list_back_rec(_n->prev, rm);

    LOG(" ");
    if (_n == rm) {
        LOG("*");
    }
    LOG("%s", node_str(n));
}

static void dump_list_back(list_t *end, list_t *rm)
{
    assert(end != NULL);
    assert(get_li(end) == NULL);
    if (get_li(end->next) != NULL) {
        LOG(" 1HEAD");
    }
    dump_list_back_rec(end->prev, rm);
}

static void dump_list(list_t *list, list_t *rm)
{
    LOG("%02"_Pz"u", list_len(list));
    if (get_li(list->prev) != NULL) {
        LOG(" T<");
        dump_list_back(list, rm);
    }
    else
    if (get_li(list->next) != NULL) {
        LOG(" H>");
        dump_list_fore(list, rm);
    }
    else {
        LOG(" EMPTY");
    }
}

#endif

#if DEBUG || defined(PSTRACE)

static void dump_ey(
    ctxt_t *c,
    char const *msg __unused,
    node_t *p __unused)
{
#if DEBUG
    LOG("BEGIN EY\n");
    for (cp_dict_each_rev(_n, c->ey)) {
        edge_t *e = get_ey(_n);
        LOG("%s ", edge_str(e));
        dump_list(&e->list, e->rm);
        LOG("\n");
    }
    LOG("END EY\n");
#endif

#ifdef PSTRACE
    if (cp_debug_ps != NULL) {
        /* begin page */
        cp_ps_page_begin(cp_debug_ps, ++cp_debug_ps_page_cnt);

        /* print info */
        cp_printf(cp_debug_ps, "30 30 moveto (TRI: %s) show\n", msg);
        cp_printf(cp_debug_ps, "30 55 moveto (%s) show\n", node_str(p));

        /* sweep line */
        cp_printf(cp_debug_ps, "0.8 setgray 1 setlinewidth\n");
        cp_printf(cp_debug_ps,
            "newpath %g dup 0 moveto %u lineto stroke\n",
            CP_PS_X(p->coord->x),
            CP_PS_PAPER_Y);
        cp_printf(cp_debug_ps, "newpath %g %g 3 0 360 arc closepath fill\n",
            CP_PS_XY(*p->coord));

        /* sweep state */
        cp_printf(cp_debug_ps, "3 setlinewidth\n");
        for (cp_dict_each(_e, c->ey)) {
            edge_t *e = get_ey(_e);
            cp_printf(cp_debug_ps,
                "0 %g 0 setrgbcolor\n", (e->type % 3) * 0.5);
            cp_printf(cp_debug_ps,
                "newpath %g %g moveto %g %g lineto stroke\n",
                CP_PS_XY(*e->src->coord),
                CP_PS_XY(*e->dst->coord));
            if (e->type == TOP) {
                cp_printf(cp_debug_ps, "1 0 0 setrgbcolor 4 setlinewidth\n");
                node_t *m = NULL;
                for (cp_list_each(_n, &e->list)) {
                    node_t *n = get_li(_n);
                    if (n == NULL) {
                        break;
                    }
                    cp_printf(cp_debug_ps, "newpath %g %g 4 0 360 arc closepath fill\n",
                        CP_PS_XY(*n->coord));
                    if (m != NULL) {
                        cp_printf(cp_debug_ps, "newpath %g %g moveto %g %g lineto stroke\n",
                            CP_PS_XY(*n->coord),
                            CP_PS_XY(*m->coord));
                    }
                    m = n;
                }
            }
        }

        /* triangles */
        cp_printf(cp_debug_ps, "2 setlinewidth\n");
        for (cp_v_each(i, c->tri)) {
            cp_printf(cp_debug_ps, "0 %g 0.8 setrgbcolor\n", cp_double(i % 3) * 0.5);
            cp_size3_t *t = &c->tri->data[i];
            cp_vec2_t *p0 = cp_vec2_arr_ref(c->point_arr, t->p[0]);
            cp_vec2_t *p1 = cp_vec2_arr_ref(c->point_arr, t->p[1]);
            cp_vec2_t *p2 = cp_vec2_arr_ref(c->point_arr, t->p[2]);
            cp_printf(cp_debug_ps,
                "newpath %g %g moveto %g %g lineto %g %g lineto closepath stroke\n",
                CP_PS_XY(*p0),
                CP_PS_XY(*p1),
                CP_PS_XY(*p2));
        }

        /* end page */
        cp_ps_page_end(cp_debug_ps);
    }
#endif
}

#else
#define dump_ey(...) ((void)0)
#endif

static int pt_case(node_t *p)
{
    int is = coord_cmp(p->coord, p->in->src->coord);
    assert(is != 0);
    int id = coord_cmp(p->coord, p->out->dst->coord);
    assert(id != 0);
    if (is != id) {
        return CASE_BEND;
    }
    return is < 0 ? CASE_START : CASE_END;
}

static void top_bottom2(
    node_t **t,
    node_t **b,
    node_t *u,
    node_t *v)
{
    assert(!cp_equ(u->coord->y, v->coord->y));
    if (u->coord->y < v->coord->y) {
        *b = u;
        *t = v;
    }
    else {
        *b = v;
        *t = u;
    }
}


/*
 * Compare by p.x, secondarily by p.y, i.e., node_t is interpreted
 * as the point 'p' and sorted accordingly.
 */
static int cmp_nx_p(
    node_t *a,
    node_t *b)
{
    if (a == b) {
        return 0;
    }

    int i = coord_cmp(a->coord, b->coord);
    if (i != 0) {
        return i;
    }

    /* same coordinates: order by corner type */
    i = pt_case(a) - pt_case(b);
    if (i != 0) {
        return i;
    }

    /* That was easy.  Now it's getting complicated.
     *
     * The order is clear, though: we cannot handle truely
     * intersecting/crossing paths, meaning there is a truely 'lower'
     * or more 'left' corner when looking at the edges.  csg2-bool
     * should not produce this, though, so that restriction is fine.
     */
    if (i == CASE_BEND) {
        /* FIXME: This might not happen by the way we connect edges
         * in the csg2-bool module.
         * Anyway: check that it cannot happen or fill in code here:
         * the bend below the other one should be smaller.  The bends
         * should not cross.*/
        assert(0 && "FIXME: add test case to trigger this or prove it cannot happen");
    }

    /* For START: The outer one should come first.
     * For END: The inner one should come first.
     * By the way we construct the polygons in csg2-bool, there
     * must be no crossing.
     * Still, there are two cases: one is completely above the other,
     * or one is in between the other.
     */
    node_t *at, *ab, *bt, *bb;
    top_bottom2(&at, &ab, a->in->src, a->out->dst);
    top_bottom2(&bt, &bb, b->in->src, b->out->dst);
    int at_x_bt = cp_vec2_right_normal3_z(at->coord, a->coord, bt->coord);
    int ab_x_bt = cp_vec2_right_normal3_z(ab->coord, a->coord, bt->coord);
    int at_x_bb = cp_vec2_right_normal3_z(at->coord, a->coord, bb->coord);
    assert((at_x_bt != 0) && "collinear edges are not expected here");
    assert((ab_x_bt != 0) && "collinear edges are not expected here");
    assert((at_x_bb != 0) && "collinear edges are not expected here");
    if (at_x_bt != ab_x_bt) {
        /* b is inside a => END: b before a, START: a before b */
        assert((at_x_bb == at_x_bt) && "crossing start/end is not expected here");
        assert(ab_x_bt == cp_vec2_right_normal3_z(ab->coord, a->coord, bb->coord));
        assert(ab_x_bt == (i == CASE_END ? +1 : -1));
        return at_x_bt;
    }
    if (at_x_bt != at_x_bb) {
        /* a is inside b: analog to previous case */
        assert(at_x_bb == cp_vec2_right_normal3_z(ab->coord, a->coord, bb->coord));
        assert(at_x_bt == (i == CASE_END ? +1 : -1));
        return at_x_bt;
    }

    /* one is completely above the other */
    assert(at_x_bt == cp_vec2_right_normal3_z(ab->coord, a->coord, bb->coord));
    return i == CASE_END ? +at_x_bt : -at_x_bt;
}

static int cmp_nx(
    cp_dict_t *_a,
    cp_dict_t *_b,
    void *user __unused)
{
    return cmp_nx_p(get_nx(_a), get_nx(_b));
}

static inline int pt2_pt_cmp(
    node_t const *a1,
    node_t const *a2,
    node_t const *b)
{
    return cp_vec2_right_normal3_z(a1->coord, a2->coord, b->coord);
}

static int cmp_ey_pe(
    node_t *np,
    edge_t *b)
{
    node_t *nl, *nr;
    left_right(&nl, &nr, b);
    cp_vec2_t *p = np->coord;
    cp_vec2_t *l = nl->coord;
    cp_vec2_t *r = nr->coord;

    /* quick check for equality */
    if (p == l) {
        return cp_vec2_right_normal3_z(r, l, np->in->src->coord);
    }
    assert(p != r); /* why can this not happen? */

    /* p should be between l and r. */
    assert(cp_leq(l->x, p->x) && cp_leq(p->x, r->x));

    /* equal x coord: compare y coord */
    if (cp_equ(l->x, r->x)) {
        /* should not be in between l and r: it would mean we have collinear
         * adjacent edges.  It should also not be equal, because then p should
         * be equal to either l or r, and we tested that already above. */
        assert(
            ((p->y > l->y) && (p->y > r->y)) ||
            ((p->y < l->y) && (p->y < r->y)));

        return p->y < l->y ? -1 : +1;
    }

    /* get y value at p's x coord: */
    double t01 = cp_t01(l->x, p->x, r->x);
    /* exactify at ends */
    if (cp_equ(t01, 0)) { t01 = 0; }
    if (cp_equ(t01, 1)) { t01 = 1; }
    assert(t01 >= 0);
    assert(t01 <= 1);
    double y = cp_lerp(l->y, r->y, t01);

    /* p should not be right on the edge, unless equal to an end point,
     * which we checked already. */
    assert(!cp_equ(p->y, y));

    return p->y < y ? -1 : +1;
}

static int cmp_ey(
    node_t *a,
    cp_dict_t *b,
    void *user __unused)
{
    /* If a is exactly on b (since we assume we have no
     * degenerate edges, this can only happen at src or
     * dst of the edge), this is assumed to be equal.
     */
    return cmp_ey_pe(a, get_ey(b));
}

/**
 * Find p, returns whether it was found.
 *
 * ref will be assigned the reference insertion position.
 *
 * If p was found, s and t will be the edges whose endpoint is p.
 * If s and t's x coordinate are
 * s the one with the larger y coordinate.
 *
 * ref is only set if CASE_START is returned.
 */
static case_t find(
    cp_dict_ref_t *ref,
    edge_t **s,
    edge_t **t,
    ctxt_t *c,
    node_t *p)
{
    edge_t *e1 = NULL;
    edge_t *e2 = NULL;
    if (cp_dict_is_member(&p->in->node_ey)) {
        e1 = p->in;
        e2 = p->out;
    }
    else
    if (cp_dict_is_member(&p->out->node_ey)) {
        e1 = p->out;
        e2 = p->in;
    }
    else {
        /* Find the insertion position by dict lookup */
        cp_dict_t *_e __unused = cp_dict_find_ref(ref, p, c->ey, cmp_ey, NULL, 0);
        assert(_e == NULL);
        /* p is not part of active list => we have a start.  Depending on ref,
         * find s and t. */
        if (ref->child == 0) {
            /* p is below reference edge */
            *s = get_ey(ref->parent);
            *t = get_ey(cp_dict_prev0(ref->parent));
        }
        else {
            /* p is above reference edge */
            *s = get_ey(cp_dict_next0(ref->parent));
            *t = get_ey(ref->parent);
        }

        return CASE_START;
    }

    node_t *left_e1 = left(e1);
    assert(coord_cmp(left_e1->coord, p->coord) <= 0);

    /* if the adjacent edge is in the tree, we have an end, otherwise a bend */
    if (!cp_dict_is_member(&e2->node_ey)) {
        assert(coord_cmp(right(e2)->coord, p->coord) > 0);
        *s = e1;
        *t = e2;
        return CASE_BEND;
    }

    LOG("e1=%p, e2=%p\n", e1, e2);
    LOG("e1=%s, e2=%s\n", edge_str(e1), edge_str(e2));
    LOG("e1.type=%u, e2.type=%u\n", e1->type, e2->type);

    node_t *left_e2 = left(e2);
    assert(left_e1->coord != left_e2->coord);
    /* Find the edge that is 'under' the other. */
    /* s becomes top, t becomes bottom: assign e1 or e2 accordingly.
     * We use the cross produce to see which one is the top one.
     */
    double z = cp_vec2_right_cross3_z(
        left_e1->coord, p->coord, left_e2->coord);
    assert(!cp_equ(z, 0));
    if (z > 0) {
        *s = e1;
        *t = e2;
    }
    else {
        *s = e2;
        *t = e1;
    }

    /* we have an end */
    return CASE_END;
}

static void insert_ey(
    ctxt_t *c,
    cp_dict_ref_t *ref,
    edge_t *s)
{
    cp_dict_insert_ref(&s->node_ey, ref, &c->ey);
}

static void insert2_ey(
    ctxt_t *c,
    cp_dict_ref_t *ref,
    edge_t *l,
    edge_t *h)
{
    if (ref->child == 0) {
        /* downward: ref node is above */
        insert_ey(c, ref, l);
        insert_ey(c, ref, h);
    }
    else {
        /* upward: ref node is below */
        insert_ey(c, ref, h);
        insert_ey(c, ref, l);
    }
}

static edge_t *prev(edge_t *e)
{
    assert(e->type == TOP);
    edge_t *f = CP_BOX_OF(e->list.prev, edge_t, list);
    assert(f->type == BOT);
    return f;
}

static edge_t *next(edge_t *e)
{
    assert(e->type == BOT);
    edge_t *f = CP_BOX_OF(e->list.next, edge_t, list);
    assert(f->type == TOP);
    return f;
}

#define assert_inactive(e) \
    do{ \
        edge_t *__e __unused = (e); \
        assert(!cp_dict_is_member(&__e->node_ey)); \
        assert(__e->type == INACTIVE); \
        assert(__e->rm == NULL); \
        assert(__e->list.next == &__e->list); \
        assert(__e->list.prev == &__e->list); \
        assert(__e->list.node == NULL); \
    }while(0)

#define assert_active(e) \
    do{ \
        edge_t *__e __unused = (e); \
        assert(cp_dict_is_member(&__e->node_ey)); \
        assert((__e->type == TOP) || (__e->type == BOT)); \
        assert(__e->rm != NULL); \
        assert(__e->list.next != &__e->list); \
        assert(__e->list.prev != &__e->list); \
        assert(__e->list.node == NULL); \
    }while(0)

#define assert_active_pair(l,h) \
    do{ \
        edge_t *__l = (l); \
        edge_t *__h = (h); \
        assert_active(__l); \
        assert_active(__h); \
        assert(__l->list.next == &__h->list); \
        assert(__h->list.prev == &__l->list); \
        assert(__l->rm == __h->rm); \
    }while(0)

static void add_triangle(
    ctxt_t *c,
    cp_vec2_t *u,
    cp_vec2_t *v,
    cp_vec2_t *w)
{
    LOG("TRIANGLE: %s -- %s -- %s\n", coord_str(u), coord_str(v), coord_str(w));
    /* all triangles should be clockwise here (like our polygon paths) */
    cp_size3_t *t = cp_v_push0(c->tri);
    t->p[0] = cp_vec2_arr_idx(c->point_arr, u);
    t->p[1] = cp_vec2_arr_idx(c->point_arr, v);
    t->p[2] = cp_vec2_arr_idx(c->point_arr, w);
}

/**
 * Triangulate a chain.
 *
 * Back(wards) = up == clockwise.
 */
static void chain_tri(
    ctxt_t *c,
    list_t *e,
    bool back)
{
    assert(get_li(e) != NULL);

    list_t *q = e->step[back];
    if (get_li(q) == NULL) {
        return;
    }
    cp_vec2_t *p[3];
    p[0] = get_li(e)->coord;
    p[1] = get_li(q)->coord;
    unsigned del_count = 0;
    for (;;) {
        assert(e->step[back] == q);
        assert(p[0] != NULL);
        assert(p[1] != NULL);
        assert(get_li(e->step[back])->coord == p[1]);

        list_t *w = q->step[back];
        if (get_li(w) == NULL) {
            return;
        }
        if (e == w) {
            return;
        }
        assert(q != w);
        p[2] = get_li(w)->coord;

        /* collapsed edge? => delete two points */
        if (p[2] == p[0]) {
            del_count = 2;
        }

        if (del_count > 0) {
            del_count--;
        }
        else {
            /* pe--pq--pw is CCW if !back, CW if back. */
            double z = cp_vec2_left_cross3_z(p[back], p[!back], p[2]);
            if (cp_leq(z, 0)) {
                LOG("collinear: %s %s %s\n",
                    coord_str(p[0]),
                    coord_str(p[1]),
                    coord_str(p[2]));
                return;
            }

            add_triangle(c, p[!back], p[back], p[2]);
        }

        list_remove(c, q);

        q = w;
        p[1] = p[2];
    }
}

static void start_lh(
    edge_t **hp,
    edge_t **lp,
    node_t *p)
{
    edge_t *h = p->out;
    edge_t *l = p->in;
    assert(l != h);
    assert(l->dst == p);
    assert(h->src == p);
    assert(left(h)->coord == p->coord);
    assert(left(l)->coord == p->coord);
    assert(right(h)->coord == h->dst->coord);
    assert(right(l)->coord == l->src->coord);
    double z = cp_vec2_right_cross3_z(
        l->src->coord, p->coord, h->dst->coord);
    assert(!cp_equ(z, 0));
    if (z < 0) {
        CP_SWAP(&l, &h);
    }
    assert_inactive(l);
    assert_inactive(h);
    *hp = h;
    *lp = l;
}

static bool transition_proper_start(
    ctxt_t *c,
    node_t *p,
    cp_dict_ref_t *ref)
{
    LOG("PROPER START %s\n", node_str(p));

    edge_t *h, *l;
    start_lh(&h, &l, p);

    h->type = TOP;
    l->type = BOT;
    insert2_ey(c, ref, l, h);

    h->rm = l->rm = list_alloc(c, p);
    cp_list_insert(&l->list, &h->list);
    cp_list_insert(h->rm, &l->list);

    assert_active_pair(l, h);

    dump_ey(c, "proper start", p);

    return true;
}

static bool transition_bend(
    ctxt_t *c,
    node_t *p,
    edge_t *s,
    edge_t *t)
{
    LOG("BEND %s\n", node_str(p));
    LOG("s=%s, t=%s\n", edge_str(s), edge_str(t));

    assert_active(s);
    assert_inactive(t);

    list_t *lp = list_alloc(c, p);

    /* s becomes inactive, t becomes active: swap s and t */

    cp_list_swap(&t->list, &s->list);
    t->type = s->type;
    s->type = INACTIVE;

    cp_dict_swap_update_root(&c->ey, &s->node_ey, &t->node_ey);
    assert(c->ey != &s->node_ey);

    t->rm = lp;
    s->rm = NULL;

    if (t->type == TOP) {
        LOG("TOP\n");
        edge_t *l = prev(t);
        cp_list_insert(&t->list, lp);
        l->rm = lp;
        chain_tri(c, lp, false);
        assert_active_pair(l, t);
        assert_inactive(s);
        dump_ey(c, "bend top", p);
    }
    else {
        assert(t->type == BOT);
        LOG("BOT\n");
        edge_t *h = next(t);
        cp_list_insert(lp, &t->list);
        h->rm = lp;
        chain_tri(c, lp, true);
        assert_active_pair(t, h);
        assert_inactive(s);
        dump_ey(c, "bend bot", p);
    }

    return true;
}

static bool transition_proper_end(
    ctxt_t *c,
    node_t *p,
    edge_t *s,
    edge_t *t)
{
    LOG("PROPER END %s\n", node_str(p));

    assert_active_pair(t, s);

    list_t *lp = list_alloc(c, p);

    cp_list_insert(&s->list, lp);
    chain_tri(c, lp, false);

    list_remove(c, s->list.next);
    list_remove(c, s->list.next);
    cp_list_remove(&s->list);

    cp_dict_remove(&s->node_ey, &c->ey);
    cp_dict_remove(&t->node_ey, &c->ey);

    s->type = t->type = INACTIVE;
    s->rm = t->rm = NULL;

    dump_ey(c, "proper end", p);

    assert_inactive(t);
    assert_inactive(s);

    return true;
}

static bool transition_improper_start(
    ctxt_t *c,
    node_t *p,
    cp_dict_ref_t *ref,
    edge_t *s,
    edge_t *t)
{
    LOG("IMPROPER START %s (%u %s-.-%s)\n",
        node_str(p),
        pt_case(p),
        node_str(p->in->src),
        node_str(p->out->dst));
    edge_t *h, *l;
    start_lh(&h, &l, p);

    /* from bottom to top: t l (p) h s */

    assert_active_pair(t, s);

    h->type = BOT;
    l->type = TOP;
    insert2_ey(c, ref, l, h);

    /* split list, have rm in s list */
    assert(s->rm == t->rm);
    node_t *rmn = get_li(s->rm);

    cp_list_split(s->rm, &s->list);

    bool same = (p->coord == s->rm->node->coord);
    if (same) {
        LOG("SAME POINT\n");
    }

    if (!same) {
        list_t *lph = list_alloc(c, p);
        cp_list_insert(lph, &s->list);
        s->rm = lph;
    }
    h->rm = s->rm;
    cp_list_insert(&h->list, &s->list);

    chain_tri(c, s->rm, true);

    /* make a copy of the list cell around t/s->rm */
    list_t *rml = list_alloc(c, rmn);
    cp_list_insert(&t->list, rml);
    if (!same) {
        list_t *lpl = list_alloc(c, p);
        cp_list_insert(&t->list, lpl);
        t->rm = lpl;
    }
    l->rm = t->rm;
    cp_list_insert(&t->list, &l->list);

    chain_tri(c, t->rm, false);

    dump_ey(c, "improper start", p);

    assert_active_pair(h, s);
    assert_active_pair(t, l);
    return true;
}

static bool transition_improper_end(
    ctxt_t *c,
    node_t *p,
    edge_t *s,
    edge_t *t)
{
    LOG("IMPROPER END %s (%u %s-.-%s)\n",
        node_str(p),
        pt_case(p),
        node_str(p->in->src),
        node_str(p->out->dst));

    edge_t *l = prev(t);
    edge_t *h = next(s);

    assert_active_pair(s, h);
    assert_active_pair(l, t);

    list_t *lp = list_alloc(c, p);

    cp_list_insert(lp, &s->list);
    chain_tri(c, lp, true);
    cp_list_remove(lp);
    cp_list_remove(&s->list);

    cp_list_insert(&t->list, lp);
    chain_tri(c, lp, false);
    cp_list_remove(&t->list);

    cp_list_insert(&l->list, &h->list);
    l->rm = h->rm = lp;

    cp_dict_remove(&s->node_ey, &c->ey);
    cp_dict_remove(&t->node_ey, &c->ey);

    dump_ey(c, "improper end", p);

    assert_active_pair(l, h);

    return true;
}

static bool transition(
    ctxt_t *c,
    node_t *p)
{
    cp_dict_ref_t ref;
    edge_t *s, *t;
    switch (find(&ref, &s, &t, c, p)) {
    case CASE_START:
        LOG("START: ");
        if ((s == NULL) || (s->type == BOT)) {
            assert((t == NULL) || (t->type == TOP));
            return transition_proper_start(c, p, &ref);
        }
        else {
            assert(s->type == TOP);
            assert(t->type == BOT);
            return transition_improper_start(c, p, &ref, s, t);
        }

    case CASE_BEND:
        LOG("BEND: ");
        assert(s != NULL);
        assert(t != NULL);
        return transition_bend(c, p, s, t);

    case CASE_END:
        LOG("END: ");
        assert(s != NULL);
        assert(t != NULL);
        if (s->type == TOP) {
            assert(t->type == BOT);
            return transition_proper_end(c, p, s, t);
        }
        else {
            assert(s->type == BOT);
            assert(t->type == TOP);
            return transition_improper_end(c, p, s, t);
        }
    }

    return true;
}

extern bool cp_csg2_tri_set(
    cp_err_t *t,
    cp_vec2_arr_ref_t *point_arr,
    cp_v_size3_t *tri,
    node_t *node,
    size_t n)
{
    /* Algorithm: Fast Triangulation, Hertel & Mehlhorn 1983
     * Currently non-optimised version, runtime O(n log n).
     *
     * What they fail to mention in the paper is that some nodes need
     * to be in two lists at the same time, which requires dirty
     * tricks.  It is solved here by the list_t structure.
     *
     * A simplification used here neglects co_p special cases, which
     * is achieved by an imaginary minimal rotation around z by using
     * lexicographic order on (x,y) coordinates.  This also means that
     * the algorith handles subsequent colinear edges correctly without
     * needing to take special care.
     *
     * All auxilary structures are on the stack and allocated at
     * the very beginning -- no dynamic allocation for temporaries
     * is used.  The algorithm allocates only the new triangles on the
     * heap as they are discovered.
     */

    /* allocate list cells */
    size_t list_size = n * 2;
    list_t list_data[list_size];
    CP_ZERO(&list_data);

    /* init context */
    ctxt_t c = {
        .point_arr = point_arr,
        .tri = tri,
        .t = t,
        .nx = NULL,
        .ey = NULL,
        .list_data = list_data,
        .list_size = list_size,
        .list_end = 0,
    };
    cp_list_init(&c.list_free);

    /* connect nodes */
    for (cp_size_each(i, n)) {
        node_t *p = &node[i];
        p->out->src = p->in->dst = p;
    }

    /* insert nodes into set, ordered by coord_cmp() */
    for (cp_size_each(i, n)) {
        node_t *p = &node[i];
        cp_list_init(&p->out->list);
        cp_dict_t *dup __unused =
            cp_dict_insert(&p->node_nx, &c.nx, cmp_nx, NULL, 0);
        if (dup != NULL) {
            cp_vchar_printf(&t->msg, "Duplicate point in polygon path.\n");
            t->loc = p->loc;
            return false;
        }
    }

    /* traverse in lexicographic order, maintaining the Y structure 'c.ey'. */
    size_t i = 0;
    for (cp_dict_each(_p, c.nx)) {
        LOG("\nPOINT %"_Pz"u %"_Pz"u: %s\n", i, n, node_str(get_nx(_p)));
        if (!transition(&c, get_nx(_p))) {
            return false;
        }
        i++;
    }

    return true;
}

extern bool cp_csg2_tri_path(
    cp_err_t *t,
    cp_csg2_poly_t *g,
    cp_csg2_path_t *s)
{
    size_t n = s->point_idx.size;

    /* allocate */
    node_t node[n];
    CP_ZERO(&node);
    edge_t edge[n];
    CP_ZERO(&edge);

    /* Init nodes and edges and insert into X structure 'px'
     * To do multiple paths in one go, this would need to be
     * restructured, because the storage for the edges would not
     * be so simply enumerable.
     */
    for (cp_v_each(i, &s->point_idx)) {
        node_t *p = &node[i];
        cp_vec2_loc_t *v= cp_csg2_path_nth(g, s, i);
        p->loc = v->loc;
        p->coord = &v->coord;
        p->out = &edge[i];
        p->in = &edge[cp_wrap_sub1(i,n)];
    }

    return cp_csg2_tri_set(t, &CP_VEC2_ARR_REF(&g->point, coord), &g->triangle, node, n);
}

extern bool cp_csg2_tri_poly(
    cp_err_t *t,
    cp_csg2_poly_t *g)
{
    /* count edges */
    size_t n = 0;
    for (cp_v_each(i, &g->path)) {
        cp_csg2_path_t *s = &g->path.data[i];
        n += s->point_idx.size;
    }

    /* allocate */
    node_t node[n];
    CP_ZERO(&node);
    edge_t edge[n];
    CP_ZERO(&edge);

    /* make edges */
    size_t m = g->path.size;
    size_t o = 0;
    for (cp_v_each(i, &g->path)) {
        cp_csg2_path_t *s = &g->path.data[i];
        size_t k = s->point_idx.size;
        for (cp_v_each(j, &s->point_idx)) {
            node_t *p = &node[o + j];
            cp_vec2_loc_t *v= cp_csg2_path_nth(g, s, j);
            p->loc = v->loc;
            p->coord = &v->coord;
            p->out = &edge[o + j];
            p->in = &edge[o + cp_wrap_sub1(j,k)];
        }
        o += k;
    }

    /* Expect n-2 triangles for a single polygon without holes.  Each
     * inner polygon will add 2 triangles, so we max. number of triangles
     * is n-2 + 2*(m - 1). */
    size_t tri_cnt = (n - 2) + 2*(m - 1);
    cp_v_clear(&g->triangle, tri_cnt);

    /* run the triangulation algorithm */
    if (!cp_csg2_tri_set(t, &CP_VEC2_ARR_REF(&g->point, coord), &g->triangle, node, n)) {
        return false;
    }
    assert(g->triangle.size  <= tri_cnt);
    return true;
}

static bool csg2_tri_circle(
    cp_err_t *t,
    cp_csg2_circle_t *r)
{
    (void)t;
    (void)r;
    return true;
}

static bool csg2_tri_v_csg2(
    cp_err_t *t,
    cp_v_csg2_p_t *r);

static bool csg2_tri_layer(
    cp_err_t *t,
    cp_csg2_layer_t *r)
{
    return csg2_tri_v_csg2(t, &r->root.add);
}

static bool csg2_tri_stack(
    cp_err_t *t,
    cp_csg2_stack_t *r)
{
    for (cp_v_each(i, &r->layer)) {
        if (!csg2_tri_layer(t, &r->layer.data[i])) {
            return false;
        }
    }
    return true;
}

static bool csg2_tri_sub(
    cp_err_t *t,
    cp_csg2_sub_t *r)
{
    return
        csg2_tri_v_csg2(t, &r->add.add) &&
        csg2_tri_v_csg2(t, &r->sub.add);
}

static bool csg2_tri_add(
    cp_err_t *t,
    cp_csg2_add_t *r)
{
    return csg2_tri_v_csg2(t, &r->add);
}

static bool csg2_tri_cut(
    cp_err_t *t,
    cp_csg2_cut_t *r)
{
    for (cp_v_each(i, &r->cut)) {
        if (!csg2_tri_v_csg2(t, &r->cut.data[i]->add)) {
            return false;
        }
    }
    return true;
}

static bool csg2_tri_csg2(
    cp_err_t *t,
    cp_csg2_t *r)
{
    switch (r->type) {
    case CP_CSG2_CIRCLE:
        return csg2_tri_circle(t, &r->circle);

    case CP_CSG2_POLY:
        return cp_csg2_tri_poly(t, &r->poly);

    case CP_CSG2_STACK:
        return csg2_tri_stack(t, &r->stack);

    case CP_CSG2_ADD:
        return csg2_tri_add(t, &r->add);

    case CP_CSG2_SUB:
        return csg2_tri_sub(t, &r->sub);

    case CP_CSG2_CUT:
        return csg2_tri_cut(t, &r->cut);
    }

    CP_DIE("2D object type: %#x", r->type);
}

static bool csg2_tri_v_csg2(
    cp_err_t *t,
    cp_v_csg2_p_t *r)
{
    for (cp_v_each(i, r)) {
        if (!csg2_tri_csg2(t, r->data[i])) {
            return false;
        }
    }
    return true;
}

extern bool cp_csg2_tri_tree(
    cp_err_t *t,
    cp_csg2_tree_t *r)
{
    if (r->root == NULL) {
        return true;
    }
    return csg2_tri_csg2(t, r->root);
}
