/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* Note that this algorithm (without the triangulation part)
 * could also be used to generate a disassembly into a set of
 * polygons that are non-overlapping, without negative parts.
 * These polygons may, however, share edges.  Also, single
 * paths may have the same vertex more than twice (this
 * algorithm does not disasemble paths).
 */

#include <hob3lop/op-trianglify.h>
#include "op-sweep-internal.h"

/**
 * Removes/disables a vertex.  If both ends of an edge end up disabled,
 * then remove the edge from the list.
 */
static inline void vertex_remove(
    data_t *data CP_UNUSED,
    vertex_t *v)
{
    edge_t *e = edge_of(v);
    e->v_dis[v->side] = true;
    if (e->v_dis[!v->side]) {
        list_edge_remove(e);
        if (!result_is_member(data, &e->left)) {
            edge_delete(data, e);
        }
    }
}

/**
 * Removes a vertex and its possible equal buddy, i.e., this really
 * removes the vertex from the list.
 */
static inline void vertex_remove2(
    data_t *data,
    vertex_t *v)
{
    vertex_t *w = vertex_eq_buddy(v);
    vertex_remove(data, v);
    if (w != NULL) {
        vertex_remove(data, w);
    }
}

static inline edge_t *tri_edge_new(
    data_t *data,
    vertex_t *left,
    vertex_t *rigt,
    bool back,
    bool inner)
{
    edge_t *e = edge_new(data, &left->vec2, &rigt->vec2, 0, false);
    e->back = back;
    e->inner = inner;
    list_edge_init(data, e);
    e->left.point_idx = left->point_idx;
    e->rigt.point_idx = rigt->point_idx;
    assert(vertex_valid(&e->left));
    assert(vertex_valid(&e->rigt));
    assert(!e->v_dis[0]);
    assert(!e->v_dis[1]);
    return e;
}

static bool edge_is_outline(
    vertex_t *p,
    vertex_t *q)
{
    edge_t *e = edge_of(p);
    if (e == edge_of(q)) {
        return !e->inner;
    }
    vertex_t *w = vertex_eq_buddy(q);
    if (w && (e == edge_of(w))) {
        return !e->inner;
    }

    p = vertex_eq_buddy(p);
    if (p) {
        e = edge_of(p);
        if (e == edge_of(q)) {
            return !e->inner;
        }
        if (w && (e == edge_of(w))) {
            return !e->inner;
        }
    }

    return false;

#if 0
    /* this does not work because the 'LEFT' and 'RIGT' stuff does not reflect
     * how the edges are oriented in the list */

    /* try via p */
    if (p->side == LEFT) {
        edge_t *e = edge_of(p);
        vertex_t *w = q;
        if (q->side == LEFT) {
            w = vertex_eq_buddy(w);
        }
        if (w && (edge_of(w) == e)) {
            return !e->inner;
        }
    }
    else {
        cq_assert(p->side == RIGT);
        edge_t *e = edge_of(p);
        vertex_t *w = q;
        if (q->side == RIGT) {
            w = vertex_eq_buddy(q);
        }
        if (w && (edge_of(w) == e)) {
            return !e->inner;
        }
    }

    /* try via q */
    CP_SWAP(&p, &q);
    if (p->side == LEFT) {
        edge_t *e = edge_of(p);
        vertex_t *w = q;
        if (q->side == LEFT) {
            w = vertex_eq_buddy(q);
        }
        if (w && (edge_of(w) == e)) {
            return !e->inner;
        }
    }
    else {
        cq_assert(p->side == RIGT);
        edge_t *e = edge_of(p);
        vertex_t *w = q;
        if (q->side == RIGT) {
            w = vertex_eq_buddy(q);
        }
        if (w && (edge_of(w) == e)) {
            return !e->inner;
        }
    }

    return false;
#endif
}

