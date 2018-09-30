/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */
/*
 * This is adapted from Francisco Martinez del Rio (2011), v1.4.1.
 * See: http://www4.ujaen.es/~fmartin/bool_op.html
 *
 * Note:
 * The original implementation is not clean C/C++:
 *
 *   - The allocator uses a vector to allocate new SweepEvents:
 *     it pushes a new structure and uses the pointer to the last
 *     entry throughout the algorithm.  However, this is badly broken
 *     because pushing might realloc the vector to a different memory
 *     location so that all previously allocated events become broken
 *     pointers.
 *
 *   - The algorithm uses float comparison <=, >=, <, >, and even == freely.
 *     Only in a few cases, an epsilon is checked, but for my taste, this is
 *     not clean enough.
 *
 *   - The InOut and inside bools are left uninitialised in the constructor.
 *     This is OK (the algorithm sets them before using them), but it is
 *     confusing and dirty.
 *
 *   - The InOut, inside, pl, type handling to determine inside and outside
 *     is difficult to understand and to extend.  I tend to think that a
 *     up side/low side in/out info is more helpful.   Sean Connelly uses
 *     this approach in his JavaScript adaption.  I want this in order to be
 *     able to handle multiple polygon at once.
 *
 * This implements most of the algorithm using dictionaries instead of, say,
 * a heap for the pqueue.  This avoids 'realloc' and makes it easier to use
 * pool memory.
 *
 * An approach to get rid of inOut/inside/pl/type handling is to mark
 * what side of an edge is inside of which polygon.  This is usually
 * available from the input polygons (from the order of the points).
 * Or it could be derived during the algorithm.  Self-intersecting
 * polygons should continue to work -- we won't have those, but I do
 * not want to make the algorithm worse in any way.  Marking in/out
 * side of edges beforehand would be note also to have inverted
 * polygons so that SUB can be the same as ADD (i.e., we could handle
 * multiple polygons in a SUB operation at once).  Also, CUT could
 * be the same by inverting both polygons.
 *
 * The polygons output by this algorithm have no predefined point
 * direction and are always non-self-intersecting and disjoint (except
 * for single points) but there may be holes.  (And that is exactly
 * what the triangulation algorithm can handle.)  We want this output
 * to become the new input for another round of the algorithm, so to
 * avoid the need to order the points in a polygon correctly after
 * this algorithm is run, I'd rather let the algorithm decide what's
 * in and out during the run, only marking whether a polygon is POS or
 * NEG.
 */

#define DEBUG 0

#include <stdio.h>
#include <cpmat/dict.h>
#include <cpmat/list.h>
#include <cpmat/ring.h>
#include <cpmat/mat.h>
#include <cpmat/pool.h>
#include <cpmat/alloc.h>
#include <cpmat/vec.h>
#include <cpmat/panic.h>
#include <csg2plane/csg2.h>
#include <csg2plane/ps.h>
#include "internal.h"

/**
 * Whether to optimise some trivial cases.
 * This can be disabled to debug the main algorithm.
 *
 * 0 = no optimisations
 * 1 = empty polygon optimisation
 * 2 = bounding box optimisation
 * 3 = x-coordinate max optimisation => nothing more to do
 * 4 = x-coordinate max optimisation => copy all the rest
 */
#define OPT 3 /* FIXME: 4 is currently buggy */

typedef enum {
    E_NORMAL,
    E_IGNORE,
    E_SAME,
    E_DIFF
} event_type_t;

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
         * polygon ID corresponding to that bit number.
         */
        size_t below;
    } in;

    struct {
        unsigned poly_id;
        event_type_t type;
        bool in_out;
        bool inside;
    } io;

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

    /** bounding boxes of polygons */
    cp_vec2_minmax_t bb[2];

    /** minimal max x coordinate of input polygons */
    cp_dim_t minmaxx;

    /** which bool operation? */
    cp_bool_op_t op;
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
static char const *ev_type_str(unsigned x)
{
    switch (x) {
    case E_NORMAL: return "normal";
    case E_IGNORE: return "ignore";
    case E_SAME:   return "same";
    case E_DIFF:   return "diff";
    }
    return "?";
}

__unused
static char const *__ev_str(char *s, size_t n, event_t const *x)
{
    if (x == NULL) {
        return "NULL";
    }
    if (x->left) {
        snprintf(s, n, "#("FD2"--"FD2")  %cin %cio %s %u",
            CP_V01(x->p->coord),
            CP_V01(x->other->p->coord),
            x->io.inside ? '+' : '-',
            x->io.in_out ? '+' : '-',
            ev_type_str(x->io.type),
            x->io.poly_id);
    }
    else {
        snprintf(s, n, " ("FD2"--"FD2")# %cin %cio %s %u",
            CP_V01(x->other->p->coord),
            CP_V01(x->p->coord),
            x->io.inside ? '+' : '-',
            x->io.in_out ? '+' : '-',
            ev_type_str(x->io.type),
            x->io.poly_id);
    }
    s[n-1] = 0;
    return s;
}

