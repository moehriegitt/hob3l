/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */
/*
 * This is adapted from Francisco Martinez del Rio (2011), v1.4.1.
 * See: http://www4.ujaen.es/~fmartin/bool_op.html
 *
 * The inside/outside idea is the same as described by Sean Conelly in his
 * polybooljs project.
 * See: https://github.com/voidqk/polybooljs
 *
 * The Conelly idea is also a bit complicated, and this library uses xor based
 * bit masks instead, which may be less obvious, but also allows the algorithm
 * to handle polygons with self-overlapping edges.  This feature is not
 * exploited, but I could remove an error case.  The bitmasks allow extension
 * to more than 2 polygons.  The boolean function is stored in a bitmask that
 * maps in/out masks for multiple polygons to a single bit.
 *
 * This implements most of the algorithm using dictionaries instead of, say,
 * a heap for the pqueue.  This avoids 'realloc' and makes it easier to use
 * pool memory.  BSTs worst case is just as good (and we do not need to merge
 * whole pqueue trees, but have only insert/remove operations).
 *
 * The polygons output by this algorithm have no predefined point
 * direction and are always non-self-intersecting and disjoint (except
 * for single points) but there may be holes.  The subsequent triangulation
 * algorithm does not care about point order -- it determines the
 * inside/outside information implicitly and outputs triangles in the correct
 * point order.  But for generating the connective triangles between two 2D
 * layers for the STL output, the paths output by this algorithm must have
 * the correct point order so that STL can compute the correct normal for those
 * triangles.  Therefore, this algorithm also takes care of getting the path
 * point order right.
 */

#define DEBUG 0

#include <stdio.h>
#include <hob3lbase/dict.h>
#include <hob3lbase/list.h>
#include <hob3lbase/ring.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/pool.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/panic.h>
#include <hob3l/csg2.h>
#include <hob3l/ps.h>
#include <hob3l/csg2-bitmap.h>
#include "internal.h"

typedef struct event event_t;

/**
 * Points found by algorithm
 */
typedef struct {
    cp_dict_t node_pt;

    cp_vec2_t coord;
    cp_loc_t loc;

    /**
     * Index in output point array.
     * Initialised to CP_SIZE_MAX.
     */
    size_t idx;

    /**
     * Number of times this point is used in the resulting polygon. */
    size_t path_cnt;
} point_t;

typedef CP_VEC_T(point_t*) v_point_p_t;

/**
 * Events when the algorithm progresses.
 * Points with more info in the left-right plain sweep.
 */
struct event {
    /**
     * Storage in s and chain is mutually exclusive so we
     * use a union here.
     */
    union {
        /**
         * Node for storing in ctxt::s */
        cp_dict_t node_s;

        /**
         * Node for connecting nodes into a ring (there is no
         * root node, but polygon starts are found by using
         * ctxt::poly and starting from the edge that was inserted
         * there. */
        cp_ring_t node_chain;
    };

    /**
     * Storage in q, end, and poly is mutually exclusive,
     * so we use a union here.
     */
    union {
        /** Node for storing in ctxt::q */
        cp_dict_t node_q;
        /** Node for storing in ctxt::end */
        cp_dict_t node_end;
        /** Node for storing in ctxt::poly */
        cp_list_t node_poly;
    };

    cp_loc_t loc;
    point_t *p;
    event_t *other;

    struct {
        /**
         * Mask of poly IDs that have this edge.  Due to overlapping
         * edges, this is a set.  For self-overlapping edges, the
         * corresponding bit is the lowest bit of the overlapped edge
         * count.
         * This mask can be used to compute 'above' from 'below',
         * because a polygon edge will change in/out for a polygon:
         * above = below ^ owner.
         */
        size_t owner;

        /**
         * Mask of whether 'under' this edge, it is 'inside' of the
         * polygon.  Each bit corresponds to inside/outside of the
         * polygon ID corresponding to that bit number.  This is only
         * maintained while the edge is in s, otherwise, only owner and
         * start are used.
         */
        size_t below;
    } in;

    /**
     * Whether this is a left edge (false = right edge)*/
    bool left;

    /**
     * Whether the event point is already part of a path. */
    bool used;

    /**
     * line formular cache to compute intersections with the same
     * precision throughout the algorithm */
    struct {
        /** slope */
        double a;
        /** offset */
        double b;
        /** false: use ax+b; true: use ay+b */
        bool swap;
    } line;

#ifdef PSTRACE
    /**
     * For debug printing */
    size_t debug_tag;
#endif
};

#define _LINE_X(swap,c) ((c)->v[(swap)])
#define _LINE_Y(swap,c) ((c)->v[!(swap)])

/**
 * Accessor of the X or Y coordinate, depending on line.swap.
 * This returns X if not swapped, Y otherwise.
 */
#define LINE_X(e,c) _LINE_X((e)->line.swap, c)

/**
 * Accessor of the X or Y coordinate, depending on line.swap.
 * This returns Y if not swapped, X otherwise.
 */
#define LINE_Y(e,c) _LINE_Y((e)->line.swap, c)


typedef CP_VEC_T(event_t*) v_event_p_t;

/**
 * All data needed during the algorithms runtime.
 */
typedef struct {
    /** Memory pool to use */
    cp_pool_t *pool;

    /** Error output */
    cp_err_t *err;

    /** new points found by the algorithm */
    cp_dict_t *pt;

    /** pqueue of events */
    cp_dict_t *q;

    /** sweep line status */
    cp_dict_t *s;

    /** output segments in a dictionary of open ends */
    cp_dict_t *end;

    /** list of output polygon points (note that a single polyogn
      * may be inserted multiple times) */
    cp_list_t poly;

    /** Bool function bitmap */
    cp_csg2_op_bitmap_t *comb;

    /** Number of valid bits in comb */
    size_t comb_size;

    /** Whether to output all points or to drop those of adjacent collinear
     * lines. */
    bool all_points;
} ctxt_t;

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
static char const *__pt_str(char *s, size_t n, point_t const *x)
{
    if (x == NULL) {
        return "NULL";
    }
    snprintf(s, n, FD2, CP_V01(x->coord));
    s[n-1] = 0;
    return s;
}

#define pt_str(p) __pt_str((char[50]){}, 50, p)

__unused
static char const *__ev_str(char *s, size_t n, event_t const *x)
{
    if (x == NULL) {
        return "NULL";
    }
    if (x->left) {
        snprintf(s, n, "#("FD2"--"FD2")  o0x%"_Pz"x b0x%"_Pz"x",
            CP_V01(x->p->coord),
            CP_V01(x->other->p->coord),
            x->in.owner,
            x->in.below);
    }
    else {
        snprintf(s, n, " ("FD2"--"FD2")# o0x%"_Pz"x b0x%"_Pz"x",
            CP_V01(x->other->p->coord),
            CP_V01(x->p->coord),
            x->in.owner,
            x->in.below);
    }
    s[n-1] = 0;
    return s;
}

#define ev_str(x) __ev_str((char[80]){}, 80, x)

#ifdef PSTRACE
static void debug_print_chain(
    event_t *e0,
    size_t tag)
{
    if (e0->debug_tag == tag) {
        return;
    }

    e0->debug_tag = tag;
    cp_printf(cp_debug_ps, "newpath %g %g moveto", CP_PS_XY(e0->p->coord));

    event_t *e1 = CP_BOX_OF(cp_ring_step(&e0->node_chain, 0), event_t, node_chain);
    if (e0 == e1) {
        e1 = CP_BOX_OF(cp_ring_step(&e0->node_chain, 1), event_t, node_chain);
    }
    assert(e0 != e1);

    e1->debug_tag = tag;
    cp_printf(cp_debug_ps, " %g %g lineto", CP_PS_XY(e1->p->coord));

    event_t *ey __unused = e0;
    event_t *ez __unused = e1;
    bool close = false;
    for (cp_ring_each(_ei, &e0->node_chain, &e1->node_chain)) {
        event_t *ei = CP_BOX_OF(_ei, event_t, node_chain);
        ez = ei;
        ei->debug_tag = tag;
        cp_printf(cp_debug_ps, " %g %g lineto", CP_PS_XY(ei->p->coord));
        close = !cp_ring_is_end(_ei);
    }
    if (close) {
        cp_printf(cp_debug_ps, " closepath");
    }
    cp_printf(cp_debug_ps, " stroke\n");
    if (!close && !cp_ring_is_end(&e0->node_chain)) {
        /* find other end */
        cp_printf(cp_debug_ps, "newpath %g %g moveto", CP_PS_XY(e0->p->coord));
        for (cp_ring_each(_ei, &e1->node_chain, &e0->node_chain)) {
            event_t *ei = CP_BOX_OF(_ei, event_t, node_chain);
            ey = ei;
            ei->debug_tag = tag;
            cp_printf(cp_debug_ps, " %g %g lineto", CP_PS_XY(ei->p->coord));
        }
        cp_printf(cp_debug_ps, " stroke\n");
    }

    if (!close) {
        cp_debug_ps_dot(CP_PS_XY(ey->p->coord), 7);
        cp_debug_ps_dot(CP_PS_XY(ez->p->coord), 7);
    }
}
#endif

