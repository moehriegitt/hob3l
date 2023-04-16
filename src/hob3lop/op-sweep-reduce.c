/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * The algorithm used here is a plane sweep on the arrangement
 * produced by the main sweep algorithm, so the algorithm here handles
 * no degeneracies, no overlap, and no intersections.  The plane
 * sweep, therefore, has only start and end events.  The sweep line is
 * used to track the in/out information of the polygons, and this is
 * done for a set of polygons, each occupying one bit in a 'member'
 * mask (to which polygons the edge belongs) and a 'below' mask (which
 * polygon's inside is below the edge).  The xor of member and below
 * is the 'above' information: which polygon's inside is above the
 * edge.  The scan line is used to set below based on the edge info
 * below: when an edge inserted at the bottom, there is no lower edge,
 * i.e., it is assumed that this is a transition from outside to
 * inside for all polygons, i.e., the 'below' mask is 0 (below is outside
 * of any polygon).  Otherwise, the edge's 'below' info is the predecessor
 * edge's 'above' info.
 *
 * At the end, the 'comb' table is used to evaluate the boolean function:
 * the boolean function is evaluated for 'below' and for 'above' and if
 * the boolean value is different, then the edge is in the final polygon.
 */

#include "op-sweep-internal.h"

static inline bool comb_eval(
    cp_bool_bitmap_t const *comb,
    size_t comb_size CP_UNUSED,
    size_t i)
{
    assert(i < comb_size);
    return cp_bool_bitmap_get(comb, i);
}

extern void cq_sweep_reduce(
    cq_sweep_t *data,
    cp_bool_bitmap_t const *comb,
    size_t comb_size)
{
    data->phase = REDUCE;

    for (cp_dict_each_robust(o, data->result)) {
        vertex_t *v = agenda_get_vertex(o);
        cq_sweep_trace_begin_page(data, v, NULL, NULL, NULL);

        edge_t *e = edge_of(v);

        if (v->side == LEFT) {
            edge_t *othr CP_UNUSED = state_edge_insert(data, v);
            assert(othr == NULL);
            e->below = 0;
            edge_t *p = tree_edge_prev(e);
            if (p != NULL) {
                e->below = p->below ^ p->member;
            }
            size_t above = e->below ^ e->member;
            e->keep = comb_eval(comb, comb_size, e->below) != comb_eval(comb, comb_size, above);

            PSPR("0 0 0 setrgbcolor %g %g moveto (member 0x%zx, below 0x%zx, keep %d) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++),
                e->member, e->below, e->keep);

            /* delete edge from result if not kept (start node) */
            if (!e->keep) {
                result_remove(data, &e->left);
            }
        }
        else {
            assert(v->side == RIGT);
            state_edge_remove(data, e);

            /* delete edge from result if not kept (end node) */
            if (!e->keep) {
                result_remove(data, v);
            }

            /* clear edge data for next phases */
            e->clear = (edge_clear_t){};
        }
        cq_sweep_trace_end_page(data);
    }

    cq_sweep_trace_begin_page(data, NULL, NULL, NULL, NULL);
    cq_sweep_trace_end_page(data);
}
