/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include "op-sweep-internal.h"

#if CQ_TRACE

extern void cq_sweep_trace_end_page(
    data_t *data)
{
    data->ps_line = 0;
    cq_ps_page_end();
}

extern void cq_sweep_trace_begin_page(
    data_t *data,
    vertex_t *i,
    xing_t *q,
    bundle_t *b,
    cq_csg2_poly_t *r)
{
    static double const ll = 10;

    data->ps_line = 0;
    cq_ps_page_begin();

    PSPR("0 0 0 setrgbcolor\n");
    cq_vec2if_t sc = CQ_VEC2IF_NAN;
    if (i != NULL) {
        sc = cq_vec2if_from_vec2(&i->vec2);
    }
    if (q != NULL) {
        sc = q->vec2if;
    }
    if (sc.x.d > 0) {
         /* the scan line */
         PSPR("1 setlinewidth\n");
         PSPR("0.6 0.6 0 setrgbcolor\n");
         double x = cq_f_from_dimif(&sc.x);
         cq_ps_line(x, CQ_DIM_MIN, x, CQ_DIM_MAX);
         double y = cq_f_from_dimif(&sc.y);
         cq_ps_line(CQ_DIM_MIN, y, CQ_DIM_MAX, y);
    }

    /* show filled polygon: (a) the work list */
    if (data->phase == POLY) {
        PSPR("0.8 0.8 1 setrgbcolor\n");
        PSPR("newpath\n");
        for (cp_dict_each(d, data->state)) {
            edge_t *e = tree_get_edge(d);
            if (!e->back) {
                continue;
            }

            PSPR("%g %g moveto\n", cq_ps_coord_x(e->rigt.x), cq_ps_coord_y(e->rigt.y));
            PSPR("%g %g lineto\n", cq_ps_coord_x(e->left.x), cq_ps_coord_y(e->left.y));
            for(edge_t *n = list_get_edge(e->list.next);
                e != n;
                n = list_get_edge(n->list.next))
            {
                PSPR("%g %g lineto\n",
                    cq_ps_coord_x(list_edge_get_end(n,1)->x),
                    cq_ps_coord_y(list_edge_get_end(n,1)->y));
            }
            PSPR("closepath\n");
        }
        PSPR("fill\n");
    }
    if (data->phase == TRIANGLE) {
        int col = 0;
        for (cp_dict_each(d, data->state)) {
            edge_t *e = tree_get_edge(d);
            if (!e->back) {
                continue;
            }

            switch (col = (col + 1) % 3) {
            case 0: PSPR("0.8 0.8 1   setrgbcolor\n"); break;
            case 1: PSPR("0.8 1   0.8 setrgbcolor\n"); break;
            case 2: PSPR("1   0.8 0.8 setrgbcolor\n"); break;
            }

            PSPR("newpath\n");
            PSPR("%g %g moveto\n", cq_ps_coord_x(e->rigt.x), cq_ps_coord_y(e->rigt.y));
            PSPR("%g %g lineto\n", cq_ps_coord_x(e->left.x), cq_ps_coord_y(e->left.y));
            for(edge_t *n = list_get_edge(e->list.next);
                e != n;
                n = list_get_edge(n->list.next))
            {
                if (!n->v_dis[!n->back]) {
                    PSPR("%g %g lineto\n",
                        cq_ps_coord_x(list_edge_get_end(n,1)->x),
                        cq_ps_coord_y(list_edge_get_end(n,1)->y));
                }
            }
            PSPR("closepath\n");
            PSPR("fill\n");
        }
    }

    /* show filled polygon: (c) the result structure 'triangle' */
    if (r != NULL) {
        PSPR("1 setlinewidth\n");
        for (cp_v_eachp(p, &r->tri)) {
            cp_vec2_loc_t *v0 = cp_v_nth_ptr(&r->point, p->p[0]);
            cp_vec2_loc_t *v1 = cp_v_nth_ptr(&r->point, p->p[1]);
            cp_vec2_loc_t *v2 = cp_v_nth_ptr(&r->point, p->p[2]);

            cq_vec2_t w0 = cq_import_vec2(&v0->coord);
            cq_vec2_t w1 = cq_import_vec2(&v1->coord);
            cq_vec2_t w2 = cq_import_vec2(&v2->coord);

            PSPR("1 1 0.8 setrgbcolor\n");
            PSPR("newpath %g %g moveto %g %g lineto %g %g lineto closepath fill\n",
                cq_ps_coord_x(w0.x), cq_ps_coord_y(w0.y),
                cq_ps_coord_x(w1.x), cq_ps_coord_y(w1.y),
                cq_ps_coord_x(w2.x), cq_ps_coord_y(w2.y));

            double w[2] = { 1.0, 5.0 };
            PSPR("0.9 0.7 0.7 setrgbcolor\n");
            PSPR("%g setlinewidth\n", w[!!(p->flags & CP_CSG2_TRI_OUTLINE_01)]);
            PSPR("newpath %g %g moveto %g %g lineto stroke\n",
                cq_ps_coord_x(w0.x), cq_ps_coord_y(w0.y),
                cq_ps_coord_x(w1.x), cq_ps_coord_y(w1.y));

            PSPR("%g setlinewidth\n", w[!!(p->flags & CP_CSG2_TRI_OUTLINE_12)]);
            PSPR("newpath %g %g moveto %g %g lineto stroke\n",
                cq_ps_coord_x(w1.x), cq_ps_coord_y(w1.y),
                cq_ps_coord_x(w2.x), cq_ps_coord_y(w2.y));

            PSPR("%g setlinewidth\n", w[!!(p->flags & CP_CSG2_TRI_OUTLINE_20)]);
            PSPR("newpath %g %g moveto %g %g lineto stroke\n",
                cq_ps_coord_x(w0.x), cq_ps_coord_y(w0.y),
                cq_ps_coord_x(w2.x), cq_ps_coord_y(w2.y));
        }
    }

    /* show rigt_most */
    if ((data->phase & (HAVE_LIST|HAVE_STATE|HAVE_RM)) == (HAVE_LIST|HAVE_STATE|HAVE_RM)) {
        PSPR("4 setlinewidth\n");
        for (cp_dict_each(d, data->state)) {
            edge_t *e = tree_get_edge(d);
            vertex_t *v = e->rigt_most;
            if (v) {
                PSPR("0 0 1 setrgbcolor\n");
                cq_ps_dot(e->rigt_most->x, e->rigt_most->y, 2);

                if (e->back) {
                    PSPR("1 0 1 setrgbcolor\n");
                }
                else {
                    PSPR("0 0 1 setrgbcolor\n");
                }
                double x1 = cq_ps_coord_x(v->x);
                double y1 = cq_ps_coord_y(v->y);
                double x2 = cq_ps_coord_x(other_end(v)->x);
                double y2 = cq_ps_coord_y(other_end(v)->y);
                double xd = (x2 - x1);
                double yd = (y2 - y1);
                double l  = sqrt((xd * xd) + (yd * yd));
                if (l > (2*ll)) {
                    double tb = ll / l;
                    double ta = 1 - tb;
                    PSPR("newpath %g %g moveto %g %g lineto stroke\n",
                        (x1 * ta) + (x2 * tb), (y1 * ta) + (y2 * tb),
                        (x2 * .5) + (x1 * .5), (y2 * .5) + (y1 * .5));
                }
            }
        }
    }

    /* show filled polygon: (b) the result structure 'point_idx' */
    if (r != NULL) {
#if 1 /* single polygon (correct visualisation, but one color, not easily debuggable) */
        PSPR("0.8 1 0.8 setrgbcolor\n");
        PSPR("newpath\n");
        for (cp_v_eachp(p, &r->path)) {
            char const *cmd = "moveto";
            for (cp_v_eachv(vi, &p->point_idx)) {
                cp_vec2_loc_t *v = cp_v_nth_ptr(&r->point, vi);
                cq_vec2_t w = cq_import_vec2(&v->coord);
                PSPR("%g %g %s\n", cq_ps_coord_x(w.x), cq_ps_coord_y(w.y), cmd);
                cmd = "lineto";
            }
            PSPR("closepath\n");
        }
        PSPR("fill\n");
#else
        int ci = 0;
        for (cp_v_eachp(p, &r->path)) {
            char const *cmd = "moveto";
            switch ((ci++) % 3) {
            case 0: PSPR("0.8 1 0.8 setrgbcolor\n"); break;
            case 1: PSPR("0.8 1 1   setrgbcolor\n"); break;
            case 2: PSPR("1 1 0.8   setrgbcolor\n"); break;
            }
            PSPR("newpath\n");
            for (cp_v_eachv(vi, &p->point_idx)) {
                cp_vec2_loc_t *v = cp_v_nth_ptr(&r->point, vi);
                cq_vec2_t w = cq_import_vec2(&v->coord);
                PSPR("%g %g %s\n", cq_ps_coord_x(w.x), cq_ps_coord_y(w.y), cmd);
                cmd = "lineto";
            }
            PSPR("closepath\n");
            PSPR("fill\n");
        }
#endif
    }

    /* show polygon */
    PSPR("0.4 setlinewidth\n");
    PSPR("0.7 0.7 0.7 setrgbcolor\n");
    for (cp_v_eachv(o, data->edges)) {
        if (edge_is_deleted_debug(data, o)) {
            continue;
        }
        cq_ps_line(o->left.x, o->left.y, o->rigt.x, o->rigt.y);
    }

    /* show line ends */
    if (data->phase == INTERSECT) {
        PSPR("4 setlinewidth\n");
        for (cp_v_eachv(o, data->edges)) {
            if (edge_is_deleted_debug(data, o)) {
                continue;
            }
            if (o->member & 1) {
                PSPR("0.7 0.7 0.7 setrgbcolor\n");
            }
            else {
                PSPR("0.7 0.7 1 setrgbcolor\n");
            }
            double x1 = cq_ps_coord_x(o->left.x);
            double y1 = cq_ps_coord_y(o->left.y);
            double x2 = cq_ps_coord_x(o->rigt.x);
            double y2 = cq_ps_coord_y(o->rigt.y);
            double xd = (x2 - x1);
            double yd = (y2 - y1);
            double l  = sqrt((xd * xd) + (yd * yd));
            if (l > (2*ll)) {
                double tb = ll / l;
                double ta = 1 - tb;
                PSPR("newpath %g %g moveto %g %g lineto stroke\n",
                    (x1 * ta) + (x2 * tb), (y1 * ta) + (y2 * tb),
                    (x2 * ta) + (x1 * tb), (y2 * ta) + (y1 * tb));
            }
        }
    }

    /* show intersection roundings */
    if (1) {
        PSPR("0.4 setlinewidth\n");
        for (cp_v_eachv(v, data->xings)) {
            cq_dim_t xi = cq_round(&v->x);
            cq_dim_t yi = cq_round(&v->y);
            double x = cq_f_from_dimif(&v->x);
            double y = cq_f_from_dimif(&v->y);;
            if (v->some_edge_tb[0] || v->some_edge_tb[1]) {
                PSPR("0 1 0 setrgbcolor\n");
            }
            else {
                PSPR("1 0 0 setrgbcolor\n");
            }
            cq_ps_line(x, y, xi, yi);
            if (0) {
                cq_ps_box(xi-0.5, yi-0.5, xi+0.5, yi+0.5);
            }
        }
    }

    /* show state */
    PSPR("3 setlinewidth\n");
    PSPR("0 0 1 setrgbcolor\n");
    if ((data->phase & (HAVE_STATE|HAVE_LIST)) == HAVE_STATE) {
        for (cp_dict_each(d, data->state)) {
            edge_t *e = tree_get_edge(d);
            double xi = (e->left.x + e->rigt.x)/2.0;
            double yi = (e->left.y + e->rigt.y)/2.0;
            cq_ps_dot(xi, yi, 1);
        }

        /* show state order by linking the center of lines */
        PSPR("0.4 setlinewidth\n");
        bool s0 = true;
        for (cp_dict_each(d, data->state)) {
            edge_t *e = tree_get_edge(d);
            if (i && (e->rigt.x == i->x)) {
                /* skip right end lines */
                continue;
            }
            double xi = (e->left.x + e->rigt.x)/2.0;
            double yi = (e->left.y + e->rigt.y)/2.0;
            double x = cq_ps_coord_x(xi);
            double y = cq_ps_coord_y(yi);
            if (s0) {
                cq_ps_dot(xi, yi, 1.2);
                PSPR("newpath %g %g moveto ", x, y);
            }
            else {
                PSPR("%g %g lineto ", x, y);
            }
            s0 = false;
        }
        if (!s0) {
            PSPR("stroke\n");
        }
    }

    /* show intersections */
    if (data->phase == INTERSECT) {
        PSPR("3 setlinewidth\n");
        for (cp_v_eachv(v, data->xings)) {
            double x = cq_f_from_dimif(&v->x);
            double y = cq_f_from_dimif(&v->y);;
            if (v->some_edge_tb[0] || v->some_edge_tb[1]) {
                PSPR("0 1 0 setrgbcolor\n");
            }
            else {
                PSPR("1 0 0 setrgbcolor\n");
            }
            cq_ps_dot(x, y, 1.5);
        }
    }

    /* start or end event */
    if (i != NULL) {
        /* print info: */
        PSPR("0 0 0 setrgbcolor\n");
        PSPR("%g %g moveto (event %s %d %d .. %d %d) show\n",
            cq_ps_left(), cq_ps_line_y(data->ps_line++),
            i->side == LEFT ? "LEFT" : "RIGT",
            i->x, i->y,
            other_end(i)->x, other_end(i)->y);

        /* show left and right vertex */
        PSPR("1 0 1 setrgbcolor\n");
        PSPR("1 setlinewidth\n");
        edge_t *othr = tree_edge_prev(edge_of(i));
        if (othr != NULL) {
            PSPR("%g %g moveto (prev %d %d .. %d %d) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++),
                othr->left.x, othr->left.y,
                othr->rigt.x, othr->rigt.y);
            cq_ps_line(othr->left.x, othr->left.y, othr->rigt.x, othr->rigt.y);
        }
        othr = tree_edge_next(edge_of(i));
        if (othr != NULL) {
            PSPR("%g %g moveto (next %d %d .. %d %d) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++),
                othr->left.x, othr->left.y,
                othr->rigt.x, othr->rigt.y);
            cq_ps_line(othr->left.x, othr->left.y, othr->rigt.x, othr->rigt.y);
        }

        /* show current vertex */
        if (i->side == LEFT) {
            PSPR("0 0.8 0 setrgbcolor\n");
        }
        else {
            PSPR("0.8 0 0 setrgbcolor\n");
        }
        PSPR("2 setlinewidth\n");
        cq_ps_dot(i->x, i->y, 1);
        vertex_t *j = other_end(i);
        cq_ps_line(i->x, i->y, j->x, j->y);
    }

    /* xing event */
    if (q != NULL) {
        /* print info: */
        PSPR("0 0 0 setrgbcolor\n");
        PSPR("%g %g moveto (event %s %d+%llu/%llu %d+%llu/%llu) show\n",
            cq_ps_left(), cq_ps_line_y(data->ps_line++),
            "XING",
            q->x.i, q->x.n, q->x.d,
            q->y.i, q->y.n, q->y.d);

        /* show edges */
        PSPR("2 setlinewidth\n");
        for (cp_v_eachv(o, data->edges)) {
            if (edge_is_deleted_debug(data, o)) {
                continue;
            }
            if (o->prev_xing == q) {
                if (o->next_xing == q) {
                    PSPR("1 0 0 setrgbcolor\n");
                }
                else {
                    PSPR("1 0 1 setrgbcolor\n");
                }
            }
            else
            if (o->next_xing == q) {
                PSPR("0 1 1 setrgbcolor\n");
            }
            else {
                edge_t *n = tree_edge_next(o);
                edge_t *p = tree_edge_prev(o);
                if (n && (n->next_xing == q)) {
                    PSPR("0 1 0 setrgbcolor\n");
                }
                else
                if (p && (p->prev_xing == q)) {
                    PSPR("1 1 0 setrgbcolor\n");
                }
                else {
                    continue;
                }
            }

            PSPR("%g %g moveto (prev %d %d .. %d %d) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++),
                o->left.x, o->left.y,
                o->rigt.x, o->rigt.y);
            cq_ps_line(o->left.x, o->left.y, o->rigt.x, o->rigt.y);
        }

        /* show current vertex */
        PSPR("1 0 0 setrgbcolor\n");
        double x = cq_f_from_dimif(&q->x);
        double y = cq_f_from_dimif(&q->y);
        cq_ps_dot(x, y, 1);
    }

    if (data->phase & HAVE_RESULT) {
        /* show final edges */
        for (cp_dict_each(o_, data->result)) {
            vertex_t *o1 = agenda_get_vertex(o_);
            if (o1->side != LEFT) {
                continue;
            }
            edge_t *o = edge_of(o1);
            if (data->phase & HAVE_LIST) {
                PSPR("0.5 0.5 0.5 setrgbcolor\n");
            }
            else
            if (o->member & 1) {
                PSPR("0 0 0 setrgbcolor\n");
            }
            else {
                PSPR("0 0 1 setrgbcolor\n");
            }
            double x1 = cq_ps_coord_x(o->left.x);
            double y1 = cq_ps_coord_y(o->left.y);
            double x2 = cq_ps_coord_x(o->rigt.x);
            double y2 = cq_ps_coord_y(o->rigt.y);
            double xd = (x2 - x1);
            double yd = (y2 - y1);
            double l  = sqrt((xd * xd) + (yd * yd));
            if (l > (2*ll)) {
                double tb = ll / l;
                double ta = 1 - tb;
                PSPR("2 setlinewidth\n");
                PSPR("newpath %g %g moveto %g %g lineto stroke\n",
                    (x1 * ta) + (x2 * tb), (y1 * ta) + (y2 * tb),
                    (x2 * ta) + (x1 * tb), (y2 * ta) + (y1 * tb));
            }
            PSPR("0.4 setlinewidth\n");
            PSPR("newpath %g %g moveto %g %g lineto stroke\n",
                x1, y1, x2, y2);
        }
    }

    /* show state in POLY or TRIANGLE phase */
    if ((data->phase & (HAVE_LIST|HAVE_STATE)) == (HAVE_LIST|HAVE_STATE)) {
        PSPR("1 setlinewidth\n");
        for (cp_dict_each(d, data->state)) {
            edge_t *e = tree_get_edge(d);
            if (e->back) {
                PSPR("1 0 1 setrgbcolor\n");
            }
            else {
                PSPR("0 0 1 setrgbcolor\n");
            }
            cq_ps_line(e->left.x, e->left.y, e->rigt.x, e->rigt.y);
        }
    }

    /* pixel/bundle event */
    if (b != NULL) {
        /* print info: */
        PSPR("0 0 0 setrgbcolor\n");
        PSPR("%g %g moveto (event %s %d %d) show\n",
            cq_ps_left(), cq_ps_line_y(data->ps_line++),
            "PIX",
            b->x, b->y);

        /* show current vertex */
        PSPR("1 0 0 setrgbcolor\n");
        cq_ps_dot(b->x, b->y, 1);

        /* show tolerance square */
        PSPR("0.4 setlinewidth\n");
        cq_ps_box(b->x - 0.5, b->y - 0.5, b->x + 0.5, b->y + 0.5);
    }

}

#endif /* CQ_TRACE */