#if DEBUG || defined(PSTRACE)

static void debug_print_s(
    ctxt_t *c,
    char const *msg,
    event_t *es,
    event_t *epr __unused,
    event_t *ene __unused)
{
#if DEBUG
    LOG("S %s\n", msg);
    for (cp_dict_each(_e, c->s)) {
        event_t *e = CP_BOX_OF(_e, event_t, node_s);
        LOG("S: %s\n", ev_str(e));
    }
#endif

#ifdef PSTRACE
    /* output to postscript */
    if (cp_debug_ps_page_begin()) {
        /* print info */
        cp_printf(cp_debug_ps, "30 30 moveto (CSG: %s) show\n", msg);
        cp_printf(cp_debug_ps, "30 45 moveto (%s =prev) show\n", epr ? ev_str(epr) : "NULL");
        cp_printf(cp_debug_ps, "30 60 moveto (%s =this) show\n", es  ? ev_str(es)  : "NULL");
        cp_printf(cp_debug_ps, "30 75 moveto (%s =next) show\n", ene ? ev_str(ene) : "NULL");

        /* sweep line */
        cp_printf(cp_debug_ps, "0.8 setgray 1 setlinewidth\n");
        cp_printf(cp_debug_ps,
            "newpath %g dup 0 moveto %u lineto stroke\n",
            CP_PS_X(es->p->coord.x),
            CP_PS_PAPER_Y);
        if (!es->left) {
            cp_printf(cp_debug_ps,
                "2 setlinewidth newpath %g %g moveto %g %g lineto stroke\n",
                CP_PS_XY(es->p->coord),
                CP_PS_XY(es->other->p->coord));
        }

        /* pt */
        cp_printf(cp_debug_ps, "0.8 setgray\n");
        for (cp_dict_each(_p, c->pt)) {
            point_t *p = CP_BOX_OF(_p, point_t, node_pt);
            cp_debug_ps_dot(CP_PS_XY(p->coord), 3);
        }

        /* s */
        cp_printf(cp_debug_ps, "3 setlinewidth\n");
        size_t i = 0;
        for (cp_dict_each(_e, c->s)) {
            event_t *e = CP_BOX_OF(_e, event_t, node_s);
            cp_printf(cp_debug_ps,
                "0 %g 0 setrgbcolor\n", three_steps(i));
            cp_debug_ps_dot(CP_PS_XY(e->p->coord), 3);
            cp_printf(cp_debug_ps,
                "newpath %g %g moveto %g %g lineto stroke\n",
                CP_PS_XY(e->p->coord),
                CP_PS_XY(e->other->p->coord));
            i++;
        }

        /* chain */
        cp_printf(cp_debug_ps, "4 setlinewidth\n");
        i = 0;
        for (cp_dict_each(_e, c->end)) {
            cp_printf(cp_debug_ps, "1 %g 0 setrgbcolor\n", three_steps(i) * 0.6);
            event_t *e0 = CP_BOX_OF(_e, event_t, node_end);
            cp_debug_ps_dot(CP_PS_XY(e0->p->coord), 4);
            debug_print_chain(e0, cp_debug_ps_page_cnt);
            i++;
        }

        /* poly */
        cp_printf(cp_debug_ps, "2 setlinewidth\n");
        i = 0;
        for (cp_list_each(_e, &c->poly)) {
            cp_printf(cp_debug_ps, "0 %g 0.8 setrgbcolor\n", three_steps(i));
            event_t *e0 = CP_BOX_OF(_e, event_t, node_poly);
            cp_debug_ps_dot(CP_PS_XY(e0->p->coord), 4);
            debug_print_chain(e0, ~cp_debug_ps_page_cnt);
            i++;
        }

        /* end page */
        cp_ps_page_end(cp_debug_ps);
    }
#endif
}

#else
#define debug_print_s(...) ((void)0)
#endif

/**
 * Compare two points
 */
static int pt_cmp(
    point_t const *a,
    point_t const *b)
{
    if (a == b) {
        return 0;
    }
    return cp_vec2_lex_pt_cmp(&a->coord, &b->coord);
}

/**
 * Compare a vec2 with a point in a dictionary.
 */
static int pt_cmp_d(
    cp_vec2_t *a,
    cp_dict_t *_b,
    void *user __unused)
{
    point_t *b = CP_BOX_OF(_b, point_t, node_pt);
    return cp_vec2_lex_pt_cmp(a, &b->coord);
}

static cp_dim_t rasterize(cp_dim_t v)
{
    return cp_pt_epsilon * round(v / cp_pt_epsilon);
}

/**
 * Allocate a new point and remember in our point dictionary.
 *
 * This will either return a new point or one that was found already.
 */
static point_t *pt_new(
    ctxt_t *c,
    cp_loc_t loc,
    cp_vec2_t const *_coord)
{
    cp_vec2_t coord = {
       .x = rasterize(_coord->x),
       .y = rasterize(_coord->y),
    };

    /* normalise coordinates around 0 to avoid funny floats */
    if (cp_eq(coord.x, 0)) { coord.x = 0; }
    if (cp_eq(coord.y, 0)) { coord.y = 0; }

    cp_dict_ref_t ref;
    cp_dict_t *pt = cp_dict_find_ref(&ref, &coord, c->pt, pt_cmp_d, NULL, 0);
    if (pt != NULL) {
        return CP_BOX_OF(pt, point_t, node_pt);
    }

    point_t *p = CP_POOL_NEW(c->pool, *p);
    p->loc = loc;
    p->coord = coord;
    p->idx = CP_SIZE_MAX;

    LOG("new pt: %s (orig: "FD2")\n", pt_str(p), CP_V01(*_coord));

    cp_dict_insert_ref(&p->node_pt, &ref, &c->pt);
    return p;
}

/**
 * Allocate a new event
 */
static event_t *ev_new(
    ctxt_t *c,
    cp_loc_t loc,
    point_t *p,
    bool left,
    event_t *other)
{
    event_t *r = CP_POOL_NEW(c->pool, *r);
    r->loc = loc;
    r->p = p;
    r->left = left;
    r->other = other;
    return r;
}

/**
 * bottom/top compare of edge pt1--pt2 vs point pt: bottom is smaller, top is larger
 */
static inline int pt2_pt_cmp(
    point_t const *a1,
    point_t const *a2,
    point_t const *b)
{
    return cp_vec2_right_normal3_z(&a1->coord, &a2->coord, &b->coord);
}

static inline point_t *left(event_t const *ev)
{
    return ev->left ? ev->p : ev->other->p;
}

static inline point_t *right(event_t const *ev)
{
    return ev->left ? ev->other->p : ev->p;
}

/**
 * Event order in Q: generally left (small) to right (large):
 *    - left coordinates before right coordinates
 *    - bottom coordinates before top coordinates
 *    - right ends before left ends
 *    - points below an edge before points above an edge
 */
static int ev_cmp(event_t const *e1, event_t const *e2)
{
    /* Different points compare with different comparison */
    if (e1->p != e2->p) {
        int i = pt_cmp(e1->p, e2->p);
        assert((i != 0) && "Same coordinates found in different point objects");
        return i;
    }

    /* right vs left endpoint?  right comes first (= is smaller) */
    int i = e1->left - e2->left;
    if (i != 0) {
        return i;
    }

    /* same endpoint, same direction: lower edge comes first
     * Note that this might still return 0, making the events equal.
     * This is OK, it's collinear segments with the same endpoint and
     * direction.  These will be split later, processing order does
     * not matter.
     */
    return pt2_pt_cmp(left(e1), right(e1), e2->other->p);
}