static inline void flush_triangle_(
    data_t *data,
    cq_csg2_poly_t *r,
    vertex_t *p,
    bool back)
{
start_again:;
    vertex_t *q = vertex_list_step_neq(p, back);
    if (!vertex_valid(q)) {
        return;
    }
    for (;;) {
        vertex_t *w = vertex_list_step_neq(q, back);
        if (!vertex_valid(w)) {
            return;
        }
        cq_dimw_t i = cq_vec2_right_cross3_z(&p->vec2, &q->vec2, &w->vec2);
        if (back) {
            i = -i;
        }
        if (i < 0) {
            return;
        }

        if (i == 0) {
            assert(p->point_idx != q->point_idx);
            assert(w->point_idx != q->point_idx);
            if (p->point_idx == w->point_idx) {
                PSPR("0 0 0 setrgbcolor %g %g moveto (remove A) show\n",
                    cq_ps_left(), cq_ps_line_y(data->ps_line++));
                vertex_remove2(data, q);
                q = vertex_list_step(w, back);
                if (q == p) {
                    /* this was the last two-point 'triangle' */
                    return;
                }
                PSPR("0 0 0 setrgbcolor %g %g moveto (remove B) show\n",
                    cq_ps_left(), cq_ps_line_y(data->ps_line++));
                vertex_remove2(data, w);
                goto start_again;
            }
            /* FIXME:
             * Is this always OK or do we need the 'remove2' below in some cases?
             * This return was added to avoid non-2 manifold STL exports when the
             * triangulation uses less vertices. corner17 and err_test20 show the
             * possible situations.
             * Maybe in some degenerate situations, we may need the remove2 below
             * to work properly?  No problem was found yet, though.
             */
            return;
        }
        else {
            assert(p->point_idx != q->point_idx);
            assert(q->point_idx != w->point_idx);
            assert(p->point_idx != w->point_idx);
            cp_csg2_tri_t tri = {};
            if (back) {
                tri.p[0] = q->point_idx;
                tri.p[1] = p->point_idx;
                tri.p[2] = w->point_idx;
                if (edge_is_outline(q, p)) {
                    tri.flags |= CP_CSG2_TRI_OUTLINE_01;
                }
                if (edge_is_outline(p, w)) {
                    tri.flags |= CP_CSG2_TRI_OUTLINE_12;
                }
                if (edge_is_outline(w, q)) {
                    tri.flags |= CP_CSG2_TRI_OUTLINE_20;
                }
            }
            else {
                tri.p[0] = p->point_idx;
                tri.p[1] = q->point_idx;
                tri.p[2] = w->point_idx;
                if (edge_is_outline(p, q)) {
                    tri.flags |= CP_CSG2_TRI_OUTLINE_01;
                }
                if (edge_is_outline(q, w)) {
                    tri.flags |= CP_CSG2_TRI_OUTLINE_12;
                }
                if (edge_is_outline(w, p)) {
                    tri.flags |= CP_CSG2_TRI_OUTLINE_20;
                }
            }
            cp_v_push(&r->tri, tri);
        }
        vertex_remove2(data, q);
        q = w;
    }
}

static void flush_triangle_fore(
    data_t *data,
    cq_csg2_poly_t *r,
    vertex_t *p)
{
    return flush_triangle_(data, r, p, false);
}

static void flush_triangle_back(
    data_t *data,
    cq_csg2_poly_t *r,
    vertex_t *p)
{
    return flush_triangle_(data, r, p, true);
}

static void flush_triangle_both(
    data_t *data,
    cq_csg2_poly_t *r,
    vertex_t *s,
    vertex_t *t)
{
    flush_triangle_back(data, r, s);
    flush_triangle_fore(data, r, t);
}

/**
 * Use the output of cq_sweep_intersect() or cq_sweep_reduce()
 * to find a triangulation.
 *
 * Typically, using this function means to do something like:
 *
 *    cq_sweep_t *s = cq_sweep_new(pool, 0);
 *    cq_sweep_add_...(...); // add edges
 *    cq_sweep_intersect(s); // find intersections
 *    cq_sweep_reduce(s, comb, comb_size); // to boolean operation(s)
 *    cp_csg2_poly_t *r = ...
 *    bool ok = cq_sweep_trianglify(s, r);
 *    cq_sweep_delete(s);
 *
 * `r` must be empty when this is invoked, as this does not
 * compare the `point` array for equal points.
 *
 * The result is stored in `r`, in the `point` and `triangle`
 * members.
 */
