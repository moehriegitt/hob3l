/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/arith.h>
#include "op-sweep-internal.h"

extern int cq_sweep_result_vertex_cmp(
    cp_dict_t *a_,
    cp_dict_t *b_,
    data_t *data CP_UNUSED)
{
    vertex_t *va = agenda_get_vertex(a_);
    vertex_t *vb = agenda_get_vertex(b_);

    int i = vec2_cmp(&va->vec2, &vb->vec2);
    if (i != 0) {
        return i;
    }

    i = CP_CMP(vb->side, va->side); /* do RIGT (1) before LEFT (0) */
    if (i != 0) {
        return i;
    }

    /* insert bottom to top (by angle) */
    vertex_t const *oa = other_end(va);
    vertex_t const *ob = other_end(vb);
    i = CP_SIGN(cq_vec2_right_cross3_z(&ob->vec2, &va->vec2, &oa->vec2));
    if (i != 0) {
        return i;
    }

    /* all overlapping edges should be equal edges */
    assert(vec2_cmp(&other_end(va)->vec2, &other_end(vb)->vec2) == 0);
    return 0;
}

extern int cq_sweep_tree_vertex_edge_cmp(
    vertex_t *v,
    cp_dict_t *e_,
    void *user CP_UNUSED)
{
    edge_t const *e = tree_get_edge(e_);
    cq_assert(v->side == LEFT);

    int i = vec2_edge_cmp_exact(&v->vec2, e);
    if (i != 0) {
        return i;
    }

    /* compare by seeing how the rigt ends are oriented wrt. the
     * new (equal) scan line vertex, which is the origin here.
     *
     * If r is below v--END(e), v should be below e.  So we
     * need a negative result in this case, so in this case,
     * the vertices must go in counter-clockwise: END(e), v, r:
     */
    vertex_t const *r = rigt_end(v);
    assert(r != v);
    i = CP_SIGN(cq_vec2_right_cross3_z(&e->rigt.vec2, &v->vec2, &r->vec2));
    if (i != 0) {
        return i;
    }

    /* it could be that we insert a collinear continuation of the
     * edge: do not fail on this, but just insert it somewhere.  It just
     * means that LEFT events are processed before RIGT events, which
     * is generally a good heuristics to avoid superfluous intersection
     * computations.
     * For the 'poly' phase, in this case, an edge 'u' is delayed, and
     * the new edge must be below that pending edge so that 'u' ends
     * up next to the uppermost left edge later.
     */
    if (vec2_cmp(&v->vec2, &e->rigt.vec2) == 0) {
        return -1; /* must be -1, see op-trianglify.c */
    }

    return 0;
}

extern int cq_sweep_bundle_vec2_edge_cmp(
    cq_vec2_t const *v,
    cp_dict_t *e_,
    void *user CP_UNUSED)
{
    edge_t const *e = tree_get_edge(e_);
    return vec2_edge_cmp_tolerant(v, e);
}

extern void cq_sweep_bundle_aug_ev(
    cp_dict_aug_t *aug CP_UNUSED,
    cp_dict_t *a_,
    cp_dict_t *b_,
    cp_dict_aug_type_t c)
{
    assert(a_ != NULL);
    edge_t *a = tree_get_edge(a_);
    switch (c) {
    case CP_DICT_AUG_LEFT:
    case CP_DICT_AUG_RIGHT:
        cq_sweep_bundle_update_sum_member(b_);
        cq_sweep_bundle_update_sum_member(a_);
        break;

    case CP_DICT_AUG_NOP:
    case CP_DICT_AUG_ADD:
    case CP_DICT_AUG_JOIN:
    case CP_DICT_AUG_CUT_LEAF:
        cq_sweep_bundle_update_sum_member(a_);
        break;

    case CP_DICT_AUG_NOP2:
        cq_sweep_bundle_update_sum_member(a_);
        cq_sweep_bundle_update_sum_member(a_->parent);
        break;

    case CP_DICT_AUG_FINI:
        cq_sweep_bundle_update_sum_member_rec(a_);
        break;

    case CP_DICT_AUG_CUT_SWAP:
        break;

    case CP_DICT_AUG_SPLIT:
        a->sum_member = a->member;
        break;
    }
}

/** Searching a pixel with a bundle: for searching.
 *
 * Now, pixels and bundles are the same type (bundle_t), so this is
 * kinda weird in terms of type safety.
 */
extern int cq_sweep_state_pixel_bundle_cmp(
    bundle_t const *a,
    cp_dict_t *b_,
    data_t *data CP_UNUSED)
{
    bundle_t const *b = state_get_bundle(b_);

    /* compare with top edge */
    int i = vec2_edge_cmp_tolerant(&a->vec2, bundle_get_top(b));
    if (i >= 0) { /* above or equal: we have a result */
        return i;
    }

    /* compare with bottom edge */
    i = vec2_edge_cmp_tolerant(&a->vec2, bundle_get_bot(b));
    if (i <= 0) { /* below or equal: we have a result */
        return i;
    }

    /* v is in between => it crosses => equal */
    return 0;
}

/** Compare a bundle with a bundle: for insertion */
extern int cq_sweep_state_bundle_bundle_cmp(
    bundle_t *a,
    cp_dict_t *b_,
    data_t *data CP_UNUSED)
{
    bundle_t const *b = state_get_bundle(b_);

    /* If the source is not equal, compare by point relation (by rounded
     * pixel value) */
    if (!cq_vec2_eq(&a->vec2, &b->vec2)) {
        /* comparing with a single edge should suffice,
         * because the bundles are disjoint. */
        int i = vec2_edge_cmp_exact(&a->vec2, bundle_get_top(b));
        assert(i != 0); /* not disjoint? */
        assert(i == vec2_edge_cmp_exact(&a->vec2, bundle_get_bot(b))); /* crossing? */
        return i;
    }

    /* for the same source pixel, compare the angle.  Again, one
     * edge comparison should surfice.
     *
     * If end(a) is below start(a)--end(b), a should be below b.
     * So we need a negative result in this case, so in this case,
     * the vertices must go in counter-clockwise: END(e), v, r:
     */
    vertex_t const *r = &bundle_get_top(a)->rigt;
    int i = CP_SIGN(cq_vec2_right_cross3_z(&bundle_get_top(b)->rigt.vec2, &a->vec2, &r->vec2));
    assert(i != 0); /* not disjoint? */
    return i;
}