/**
 * Segment order in S: generally bottom (small) to top (large)
 *
 * This was ported from a C++ Less() comparison, which seems to
 * pass the new element as second argument.  Our data structures
 * pass the new element as first argument, and in some cases,
 * this changes the order of edges (if the left end point of the
 * new edge is on an existing edge).  Therefore, we have
 * __seg_cmp() and seg_cmp() to swap arguments.
 * Well, this essentially means that this function is broken, because
 * it should hold that seg_cmp(a,b) == -seg_cmp(b,a), but it doesn't.
 * Some indications is clearly mapping -1,0,+1 to -1,-1,+1...
 */
static int __seg_cmp(event_t const *e1, event_t const *e2)
{
    /* Only left edges are inserted into S */
    assert(e1->left);
    assert(e2->left);

    if (e1 == e2) {
        return 0;
    }

    int e1_p_cmp = pt2_pt_cmp(e1->p, e1->other->p, e2->p);
    int e1_o_cmp = pt2_pt_cmp(e1->p, e1->other->p, e2->other->p);

    LOG("seg_cmp: %s vs %s: %d %d\n", ev_str(e1), ev_str(e2), e1_p_cmp, e1_o_cmp);

    if ((e1_p_cmp != 0) || (e1_o_cmp != 0)) {
        /* non-collinear */
        /* If e2->p is on e1, use right endpoint location to compare */
        if (e1_p_cmp == 0) {
            return e1_o_cmp;
        }

        /* different points */
        if (ev_cmp(e1, e2) > 0) {
            /* e2 is above e2->p? => e1 is below */
            return pt2_pt_cmp(e2->p, e2->other->p, e1->p) >= 0 ? -1 : +1;
        }

        /* e1 came first */
        return e1_p_cmp <= 0 ? -1 : +1;
    }

    /* segments are collinear. some consistent criterion is used for comparison */
    if (e1->p == e2->p) {
        return (e1 < e2) ? -1 : +1;
    }

    /* compare events */
    return ev_cmp(e1, e2);
}

static int seg_cmp(event_t const *e2, event_t const *e1)
{
    return -__seg_cmp(e1,e2);
}

/** dict version of ev_cmp for node_q */
static int ev_cmp_q(
    cp_dict_t *_e1,
    cp_dict_t *_e2,
    void *user __unused)
{
    event_t *e1 = CP_BOX_OF(_e1, event_t, node_q);
    event_t *e2 = CP_BOX_OF(_e2, event_t, node_q);
    return ev_cmp(e1, e2);
}
/** dict version of seg_cmp for node_s */
static int seg_cmp_s(
    cp_dict_t *_e1,
    cp_dict_t *_e2,
    void *user __unused)
{
    event_t *e1 = CP_BOX_OF(_e1, event_t, node_s);
    event_t *e2 = CP_BOX_OF(_e2, event_t, node_s);
    return seg_cmp(e1, e2);
}

static void q_insert(
    ctxt_t *c,
    event_t *e)
{
    assert((pt_cmp(e->p, e->other->p) < 0) == e->left);
    cp_dict_insert(&e->node_q, &c->q, ev_cmp_q, NULL, 1);
}

static void q_remove(
    ctxt_t *c,
    event_t *e)
{
    cp_dict_remove(&e->node_q, &c->q);
}

static inline event_t *q_extract_min(ctxt_t *c)
{
    return CP_BOX0_OF(cp_dict_extract_min(&c->q), event_t, node_q);
}

static void s_insert(
    ctxt_t *c,
    event_t *e)
{
    cp_dict_t *o __unused = cp_dict_insert(&e->node_s, &c->s, seg_cmp_s, NULL, 0);
    assert(o == NULL);
}

static void s_remove(
    ctxt_t *c,
    event_t *e)
{
    cp_dict_remove(&e->node_s, &c->s);
}

__unused
static void get_coord_on_line(
    cp_vec2_t *r,
    event_t *e,
    cp_vec2_t const *p)
{
    LINE_X(e,r) = LINE_X(e,p);
    LINE_Y(e,r) = e->line.b + (e->line.a * LINE_X(e,p));
}

static void q_add_orig(
    ctxt_t *c,
    cp_loc_t loc,
    cp_vec2_t *coord1,
    cp_vec2_t *coord2,
    size_t poly_id)
{
    point_t *p1 = pt_new(c, loc, coord1);
    point_t *p2 = pt_new(c, loc, coord2);

    if (p1 == p2) {
        /* edge consisting of only one point (or two coordinates
         * closer than pt_epsilon collapsed) */
        return;
    }

    event_t *e1 = ev_new(c, loc, p1, true,  NULL);
    e1->in.owner = ((size_t)1) << poly_id;

    event_t *e2 = ev_new(c, loc, p2, false, e1);
    e2->in = e1->in;
    e1->other = e2;

    if (pt_cmp(e1->p, e2->p) > 0) {
        e1->left = false;
        e2->left = true;
    }

    /* compute origin and slope */
    cp_vec2_t d;
    d.x = e2->p->coord.x - e1->p->coord.x;
    d.y = e2->p->coord.y - e1->p->coord.y;
    e1->line.swap = cp_lt(fabs(d.x), fabs(d.y));
    e1->line.a = LINE_Y(e1, &d) / LINE_X(e1, &d);
    e1->line.b = LINE_Y(e1, &e1->p->coord) - (e1->line.a * LINE_X(e1, &e1->p->coord));
    assert(cp_le(e1->line.a, +1));
    assert(cp_ge(e1->line.a, -1) ||
        CONFESS("a=%g (%g,%g--%g,%g)",
            e1->line.a, e1->p->coord.x, e1->p->coord.y, e2->p->coord.x, e2->p->coord.y));

    /* other direction edge is on the same line */
    e2->line = e1->line;

#ifndef NDEBUG
    /* check computation */
    cp_vec2_t g;
    get_coord_on_line(&g, e1, &e2->p->coord);
    assert(cp_vec2_eq(&g, &e2->p->coord));
    get_coord_on_line(&g, e2, &e1->p->coord);
    assert(cp_vec2_eq(&g, &e1->p->coord));
#endif

    /* Insert.  For 'equal' entries, order does not matter */
    q_insert(c, e1);
    q_insert(c, e2);
}

#ifndef NDEBUG
#  define divide_segment(c,e,p)  __divide_segment(__FILE__, __LINE__, c, e, p)
#else
#  define __divide_segment(f,l,c,e,p) divide_segment(c,e,p)
#endif

static void __divide_segment(
    char const *file __unused,
    int line __unused,
    ctxt_t *c,
    event_t *e,
    point_t *p)
{
    assert(p != e->p);
    assert(p != e->other->p);

    assert(e->left);
    event_t *o = e->other;

    assert(!cp_dict_is_member(&o->node_s));

    /*
     * Split an edge at a point p on that edge (we assume that p is correct -- no
     * check is done).
     *      p              p
     * e-------.       e--.l--.
     *  `-------o       `--r`--o
     */

    event_t *r = ev_new(c, p->loc, p, false, e);
    event_t *l = ev_new(c, p->loc, p, true,  o);

    /* relink buddies */
    o->other = l;
    e->other = r;
    assert(r->other == e);
    assert(l->other == o);

    /* copy in/out tracking -- the caller must set this up appropriately */
    r->in = e->in;
    l->in = o->in;

    /* copy edge slope and offset */
    l->line = r->line = e->line;

    /* If the middle point is rounded, the order of l and o may
     * switch.  This must not happen with e--r, because e is already
     * processed, so we'd need to go back in time to fix.
     * Any caller must make sure that p is in the correct place wrt.
     * e, in particular 'find_intersection', which computes a new point.
     */
    if (ev_cmp(l, o) > 0) {
        /* for the unprocessed part, we can fix the anomality by swapping. */
        o->left = true;
        l->left = false;
    }

    /* For e--r, if we encounter the same corner case, remove the edges from S
     * and put it back into Q -- this should work because the edges were adjacent,
     * we we can process them again. */
    if (ev_cmp(e, r) > 0) {
        r->left = true;
        e->left = false;
        if (cp_dict_is_member(&e->node_s)) {
            s_remove(c, e);
            q_insert(c, e);
        }
    }

    /* handle new events later */
    q_insert(c, l);
    q_insert(c, r);
}

