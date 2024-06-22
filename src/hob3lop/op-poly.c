/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

/*
 * This is implemented using Hertel&Mehlhorn's algorithm, i.e.,
 * running another plane sweep.  It is assumed that no
 * intersections or overlap exists anymore.  But multi-ended
 * points cannot be avoided.
 *
 * This uses a modified algorithm compared to Hertel&Mehlhorn:
 * no ears are clipped to form triangle; the distinction
 * between a proper vs. improper start/end are not made:
 * both are handled the same, i.e., there are loops for both.
 * But the polylines are run backwards for inner paths.
 */

#include <hob3lop/op-poly.h>
#include "op-sweep-internal.h"

/**
 * Whether to use a heuristics to try to improve the path generation.
 */
#define DELAY 0

static void cq_sweep_poly_append(
    cq_csg2_poly_t *r,
    size_t vi,
    unsigned path_idx_offs,
    unsigned point_idx)
{
    cp_csg2_path_t *v = &r->path.data[vi];
    assert(point_idx < r->point.size);
    size_t path_idx = r->point.data[point_idx].aux1 - path_idx_offs;
    if (path_idx >= v->point_idx.size) {
        r->point.data[point_idx].aux1 = (unsigned)(v->point_idx.size + path_idx_offs);
        cp_v_push(&v->point_idx, point_idx);
        return;
    }

    assert(v->point_idx.data[path_idx] == point_idx);

    /* there's a loop, cut it off.  It runs from path_idx .. size-1 */

    cp_csg2_path_t *w = cp_v_push0(&r->path);
    v = &r->path.data[vi]; /* push0 invalidates v, so reload it */

    size_t len = v->point_idx.size - path_idx;
    cp_v_init0(&w->point_idx, len);
    cp_v_copy(&w->point_idx, 0, &v->point_idx, path_idx, len);
    cp_v_set_size(&v->point_idx, path_idx + 1); /* keep the vertex in the old path, too */

    /* clear the positions that are not in array anymore */
    for (cp_v_each(i, &w->point_idx, 1)) {
        size_t idx = w->point_idx.data[i];
        r->point.data[idx].aux1 = path_idx_offs - 1;
    }

    assert(w->point_idx.data[0] == point_idx);
    assert(v->point_idx.data[path_idx] == point_idx);
}

/**
 * Use the output of cq_sweep_intersect() or cq_sweep_reduce()
 * and construct a correct polygon (with edge order and all).
 *
 * Typically, using this function means to do something like:
 *
 *    cq_sweep_t *s = cq_sweep_new(pool, 0);
 *    cq_sweep_add_...(...); // add edges
 *    cq_sweep_intersect(s); // find intersections
 *    cq_sweep_reduce(s, comb, comb_size); // to boolean operation(s)
 *    cp_csg2_poly_t *r = ...
 *    bool ok = cq_sweep_poly(s, r);
 *    cq_sweep_delete(s);
 *
 * `r` must be empty when this is invoked, as this does not
 * compare the `point` array for equal points.
 *
 * This uses a simplified algorithm based on Hertel&Mehlhorn's
 * triangulation plane sweep to construct the polygon paths.
 * I.e., instead of producing triangles, it just outputs the
 * paths.
 *
 * This returns true on success, or false if there were open
 * polygons or if a line crossing the given arrangement crosses
 * an odd number of segments (i.e., there is simple definition
 * of 'inside').  If this happens, the arrangement needs to be
 * repaired.
 *
 * The result is stored in `r`, in the `point` and `path`
 * members.
 *
 * This works per vertex so that it can resolve suboptimal
 * ordering around a vertex
 */
extern bool cq_sweep_poly(
    cp_err_t *err,
    cq_sweep_t *data,
    cq_csg2_poly_t *r)
{
    if (data == NULL) {
        return true;
    }

    data->phase = POLY;

    /* to make 'aux' value unique within each path */
    unsigned path_idx_offs = 0;
    vertex_t *s = NULL;
    cq_vec2_t last_pt;
    unsigned idx;
    point_idx_init(&last_pt, &idx);
    for (cp_dict_each_robust(o, data->result)) {
        vertex_t *t = agenda_get_vertex(o);

        /* set point index */
        t->point_idx = point_idx_get(&last_pt, &idx, r, &t->vec2);

        /* the main loop operates on pairs of edges */
        if (s == NULL) {
            s = t;
            continue;
        }

        if (!cq_vec2_eq(&s->vec2, &t->vec2)) {
            err_msg(err, data->loc, "Found hole in polygon");
            goto sweep_return_false;
        }

        edge_t *es = edge_of(s);
        edge_t *et = edge_of(t);
        assert(es != et);

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
            state_edge_replace(data, es, et);
            et->back = es->back;
            list_edge_init(data, et);
            list_edge_insert(data, es, et);

            PSPR("0 0 0 setrgbcolor %g %g moveto (BEND %s) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++),
                et->back ? "back" : "fore");
        }
        else
        if (s->side == LEFT) {
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

            /* make ring (at this point, it does not matter which way */
            list_edge_init2(data, es, et);

            PSPR("0 0 0 setrgbcolor %g %g moveto (START %s) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++),
                et->back   ? "back"   : "fore");
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

                PSPR("0 0 0 setrgbcolor %g %g moveto (END inner (pio=%u)) show\n",
                    cq_ps_left(), cq_ps_line_y(data->ps_line++), path_idx_offs);

                /* output path */
                path_idx_offs += 3;
                edge_t *e = es->back ? es : et;
                assert(e->back);
                size_t vi = r->path.size;
                (void)cp_v_push0(&r->path);
                unsigned first = e->rigt.point_idx;
                cq_sweep_poly_append(r, vi, path_idx_offs, first);
                cq_sweep_poly_append(r, vi, path_idx_offs, e->left.point_idx);
                unsigned cnt = 0;
                edge_t *n = list_edge_next(e);
                assert(e != n);
                for (;;) {
                    edge_t *nn = list_edge_next(n);
                    unsigned pi = list_edge_get_end(n,1)->point_idx;
                    if (nn == e) {
                        assert(pi == first);
                        break;
                    }
                    cq_sweep_poly_append(r, vi, path_idx_offs, pi);
                    cnt++;
                    n = nn;
                }
                path_idx_offs += cnt;
            }
            else {
                state_edge_remove(data, es);
                state_edge_remove(data, et);

                /* this is the end of an 'arm' */
                list_edge_merge(data, et, es);

                PSPR("0 0 0 setrgbcolor %g %g moveto (END connect) show\n",
                    cq_ps_left(), cq_ps_line_y(data->ps_line++));
            }
        }

        cq_sweep_trace_end_page(data);

        s = NULL;
    }

    assert(data->agenda_vertex == NULL);

    cq_sweep_trace_begin_page(data, NULL, NULL, NULL, r);
    cq_sweep_trace_end_page(data);

    return true;

    /* the edges in the state need to be removed so the
     * next phase (e.g., triangulate) does not run into
     * misinitialised edges. */
sweep_return_false:
    while (data->state != NULL) {
        state_edge_remove(data, tree_get_edge(data->state));
    }
    return false;
}