#define ev_str(x) __ev_str((char[50]){}, 50, x)

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

    bool close = false;
    for (cp_ring_each(_ei, &e0->node_chain, &e1->node_chain)) {
        event_t *ei = CP_BOX_OF(_ei, event_t, node_chain);

        ei->debug_tag = tag;
        cp_printf(cp_debug_ps,
            " %g %g lineto", CP_PS_XY(ei->p->coord));
        close = !cp_ring_is_end(_ei);
    }
    if (close) {
        cp_printf(cp_debug_ps, " closepath");
    }
    cp_printf(cp_debug_ps, " stroke\n");
}
#endif

#if DEBUG || defined(PSTRACE)

static void debug_print_s(
    ctxt_t *c,
    char const *msg,
    event_t *es)
{
#if DEBUG
    LOG("DEBUG: S %s\n", msg);
    for (cp_dict_each(_e, c->s)) {
        event_t *e = CP_BOX_OF(_e, event_t, node_s);
        LOG("S: %s\n", ev_str(e));
    }
#endif

#ifdef PSTRACE
    /* output to postscript */
    if (cp_debug_ps != NULL) {
        /* begin page */
        cp_ps_page_begin(cp_debug_ps, ++cp_debug_ps_page_cnt);

        /* print info */
        cp_printf(cp_debug_ps, "30 30 moveto (CSG: %s) show\n", msg);
        cp_printf(cp_debug_ps, "30 55 moveto (%s) show\n", ev_str(es));

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
            cp_printf(cp_debug_ps, "newpath %g %g 3 0 360 arc closepath fill\n",
                CP_PS_XY(p->coord));
        }

        /* s */
        cp_printf(cp_debug_ps, "3 setlinewidth\n");
        size_t i = 0;
        for (cp_dict_each(_e, c->s)) {
            event_t *e = CP_BOX_OF(_e, event_t, node_s);
            cp_printf(cp_debug_ps,
                "0 %g 0 setrgbcolor\n", cp_double(i % 3) * 0.5);
            cp_printf(cp_debug_ps,
                "newpath %g %g 3 0 360 arc closepath fill\n",
                CP_PS_XY(e->p->coord));
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
            cp_printf(cp_debug_ps, "1 %g 0 setrgbcolor\n", cp_double(i % 3) * 0.3);
            event_t *e0 = CP_BOX_OF(_e, event_t, node_end);
            cp_printf(cp_debug_ps, "newpath %g %g 4 0 360 arc closepath fill\n",
                CP_PS_XY(e0->p->coord));
            debug_print_chain(e0, cp_debug_ps_page_cnt);
            i++;
        }

        /* poly */
        cp_printf(cp_debug_ps, "2 setlinewidth\n");
        i = 0;
        for (cp_list_each(_e, &c->poly)) {
            cp_printf(cp_debug_ps, "0 %g 0.8 setrgbcolor\n", cp_double(i % 3) * 0.5);
            event_t *e0 = CP_BOX_OF(_e, event_t, node_poly);
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
 * Compare two coordinates
 */
static inline int coord_cmp(
    cp_vec2_t const *a,
    cp_vec2_t const *b)
{
    return cp_vec2_lex_cmp(a, b);
}

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
    return coord_cmp(&a->coord, &b->coord);
}

/**
 * Compare a vec2 with a point in a dictionary.
 */
static int pt_cmp_d(
    cp_vec2_t const *a,
    cp_dict_t *_b,
    void *user __unused)
{
    point_t *b = CP_BOX_OF(_b, point_t, node_pt);
    return coord_cmp(a, &b->coord);
}

/**
 * Allocate a new point and remember in our point dictionary.
 *
 * This will either return a new point or one that was found already.
 */
static point_t *pt_new(
    ctxt_t *c,
    cp_loc_t loc,
    cp_vec2_t const *coord)
{
    cp_dict_ref_t ref;
    cp_dict_t *pt = cp_dict_find_ref(&ref, coord, c->pt, pt_cmp_d, NULL, 0);
    if (pt != NULL) {
        return CP_BOX_OF(pt, point_t, node_pt);
    }

    point_t *p = CP_POOL_NEW(c->pool, *p);
    p->loc = loc;
    p->coord = *coord;
    p->idx = CP_SIZE_MAX;

    /* normalise coordinates around 0 to avoid funny floats */
    if (cp_equ(p->coord.x, 0)) { p->coord.x = 0; }
    if (cp_equ(p->coord.y, 0)) { p->coord.y = 0; }

    LOG("DEBUG: new pt: %s\n", pt_str(p));

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
            //assert(0);
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
    unsigned poly_id)
{
    point_t *p1 = pt_new(c, loc, coord1);
    point_t *p2 = pt_new(c, loc, coord2);

    if (p1 == p2) {
        /* edge consisting of only one point */
        return;
    }

    event_t *e1 = ev_new(c, loc, p1, true,  NULL);
    e1->io.poly_id = poly_id;
    e1->io.type = E_NORMAL;
    e1->in.owner = ((size_t)1) << poly_id;

    event_t *e2 = ev_new(c, loc, p2, false, e1);
    e2->io.poly_id = poly_id;
    e2->io.type = E_NORMAL;
    e2->in.owner= ((size_t)1) << poly_id;

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
    assert(cp_leq(e1->line.a, +1));
    assert(cp_geq(e1->line.a, -1));

    /* other direction edge is on the same line */
    e2->line = e1->line;

#ifndef NDEBUG
    /* check computation */
    cp_vec2_t g;
    get_coord_on_line(&g, e1, &e2->p->coord);
    assert(cp_vec2_equ(&g, &e2->p->coord));
    get_coord_on_line(&g, e2, &e1->p->coord);
    assert(cp_vec2_equ(&g, &e1->p->coord));
#endif

    /* Insert.  For 'equal' entries, order does not matter */
    q_insert(c, e1);
    q_insert(c, e2);
}

static void divide_segment(
    ctxt_t *c,
    event_t *e,
    point_t *p)
{
    assert(e->io.poly_id == e->other->io.poly_id);
    event_t *r = ev_new(c, p->loc, p, false, e);
    r->io.poly_id = e->io.poly_id;
    r->io.type = e->io.type;
    r->in.owner = e->in.owner;
    r->in.below = e->in.below;

    event_t *l = ev_new(c, p->loc, p, true, e->other);
    l->io.poly_id = e->io.poly_id;
    l->io.type = e->other->io.type;
    l->in.owner = e->other->in.owner;
    l->in.below = e->other->in.below;

    if (ev_cmp(l, e->other) > 0) {
        e->other->left = true;
        l->left = false;
    }

    l->line = r->line = e->line;

    e->other->other = l;
    e->other = r;

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
    LOG("DEBUG: insert %s\n", ev_str(e));
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
    LOG("DEBUG: join   %s with %s\n", ev_str(o1), ev_str(e));
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
    LOG("DEBUG: poly   %s\n", ev_str(e));
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

#if 0
/**
 * An event is interpreted as edge by using e->p as source and e->other->p as
 * destination.
 */
static bool edge_is_cw(ctxt_t *c, event_t *e)
{
    /* at left edge, we have in_out information */
    if (!e->left) {
        assert(e->other->left);
        return !edge_is_cw(c, e->other);
    }

    switch (c->op) {
    case CP_OP_ADD:
    case CP_OP_SUB:
        return (e->inside != e->in_out);
    case CP_OP_CUT:
        return (e->inside == e->in_out);
    case CP_OP_XOR:
        return e->in_out; /* untested, because SCAD does not support this */
    }
    CP_DIE("unhandled op: %u\n", c->op);
}
#endif

/**
 * Construct the poly from the chains */
static void path_make(
    ctxt_t *c,
    cp_csg2_poly_t *r,
    cp_csg2_path_t *p,
    event_t *e0)
{
    assert(p->point_idx.size == 0);
    event_t *e1 = CP_BOX_OF(cp_ring_step(&e0->node_chain, 0), event_t, node_chain);

    (void)c;
#if 0 /* FIXME: this fails.  why? */
    /* determine direction by checking inside vs. outside */
    assert(e0->p != e1->p);
    assert(e0->other->p != e1->other->p);

    /* e0 may not be connected to e1 via 'other' point, because it may be
     * that this is exactly the connection point, whether one edge is
     * missing.  E.g. the triangle a--b--c may be stored by using a,b
     * from even a--b and using c from b--c.  In poly_add(), we use
     * exactly one of those ends.
     */
    /* make it so that the path is clockwise around the inner part */
    /* if e1->other->p == e0->p, then e1 points toward e0, i.e., should point
     * into ccw direction, i.e., we negate. */

    bool cw1 = edge_is_cw(c, e0) != (e0->other->p == e1->p);
    bool cw2 = edge_is_cw(c, e1) == (e1->other->p == e0->p);
    assert(cw1 == cw2);

    if (edge_is_cw(c, e0) != (e0->other->p == e1->p)) {
        CP_SWAP(&e0, &e1);
    }
#endif

    /* first and second point */
    path_add_point(r, p, e0);
    path_add_point(r, p, e1);
    for (cp_ring_each(_ei, &e0->node_chain, &e1->node_chain)) {
        event_t *ei = CP_BOX_OF(_ei, event_t, node_chain);
        path_add_point(r, p, ei);
    }

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
    cp_csg2_init((cp_csg2_t*)r, CP_CSG2_POLY, loc);

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
    LOG("DEBUG: out:   %s (%p)\n", ev_str(e), e);

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
        assert(!cp_ring_is_end(&o1->node_chain));
        assert(!cp_ring_is_end(&o2->node_chain));
        /* put in poly list */
        poly_add(c, o1);
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
    LOG("DEBUG: A: %g %g %u %u\n", ka, ma, ks, ms);
    if (fabs(ka) < fabs(ma)) {
        CP_SWAP(&ka, &ma);
        CP_SWAP(&kb, &mb);
        CP_SWAP(&ks, &ms);
    }
    /* ka is closer to +-1 than ma; ma is closer to 0 than ka */

    if (ks != ms) {
        if (cp_equ(ma,0)) {
            _LINE_X(ks,r) = mb;
            _LINE_Y(ks,r) = (ka * mb) + kb;
            return;
        }
        /* need to switch one of the two into opposite axis.  better do this
         * with ka/kb/ks, because we're closer to +-1 there */
        assert(!cp_equ(ka,0));
        ka = 1/ka;
        kb *= -ka;
        ks = ms;
    }

    LOG("DEBUG: B: %g %g %u %u\n", ka, ma, ks, ms);
    assert(!cp_equ(ka, ma) && "parallel lines should be handled in find_intersection, not here");
    assert((ks == ms) || cp_equ(ma,0));
    double q = (mb - kb) / (ka - ma);
    _LINE_X(ks,r) = q;
    _LINE_Y(ks,r) = (ka * q) + kb;
}

static bool in_order(cp_f_t a, cp_f_t b, cp_f_t c)
{
    return (a < c) ? (cp_leq(a,b) && cp_leq(b,c)) : (cp_geq(a,b) && cp_geq(b,c));
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
#if 1
    /* Intersections are always calculated from the original input data so that
     * no errors add up. */

    /* parallel/collinear? */
    if ((e0->line.swap == e1->line.swap) && cp_equ(e0->line.a, e1->line.a)) {
        /* properly parallel? */
        *collinear = cp_equ(e0->line.b, e1->line.b);
        return NULL;
    }

    /* get intersection point */
    cp_vec2_t i;
    intersection_point(
        &i,
        e0->line.a, e0->line.b, e0->line.swap,
        e1->line.a, e1->line.b, e1->line.swap);

    /* check whether g is on e0 and e1 */
    if (!in_order(p0->coord.x, i.x, p0b->coord.x) ||
        !in_order(p0->coord.y, i.y, p0b->coord.y) ||
        !in_order(p1->coord.x, i.x, p1b->coord.x) ||
        !in_order(p1->coord.y, i.y, p1b->coord.y))
    {
        return NULL;
    }

    /* check whether g is exactly an end */
    if (cp_vec2_equ(&i, &p0->coord))  { return p0; }
    if (cp_vec2_equ(&i, &p0b->coord)) { return p0b; }
    if (cp_vec2_equ(&i, &p1->coord))  { return p1; }
    if (cp_vec2_equ(&i, &p1b->coord)) { return p1b; }

    /* new point */
    return pt_new(c, p0->loc, &i);
#else
    /* ORIGINAL: like in Martinez' sample implementation, where the rounding error
     * will build up along the lines as more and more pieces are cut off.
     */
    cp_vec2_t d0;
    cp_vec2_sub(&d0, &p0b->coord, &p0->coord);
    cp_vec2_t d1;
    cp_vec2_sub(&d1, &p1b->coord, &p1->coord);
    cp_vec2_t e;
    cp_vec2_sub(&e, &p1->coord, &p0->coord);

    LOG("DEBUG: intersection: %s--%s (d=%s) with %s--%s (d=%s)\n",
        pt_str(p0), pt_str(p0b), coord_str(&d0),
        pt_str(p1), pt_str(p1b), coord_str(&d1));

    /* See Schneider, Eberly (2003) */
    cp_f_t cross = cp_vec2_cross_z(&d0, &d1);
    cp_f_t sqr_cross = cross * cross;
    cp_f_t sqr_len0 = cp_vec2_sqr_len(&d0);
    cp_f_t sqr_len1 = cp_vec2_sqr_len(&d1);

    /* single intersection point? */
    if (sqr_cross > (cp_sqr_epsilon * sqr_len0 * sqr_len1)) {
        /* check whether the intersection is really inside the segments */
        cp_f_t s = cp_vec2_cross_z(&e, &d1) / cross;
        cp_f_t t = cp_vec2_cross_z(&e, &d0) / cross;

        /* not without segments? */
        if (cp_lt(s, 0) || cp_gt(s, 1) || cp_lt(t, 0) || cp_gt(t, 1)) {
            return NULL;
        }

        /* equal to end points? */
        if (cp_equ(s, 0)) { return p0; }
        if (cp_equ(s, 1)) { return p0b; }
        if (cp_equ(t, 0)) { return p1; }
        if (cp_equ(t, 1)) { return p1b; }

        /* we indeed have a single, non-trivial intersection point */
        cp_vec2_t i;
        cp_vec2_mul(&i, &d0, s);
        cp_vec2_add(&i, &i, &p0->coord);
        return pt_new(c, p0->loc, &i);
    }

    /* parallel: same or disjoint? */
    cp_f_t sqr_len_e = cp_vec2_sqr_len(&e);
    cross = cp_vec2_cross_z(&e, &d0);
    sqr_cross = cross * cross;
    if (sqr_cross > (cp_sqr_epsilon * sqr_len0 * sqr_len_e)) {
        return NULL;
    }

    /* find intersections on same line */
    CP_NYI("overlapping lines");
    (void)o2_p;
    return NULL;
#endif
}

static bool check_intersection(
    ctxt_t *c,
    event_t *e1,
    event_t *e2)
{
    bool collinear;
    point_t *ip = find_intersection(&collinear, c, e1, e2);

    LOG("DEBUG: #intersect = %p %u\n", ip, collinear);

    if (ip != NULL) {
        if ((e1->p == e2->p) || (e1->other->p == e2->other->p)) {
            return true;
        }
        if ((e1->p != ip) && (e1->other->p != ip)) {
            divide_segment(c, e1, ip);
        }
        if ((e2->p != ip) && (e2->other->p != ip)) {
            divide_segment(c, e2, ip);
        }
        return true;
    }

    if (!collinear) {
        return true;
    }

    /* check for overlap (note: e1 and e2 are both left events) */
    assert(pt_cmp(e1->p, e1->other->p) < 0);
    assert(pt_cmp(e2->p, e2->other->p) < 0);
    if (pt_cmp(e1->other->p, e2->p) < 0) {
        return true;
    }
    if (pt_cmp(e2->other->p, e1->p) < 0) {
        return true;
    }

    /* self-overlap? */
    if (e1->io.poly_id == e2->io.poly_id) {
        cp_vchar_printf(&c->err->msg,
            "Overlapping edge in same polygon is not supported.\n");
        c->err->loc = e1->loc;
        return false;
    }

    event_t *sev[4];
    size_t sev_cnt = 0;
    intersection_add_ev(sev, &sev_cnt, e1, e2);
    intersection_add_ev(sev, &sev_cnt, e1->other, e2->other);
    assert(sev_cnt >= 2);
    assert(sev_cnt <= cp_countof(sev));

    unsigned same_or_diff = (e1->io.in_out == e2->io.in_out) ? E_SAME : E_DIFF;

    if (sev_cnt == 2) {
        assert(sev[0] == NULL);
        assert(sev[1] == NULL);
        e1->io.type = e1->other->io.type = E_IGNORE;
        e2->io.type = e2->other->io.type = same_or_diff;
        return true;
    }
    if (sev_cnt == 3) {
        assert(sev[1] != NULL);
        assert((sev[0] == NULL) || (sev[2] == NULL));
        sev[1]->io.type = sev[1]->other->io.type = E_IGNORE;
        (sev[0] ?: sev[2])->other->io.type = same_or_diff;
        divide_segment(c, sev[0] ?: sev[2]->other, sev[1]->p);
        return true;
    }

    assert(sev_cnt == 4);
    assert(sev[0] != NULL);
    assert(sev[1] != NULL);
    assert(sev[2] != NULL);
    assert(sev[3] != NULL);

    if (sev[0] != sev[3]->other) {
        sev[1]->io.type = E_IGNORE;
        sev[2]->io.type = same_or_diff;
        divide_segment(c, sev[0], sev[1]->p);
        divide_segment(c, sev[1], sev[2]->p);
        return true;
    }

    sev[1]->io.type = sev[1]->other->io.type = E_IGNORE;
    divide_segment(c, sev[0], sev[1]->p);

    sev[3]->other->io.type = same_or_diff;
    divide_segment(c, sev[3]->other, sev[2]->p);

    return true;
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

static bool ev_left(
    ctxt_t *c,
    event_t *e)
{
    assert(!cp_dict_is_member(&e->node_s));
    assert(!cp_dict_is_member(&e->other->node_s));
    LOG("DEBUG: insert_s: %p (%p)\n", e, e->other);
    cp_dict_t *o __unused = cp_dict_insert(&e->node_s, &c->s, seg_cmp_s, NULL, 0);
    assert(o == NULL);

    event_t *prev = s_prev(e);
    event_t *prpr = s_prev(prev);
    assert(e->left);
    assert((prev == NULL) || prev->left);
    assert((prpr == NULL) || prpr->left);

    if (prev == NULL) {
        e->io.inside = false;
        e->io.in_out = false;
    }
    else
    if (prev->io.type != E_NORMAL) {
        if (prpr == NULL) {
            e->io.inside = true;
            e->io.in_out = false;
        }
        else
        if (prev->io.poly_id == e->io.poly_id) {
            e->io.inside = !prpr->io.in_out;
            e->io.in_out = !prev->io.in_out;
        }
        else {
            e->io.inside = !prev->io.in_out;
            e->io.in_out = !prpr->io.in_out;
        }
    }
    else
    if (e->io.poly_id == prev->io.poly_id) {
        e->io.inside = prev->io.inside;
        e->io.in_out = !prev->io.in_out;
    }
    else {
        e->io.inside = !prev->io.in_out;
        e->io.in_out = prev->io.inside;
    }

    debug_print_s(c, "left after insert", e);

    event_t *next = s_next(e);
    if (next != NULL) {
        if (!check_intersection(c, e, next)) {
            return false;
        }
    }
    if (prev != NULL) {
        if (!check_intersection(c, prev, e)) {
            return false;
        }
    }

    debug_print_s(c, "left after intersect", e);
    return true;
}

static bool ev_right(
    ctxt_t *c,
    event_t *e)
{
    event_t *sli = e->other;
    event_t *next = s_next(sli);
    event_t *prev = s_prev(sli);

    /* first remove from s */
    LOG("DEBUG: remove_s: %p (%p)\n", e->other, e);
    cp_dict_remove(&sli->node_s, &c->s);
    assert(!cp_dict_is_member(&e->node_s));
    assert(!cp_dict_is_member(&e->other->node_s));

    /* then add to out */
    switch (e->io.type) {
    case E_NORMAL:
        switch(c->op) {
        case CP_OP_CUT:
            if (sli->io.inside) {
                chain_add(c, e);
            }
            break;
        case CP_OP_ADD:
            if (!sli->io.inside) {
                chain_add(c, e);
            }
            break;
        case CP_OP_SUB:
            if (((e->io.poly_id == 0) && !sli->io.inside) ||
                ((e->io.poly_id == 1) &&  sli->io.inside))
            {
                chain_add(c, e);
            }
            break;
        case CP_OP_XOR:
            chain_add(c, e);
            break;
        }
        break;
    case E_SAME:
        if ((c->op == CP_OP_CUT) || (c->op == CP_OP_ADD)) {
            chain_add(c, e);
        }
        break;
    case E_DIFF:
        if (c->op == CP_OP_SUB) {
            chain_add(c, e);
        }
        break;
    default:
        break;
    }

    if ((next != NULL) && (prev != NULL)) {
        if (!check_intersection(c, prev, next)) {
            return false;
        }
    }

    debug_print_s(c, "right after intersect", e);
    return true;
}

static inline event_t *q_extract_min(ctxt_t *c)
{
    return CP_BOX0_OF(cp_dict_extract_min(&c->q), event_t, node_q);
}

extern bool cp_csg2_op_poly(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_poly_t *r,
    cp_loc_t loc,
    cp_csg2_poly_t *a,
    cp_csg2_poly_t *b,
    cp_bool_op_t op)
{
    TRACE("#a.path=%"_Pz"u #b.path=%"_Pz"u", a->path.size, b->path.size);

#if OPT >= 1
    /* trivial case: empty polygon */
    if ((a->path.size == 0) || (b->path.size == 0)) {
        LOG("DEBUG: one polygon is empty\n");
        switch (op) {
        case CP_OP_CUT:
            return true;

        case CP_OP_SUB:
            CP_SWAP(r, a);
            return true;

        case CP_OP_ADD:
        case CP_OP_XOR:
            CP_SWAP(r, a->path.size == 0 ? b : a);
            return true;
        }
        CP_DIE("Unrecognised operation");
    }
#endif

    /* make context */
    ctxt_t c = {
        .pool = pool,
        .err = t,
        .op = op,
        .bb = { CP_MINMAX_EMPTY, CP_MINMAX_EMPTY },
    };
    cp_list_init(&c.poly);

    /* trivial case: bounding box does not overlap */
    cp_csg2_poly_minmax(&c.bb[0], a);
    cp_csg2_poly_minmax(&c.bb[1], b);
    c.minmaxx = cp_min(c.bb[0].max.x, c.bb[1].max.x);

#if OPT >= 2
    if (cp_gt(c.bb[0].min.x, c.bb[1].max.x) ||
        cp_gt(c.bb[1].min.x, c.bb[0].max.x) ||
        cp_gt(c.bb[0].min.y, c.bb[1].max.y) ||
        cp_gt(c.bb[1].min.y, c.bb[0].max.y))
    {
        LOG("DEBUG: bounding boxes do not overlap: copy\n");
        switch (op) {
        case CP_OP_CUT:
            return true;

        case CP_OP_SUB:
            CP_SWAP(r, a);
            return true;

        case CP_OP_ADD:
        case CP_OP_XOR:
            CP_SWAP(r, a);
            cp_csg2_poly_merge(r, b);
            return true;
        }
        assert(0 && "Unrecognised operation");
    }
#endif

    /* initialise queue */
    LOG("DEBUG: poly 0: #path=%"_Pz"u\n", a->path.size);
    for (cp_v_each(i, &a->path)) {
        cp_csg2_path_t *p = &cp_v_nth(&a->path, i);
        for (cp_v_each(j, &p->point_idx)) {
            cp_vec2_loc_t *pj = cp_csg2_path_nth(a, p, j);
            cp_vec2_loc_t *pk = cp_csg2_path_nth(a, p, cp_wrap_add1(j, p->point_idx.size));
            q_add_orig(&c, pj->loc, &pj->coord, &pk->coord, 0);
        }
    }
    LOG("DEBUG: poly 1: #path=%"_Pz"u\n", b->path.size);
    for (cp_v_each(i, &b->path)) {
        cp_csg2_path_t *p = &cp_v_nth(&b->path, i);
        for (cp_v_each(j, &p->point_idx)) {
            cp_vec2_loc_t *pj = cp_csg2_path_nth(b, p, j);
            cp_vec2_loc_t *pk = cp_csg2_path_nth(b, p, cp_wrap_add1(j, p->point_idx.size));
            q_add_orig(&c, pj->loc, &pj->coord, &pk->coord, 1);
        }
    }
    LOG("DEBUG: start\n");

    /* run algorithm */
    size_t ev_cnt __unused = 0;
    for (;;) {
        event_t *e = q_extract_min(&c);
        if (e == NULL) {
            break;
        }

        LOG("\nDEBUG: event %"_Pz"u: %s o=(%sin %sio)\n",
            ++ev_cnt,
            ev_str(e),
            e->other->inside ? "+" : "-",
            e->other->in_out ? "+" : "-");

#if OPT >= 3
        /* trivial: all the rest is cut away */
        if ((op == CP_OP_CUT && cp_gt(e->p->coord.x, c.minmaxx)) ||
            (op == CP_OP_SUB && cp_gt(e->p->coord.x, c.bb[0].max.x)))
        {
            break;
        }
#endif

#if OPT >= 4
        /* trivial: nothing more to merge */
        if ((op == CP_OP_ADD && (e->p->coord.x > c.minmaxx))) {
            if (!e->left) {
                CP_ZERO(&e->node_s);
                CP_ZERO(&e->other->node_s);
                chain_add(&c, e);
            }
            while ((e = q_extract_min(&c)) != NULL) {
                if (!e->left) {
                    CP_ZERO(&e->node_s);
                    CP_ZERO(&e->other->node_s);
                    chain_add(&c, e);
                }
            }
            break;
        }
#endif

        /* do real work on event */
        if (e->left) {
            if (!ev_left(&c, e)) {
                return false;
            }
        }
        else {
            if (!ev_right(&c, e)) {
                return false;
            }
        }
    }

    poly_make(r, &c, loc);

    return true;
}

static bool csg2_op_poly(
    cp_csg2_poly_t *o,
    cp_csg2_poly_t *a)
{
    TRACE();
    CP_SWAP(o, a);
    return true;
}

static bool csg2_op_csg2(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_poly_t *o,
    cp_csg2_t *a);

#define AUTO_POLY(oi, loc) \
    cp_csg2_poly_t oi; \
    CP_ZERO(&oi); \
    cp_csg2_init((cp_csg2_t*)&oi, CP_CSG2_POLY, loc);

static bool csg2_op_v_csg2(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_poly_t *o,
    cp_loc_t loc,
    cp_v_csg2_p_t *a)
{
    TRACE("n=%"_Pz"u", a->size);
    for (cp_v_each(i, a)) {
        cp_csg2_t *ai = cp_v_nth(a,i);

        if (i == 0) {
            if (!csg2_op_csg2(pool, t, r, zi, o, ai)) {
                return false;
            }
        }
        else {
            AUTO_POLY(oi, ai->loc);
            if (!csg2_op_csg2(pool, t, r, zi, &oi, ai)) {
                return false;
            }

            if (!cp_csg2_op_poly(pool, t, o, loc, o, &oi, CP_OP_ADD)) {
                return false;
            }
        }
    }
    return true;
}

static bool csg2_op_add(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_poly_t *o,
    cp_csg2_add_t *a)
{
    TRACE();
    return csg2_op_v_csg2(pool, t, r, zi, o, a->loc, &a->add);
}

static bool csg2_op_layer(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    cp_csg2_poly_t *o,
    cp_csg2_layer_t *a)
{
    TRACE();
    return csg2_op_add(pool, t, r, a->zi, o, &a->root);
}

static bool csg2_op_sub(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_poly_t *o,
    cp_csg2_sub_t *a)
{
    TRACE();
    if (!csg2_op_add(pool, t, r, zi, o, &a->add)) {
        return false;
    }

    AUTO_POLY(os, a->sub.loc);
    if (!csg2_op_add(pool, t, r, zi, &os, &a->sub)) {
        return false;
    }

    return cp_csg2_op_poly(pool, t, o, a->loc, o, &os, CP_OP_SUB);
}

static bool csg2_op_cut(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_poly_t *o,
    cp_csg2_cut_t *a)
{
    TRACE();
    for (cp_v_each(i, &a->cut)) {
        cp_csg2_add_t *b = cp_v_nth(&a->cut, i);
        if (i == 0) {
            if (!csg2_op_add(pool, t, r, zi, o, b)) {
                return false;
            }
        }
        else {
            AUTO_POLY(oc, b->loc);
            if (!csg2_op_add(pool, t, r, zi, &oc, b)) {
                return false;
            }
            if (!cp_csg2_op_poly(pool, t, o, b->loc, o, &oc, CP_OP_CUT)) {
                return false;
            }
        }
    }

    return true;
}

static bool csg2_op_stack(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_poly_t *o,
    cp_csg2_stack_t *a)
{
    TRACE();

    cp_csg2_layer_t *l = cp_csg2_stack_get_layer(a, zi);
    if (l == NULL) {
        return true;
    }
    if (zi != l->zi) {
        assert(l->zi == 0); /* not visited: must be empty */
        return true;
    }

    assert(zi == l->zi);
    return csg2_op_layer(pool, t, r, o, l);
}

static bool csg2_op_csg2(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_poly_t *o,
    cp_csg2_t *a)
{
    TRACE();
    switch (a->type) {
    case CP_CSG2_CIRCLE:
        CP_NYI("circle");
        return false;

    case CP_CSG2_POLY:
        return csg2_op_poly(o, cp_csg2_poly(a));

    case CP_CSG2_ADD:
        return csg2_op_add(pool, t, r, zi, o, cp_csg2_add(a));

    case CP_CSG2_SUB:
        return csg2_op_sub(pool, t, r, zi, o, cp_csg2_sub(a));

    case CP_CSG2_CUT:
        return csg2_op_cut(pool, t, r, zi, o, cp_csg2_cut(a));

    case CP_CSG2_STACK:
        return csg2_op_stack(pool, t, r, zi, o, cp_csg2_stack(a));
    }

    CP_DIE("2D object type");
    return false;
}

extern bool cp_csg2_op_add_layer(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    cp_csg2_tree_t *a,
    size_t zi)
{
    TRACE();
    cp_csg2_stack_t *s = cp_csg2_stack(r->root);
    assert(zi < s->layer.size);

    AUTO_POLY(o, NULL);
    if (!csg2_op_csg2(pool, t, r, zi, &o, a->root)) {
        return false;
    }
    LOG("DEBUG: #o.point: %"_Pz"u\n", o.point.size);

    if (o.point.size > 0) {
        /* new layer */
        cp_csg2_layer_t *layer = cp_csg2_stack_get_layer(s, zi);
        assert(layer != NULL);
        cp_csg2_add_init_perhaps(&layer->root, NULL);

        layer->zi = zi;

        cp_v_nth(&r->flag, zi) |= CP_CSG2_FLAG_NON_EMPTY;

        /* use new polygon */
        cp_csg2_t *_o2 = cp_csg2_new(CP_CSG2_POLY, NULL);
        cp_csg2_poly_t *o2 = &_o2->poly;
        CP_SWAP(&o, o2);

        /* single polygon per layer */
        cp_v_push(&layer->root.add, _o2);
    }

    return true;
}

extern void cp_csg2_op_tree_init(
    cp_csg2_tree_t *r,
    cp_csg2_tree_t const *a)
{
    TRACE();
    r->root = cp_csg2_new(CP_CSG2_STACK, NULL);

    size_t cnt = a->z.size;

    cp_csg2_stack_t *c = cp_csg2_stack(r->root);
    cp_v_init0(&c->layer, cnt);

    cp_v_init0(&r->z, cnt);
    cp_v_copy_arr(&r->z, 0, &a->z, 0, cnt);

    cp_v_init0(&r->flag, cnt);
}