/**
 * Compare two nodes for insertion into c->end.
 * For correct insertion order (selection of end node for
 * comparison), be sure to connect the node before
 * inserting.
 */
static int pt_cmp_end_d(
    cp_dict_t *_a,
    cp_dict_t *_b,
    void *user __unused)
{
    event_t *a = CP_BOX_OF(_a, event_t, node_end);
    event_t *b = CP_BOX_OF(_b, event_t, node_end);
    return pt_cmp(a->p, b->p);
}

/**
 * Try to insert a node into a the polygon chain end store.
 * If a duplicate is found, extract and return it instead of inserting e.
 */
static event_t *chain_insert_or_extract(
    ctxt_t *c,
    event_t *e)
{
    LOG("insert %s\n", ev_str(e));
    cp_dict_t *_r = cp_dict_insert(&e->node_end, &c->end, pt_cmp_end_d, NULL, 0);
    if (_r == NULL) {
        return NULL;
    }
    cp_dict_remove(_r, &c->end);
    return CP_BOX_OF(_r, event_t, node_end);
}

/**
 * Connect an edge e to a polygon point o1 that may already be
 * connected to more points.
 */
static void chain_join(
    event_t *o1,
    event_t *e)
{
    LOG("join   %s with %s\n", ev_str(o1), ev_str(e));
    assert(cp_ring_is_end(&o1->node_chain));
    assert(cp_ring_is_end(&e->node_chain));
    cp_ring_join(&o1->node_chain, &e->node_chain);
}

/**
 * Insert into polygon output list */
static void poly_add(
    ctxt_t *c,
    event_t *e)
{
    LOG("poly   %s\n", ev_str(e));
    assert(!cp_dict_is_member(&e->node_q));
    assert(!cp_dict_is_member(&e->node_end));
    cp_list_init(&e->node_poly);
    cp_list_insert(&c->poly, &e->node_poly);
}

/**
 * Add a point to a path.
 * If necessary, allocate a new point */
static void path_add_point(
    cp_csg2_poly_t *r,
    cp_csg2_path_t *p,
    event_t *e)
{
    assert(!cp_ring_is_end(&e->node_chain) && "Polygon chain is too short or misformed");

    /* mark event used in polygon */
    assert(!e->used);
    e->used = true;

    /* possibly allocate a point */
    size_t idx = e->p->idx;
    if (idx == CP_SIZE_MAX) {
        cp_vec2_loc_t *v = cp_v_push0(&r->point);
        e->p->idx = idx = cp_v_idx(&r->point, v);
        v->coord = e->p->coord;
        v->loc = e->p->loc;
    }
    assert(idx < r->point.size);

    /* append point to path */
    cp_v_push(&p->point_idx, idx);
}

static void path_add_point3(
    ctxt_t *c,
    cp_csg2_poly_t *r,
    cp_csg2_path_t *p,
    event_t *prev,
    event_t *cur,
    event_t *next)
{
    if (c->all_points ||
        (cur->p->path_cnt > 1) ||
        !cp_vec2_in_line(&prev->p->coord, &cur->p->coord, &next->p->coord))
    {
        path_add_point(r, p, cur);
    }
}

/**
 * Construct the poly from the chains */
static void path_make(
    ctxt_t *c,
    cp_csg2_poly_t *r,
    cp_csg2_path_t *p,
    event_t *e0)
{
    assert(p->point_idx.size == 0);
    event_t *ex = CP_BOX_OF(cp_ring_step(&e0->node_chain, 0), event_t, node_chain);
    event_t *e1 = CP_BOX_OF(cp_ring_step(&e0->node_chain, 1), event_t, node_chain);

    /* make it so that e1 equals e0->other, and ex is the other end */
    assert((e1->p == e0->other->p) || (ex->p == e0->other->p));
    if (ex->p == e0->other->p) {
        /* for some reason, none of my tests triggers this, but I cannot see
         * why it couldn't happen */
        CP_SWAP(&e1, &ex);
    }
    assert(e1->p == e0->other->p);

    /* Four cases that collapse to two (no need to check whether e1 or ex is above):
     * If e0-e1 is below e0-ex, and e0->in.below==0, then move along e0->e1.
     * If e0-e1 is above e0-ex, and e0->in.below==1, then move along e1->e0.
     * If e0-e1 is below e0-ex, and e0->in.below==1, then move along e1->e0.
     * If e0-e1 is above e0-ex, and e0->in.below==0, then move along e0->e1.
     */
    if (e0->in.below) {
        CP_SWAP(&e0, &e1);
    }

#if 0
    /* first and second point */
    path_add_point(r, p, e0);
    path_add_point(r, p, e1);
    for (cp_ring_each(_ei, &e0->node_chain, &e1->node_chain)) {
        event_t *ei = CP_BOX_OF(_ei, event_t, node_chain);
        path_add_point(r, p, ei);
    }
#else
    /* handle triples in order to add points that are not collinear */
    event_t *ea = e0;
    event_t *eb = e1;
    for (cp_ring_each(_ec, &e0->node_chain, &e1->node_chain)) {
        event_t *ec = CP_BOX_OF(_ec, event_t, node_chain);
        path_add_point3(c, r, p, ea, eb, ec);
        ea = eb;
        eb = ec;
    }
    path_add_point3(c, r, p, ea, eb, e0);
    path_add_point3(c, r, p, eb, e0, e1);
#endif

    assert((p->point_idx.size >= 3) && "Polygon chain is too short");
}

/**
 * Construct the poly from the chains */
static void poly_make(
    cp_csg2_poly_t *r,
    ctxt_t *c,
    cp_loc_t loc)
{
    CP_ZERO(r);
    CP_CSG2_INIT(r, CP_CSG2_POLY, loc);

    assert((c->end == NULL) && "Some poly chains are still open");

    for (cp_list_each(_e, &c->poly)) {
        event_t *e = CP_BOX_OF(_e, event_t, node_poly);
        if (!e->used) {
            cp_csg2_path_t *p = cp_v_push0(&r->path);
            path_make(c, r, p, e);
        }
    }
}

/**
 * Add an edge to the output edge.  Only right events are added.
 */