extern bool cq_sweep_trianglify(
    cp_err_t *err,
    cq_sweep_t *data,
    cq_csg2_poly_t *r)
{
    if (data == NULL) {
        return true;
    }

    data->phase = TRIANGLE;

    /* the rings this maintains all run bottom to top around the interior of
     * the polygon (this is reversed from the paper, because if we use the
     * same algo to produce clock-wise polygons, then we need it this way). */

    /* to make 'aux' value unique within each path */
    vertex_t *s = NULL;
    cq_vec2_t last_pt = {};
    for (cp_dict_each_robust(o, data->result)) {
        vertex_t *t = agenda_get_vertex(o);

        /* set point index */
        if ((r->point.size == 0) || !cq_vec2_eq(&last_pt, &t->vec2)) {
            last_pt = t->vec2;
            cp_v_push(&r->point, ((cp_vec2_loc_t){ .coord = cq_export_vec2(&last_pt) }));
        }
        assert(r->point.size > 0);
        assert(r->point.size <= ~(__typeof__(t->point_idx))0);
        t->point_idx = (r->point.size - 1) & ~(__typeof__(t->point_idx))0;

        /* the main loop operates on pairs of edges */
        if (s == NULL) {
            s = t;
            continue;
        }

        if (!cq_vec2_eq(&s->vec2, &t->vec2)) {
            err_msg(err, data->loc, "Found hole in polygon");
            return false;
        }

        edge_t *es = edge_of(s);
        edge_t *et = edge_of(t);
        assert((t->side == LEFT) == !state_edge_is_member(data, et));
        assert((s->side == LEFT) == !state_edge_is_member(data, es));

        cq_sweep_trace_begin_page(data, t, NULL, NULL, r);
        PSPR("0 0 0 setrgbcolor %g %g moveto (s= %s %d %d .. %d %d) show\n",
            cq_ps_left(), cq_ps_line_y(data->ps_line++),
            s->side == LEFT ? "LEFT" : "RIGT",
            s->x, s->y,
            other_end(s)->x, other_end(s)->y);

        if (s->side != t->side) {
            /* BEND */
            cq_assert(s->side == RIGT); /* ensure the order is right: RIGT before LEFT */
            cq_assert(t->side == LEFT);
            /* This uses 'replace' in O(1), not 'remove' + 'insert' for O(log n)
             * => total runtime is O(n + s log s) (n = #edges, s = #starts) */
            other_end(t)->point_idx = -1U;

            state_edge_replace(data, es, et);
            et->back = es->back;
            list_edge_init(data, et);

            PSPR("0 0 0 setrgbcolor %g %g moveto (BEND %s) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++),
                et->back ? "back" : "fore");

            /* chain and reset left most vertices */
            et->rigt_most = t;
            if (et->back) {
                /* et is a top edge */
                list_edge_chain(data, et, es);
                assert(es == list_edge_next(et));
                list_edge_prev(et)->rigt_most = s;
                flush_triangle_fore(data, r, t);
            }
            else {
                /* es is a bottom edge */
                list_edge_chain(data, es, et);
                assert(es == list_edge_prev(et));
                list_edge_next(et)->rigt_most = s;
                flush_triangle_back(data, r, t);
            }
        }
        else
        if (s->side == LEFT) {
            cq_assert(t->side == LEFT);
            other_end(s)->point_idx = -1U;
            other_end(t)->point_idx = -1U;

            /* START: s is bottom, t is top (see cq_sweep_result_vertex_cmp) */
            state_edge_insert_successfully(data, s);
            state_edge_insert_successfully(data, t);
            assert(tree_edge_next(es) == et); /* should be adjacent in state */

            /* set order */
            edge_t *er = tree_edge_prev(es);
            es->back = true; /* at the bottom, 'back' == clock wise */
            if (er != NULL) {
                es->back = !er->back;
            }
            et->back = !es->back;

            PSPR("0 0 0 setrgbcolor %g %g moveto (START %s) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++),
                es->back  ? "proper"   : "improper");

            /* make a new loop */
            list_edge_init2(data, es, et);

            if (es->back) {
                /* right of s--t is inside: proper start */
                es->rigt_most = s;
                et->rigt_most = t;
            }
            else {
                /* left of s--t is inside: improper start */
                /* rewire: split the surrounding loop at the left-most
                 * vertex, then route the upper loop through et, and the
                 * lower loop through es. */

                /* a few checks first */
                assert(et->back);
                assert(er != NULL);
                assert(er->rigt_most != NULL);
                vertex_t *lms = er->rigt_most;
                edge_t *elms = edge_of(lms);
                edge_t *elmt = list_edge_next(elms);
                vertex_t *lmt CP_UNUSED = vertex_eq_buddy(lms);
                assert(lmt != NULL);
                assert(elmt == edge_of(lmt));
                assert(cq_vec2_eq(&lmt->vec2, &lms->vec2));

                /* * [(r<-)u<-...<-lmt<-lms<-...<-r(<-u)]    [(s<-)t<-s(<-t)]  */
                list_edge_chain(data, et, elmt);
                /*   [(r<-)u<-...<-lmt<-t<-s<-lms<-...<-r(<-u)]  */
                assert(vertex_list_next(t) == lmt);

                list_edge_chain(data, es, er);
                /*   [(t<-)u<-...<-lmt<-t(<-u)]  [(r<-)s<-lms<-...<-r(<-s)]  */
                assert(vertex_list_prev(s) == lms);

                vertex_t *sr = lms;
                vertex_t *tr = lmt;
                if (lms->point_idx != s->point_idx) {
                    /* if lms != s, we introduce new edges (or maybe half
                     * edges -- they overlap), otherwise it is getting too
                     * complicated with the rigt_most vertices. */
                    edge_t *ht = tri_edge_new(data, lms, s, true, true);
                    list_edge_chain(data, et, ht);
                    /*   [(t<-)u<-...<-lmt<-ht<-t(<-u)] */
                    assert(vertex_list_prev(&ht->rigt) == t);
                    assert(vertex_list_next(&ht->left) == lmt);

                    edge_t *hs = tri_edge_new(data, lms, s, false, true);
                    list_edge_chain(data, hs, es);
                    /*   [(r<-)s<-hs<-lms<-...<-r(<-s)]  */
                    assert(vertex_list_next(&hs->rigt) == s);
                    assert(vertex_list_prev(&hs->left) == lms);
                    sr = &hs->rigt;
                    tr = &ht->rigt;
                }

                assert(er->back);
                assert(et->back);
                er->rigt_most = sr;
                et->rigt_most = t;
                es->rigt_most = s;
                list_edge_prev(et)->rigt_most = tr;

                flush_triangle_both(data, r, s, t);
            }
        }
        else {
            /* END: t is bottom, s is top (see cq_sweep_result_vertex_cmp) */
            assert(s->side == RIGT);
            assert(es->back == !et->back);
            assert(tree_edge_next(et) == es);
            if (et->list.edge[et->back] == &es->list) {
                state_edge_remove(data, es);
                state_edge_remove(data, et);

                assert(es->list.edge[es->back] == &et->list);
                /* a poly is finally closed (e.g., an 'inner diamond' or the
                 * main polygon is finalised => output. */

                PSPR("0 0 0 setrgbcolor %g %g moveto (END proper) show\n",
                    cq_ps_left(), cq_ps_line_y(data->ps_line++));

                assert(s->point_idx == t->point_idx);
                flush_triangle_both(data, r, s, t);
            }
            else {
                state_edge_remove(data, es);
                state_edge_remove(data, et);

                PSPR("0 0 0 setrgbcolor %g %g moveto (END improper) show\n",
                    cq_ps_left(), cq_ps_line_y(data->ps_line++));
                /* adjust rigt_most */
                edge_t *eh = list_edge_prev(es);
                edge_t *el = list_edge_next(et);
                eh->rigt_most = s;
                el->rigt_most = t;

                assert(!et->back);
                assert(es->back);
                flush_triangle_both(data, r, t, s);

                /* connect the two loops */
                list_edge_merge(data, et, es);

                /* remove a collapsed triangle that may occur if there is an
                 * improper END followed by a BEND in the same vertex (see
                 * trifail1.fig, where three polies get corrupted otherwise)
                 */
            }
        }

        cq_sweep_trace_end_page(data);

        s = NULL;
    }

    assert(data->agenda_vertex == NULL);

    /* reverse the polygon paths so they are roughly subtractive */
    cp_v_reverse(&r->path, 0, -1UL);

    cq_sweep_trace_begin_page(data, NULL, NULL, NULL, r);
    cq_sweep_trace_end_page(data);

    return true;
}