extern int cq_sweep_agenda_vertex_phase1_cmp(
    cp_dict_t *a_,
    cp_dict_t *b_,
    data_t *data CP_UNUSED)
{
    vertex_t *a = CP_BOX_OF(a_, *a, in_agenda);
    vertex_t *b = CP_BOX_OF(b_, *b, in_agenda);

    assert(!edge_is_deleted_debug(data, edge_of(a)));
    assert(!edge_is_deleted_debug(data, edge_of(b)));

    /* order: x coord first, then y coord */
    int i = vec2_cmp(&a->vec2, &b->vec2);
    if (i != 0) {
        return i;
    }

    /* order: LEFT > RIGT
     * If we did 'continuation events' like the Herter&Mehlhorn triangulation,
     * we could probably reduce the number of intersection checks.  For now,
     * we remove the old line, then insert the new one, so the upper and
     * lower edge here will be checked twice for intersections.
     */
    i = CP_CMP(b->side, a->side);
    if (i != 0) {
        return i;
    }

    /* order by angle: not really needed, but keeps the number of possible
     * paths low, which is good for fuzzing and debugging. */
    vertex_t const *c = other_end(a);
    vertex_t const *d = other_end(b);
    i = CP_CMP(cq_vec2_right_cross3_z(&c->vec2, &a->vec2, &d->vec2), 0);
    if (i != 0) {
        /* iff a--d is anti-clockwise of a--c, then i is negative */
        return i;
    }

    /* overlap: edges are inserted in any order: resolved at insertion into state */
    return 0;
}

extern int cq_sweep_agenda_vertex_phase2_cmp(
    cp_dict_t *a_,
    cp_dict_t *b_,
    data_t *data)
{
    vertex_t *a = CP_BOX_OF(a_, *a, in_agenda);
    vertex_t *b = CP_BOX_OF(b_, *b, in_agenda);

    assert(!edge_is_deleted_debug(data, edge_of(a)));
    assert(!edge_is_deleted_debug(data, edge_of(b)));

    /* order: x coord first, then y coord */
    int i = CP_CMP(a->x, b->x);
    if (i != 0) {
        return i;
    }

    /* phase 2, pass 2 orders the y coordinate differently: top to bot instead */
    i = CP_CMP(a->y, b->y);
    if (i != 0) {
        return phase_south(data) ? -i : +i;
    }

    /* order: LEFT > RIGT
     * If we did 'continuation events' like the Herter&Mehlhorn triangulation,
     * we could probably reduce the number of intersection checks.  For now,
     * we remove the old line, then insert the new one, so the upper and
     * lower edge here will be checked twice for intersections.
     */
    i = CP_CMP(b->side, a->side);
    if (i != 0) {
        return i;
    }

    /* order by angle: not really needed, but keeps the number of possible
     * paths low, which is good for fuzzing and debugging. */
    vertex_t const *c = other_end(a);
    vertex_t const *d = other_end(b);
    i = CP_CMP(cq_vec2_right_cross3_z(&c->vec2, &a->vec2, &d->vec2), 0);
    if (i != 0) {
        /* iff a--d is anti-clockwise of a--c, then i is negative */
        return i;
    }

    /* overlap: edges are inserted in any order: resolved at insertion into state */
    return 0;
}

extern int cq_sweep_agenda_xing_phase1_cmp(
    cp_dict_t *a_,
    cp_dict_t *b_,
    data_t *data)
{
    (void)data;

    xing_t *a = CP_BOX_OF(a_, *a, in_agenda);
    xing_t *b = CP_BOX_OF(b_, *b, in_agenda);

    int i = cq_dimif_cmp(&a->x, &b->x);
    if (i != 0) {
        return i;
    }

    i = cq_dimif_cmp(&a->y, &b->y);
    if (i != 0) {
        return i;
    }

    return 0;
}

extern int cq_sweep_agenda_xing_phase2_cmp(
    cp_dict_t *a_,
    cp_dict_t *b_,
    data_t *data)
{
    xing_t *a = CP_BOX_OF(a_, *a, in_agenda);
    xing_t *b = CP_BOX_OF(b_, *b, in_agenda);

    /* primarily sort by rounded coordinate value */
    int i = CP_CMP(cq_round(&a->x), cq_round(&b->x));
    if (i != 0) {
        return i;
    }

    i = CP_CMP(cq_round(&a->y), cq_round(&b->y));
    if (i != 0) {
        return phase_south(data) ? -i : +i;
    }

    /* secondary sort by exact value for sweeping the pixel */
    i = cq_dimif_cmp(&a->x, &b->x);
    if (i != 0) {
        return i;
    }

    i = cq_dimif_cmp(&a->y, &b->y);
    if (i != 0) {
        return phase_south(data) ? -i : +i;
    }

    return 0;
}

/* ********************************************************************** */
/* event processing */

static inline int ev_get_intersect(
    cq_vec2if_t *it,
    edge_t *edge,
    edge_t *othr)
{
    return cq_vec2if_intersect(it,
        edge->left.vec2, edge->rigt.vec2,
        othr->left.vec2, othr->rigt.vec2);
}

#if! CQ_TRACE
#define ev_pair(data, cur, prev, next, msg) \
        ev_pair(data, cur, prev, next)
#endif

/**
 * Update agenda and state when overlap is found.
 */