static void chain_add(
    ctxt_t *c,
    event_t *e)
{
    LOG("out:   %s (%p)\n", ev_str(e), e);

    /* the event should left and neither point should be s or q */
    assert(!e->left);
    assert(pt_cmp(e->p, e->other->p) >= 0);
    assert(!cp_dict_is_member(&e->node_s));
    assert(!cp_dict_is_member(&e->node_q));
    assert(!cp_dict_is_member(&e->other->node_s));
    assert(!cp_dict_is_member(&e->other->node_q));

    cp_ring_init(&e->node_chain);
    cp_ring_init(&e->other->node_chain);

    /*
     * This algorithm combines output edges into a polygon ring.  We
     * know that the events come in from left (bottom) to right (top),
     * i.e., we have a definitive direction.  Only right points are
     * added.
     *
     * Edges are inserted by their next connection point that will
     * come in into the c->end using node_end.  Partial polygon chains
     * consisting of more than one point will have both ends in that
     * set.
     *
     * The first edge of a new polygon is added by its left point,
     * because we know that the next connection will be to that
     * point.  Once an edge is connected, its right point will be
     * inserted because its left point is already connected, so it cannot
     * be connected again.
     *
     * A new edge first searches c->end by its left point to find a place to
     * attach.  If found, that point is extracted from c->end, connected to
     * the new edge using (using node_chain), and the new edge is inserted
     * by its right point, waiting for another edge to connect.
     *
     * If another point with the same coordinates is found when trying
     * to insert an edge, that node is extracted from c->end, the two
     * ends are connected and the new edge is not inserted because
     * both ends are connected.  This may or may not close a polygon
     * complete.
     *
     * To gather polygons, once an edge connects two ends, it is inserted
     * into the list c->poly using the node node_poly.  Polygons may have
     * multiple nodes in this list, but an O(n) search is necessary to
     * output them anyway, so this does not hurt.
     *
     * For connecting nodes, the ring data structure is used so that
     * the order by which chains are linked does not matter -- we
     * might otherwise end up trying to connect chains of opposing
     * direction.  Rings handle this, plus our implementation supports
     * 'end' nodes, which our list implementation does not.
     *
     * In total, this takes no extra space except for c->end and c->poly,
     * and takes O(n log n) time with n edges found by the algorithm.
     */

    /* Find the left point in the end array.  Note: we search by
     * 'e->other->p', while we will insert by 'e->p'. */
    assert(e->other->left);
    event_t *o1 = chain_insert_or_extract(c, e->other);
    event_t *o2 = chain_insert_or_extract(c, e);

    switch ((o1 != NULL) | ((o2 != NULL) << 1)) {
    case 0: /* none found: new chain */
        /* connect left and right point to make initial pair */
        e->p->path_cnt++;
        e->other->p->path_cnt++;
        chain_join(e, e->other);
        assert(cp_ring_is_pair(&e->node_chain, &e->other->node_chain));
        break;

    case 3: /* both found: closed */
        /* close chain */
        chain_join(o1, o2);
        /* On small scales, points may collapse in unfortunate ways, creating
         * degenerate polygons.  This filters out a-b* rings. */
        if (!cp_ring_is_pair(&o1->node_chain, &o2->node_chain)) {
            assert(!cp_ring_is_end(&o1->node_chain));
            assert(!cp_ring_is_end(&o2->node_chain));
            /* put in poly list */
            poly_add(c, o2);
            /* At this point, from o2->in.below and the z normal of the triangle
             * o1, o2, o2->other, we can derive what is inside and what is
             * outside. */
            assert(o1 != o2->other);
        }
        break;

    case 1: /* o1 found, o2 not found: connect */
        e->p->path_cnt++;
        chain_join(o1, e);
        assert(!cp_ring_is_end(&o1->node_chain));
        assert( cp_ring_is_end(&e->node_chain));
        break;

    case 2: /* o2 found, o1 not found: connect */
        e->other->p->path_cnt++;
        chain_join(o2, e->other);
        assert(!cp_ring_is_end(&o2->node_chain));
        assert( cp_ring_is_end(&e->other->node_chain));
        break;
    }
}

static void intersection_add_ev(
    event_t **sev,
    size_t *sev_cnt,
    event_t *e1,
    event_t *e2)
{
    if (e1->p == e2->p) {
        sev[(*sev_cnt)++] = NULL;
    }
    else if (ev_cmp(e1, e2) > 0) {
        sev[(*sev_cnt)++] = e2;
        sev[(*sev_cnt)++] = e1;
    }
    else {
        sev[(*sev_cnt)++] = e1;
        sev[(*sev_cnt)++] = e2;
    }
}

static void intersection_point(
    cp_vec2_t *r,
    cp_f_t ka, cp_f_t kb, bool ks,
    cp_f_t ma, cp_f_t mb, bool ms)
{
    if (fabs(ka) < fabs(ma)) {
        CP_SWAP(&ka, &ma);
        CP_SWAP(&kb, &mb);
        CP_SWAP(&ks, &ms);
    }
    /* ka is closer to +-1 than ma; ma is closer to 0 than ka */

    if (ks != ms) {
        if (cp_eq(ma,0)) {
            _LINE_X(ks,r) = mb;
            _LINE_Y(ks,r) = (ka * mb) + kb;
            return;
        }
        /* need to switch one of the two into opposite axis.  better do this
         * with ka/kb/ks, because we're closer to +-1 there */
        assert(!cp_eq(ka,0));
        ka = 1/ka;
        kb *= -ka;
        ks = ms;
    }

    assert(!cp_eq(ka, ma) && "parallel lines should be handled in find_intersection, not here");
    assert((ks == ms) || cp_eq(ma,0));
    double q = (mb - kb) / (ka - ma);
    _LINE_X(ks,r) = q;
    _LINE_Y(ks,r) = (ka * q) + kb;
}

static bool dim_between(cp_dim_t a, cp_dim_t b, cp_dim_t c)
{
    return (a < c) ? (cp_le(a,b) && cp_le(b,c)) : (cp_ge(a,b) && cp_ge(b,c));
}

/**
 * Returns:
 *
 * non-NULL:
 *     single intersection point within segment bounds
 *
 * NULL:
 *     *collinear == false:
 *         parallel
 *
 *     *collinear == true:
 *         collinear, but not tested for actual overlapping
 */
static point_t *find_intersection(
    bool *collinear,
    ctxt_t *c,
    event_t *e0,
    event_t *e1)
{
    assert(e0->left);
    assert(e1->left);

    *collinear = false;

    point_t *p0  = e0->p;
    point_t *p0b = e0->other->p;
    point_t *p1  = e1->p;
    point_t *p1b = e1->other->p;

    /* Intersections are always calculated from the original input data so that
     * no errors add up. */

    /* parallel/collinear? */
    if ((e0->line.swap == e1->line.swap) && cp_eq(e0->line.a, e1->line.a)) {
        /* properly parallel? */
        *collinear = cp_eq(e0->line.b, e1->line.b);
        return NULL;
    }

    /* get intersection point */
    cp_vec2_t i;
    intersection_point(
        &i,
        e0->line.a, e0->line.b, e0->line.swap,
        e1->line.a, e1->line.b, e1->line.swap);

    i.x = rasterize(i.x);
    i.y = rasterize(i.y);

    /* check whether i is on e0 and e1 */
    if (!dim_between(p0->coord.x, i.x, p0b->coord.x) ||
        !dim_between(p0->coord.y, i.y, p0b->coord.y) ||
        !dim_between(p1->coord.x, i.x, p1b->coord.x) ||
        !dim_between(p1->coord.y, i.y, p1b->coord.y))
    {
        return NULL;
    }

    /* Due to rounding, the relationship between eX->p and i may become different
     * from the one between eX->p and eX->other->p.  This will be handles in divide_segment
     * by removing and reinserting edges for reprocessing.
     */

    /* Finally, make a new point (or an old point -- pt_new will check whether we have
     * this already) */
    return pt_new(c, p0->loc, &i);
}

static bool coord_between(
    cp_vec2_t const *a,
    cp_vec2_t const *b,
    cp_vec2_t const *c)
{
    if (!dim_between(a->x, b->x, c->x)) {
        return false;
    }
    if (!dim_between(a->y, b->y, c->y)) {
        return false;
    }
    cp_dim_t dx = c->x - a->x;
    cp_dim_t dy = c->y - a->y;
    if (fabs(dx) > fabs(dy)) {
        assert(!cp_pt_eq(a->x, c->x));
        cp_dim_t t = (b->x - a->x) / dx;
        cp_dim_t y = a->y + (t * dy);
        return cp_e_eq(cp_pt_epsilon * 1.5, y, b->y);
    }
    else {
        assert(!cp_pt_eq(a->y, c->y));
        cp_dim_t t = (b->y - a->y) / dy;
        cp_dim_t x = a->x + (t * dx);
        return cp_e_eq(cp_pt_epsilon * 1.5, x, b->x);
    }
}

static bool pt_between(
    point_t const *a,
    point_t const *b,
    point_t const *c)
{
    if (a == b) {
        return true;
    }
    if (b == c) {
        return true;
    }
    assert(a != c);
    return coord_between(&a->coord, &b->coord, &c->coord);
}

/**
 * Returns 3 on overlap
 * Returns 1 if eh is on el--ol.
 * Returns 2 if el is on eh--oh.
 * Returns 0 otherwise.
 */
static unsigned ev4_overlap(
    event_t *el,
    event_t *ol,
    event_t *eh,
    event_t *oh)
{
    /*
     * The following cases exist:
     * (1) el........ol        (6) eh........oh
     *          eh...oh                 el...ol
     *
     * (2) el........ol        (7) eh........oh
     *     eh...oh                 el...ol
     *
     * (3) el........ol        (8) eh........oh
     *        eh..oh                  el..ol
     *
     * (4) el........ol        (9) eh........oh
     *          eh........oh            el........ol
     *
     * We do not care about the following ones, because they need
     * a collinearity check anyway (i.e., these must return false):
     *
     * (5) el...ol            (10) eh...oh
     *          eh...oh                 el...ol
     */
    unsigned result = 0;
    if (pt_between(el->p, eh->p, ol->p)) { /* (1),(2),(3),(4),(5),(7) */
        if (pt_between(el->p, oh->p, ol->p)) { /* (1),(2),(3) */
            return 3;
        }
        if (pt_between(eh->p, ol->p, oh->p)) { /* (4),(5) */
            return (ol->p != eh->p) ? 3 : 1; /* exclude (5) */
        }
        result = 1;
        /* (7) needs to be checked, so no 'return false' here */
    }

    if (pt_between(eh->p, el->p, oh->p)) { /* (2),(6),(7),(8),(9),(10) */
        if (pt_between(eh->p, ol->p, oh->p)) { /* (6),(7),(8) */
            return 3;
        }
        if (pt_between(el->p, oh->p, ol->p)) { /* (9),(10) */
            return (oh->p != el->p) ? 3 : 2;
        }
        return 2;
    }

    return result;
}

static void ev_ignore(
    ctxt_t *c,
    event_t *e)
{
    assert(e->in.owner == 0);
    assert(e->other->in.owner == 0);
    if (cp_dict_is_member(&e->node_s)) {
        s_remove(c, e);
    }
    if (cp_dict_is_member(&e->other->node_s)) {
        s_remove(c, e->other);
    }
    if (cp_dict_is_member(&e->node_q)) {
        q_remove(c, e);
    }
    if (cp_dict_is_member(&e->other->node_q)) {
        q_remove(c, e->other);
    }
}

static void check_intersection(
    ctxt_t *c,
    /** the lower edge in s */
    event_t *el,
    /** the upper edge in s */
    event_t *eh)
{
    event_t *ol = el->other;
    event_t *oh = eh->other;
    assert( el->left);
    assert( eh->left);
    assert( cp_dict_is_member(&el->node_s));
    assert( cp_dict_is_member(&eh->node_s));
    assert(!ol->left);
    assert(!oh->left);
    assert(!cp_dict_is_member(&ol->node_s));
    assert(!cp_dict_is_member(&oh->node_s));

    /* A simple comparison of line.a to decide about overlap will not work, i.e.,
     * because the criterion needs to be consistent with point coordinate comparison,
     * otherwise we may run into problems elsewhere.  I.e., we cannot first check for
     * collinearity and only then check for overlap.  But we need to base the
     * decision of overlap on point coordinate comparison.  So we will first try
     * for overlap, then we'll try to find a proper intersection point.
     * 'find_intersection' will, therefore, not have to deal with the case of overlap.
     * If the edges are collinear (e.g., based on an line.a criterion), it will mean
     * that the lines are paralllel or collinear but with a gap in between, i.e., they
     * will not overlap.
     *
     * The whole 'overlap' check explicitly does not use the 'normal_z' or 'line.a'
     * checks to really base this on cp_pt_eq().
     *
     * Now, if el and eh are indeed overlapping, Whether el or eh is the 'upper' edge
     * may have been decided based on a rounding error, so either case must be handled
     * correctly.
     */

    unsigned u = ev4_overlap(el, ol, eh, oh);
    if (u != 3) {
        bool collinear = false;
        point_t *ip = NULL;
        switch (u) {
        default:
            ip = find_intersection(&collinear, c, el, eh);
            break;
        case 1:
            ip = eh->p;
            break;
        case 2:
            ip = el->p;
            break;
        }

        if (ip != NULL) {
            LOG("Rel: intersect, collinear=%u (%s -- %s)\n",
                collinear, ev_str(el), ev_str(eh));

            /* If the lines meet in one point, it's ok */
            if ((el->p == eh->p) || (ol->p == oh->p)) {
                return;
            }

            if (ip == el->p) {
                /* This means that we need to reclassify the upper line again (which
                 * we thought was below, but due to rounding, it now turns out to be
                 * completely above).  The easiest is to remove it again from S
                 * and throw it back into Q to try again later. */
                s_remove(c, el);
                q_insert(c, el);
            }
            else if (ip != ol->p) {
                divide_segment(c, el, ip);
            }

            if (ip == eh->p) {
                /* Same corder case as above: we may have classified eh too early. */
                s_remove(c, eh);
                q_insert(c, eh);
            }
            else if (ip != oh->p) {
                divide_segment(c, eh, ip);
            }

            return;
        }

        /* collinear means parallel here, i.e., no intersection */
        LOG("Rel: unrelated, parallel=%u (%s -- %s)\n", collinear, ev_str(el), ev_str(eh));
        return;
    }

    /* check */
    assert(pt_cmp(el->p, ol->p) < 0);
    assert(pt_cmp(eh->p, oh->p) < 0);
    assert(pt_cmp(ol->p, eh->p) >= 0);
    assert(pt_cmp(oh->p, el->p) >= 0);

    /* overlap */
    event_t *sev[4];
    size_t sev_cnt = 0;
    intersection_add_ev(sev, &sev_cnt, el, eh);
    intersection_add_ev(sev, &sev_cnt, ol, oh);
    assert(sev_cnt >= 2);
    assert(sev_cnt <= cp_countof(sev));

    size_t owner = (eh->in.owner ^ el->in.owner);
    size_t below = el->in.below;
    size_t above = below ^ owner;

    /* We do not need to care about resetting other->in.below, because it is !left
     * and is not part of S yet, and in.below will be reset upon insertion. */
    if (sev_cnt == 2) {
        LOG("Rel: overlap same (%s -- %s)\n", ev_str(el), ev_str(eh));

        /*  eh.....oh
         *  el.....ol
         */
        assert(sev[0] == NULL);
        assert(sev[1] == NULL);
        eh->in.owner = oh->in.owner = owner;
        eh->in.below = below;

        el->in.owner = ol->in.owner = 0;
        assert(el->in.below == below);

        ev_ignore(c, el);
        return;
    }
    if (sev_cnt == 3) {
        LOG("Rel: overlap joint end (%s -- %s)\n", ev_str(el), ev_str(eh));

        /* sev:  0    1    2
         *       eh........NULL    ; sh == eh, shl == eh
         *            el...NULL
         * OR
         *            eh...NULL
         *       el........NULL    ; sh == el, shl == el
         * OR
         *     NULL........oh      ; sh == oh, shl == eh
         *     NULL...ol
         * OR
         *     NULL...oh
         *     NULL........ol      ; sh == ol, shl == el
         */
        assert(sev[1] != NULL);
        assert((sev[0] == NULL) || (sev[2] == NULL));

        /* ignore the shorter one */
        sev[1]->in.owner = sev[1]->other->in.owner = 0;

        /* split the longer one, marking the double side as overlapping: */
        event_t *sh  = sev[0] ?: sev[2];
        event_t *shl = sev[0] ?: sev[2]->other;
        sh->other->in.owner = owner;
        sh->other->in.below = below;
        if (shl == el) {
            assert((sev[1] == eh) || (sev[1] == oh));
            eh->in.below = above;
        }

        divide_segment(c, shl, sev[1]->p);

        ev_ignore(c, sev[1]);
        return;
    }

    assert(sev_cnt == 4);
    assert(sev[0] != NULL);
    assert(sev[1] != NULL);
    assert(sev[2] != NULL);
    assert(sev[3] != NULL);
    assert(
        ((sev[0] == el) && (sev[1] == eh)) ||
        ((sev[0] == eh) && (sev[1] == el)));
    assert(
        ((sev[2] == ol) && (sev[3] == oh)) ||
        ((sev[2] == oh) && (sev[3] == ol)));

    if (sev[0] != sev[3]->other) {
        LOG("Rel: overlap opposite ends (%s -- %s)\n", ev_str(el), ev_str(eh));

        /*        0   1   2   3
         *            eh......oh
         *        el......ol
         * OR:
         *        eh......oh
         *            el......ol
         */
        assert(
            ((sev[0] == el) && (sev[1] == eh) && (sev[2] == ol) && (sev[3] == oh)) ||
            ((sev[0] == eh) && (sev[1] == el) && (sev[2] == oh) && (sev[3] == ol)));

        sev[1]->in.owner = 0;
        if (sev[1] == eh) {
            sev[1]->in.below = above;
        }
        sev[2]->in.owner = owner;
        sev[2]->in.below = below;

        divide_segment(c, sev[0], sev[1]->p);
        divide_segment(c, sev[1], sev[2]->p);

        ev_ignore(c, sev[1]);
        return;
    }

    LOG("Rel: overlap subsumed (%s -- %s)\n", ev_str(el), ev_str(eh));


    /*        0   1   2   3
     *            eh..oh
     *        el..........ol
     * OR:
     *        eh..........oh
     *            el..ol
     */
    assert(
        ((sev[0] == el) && (sev[1] == eh) && (sev[2] == oh) && (sev[3] == ol)) ||
        ((sev[0] == eh) && (sev[1] == el) && (sev[2] == ol) && (sev[3] == oh)));
    assert(sev[1]->other == sev[2]);

    sev[1]->in.owner = sev[2]->in.owner = 0;
    if (sev[1] == eh) {
        sev[1]->in.below = sev[2]->in.below = above;
    }
    divide_segment(c, sev[0], sev[1]->p);

    sev[3]->other->in.owner = owner;
    sev[3]->other->in.below = below;
    divide_segment(c, sev[3]->other, sev[2]->p);

    ev_ignore(c, sev[1]);
}