static inline void ev_overlap(
    data_t *data,
    edge_t *e,
    edge_t *o)
{
    assert( state_edge_is_member(data, o));
    assert(!agenda_vertex_is_member(data, &o->left));
    assert( agenda_vertex_is_member(data, &o->rigt));
    assert(!state_edge_is_member(data, e));
    assert(!agenda_vertex_is_member(data, &e->left));
    assert( agenda_vertex_is_member(data, &e->rigt));

    /* possible situations:
     *
     * s:    |           = scanline position
     *
     * o:    !----]      left_eq         ! = edge in state
     * o:  !------]      !left_eq        [ = left in agenda
     *                                   ] = right in agenda
     * e:    |----]      i == 0
     * e:    |------]    i >  0
     * e:    |--]        i <  0
     */

    /* decide how to merge/split based on length(e) */
    int i = CP_CMP(
        edge_sqr_len(e),
        cq_vec2_sqr_dist(&e->left.vec2, &o->rigt.vec2));

    /* will left part be empty? */
    bool left_eq = cq_vec2_eq(&o->left.vec2, &e->left.vec2);
    if (i == 0) {
        if (left_eq) {
            PSPR("0 0 0 setrgbcolor %g %g moveto (overlap q-q) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++));
            /* o:    !----] => !====]
             * e:    |----]    X       */
            o->member ^= e->member;
            agenda_vertex_remove(data, &e->rigt);
            EDGE_DELETE(data, e);
        }
        else {
            PSPR("0 0 0 setrgbcolor %g %g moveto (overlap o-q) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++));
            /* o:  !------] => |-|
             * e:    |----]      !====]  */
            e->member ^= o->member;
            o->rigt.vec2 = e->left.vec2;
            assert(vec2_cmp(&o->left.vec2, &o->rigt.vec2) < 0);
            state_edge_replace(data, o, e);
            xing_move(e, o);
            agenda_vertex_remove(data, &o->rigt);
        }
    }
    else
    if (i > 0) {
        if (left_eq) {
            PSPR("0 0 0 setrgbcolor %g %g moveto (overlap q-n) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++));
            /* o:    !----]    =>   !====]
             * e:    |------]            [-]  */
            o->member ^= e->member;
            e->left.vec2 = o->rigt.vec2;
            assert(vec2_cmp(&e->left.vec2, &e->rigt.vec2) < 0);
            agenda_vertex_insert(data, &e->left);
        }
        else {
            PSPR("0 0 0 setrgbcolor %g %g moveto (overlap o-n) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++));
            /* o:  !------]    => |-|
             * e:    |------]       !====]
             * q:                        [-]
             */
            edge_t *q = edge_new(data, &o->rigt.vec2, &e->rigt.vec2, e->member, true);
            e->member ^= o->member;
            e->rigt.vec2 = o->rigt.vec2;
            assert(vec2_cmp(&e->left.vec2, &e->rigt.vec2) < 0);
            o->rigt.vec2 = e->left.vec2;
            assert(vec2_cmp(&o->left.vec2, &o->rigt.vec2) < 0);
            state_edge_replace(data, o, e);
            xing_move(e, o);
            agenda_vertex_update(data, &e->rigt);
            agenda_vertex_remove(data, &o->rigt);
            agenda_vertex_insert(data, &q->left);
            agenda_vertex_insert(data, &q->rigt);
        }
    }
    else {
        assert(i < 0);
        if (left_eq) {
            PSPR("0 0 0 setrgbcolor %g %g moveto (overlap q-o) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++));
            /* o:    !----]  => !==]
             * e:    |--]          [-]  */
            size_t o_member = o->member;
            o->member ^= e->member;
            e->member = o_member;
            e->left.vec2 = e->rigt.vec2;
            e->rigt.vec2 = o->rigt.vec2;
            assert(vec2_cmp(&e->left.vec2, &e->rigt.vec2) < 0);
            o->rigt.vec2 = e->left.vec2;
            assert(vec2_cmp(&o->left.vec2, &o->rigt.vec2) < 0);
            xing_clear_beyond(o);
            agenda_vertex_update(data, &o->rigt);
            agenda_vertex_insert(data, &e->left);
            agenda_vertex_update(data, &e->rigt);
        }
        else {
            PSPR("0 0 0 setrgbcolor %g %g moveto (overlap o-o) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++));
            /* o:  !------]  => |-|
             * e:    |--]         !==]
             * q:                    [-]  */
            e->member ^= o->member;
            edge_t *q = edge_new(data, &e->rigt.vec2, &o->rigt.vec2, o->member, true);
            o->rigt.vec2 = e->rigt.vec2;
            xing_clear_beyond(o);
            o->rigt.vec2 = e->left.vec2;
            assert(vec2_cmp(&o->left.vec2, &o->rigt.vec2) < 0);
            state_edge_replace(data, o, e);
            xing_move(e, o);
            agenda_vertex_remove(data, &o->rigt);
            agenda_vertex_insert(data, &q->left);
            agenda_vertex_insert(data, &q->rigt);
        }
    }
}

static void ev_split(
    data_t *data,
    cq_vec2_t const *i,
    edge_t *o)
{
    PSPR("0 0 0 setrgbcolor %g %g moveto (split) show\n",
        cq_ps_left(), cq_ps_line_y(data->ps_line++));
    /*       i
     * o: !--+--]  => !--]
     * q:                [--]
     */
    edge_t *q = edge_new(data, i, &o->rigt.vec2, o->member, true);
    o->rigt.vec2 = *i;
    assert(vec2_cmp(&o->left.vec2, &o->rigt.vec2) < 0);
    xing_clear_beyond(o);
    agenda_vertex_update(data, &o->rigt);
    agenda_vertex_insert(data, &q->left);
    agenda_vertex_insert(data, &q->rigt);
}

static void ev_pair(
    data_t *data,
    cq_vec2if_t const *cur,
    edge_t *prev,
    edge_t *next,
    char const *msg)
{
    if (prev == NULL) {
        return;
    }
    if (next == NULL) {
        return;
    }

    cq_vec2if_t it;
    int ii = ev_get_intersect(&it, prev, next);
    PSPR("0 0 0 setrgbcolor %g %g moveto (cmp %d) show\n",
        cq_ps_left(), cq_ps_line_y(data->ps_line++), ii);
    switch (ii) {
    default:
    case -1:
    case 0:
    case 1|2|8:
    case 1|2|16:
    case 1|4|8:
    case 1|4|16:
        return;

    case 1|2:  ev_split(data, &prev->left.vec2, next); break;
    case 1|4:  ev_split(data, &prev->rigt.vec2, next); break;
    case 1|8:  ev_split(data, &next->left.vec2, prev); break;
    case 1|16: ev_split(data, &next->rigt.vec2, prev); break;

    case 1:
        if (cq_vec2if_cmp(&it, cur) > 0) {
            PSPR("0 0 0 setrgbcolor %g %g moveto (%s intersect %d+%llu/%llu %d+%llu/%llu) show\n",
                cq_ps_left(), cq_ps_line_y(data->ps_line++), msg,
                it.x.i, it.x.n, it.x.d,
                it.y.i, it.y.n, it.y.d);
            assert(state_edge_is_member(data, prev));
            assert(state_edge_is_member(data, next));
            if ((it.x.n == 0) && (it.y.n == 0)) {
                /* crossing is integer => use ev_split */
                cq_vec2_t iti = CQ_VEC2(it.x.i, it.y.i);
                ev_split(data, &iti, prev);
                ev_split(data, &iti, next);
            }
            else {
                xing_new(data, &it, prev, next);
            }
        }
        return;
    }
}

static void ev_left(
    data_t *data,
    vertex_t *left)
{
    edge_t *othr = state_edge_insert(data, left);
    cq_sweep_trace_begin_page(data, left, NULL, NULL, NULL);

    cq_assert(left->side == LEFT);
    edge_t *edge = edge_of(left);

    if (othr != NULL) {
        PSPR("0 0 0 setrgbcolor %g %g moveto (overlap) show\n",
            cq_ps_left(), cq_ps_line_y(data->ps_line++));
         assert(vec2_cmp(&edge->left.vec2, &othr->rigt.vec2) != 0);
         ev_overlap(data, edge, othr);
    }
    else {
        /* normal case: two neighbouring edges */
        edge_t *prev = tree_edge_prev(edge);
        edge_t *next = tree_edge_next(edge);

        /* remove imminent event if there is one */
        xing_split(prev, next);
        assert((prev == NULL) || (prev->next_xing == NULL));
        assert((next == NULL) || (next->prev_xing == NULL));
        assert(edge->next_xing == NULL);
        assert(edge->prev_xing == NULL);

        cq_vec2if_t cur = cq_vec2if_from_vec2(&left->vec2);
        ev_pair(data, &cur, prev, edge, "L1");
        ev_pair(data, &cur, edge, next, "L2");
    }

    cq_sweep_trace_end_page(data);
}

static void ev_rigt(
    data_t *data,
    vertex_t *rigt)
{
    cq_sweep_trace_begin_page(data, rigt, NULL, NULL, NULL);

    cq_assert(rigt->side == RIGT);
    edge_t *edge = edge_of(rigt);

    edge_t *prev = tree_edge_prev(edge);
    edge_t *next = tree_edge_next(edge);

    assert(data->state != NULL);
    assert(edge->prev_xing == NULL);
    assert(edge->next_xing == NULL);
    state_edge_remove(data, edge);
    if (prev || next) {
        assert(data->state != NULL);
    }

#if 0 /* disable: if input is BS, we should fix it, not assert-fail */
    /* check that polygon is closed */
    if (next == NULL) {
        /* this was a top-most edge, so we should be 'outside' of any polygon again */
        assert((edge->below ^ edge->member) == 0);
    }
#endif

    if (prev && next) {
        assert(prev->next_xing == NULL);
        assert(next->prev_xing == NULL);
        cq_vec2if_t cur = cq_vec2if_from_vec2(&rigt->vec2);
        ev_pair(data, &cur, prev, next, "R");
    }
    cq_sweep_trace_end_page(data);
}

static void ev_vertex(
    data_t *data,
    vertex_t *ev)
{
    /* clear scanline when x advances */
    if (ev->side == LEFT) {
        ev_left(data, ev);
    }
    else {
        assert(ev->side == RIGT);
        ev_rigt(data, ev);
    }
}

static void ev_clear_and_reverse(
    data_t *data,
    xing_t *ev CP_UNUSED,
    edge_t *s,
    edge_t *t)
{
    /* reverse order */
    for (int i CP_UNUSED = 0;;) {
        assert(s != t);
        assert((s->next_xing == NULL) || (s->next_xing == ev));
        assert((t->next_xing == NULL) || (t->next_xing == ev));
        assert((s->prev_xing == NULL) || (s->prev_xing == ev));
        assert((t->prev_xing == NULL) || (t->prev_xing == ev));
        s->next_xing = NULL;
        s->prev_xing = NULL;
        t->next_xing = NULL;
        t->prev_xing = NULL;

#ifndef NDEBUG
        edge_t *t2a = tree_edge_prev(t);
#endif

        state_edge_swap(data, s, t);

        edge_t *t2 = tree_edge_prev(s);
        assert((t2a == s) || (t2a == t2));
        assert(t2 != NULL);
        if (t2 == t) {
            break;
        }

        edge_t *s2 = tree_edge_next(t);
        assert(s2 != NULL);
        if (s2 == t2) {
            s2->next_xing = NULL;
            s2->prev_xing = NULL;
            break;
        }

        s = s2;
        t = t2;
    }
}

static inline bool edge_south(
    edge_t *e)
{
    return e->left.y >= e->rigt.y;
}

static void ev_cross(
    data_t *data,
    xing_t *ev)
{
    cq_sweep_trace_begin_page(data, NULL, ev, NULL, NULL);

    /* find top-most and bottom most edge
     * There must be a 'some_edge', because setting 'some_edge'
     * must have happened after any unsetting of it, because
     * at the crossing, the crossing is imminent for all of the
     * edges.
     */
    assert(ev->some_edge != NULL);
    edge_t *u = NULL;
    edge_t *t = ev->some_edge;
    for (int i CP_UNUSED = 0;;) {
        assert(i++ < 100);
        u = tree_edge_next(t);
        if (u == NULL) {
            break;
        }
        if (u->prev_xing != ev) {
            break;
        }
        t = u;
    }
    assert(t != NULL);

    edge_t *r = NULL;
    edge_t *s = ev->some_edge;
    for (int i CP_UNUSED = 0;;) {
        assert(i++ < 100);
        r = tree_edge_prev(s);
        if (r == NULL) {
            break;
        }
        if (r->next_xing != ev) {
            break;
        }
        s = r;
    }
    assert(s != NULL);
    assert(s != t);
    assert(r != s);
    assert(r != t);
    assert(u != s);
    assert(u != t);

    /* set top and bottom for phase 2 */
    ev->some_edge = NULL;
    ev->some_edge_tb[edge_south(s)] = s;
    ev->some_edge_tb[edge_south(t)] = t;

    /* split top and bottom */
    xing_split(r, s);
    xing_split(t, u);

    /* reverse edges in state */
    ev_clear_and_reverse(data, ev, s, t);

    /* process top and bottom */
    cq_vec2if_t cur = ev->vec2if;
    ev_pair(data, &cur, s, u, "X1");
    ev_pair(data, &cur, r, t, "X2");

    cq_sweep_trace_end_page(data);
}

/* ********************************************************************** */

/**
 * At the very same point, END is handled first, then CROSS, then START.
 */
static inline int ev_do_first(
    vertex_t const *a,
    cq_vec2if_t const *b)
{
    assert(a != NULL);
    assert(b != NULL);
    int i = cq_vec2_vec2if_cmp(&a->vec2, b);
    if (i != 0) {
        return i > 0;
    }
    cq_assert(0 && "CROSS event interferes with LEFT/RIGT event");
    if (a->side == LEFT) {
        return 1;
    }
    return 0;
}

extern data_t *cq_sweep_new(
    cp_pool_t *tmp,
    cp_loc_t loc,
    size_t edge_cnt)
{
    /* init */
    data_t *data = POOL_NEW(tmp, *data);
    data->type = CQ_OBJ_TYPE_SWEEP;
    data->loc = loc;

    data->tmp = tmp;
    data->free_edge = EDGE_NIL;
    data->free_bundle = BUNDLE_NIL;
    data->phase = INTERSECT;
    cp_v_clear_alloc(data->tmp->alloc, data->edges, edge_cnt);

    /* prepare environment for phase 1 */
    data->agenda_vertex_cmp = cq_sweep_agenda_vertex_phase1_cmp;
    data->agenda_xing_cmp = cq_sweep_agenda_xing_phase1_cmp;

    return data;
}

extern void cq_sweep_add_edge(
    data_t *data,
    cq_vec2_t const *a,
    cq_vec2_t const *b,
    size_t member)
{
    int i = vec2_cmp(a, b);
    if (i == 0) {
        return;
    }
    if (i > 0) {
        CP_SWAP(&a, &b);
    }

    edge_t *e = edge_new(data, a, b, member, true);
    assert(!agenda_vertex_is_member(data, &e->left));
    assert(!agenda_vertex_is_member(data, &e->rigt));

    agenda_vertex_insert(data, &e->left);
    agenda_vertex_insert(data, &e->rigt);
    assert(!edge_is_deleted_debug(data, e));
    assert(agenda_vertex_is_member(data, &e->left));
    assert(agenda_vertex_is_member(data, &e->rigt));
}

extern void cq_sweep_add_v_line2(
    cq_sweep_t *s,
    cq_v_line2_t const *gon,
    size_t member)
{
    for (cp_v_eachp(i, gon)) {
        cq_sweep_add_edge(s, &i->a, &i->b, member);
    }
}

extern void cq_sweep_add_poly(
    cq_sweep_t *s,
    cq_csg2_poly_t const *gon,
    size_t member)
{
    /* prefer path over triangles (less edges are expected this way) */
    if (gon->path.size > 0) {
        for (cp_v_eachp(path, &gon->path)) {
            for (cp_v_each(ki, &path->point_idx)) {
                size_t kj = cp_wrap_add1(ki, path->point_idx.size);
                size_t hi = cp_v_nth(&path->point_idx, ki);
                size_t hj = cp_v_nth(&path->point_idx, kj);
                cq_vec2_t wi = cq_import_vec2(&cp_v_nth(&gon->point, hi).coord);
                cq_vec2_t wj = cq_import_vec2(&cp_v_nth(&gon->point, hj).coord);
                cq_sweep_add_edge(s, &wi, &wj, member);
            }
        }
    }
    else {
        for (cp_v_eachp(tri, &gon->tri)) {
            cq_vec2_t wa = cq_import_vec2(&cp_v_nth(&gon->point, tri->a).coord);
            cq_vec2_t wb = cq_import_vec2(&cp_v_nth(&gon->point, tri->b).coord);
            cq_vec2_t wc = cq_import_vec2(&cp_v_nth(&gon->point, tri->c).coord);
            cq_sweep_add_edge(s, &wa, &wb, member);
            cq_sweep_add_edge(s, &wb, &wc, member);
            cq_sweep_add_edge(s, &wc, &wa, member);
        }
    }
}

extern void cq_sweep_add_sweep(
    cq_sweep_t *s,
    cq_sweep_t *other,
    size_t member)
{
    for (cp_dict_each(o_, other->result)) {
        vertex_t *o1 = agenda_get_vertex(o_);
        if (o1->side != LEFT) {
            continue;
        }
        edge_t *o = edge_of(o1);
        cq_sweep_add_edge(s, &o->left.vec2, &o->rigt.vec2, member);
    }
}

static inline void sweep_phase1_find_intersections(
    data_t *data)
{
    /* PHASE 1: find intersections
     * This is a modified Bentley-Ottmann algorithm.
     * Modifications are mainly for corner case handling.
     * The algorithm is run with fractional arithmetic to compute
     * the intersections exactly.
     */

    /* Overlap is immediately resolved during the algorithm run,
     * i.e., edges are immediately split and merged.  This merging
     * simplifies highly overlapping problems.  Eliminating overlap
     * is necessary to simplify the intersection handling (we'd
     * otherwise need to handle points on segments are crossings,
     * and would get more crossings).
     * Because the mechanism is then already present, intersections
     * that have integer coordinates are handled directly by splitting
     * the edges.  Doing this early makes subsequent steps faster,
     * hopefully.  Also, this means that there will be no crossing events
     * coincident with start/end events, and we'll have a unambiguous criterion
     * whether a vertex is an end point or a crossing: the input set won't
     * change that (the algorithm runs virtually the same for an input of two
     * segments with an integer crossing and the same arrangement represented
     * as four segments ending in the crossing).
     * Also, this improves debuggability, because lines will not hide behind
     * other lines in the debug output...
     *
     * => this requires replacing agenda_vertex (again) with a dict_t.
     */

    /* run algorithm loop:
     * next event is either from data->agenda or from data->xing */
    while (data->agenda_vertex_min || data->agenda_xing_min) {
        if (data->agenda_xing_min &&
            (
                (data->agenda_vertex_min == NULL) ||
                (ev_do_first(
                    agenda_vertex_min(data),
                    &agenda_xing_min(data)->vec2if) != 0)))
        {
            ev_cross(data, agenda_xing_extract_min(data));
        }
        else {
            ev_vertex(data, agenda_vertex_extract_min(data));
        }
    }
    cq_sweep_trace_begin_page(data, NULL, NULL, NULL, NULL);
    PSPR("0 0 0 setrgbcolor %g %g moveto (intersections: %lu) show\n",
        cq_ps_left(), cq_ps_line_y(data->ps_line++), data->xings->size);
    cq_sweep_trace_end_page(data);

    /* The output is in data->edges and data->xings. */
}

/* ********************************************************************** */

static inline bundle_t *bundle_new_from_agenda(
    data_t *data)
{
    vertex_t *v = agenda_vertex_min(data);
    xing_t   *c = agenda_xing_min(data);
    if (c == NULL) {
        assert(v != NULL);
        return bundle_new(data, v->x, v->y);
    }

    cq_dim_t cx = cq_round(&c->x);
    cq_dim_t cy = cq_round(&c->y);
    if (v == NULL) {
        return bundle_new(data, cx, cy);
    }

    int i = CP_CMP(v->x, cx);
    if (i == 0) {
        i = CP_CMP(v->y, cy);
        if (phase_south(data)) {
            i = -i;
        }
    }

    if (i > 0) {
        return bundle_new(data, cx, cy);
    }

    return bundle_new(data, v->x, v->y);
}

/**
 * Returns:
 *    0 if neither vertex or xing agenda have an event within the hot pixel.
 *    1 if vertex agenda has the next event
 *    2 if xing agenda has the next event
 */
static inline int agenda_next_event_cmp(
    data_t *data,
    cq_vec2_t const *p)
{
    /* xing events 1:
     * those events 'before' the center come first */
    xing_t *c = agenda_xing_min(data);
    if (c != NULL) {
        if ((p->x != cq_round(&c->x)) || (p->y != cq_round(&c->y))) {
            c = NULL; /* not the same rounded pixel anymore */
        }
        else {
            /* compare pixel with next xing event */
            int ci = cq_dim_dimif_cmp(p->x, &c->x);
            if (ci == 0) {
                ci = cq_dim_dimif_cmp(p->y, &c->y);
                if (phase_south(data)) {
                    ci = -ci;
                }
            }

            /* xing events smaller than v come first */
            if (ci > 0) {
                return 2;
            }
        }
    }

    /* vertex events:
     * unfortunately, we cannot remove at the beginning of the pixel sweep
     * to save time: there may be crossings referring edges in the same
     * pixel where they are removed.  So all end and start events are
     * handled at the center of the pixel.
     */
    vertex_t *v = agenda_vertex_min(data);
    if (v != NULL) {
        if ((v->x != p->x) || (v->y != p->y)) {
            v= NULL;
        }
        else {
            return 1;
        }
    }

    /* xing events, 2:
     * and then the other intersection events */
    if (c != NULL) {
        return 2;
    }

    /* nothing left for this pixel */
    return 0;
}

static inline bool intersection_eq(
    cq_vec2if_t const *it1,
    edge_t *a,
    edge_t *b)
{
    cq_vec2if_t it2;
    switch (ev_get_intersect(&it2, a, b)) {
    case 1:
        return cq_vec2if_cmp(it1, &it2) == 0;
    default:
        return false;
    }
}

static inline void sweep_phase2_edge_reverse(
    data_t *data,
    bundle_t *p,
    xing_t *ev)
{
    /* some edge */
    edge_t *s = ev->some_edge_tb[phase_south(data)];
    if (s == NULL) {
        return;
    }
    assert(!edge_is_deleted_debug(data, s));
    assert(bundle_edge_is_member(p, s));

    /* find top-most and bottom most edge */
    edge_t *t = s;
    for (int i CP_UNUSED = 0;;) {
        assert(i++ < 100);
        edge_t *u = tree_edge_next(t);
        if (u == NULL) {
            break;
        }
        if (!intersection_eq(&ev->vec2if, t, u)) {
            break;
        }
        t = u;
    }
    assert(t != NULL);

    for (int i CP_UNUSED = 0;;) {
        assert(i++ < 100);
        edge_t *r = tree_edge_prev(s);
        if (r == NULL) {
            break;
        }
        if (!intersection_eq(&ev->vec2if, s, r)) {
            break;
        }
        s = r;
    }
    assert(s != NULL);
    if (s == t) {
        return;
    }

    /* reverse order */
    for (int i CP_UNUSED = 0;;) {
        assert(s != t);

#ifndef NDEBUG
        edge_t *t2a = tree_edge_prev(t);
#endif

        bundle_edge_swap(p, s, t);

        edge_t *t2 = tree_edge_prev(s);
        assert((t2a == s) || (t2a == t2));
        assert(t2 != NULL);
        if (t2 == t) {
            break;
        }

        edge_t *s2 = tree_edge_next(t);
        assert(s2 != NULL);
        if (s2 == t2) {
            break;
        }

        s = s2;
        t = t2;
    }
}

#if DEBUG
static inline void dump_bundle_rec(
    cp_dict_t *r,
    cp_dict_t *bot,
    cp_dict_t *top)
{
    if (r == NULL) {
        return;
    }
    dump_bundle_rec(r->edge[1], bot, top);
    edge_t *e = tree_get_edge(r);
    fprintf(stderr, "DEBUG %u:    %d %d -- %d %d%s\n", ++debug,
        e->left.x, e->left.y, e->rigt.x, e->rigt.y,
        r == bot ? (r == top ? " <= BOT,TOP" : " <= BOT ") :
        r == top ? " <= TOP " :
        "");
    dump_bundle_rec(r->edge[0], bot, top);
}

static inline void dump_bundle(
    bundle_t *b)
{
    if (b == NULL) {
        fprintf(stderr, "DEBUG %u:    NULL\n", ++debug);
        return;
    }
    if (b->bundle.root == NULL) {
        fprintf(stderr, "DEBUG %u:    NIL\n", ++debug);
        return;
    }
    dump_bundle_rec(b->bundle.root, b->bundle.bot, b->bundle.top);
}

static inline void dump_bundle_tree_rec(
    cp_dict_t *r)
{
    if (r == NULL) {
        return;
    }
    dump_bundle_tree_rec(r->edge[1]);
    bundle_t *b = state_get_bundle(r);
    assert(b->bundle.bot == cp_dict_min(b->bundle.root));
    assert(b->bundle.top == cp_dict_max(b->bundle.root));
    fprintf(stderr, "DEBUG %u: tree %p: %d %d --\n", ++debug, b, b->x, b->y);
    dump_bundle(b);
    dump_bundle_tree_rec(r->edge[0]);
}

static inline void dump_bundle_tree(
    data_t *data)
{
    if (data->state == NULL) {
        fprintf(stderr, "DEBUG %u:    EMPTY\n", ++debug);
        return;
    }
    dump_bundle_tree_rec(data->state);
}
#endif /* DEBUG */

static void sweep_phase2_pass(
    data_t *data,
    phase_t phase)
{
    /* prepare environment */
    data->phase = phase;
    data->agenda_vertex_cmp = cq_sweep_agenda_vertex_phase2_cmp;
    data->agenda_xing_cmp = cq_sweep_agenda_xing_phase2_cmp;
    assert(data->agenda_xing == NULL);

    /* put the unique crossings into the agenda */
    for (cp_v_eachv(e, data->xings)) {
        bool south CP_UNUSED = phase_south(data);
        assert((e->some_edge_tb[south] == NULL) ||
            !edge_is_deleted_debug(data, e->some_edge_tb[south]));
        xing_t *x CP_UNUSED = agenda_xing_insert(data, e);
        assert(x == NULL); /* all crossings should be unique in phase 2 */
    }

    /* put the vertices of all edges into the agenda
     * while we're at it, remove the deleted ones by swapping with
     * the last element.
     */
    for (cp_v_eachv(e, data->edges)) {
        assert(!edge_is_deleted_debug(data, e)); /* edge array should be swept */
        assert(e->in_tree.parent == NULL);
        /* init member sum */
        e->sum_member = e->member;

        /* insert */
        agenda_vertex_insert(data, &e->left);
        agenda_vertex_insert(data, &e->rigt);
    }

    /* run algorithm loop:
     * next event is either from data->agenda or from data->xing */
    while (data->agenda_vertex_min || data->agenda_xing_min) {
        /* make a new bundle, but don't dequeue yet */
        bundle_t *p = bundle_new_from_agenda(data);
        cq_sweep_trace_begin_page(data, NULL, NULL, p, NULL);

        /* maybe there is a crossing? */
        bundle_t *cur = state_bundle_find_bot(data, p);
        if (cur != NULL) {
            /* iterate all bundles we find and split the tree, starting
             * with the bottom bundle.  Only one bundle can have truly lower
             * edges and only one truly upper edges.  There may be several
             * in the middle that cross the pixel.  A single bundle can do
             * all three things: have lower, crossing, and higher edges,
             * but then this bundle is the only bundle (otherwise there'd be
             * crossings we would have encountered before). */
            assert(cur->bundle.root != NULL);

             /* split off lower bunch: min :< p, max :>= p */
            bundle_t *min = cur;
            cp_dict_t *lo_tree = bundle_split(cur, p, 1);
            if (lo_tree == NULL) {
                min = NULL; /* do not keep lowest bundle if empty */
            }
            assert(cur->bundle.root != NULL);

            cp_dict_t *equ = NULL;
            do {
                assert(cur->bundle.root != NULL);
                /* split off upper bunch: min :<= p, max :> p */
                cp_dict_t *tmp = bundle_split(cur, p, 0);
                if (tmp == NULL) {
                    /* nothing crosses => end of iteration */
                    assert(cur->bundle.root != NULL);
                    break;
                }

                /* found edge */
                bundle_edge_new(data, &cur->vec2, &p->vec2, tmp);

                /* Join new bundle: start with same order, we'll swap later */
                equ = bundle_join(equ, tmp);

                if (cur->bundle.root != NULL) {
                    /* some lines pass above => last bundle */
                    break;
                }

                /* try to move to next bundle */
                bundle_t *next = state_bundle_next(cur);

                /* remove this bundle unless it's the lowest one (and is non-empty). */
                if (cur != min) {
                    state_bundle_remove(data, cur);
                    BUNDLE_DELETE(data, cur);
                }

                /* move up */
                cur = next;
            } while (cur != NULL);

            /* need both min and max => copy node */
            assert((cur == NULL) || (cur->bundle.root != NULL));
            if (min != NULL) {
                if (cur == min) {
                    /* make a copy */
                    assert(lo_tree != NULL);
                    min = bundle_new(data, cur->x, cur->y);
                    min->bundle.root = lo_tree;
                    min->bundle.bot = cp_dict_min(min->bundle.root);
                    min->bundle.top = cp_dict_max(min->bundle.root);
                    state_bundle_insert_at(data, min, cur, 0);
                }
                else {
                    assert(min->bundle.root == NULL); /* should have been consumed */
                    /* use the old object */
                    min->bundle.root = lo_tree;
                    min->bundle.bot = cp_dict_min(min->bundle.root);
                    min->bundle.top = cp_dict_max(min->bundle.root);
                }
            }

            /* equ is the bundle for the new node */
            p->bundle.root = equ;
        }

        /* now "sweep" over the pixel: swap, add, or remove edges in p */
        for (;;) {
            int i = agenda_next_event_cmp(data, &p->vec2);
            if (i == 0) {
                break;
            }
            if (i == 1) {
                vertex_t *v = agenda_vertex_extract_min(data);
                edge_t *e = edge_of(v);
                /* insert/remove during pass 1 or pass 2, depending on slope */
                if (phase_south(data) == edge_south(e)) {
                    if (v->side == LEFT) {
                        bundle_edge_insert(p, e);
                    }
                    else {
                        bundle_edge_remove(p, e);
                        /* EDGE_DELETE(data, e); we need the vertices in pass 2 */
                    }
                }
            }
            else {
                assert(i == 2);
                xing_t *c = agenda_xing_extract_min(data);
                /* swap edge order */
                sweep_phase2_edge_reverse(data, p, c);
            }
        }

        if (p->bundle.root) {
            /* set min/max */
            p->bundle.bot = cp_dict_min(p->bundle.root);
            p->bundle.top = cp_dict_max(p->bundle.root);

            /* insert */
            state_bundle_insert(data, p);
        }
        else {
            BUNDLE_DELETE(data, p);
        }

        cq_sweep_trace_end_page(data);
    }
}

static inline void sweep_phase2_snap_round(
    data_t *data)
{
    /* PHASE 2: round intersections and route segments through them
     * This is only needed if there are non-integer intersections.
     */

    /* remove deleted edges from ursegment array */
    for (cp_v_eachp(ep, data->edges)) {
        if (edge_is_deleted(data, *ep)) {
            CP_SWAP(ep, &cp_v_last(data->edges));
            cp_v_pop(data->edges);
            cp_redo(ep);
        }
    }

    /* in phase2, edges are not added to the 'edges' array */

    /* run the sweep phase2 sweep line twice for different segment slopes */
    sweep_phase2_pass(data, SNAP_NORTH);
    sweep_phase2_pass(data, SNAP_SOUTH);

    cq_sweep_trace_begin_page(data, NULL, NULL, NULL, NULL);
    PSPR("0 0 0 setrgbcolor %g %g moveto (intersections: %lu) show\n",
        cq_ps_left(), cq_ps_line_y(data->ps_line++), data->xings->size);
    cq_sweep_trace_end_page(data);
}

/* ********************************************************************** */
/* cq_sweep_*: main intersection algorithm */

extern void cq_sweep_intersect(
    data_t *data)
{
    sweep_phase1_find_intersections(data);

    /* always run phase 2, even with no intersections, to round also
     * ursegment end points properly */
    sweep_phase2_snap_round(data);
}

extern void cq_sweep_delete(
    data_t *data)
{
    cp_v_fini_alloc(data->tmp->alloc, data->edges);
    cp_v_fini_alloc(data->tmp->alloc, data->xings);
}

extern void cq_sweep_get_v_line2(
    cq_v_line2_t *gon,
    cq_sweep_t *data)
{
    for (cp_dict_each(o_, data->result)) {
        vertex_t *o1 = agenda_get_vertex(o_);
        if (o1->side != LEFT) {
            continue;
        }
        edge_t *o = edge_of(o1);
        cp_v_push(gon, CQ_LINE2(o->left.vec2, o->rigt.vec2));
    }
}

extern bool cq_sweep_empty(
    cq_sweep_t *data)
{
    for (cp_dict_each(o_, data->result)) {
        return false;
    }
    return true;
}

extern cq_v_line2_t *cq_sweep_copy(
    cp_pool_t *tmp,
    cq_v_line2_t const *gon)
{
    size_t size_cnt = gon->size;
    cq_v_line2_t *r = NEW(*r);

    if (size_cnt >= 3) {
        cq_sweep_t *s = cq_sweep_new(tmp, NULL, size_cnt);
        cq_sweep_add_v_line2(s, gon, 1);

        cq_sweep_intersect(s);

        cp_v_init0(r, size_cnt - 2);
        cp_v_set_size(r, 0);
        cq_sweep_get_v_line2(r, s);

        cq_sweep_delete(s);
    }

    return r;
}

/* ********************************************************************** */

extern void cq_sweep_minmax(
    cq_vec2_minmax_t *bb,
    cq_sweep_t *data)
{
    for (cp_v_eachv(o, data->edges)) {
        if (edge_is_deleted(data, o)) {
            continue;
        }
        cq_vec2_minmax(bb, &o->left.vec2);
        cq_vec2_minmax(bb, &o->rigt.vec2);
    }
}

/* ********************************************************************** */
/* cq_has_intersect */

extern int cq_has_intersect(
    cq_vec2if_t *ip,
    cq_line2_t const **ap,
    cq_line2_t const **bp,
    cq_v_line2_t const *gon)
{
    for (cp_v_eachp(a, gon)) {
        for (cp_v_eachp(b, gon)) {
            cq_vec2if_t i;
            switch (cq_line2if_intersect(&i, a, b)) {
            default:
                break;

            case 1:
                goto has;

            case -1:
                /* parallel or collinear? */
                if (cq_vec2_right_cross3_z(&a->b, &a->a, &b->b) == 0) {
                    /* collinear */

                    /* squares of distances */
                    cq_dim_t a1, a2, b1, b2;
                    if (a->a.x == a->b.x) { /* vertical */
                        a1 = a->a.y;
                        a2 = a->b.y;
                        b1 = b->a.y;
                        b2 = b->b.y;
                    }
                    else {
                        a1 = a->a.x;
                        a2 = a->b.x;
                        b1 = b->a.x;
                        b2 = b->b.x;
                    }
                    if (a1 > a2) { CP_SWAP(&a1, &a2); }
                    if (b1 > b2) { CP_SWAP(&b1, &b2); }
                    assert(a1 != a2);
                    assert(b1 != b2);

                    if ((b1 < a1) && (a1 < b2)) {
                        i = CQ_VEC2IFI(a->a.x, a->a.y);
                        goto has;
                    }
                    if ((b1 < a2) && (a2 < b2)) {
                        i = CQ_VEC2IFI(a->b.x, a->b.y);
                        goto has;
                    }
                    if ((a1 < b1) && (b1 < a2)) {
                        i = CQ_VEC2IFI(b->a.x, b->a.y);
                        goto has;
                    }
                    if ((a1 < b2) && (b2 < a2)) {
                        i = CQ_VEC2IFI(b->b.x, b->b.y);
                        goto has;
                    }
                }
            }
            continue;

        has:
            if (ip) { *ip = i; }
            if (ap) { *ap = a; }
            if (bp) { *bp = b; }
            return 1;
        }
    }
    return 0;
}