static inline event_t *s_next(
    event_t *e)
{
    if (e == NULL) {
        return NULL;
    }
    return CP_BOX0_OF(cp_dict_next(&e->node_s), event_t, node_s);
}

static inline event_t *s_prev(
    event_t *e)
{
    if (e == NULL) {
        return NULL;
    }
    return CP_BOX0_OF(cp_dict_prev(&e->node_s), event_t, node_s);
}

static void ev_left(
    ctxt_t *c,
    event_t *e)
{
    assert(!cp_dict_is_member(&e->node_s));
    assert(!cp_dict_is_member(&e->other->node_s));
    LOG("insert_s: %p (%p)\n", e, e->other);
    s_insert(c, e);

    event_t *prev = s_prev(e);
    event_t *next = s_next(e);
    assert(e->left);
    assert((prev == NULL) || prev->left);

    if (prev == NULL) {
        /* should be set up correctly from q phase */
        e->in.below = 0;
    }
    else {
        /* use previous edge's above for this edge's below info */
        e->in.below = prev->in.below ^ prev->in.owner;
    }

    debug_print_s(c, "left after insert", e, prev, next);

    if (next != NULL) {
        check_intersection(c, e, next);
    }
    /* The previous 'check_intersection' may have kicked out 'e' from S due
     * to rounding, so check that e is still in S before trying to intersect.
     * If not, it is back in Q and we'll handle this later. */
    if ((prev != NULL) && cp_dict_is_member(&e->node_s)) {
        check_intersection(c, prev, e);
    }

    debug_print_s(c, "left after intersect", e, prev, next);
}

static bool op_bitmap_get(
    ctxt_t *c,
    size_t i)
{
    assert(i < c->comb_size);
    return cp_csg2_op_bitmap_get(c->comb, i);
}

static void ev_right(
    ctxt_t *c,
    event_t *e)
{
    event_t *sli = e->other;
    event_t *next = s_next(sli);
    event_t *prev = s_prev(sli);

    debug_print_s(c, "right before intersect", e, prev, next);

    /* first remove from s */
    LOG("remove_s: %p (%p)\n", e->other, e);
    s_remove(c, sli);
    assert(!cp_dict_is_member(&e->node_s));
    assert(!cp_dict_is_member(&e->other->node_s));

    /* now add to out */
    bool below_in = op_bitmap_get(c, sli->in.below);
    bool above_in = op_bitmap_get(c, sli->in.below ^ sli->in.owner);
    if (below_in != above_in) {
        assert(sli->in.owner != 0);
        e->in.below = e->other->in.below = below_in;
        chain_add(c, e);
    }

    if ((next != NULL) && (prev != NULL)) {
        check_intersection(c, prev, next);
    }

    debug_print_s(c, "right after intersect", e, prev, next);
}

static void csg2_op_poly(
    cp_csg2_lazy_t *o,
    cp_csg2_poly_t *a)
{
    assert(cp_mem_is0(o, sizeof(*o)));
    if (a->path.size > 0) {
        o->size = 1;
        o->data[0] = a;
        o->comb.b[0] = 2; /* == 0b10 */
    }
}

static void csg2_op_csg2(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg2_t *a);

static void csg2_op_v_csg2(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_v_csg2_p_t *a)
{
    TRACE("n=%"_Pz"u", a->size);
    assert(cp_mem_is0(o, sizeof(*o)));
    for (cp_v_each(i, a)) {
        cp_csg2_t *ai = cp_v_nth(a,i);
        if (i == 0) {
            csg2_op_csg2(opt, pool, r, zi, o, ai);
        }
        else {
            cp_csg2_lazy_t oi = { 0 };
            csg2_op_csg2(opt, pool, r, zi, &oi, ai);
            LOG("ADD\n");
            cp_csg2_op_lazy(opt, pool, o, &oi, CP_OP_ADD);
        }
    }
}

static void csg2_op_add(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg2_add_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    csg2_op_v_csg2(opt, pool, r, zi, o, &a->add);
}

static void csg2_op_cut(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg2_cut_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    for (cp_v_each(i, &a->cut)) {
        cp_csg2_add_t *b = cp_v_nth(&a->cut, i);
        if (i == 0) {
            csg2_op_add(opt, pool, r, zi, o, b);
        }
        else {
            cp_csg2_lazy_t oc = {0};
            csg2_op_add(opt, pool, r, zi, &oc, b);
            LOG("CUT\n");
            cp_csg2_op_lazy(opt, pool, o, &oc, CP_OP_CUT);
        }
    }
}

static void csg2_op_layer(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_csg2_lazy_t *o,
    cp_csg2_layer_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    csg2_op_add(opt, pool, r, a->zi, o, &a->root);
}

static void csg2_op_sub(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg2_sub_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    csg2_op_add(opt, pool, r, zi, o, &a->add);

    cp_csg2_lazy_t os = {0};
    csg2_op_add(opt, pool, r, zi, &os, &a->sub);
    LOG("SUB\n");
    cp_csg2_op_lazy(opt, pool, o, &os, CP_OP_SUB);
}

static void csg2_op_stack(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg2_stack_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));

    cp_csg2_layer_t *l = cp_csg2_stack_get_layer(a, zi);
    if (l == NULL) {
        return;
    }
    if (zi != l->zi) {
        assert(l->zi == 0); /* not visited: must be empty */
        return;
    }

    assert(zi == l->zi);
    csg2_op_layer(opt, pool, r, o, l);
}

static void csg2_op_csg2(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg2_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    switch (a->type) {
    case CP_CSG2_CIRCLE:
        CP_NYI("circle");
        CP_ZERO(o);
        return;

    case CP_CSG2_POLY:
        csg2_op_poly(o, cp_csg2_poly(a));
        return;

    case CP_CSG2_ADD:
        csg2_op_add(opt, pool, r, zi, o, cp_csg2_add(a));
        return;

    case CP_CSG2_SUB:
        csg2_op_sub(opt, pool, r, zi, o, cp_csg2_sub(a));
        return;

    case CP_CSG2_CUT:
        csg2_op_cut(opt, pool, r, zi, o, cp_csg2_cut(a));
        return;

    case CP_CSG2_STACK:
        csg2_op_stack(opt, pool, r, zi, o, cp_csg2_stack(a));
        return;
    }

    CP_DIE("2D object type");
    CP_ZERO(o);
}

static void cp_csg2_op_poly(
    cp_pool_t *pool,
    cp_csg2_lazy_t *r)
{
    TRACE();
    /* make context */
    ctxt_t c = {
        .pool = pool,
        .comb = &r->comb,
        .comb_size = (1U << r->size),
    };
    cp_list_init(&c.poly);

    /* initialise queue */
    for (cp_size_each(m, r->size)) {
        cp_csg2_poly_t *a = r->data[m];
        LOG("poly %"_Pz"d: #path=%"_Pz"u\n", m, a->path.size);
        for (cp_v_each(i, &a->path)) {
            cp_csg2_path_t *p = &cp_v_nth(&a->path, i);
            for (cp_v_each(j, &p->point_idx)) {
                cp_vec2_loc_t *pj = cp_csg2_path_nth(a, p, j);
                cp_vec2_loc_t *pk = cp_csg2_path_nth(a, p, cp_wrap_add1(j, p->point_idx.size));
                q_add_orig(&c, pj->loc, &pj->coord, &pk->coord, m);
            }
        }
    }
    LOG("start\n");

    /* run algorithm */
    size_t ev_cnt __unused = 0;
    for (;;) {
        event_t *e = q_extract_min(&c);
        if (e == NULL) {
            break;
        }

        LOG("\nevent %"_Pz"u: %s o=(0x%"_Pz"x 0x%"_Pz"x)\n",
            ++ev_cnt,
            ev_str(e),
            e->other->in.owner,
            e->other->in.below);

        /* do real work on event */
        if (e->left) {
            ev_left(&c, e);
        }
        else {
            ev_right(&c, e);
        }
    }

    poly_make(r->data[0], &c, r->data[0]->loc);
    if (r->data[0]->point.size == 0) {
        CP_ZERO(r);
        return;
    }
    r->size = 1;
    r->comb.b[0] = 2;
}

/* ********************************************************************** */
/* extern */

/**
 * Actually reduce a lazy poly to a single poly.
 *
 * The result is either empty (r->size == 0) or will have a single entry
 * (r->size == 1) stored in r->data[0].  If the result is empty, this
 * ensures that r->data[0] is NULL.
 *
 * Note that because lazy polygon structures have no dedicated space to store
 * a polygon, so they must reuse the space of the input polygons, so applying
 * this function with more than 2 polygons in the lazy structure will reuse
 * space from the polygons for storing the result.
 */
extern void cp_csg2_op_reduce(
    cp_pool_t *pool,
    cp_csg2_lazy_t *r)
{
    TRACE();
    if (r->size <= 1) {
        return;
    }
    cp_csg2_op_poly(pool, r);
}

/**
 * Boolean operation on two lazy polygons.
 *
 * This does 'r = r op b'.
 *
 * Only the path information is used, not the triangles.
 *
 * \p r and/or \p b are reused and cleared to construct r.  This may happen
 * immediately or later in cp_csg2_op_reduce().
 *
 * Uses \p pool for all temporary allocations (but not for constructing r).
 *
 * This uses the algorithm of Martinez, Rueda, Feito (2009), based on a
 * Bentley-Ottmann plain sweep.  The algorithm is modified:
 *
 * (1) The original algorithm (both paper and sample implementation)
 *     does not focus on reassembling into polygons the sequence of edges
 *     the algorithm produces.  This library replaces the polygon
 *     reassembling by an O(n log n) algorithm.
 *
 * (2) The original algorithm's in/out determination strategy is not
 *     extensible to processing multiple polygons in one algorithm run.
 *     It was replaceds by a bitmask xor based algorithm.  This also lifts
 *     the restriction that no self-overlapping polygons may exist.
 *
 * (3) There is handling of corner cases in than what Martinez implemented.
 *     The float business is really tricky...
 *
 * (4) Intersection points are always computed from the original line slope
 *     and offset to avoid adding up rounding errors for edges with many
 *     intersections.
 *
 * (5) Float operations have all been mapped to epsilon aware versions.
 *     (The reference implementation failed on one of my tests because of
 *     using plain floating point '<' comparison.)
 *
 * Runtime: O(k log k),
 * Space: O(k)
 * Where
 *     k = n + m + s,
 *     n = number of edges in r,
 *     m = number of edges in b,
 *     s = number of intersection points.
 *
 * Note: the operation may not actually be performed, but may be delayed until
 * cp_csg2_apply.  The runtimes are given under the assumption that cp_csg2_apply
 * follows.  Best case runtime for delaying the operation is O(1).
 */
extern void cp_csg2_op_lazy(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_lazy_t *r,
    cp_csg2_lazy_t *b,
    cp_bool_op_t op)
{
    assert(opt->max_simultaneous >= 2);
    size_t max_sim = cp_min(opt->max_simultaneous, cp_countof(r->data));
    TRACE();
    for (size_t loop = 0; loop < 3; loop++) {
        if (opt->optimise & CP_CSG2_OPT_SKIP_EMPTY) {
            /* empty? */
            if (b->size == 0) {
                if (op == CP_OP_CUT) {
                    CP_ZERO(r);
                }
                return;
            }
            if (r->size == 0) {
                if ((op == CP_OP_ADD) || (op == CP_OP_XOR)) {
                    CP_SWAP(r, b);
                }
                return;
            }
        }

        /* if we can fit the result into one structure, then try that */
        if ((r->size + b->size) <= max_sim) {
            break;
        }

        /* reduction will be necessary max 2 times */
        assert(loop < 2);

        /* otherwise reduce the larger one */
        if (r->size > b->size) {
            cp_csg2_op_reduce(pool, r);
            assert(r->size <= 1);
        }
        else {
            cp_csg2_op_reduce(pool, b);
            assert(b->size <= 1);
        }
    }

    /* it should now fit into the first one */
    assert((r->size + b->size) <= cp_countof(r->data));

    /* append b's polygons to r */
    for (cp_size_each(i, b->size)) {
        assert((r->size + i) < cp_countof(r->data));
        assert(i < cp_countof(b->data));
        r->data[r->size + i] = b->data[i];
    }

    cp_csg2_op_bitmap_repeat(&r->comb, r->size, b->size);
    cp_csg2_op_bitmap_spread(&b->comb, b->size, r->size);

    r->size += b->size;

    cp_csg2_op_bitmap_combine(&r->comb, &b->comb, r->size, op);

#ifndef NDEBUG
    /* clear with garbage to trigger bugs when accessed */
    memset(b, 170, sizeof(*b));
#endif
}

/**
 * Add a layer to a tree by reducing it from another tree.
 *
 * The tree must have been initialised by cp_csg2_op_tree_init(),
 * and the layer ID must be in range.
 *
 * r is filled from a.  In the process, a is cleared/reused, if necessary.
 *
 * Runtime: O(j * k log k)
 * Space O(k)
 *    k = see cp_csg2_op_poly()
 *    j = number of polygons + number of bool operations in tree
 */
extern void cp_csg2_op_add_layer(
    cp_csg2_tree_opt_t const *opt,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    cp_csg2_tree_t *a,
    size_t zi)
{
    TRACE();
    cp_csg2_stack_t *s = cp_csg2_stack(r->root);
    assert(zi < s->layer.size);

    cp_csg2_lazy_t ol;
    CP_ZERO(&ol);
    csg2_op_csg2(opt, pool, r, zi, &ol, a->root);
    cp_csg2_op_reduce(pool, &ol);

    cp_csg2_poly_t *o = ol.data[0];
    if (o != NULL) {
        assert(o->point.size > 0);

        /* new layer */
        cp_csg2_layer_t *layer = cp_csg2_stack_get_layer(s, zi);
        assert(layer != NULL);
        cp_csg2_add_init_perhaps(&layer->root, NULL);

        layer->zi = zi;

        cp_v_nth(&r->flag, zi) |= CP_CSG2_FLAG_NON_EMPTY;

        /* single polygon per layer */
        cp_v_push(&layer->root.add, (cp_csg2_t*)o);
    }
}

/**
 * Initialise a tree for cp_csg2_op_add_layer() operations.
 *
 * The tree is initialised with a single stack of layers of the given
 * size taken from \p a.  Also, the z values are copied from \p a.
 *
 * Runtime: O(n)
 * Space O(n)
 *    n = number of layers
 */
extern void cp_csg2_op_tree_init(
    cp_csg2_tree_t *r,
    cp_csg2_tree_t const *a)
{
    TRACE();
    r->root = cp_csg2_new(CP_CSG2_STACK, NULL);
    r->thick = a->thick;
    r->opt = a->opt;

    size_t cnt = a->z.size;

    cp_csg2_stack_t *c = cp_csg2_stack(r->root);
    cp_v_init0(&c->layer, cnt);

    cp_v_init0(&r->z, cnt);
    cp_v_copy_arr(&r->z, 0, &a->z, 0, cnt);

    cp_v_init0(&r->flag, cnt);
}
