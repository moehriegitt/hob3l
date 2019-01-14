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
#define HACK 0

/** new version of collinearity handling */
#define NEW_COLLINEAR 0

#include <stdio.h>
#include <hob3lbase/dict.h>
#include <hob3lbase/list.h>
#include <hob3lbase/ring.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/pool.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/panic.h>
#include <hob3lbase/obj.h>
#include <hob3l/csg.h>
#include <hob3l/csg2.h>
#include <hob3l/ps.h>
#include <hob3l/csg2-bitmap.h>
#include "internal.h"

typedef struct cp_path_ev cp_path_ev_t;
typedef struct cp_path_pt cp_path_pt_t;

/**
 * Points found by algorithm
 */
struct cp_path_pt {
    cp_dict_t node_pt;

    cp_vec2_loc_t v;

    /**
     * Index in output point array.
     * Initialised to CP_SIZE_MAX.
     */
    size_t point_idx;

    /**
     * Index in output face */
    size_t face_idx;

    /**
     * Number of times this point is used in the resulting polygon. */
    size_t path_cnt;

    /**
     * Next in face_idx list */
    cp_path_pt_t *next;
};

/**
 * Events when the algorithm progresses.
 * Points with more info in the left-right plain sweep.
 */
struct cp_path_ev {
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
    };

    cp_loc_t loc;
    cp_path_pt_t *p;
    cp_path_ev_t *other;

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
        /** direction vector of line */
        cp_vec2_t dir;
    } line;

#ifdef PSTRACE
    /**
     * For debug printing */
    size_t debug_tag;
#endif
};

typedef cp_path_ev_t event_t;
typedef cp_path_pt_t point_t;

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
    cp_pool_t *tmp;

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
    cp_csg2_op_bitmap_t const *comb;

    /** Number of valid bits in comb */
    size_t comb_size;

    /** Whether to output all points or to drop those of adjacent collinear
     * lines. */
    bool all_points;

    /**
     * Temporary array for processing vertices when connecting polygon chains
     * FIXME: temporary should be in pool.
     */
    v_event_p_t vert;

    /**
     * Whether to flatten the polygons into disjoint paths.  This must
     * be 'true' for constructing linear_extrudes from the polygons to
     * avoid non-2-manifold constructions, but it must be 'false' for
     * the triangulation to work on the result, because 'true'
     * introduces bends that cannot be handled by the triangulation
     * algorithm.
     */
    bool flatten;
} ctxt_t;

/**
 * Context for csg2_op_csg2 functions.
 */
typedef struct {
    cp_csg_opt_t const *opt;
    cp_pool_t *tmp;
} op_ctxt_t;

/**
 * For clearing face_idx when detecting rings: a stack to
 * go back to start of ring.
 */
typedef struct {
    point_t *head;
} stack_t;

CP_UNUSED
static char const *coord_str_(char *s, size_t n, cp_vec2_t const *x)
{
    if (x == NULL) {
        return "NULL";
    }
    snprintf(s, n, FD2, CP_V01(*x));
    s[n-1] = 0;
    return s;
}

#define coord_str(p) coord_str_((char[50]){0}, 50, p)

CP_UNUSED
static char const *pt_str_(char *s, size_t n, point_t const *x)
{
    if (x == NULL) {
        return "NULL";
    }
    snprintf(s, n, FD2, CP_V01(x->v.coord));
    s[n-1] = 0;
    return s;
}

#define pt_str(p) pt_str_((char[50]){}, 50, p)

CP_UNUSED
static char const *ev_str_(char *s, size_t n, event_t const *x)
{
    if (x == NULL) {
        return "NULL";
    }
    if (x->left) {
        snprintf(s, n, "#("FD2"--"FD2")  o0x%"CP_Z"x b0x%"CP_Z"x",
            CP_V01(x->p->v.coord),
            CP_V01(x->other->p->v.coord),
            x->in.owner,
            x->in.below);
    }
    else {
        snprintf(s, n, " ("FD2"--"FD2")# o0x%"CP_Z"x b0x%"CP_Z"x",
            CP_V01(x->other->p->v.coord),
            CP_V01(x->p->v.coord),
            x->in.owner,
            x->in.below);
    }
    s[n-1] = 0;
    return s;
}

#define ev_str(x) ev_str_((char[80]){}, 80, x)

static event_t *chain_other(event_t *e)
{
    assert(cp_ring_is_moiety(&e->node_chain));
    event_t *o = CP_BOX_OF(cp_ring_step(&e->node_chain, 0), event_t, node_chain);
    assert(e->p == o->p);
    return o;
}

#ifdef PSTRACE
static void debug_print_chain(
    event_t *e,
    size_t tag)
{
    if (e->debug_tag == tag) {
        return;
    }
    if (cp_ring_is_singleton(&e->node_chain)) {
        return;
    }

    e->debug_tag = tag;
    debug_print_chain(chain_other(e), tag);

    cp_printf(cp_debug_ps, "newpath %g %g moveto", CP_PS_XY(e->p->v.coord));

    for (;;) {
        e = e->other;
        if (e->debug_tag == tag) {
            break;
        }
        cp_printf(cp_debug_ps, " %g %g lineto", CP_PS_XY(e->p->v.coord));
        if (cp_ring_is_singleton(&e->node_chain)) {
            break;
        }
        e->debug_tag = tag;
        e = chain_other(e);
        e->debug_tag = tag;
    }
    if (e->debug_tag == tag) {
        cp_printf(cp_debug_ps, " closepath");
    }
    cp_printf(cp_debug_ps, " stroke\n");

    if (e->debug_tag != tag) {
        cp_debug_ps_dot(CP_PS_XY(e->p->v.coord), 7);
    }
}
#endif

#if DEBUG || defined(PSTRACE)
CP_PRINTF(2,6)
static void debug_print_s(
    ctxt_t *c,
    char const *msg,
    event_t *es,
    event_t *epr CP_UNUSED,
    event_t *ene CP_UNUSED,
    ...)
{
    va_list va CP_UNUSED;
#if DEBUG
    LOG("S ");
    va_start(va, ene);
    VLOG(msg, va);
    va_end(va);
    LOG("\n");
    for (cp_dict_each(_e, c->s)) {
        event_t *e = CP_BOX_OF(_e, event_t, node_s);
        LOG("S: %s\n", ev_str(e));
    }
#endif

#ifdef PSTRACE
    /* output to postscript */
    if (cp_debug_ps_page_begin()) {
        /* compute values for --debug-ps-xlat-x,y to center at \p es */
        if (es != NULL) {
            double x = CP_PS_X(es->p->v.coord.x) - (CP_PS_PAPER_X/2);
            double y = CP_PS_Y(es->p->v.coord.y) - (CP_PS_PAPER_Y/2);
            cp_printf(cp_debug_ps,
                "%g 10 moveto "
                "(center: %g %g) dup "
                "stringwidth pop neg 0 rmoveto "
                "show\n",
                CP_PS_PAPER_X - 15.0,
                cp_debug_ps_xlat_x - (x / cp_debug_ps_xform.mul_x),
                cp_debug_ps_xlat_y - (y / cp_debug_ps_xform.mul_y));
        }

        /* print info */
        cp_printf(cp_debug_ps, "30 30 moveto (CSG: ");
        va_start(va, ene);
        cp_vprintf(cp_debug_ps, msg, va);
        va_end(va);
        cp_printf(cp_debug_ps, ") show\n");
        cp_printf(cp_debug_ps, "30 45 moveto (%s =prev) show\n", epr ? ev_str(epr) : "NULL");
        cp_printf(cp_debug_ps, "30 60 moveto (%s =this) show\n", es  ? ev_str(es)  : "NULL");
        cp_printf(cp_debug_ps, "30 75 moveto (%s =next) show\n", ene ? ev_str(ene) : "NULL");

        /* sweep line */
        cp_printf(cp_debug_ps, "0.8 setgray 1 setlinewidth\n");
        cp_printf(cp_debug_ps,
            "newpath %g dup 0 moveto %u lineto stroke\n",
            CP_PS_X(es->p->v.coord.x),
            CP_PS_PAPER_Y);
        /* mark rasterization around current point */
        cp_printf(cp_debug_ps,
            "newpath %g %g moveto %g %g lineto %g %g lineto %g %g lineto closepath stroke\n",
            CP_PS_X(es->p->v.coord.x - cp_pt_epsilon),
            CP_PS_Y(es->p->v.coord.y - cp_pt_epsilon),
            CP_PS_X(es->p->v.coord.x + cp_pt_epsilon),
            CP_PS_Y(es->p->v.coord.y - cp_pt_epsilon),
            CP_PS_X(es->p->v.coord.x + cp_pt_epsilon),
            CP_PS_Y(es->p->v.coord.y + cp_pt_epsilon),
            CP_PS_X(es->p->v.coord.x - cp_pt_epsilon),
            CP_PS_Y(es->p->v.coord.y + cp_pt_epsilon));
        if (!es->left) {
            cp_printf(cp_debug_ps,
                "2 setlinewidth newpath %g %g moveto %g %g lineto stroke\n",
                CP_PS_XY(es->p->v.coord),
                CP_PS_XY(es->other->p->v.coord));
        }
        if ((epr != NULL) && (es->p == epr->other->p)) {
            cp_printf(cp_debug_ps,
                "5 setlinewidth newpath %g %g moveto %g %g lineto stroke\n",
                CP_PS_XY(es->p->v.coord),
                CP_PS_XY(epr->p->v.coord));
        }
        if ((ene != NULL) && (es->p == ene->other->p)) {
            cp_printf(cp_debug_ps,
                "5 setlinewidth newpath %g %g moveto %g %g lineto stroke\n",
                CP_PS_XY(es->p->v.coord),
                CP_PS_XY(ene->p->v.coord));
        }

        /* pt */
        cp_printf(cp_debug_ps, "0.8 setgray\n");
        for (cp_dict_each(_p, c->pt)) {
            point_t *p = CP_BOX_OF(_p, point_t, node_pt);
            cp_debug_ps_dot(CP_PS_XY(p->v.coord), 3);
        }

        /* s */
        cp_printf(cp_debug_ps, "3 setlinewidth\n");
        size_t i = 0;
        for (cp_dict_each(_e, c->s)) {
            event_t *e = CP_BOX_OF(_e, event_t, node_s);
            cp_printf(cp_debug_ps,
                "0 %g 0 setrgbcolor\n", three_steps(i));
            cp_debug_ps_dot(CP_PS_XY(e->p->v.coord), 3);
            cp_printf(cp_debug_ps,
                "newpath %g %g moveto %g %g lineto stroke\n",
                CP_PS_XY(e->p->v.coord),
                CP_PS_XY(e->other->p->v.coord));
            i++;
        }

        /* chain */
        cp_printf(cp_debug_ps, "2 setlinewidth\n");
        i = 0;
        for (cp_dict_each(_e, c->end)) {
            cp_printf(cp_debug_ps, "0 %g 0.8 setrgbcolor\n", three_steps(i));
            event_t *e0 = CP_BOX_OF(_e, event_t, node_end);
            cp_debug_ps_dot(CP_PS_XY(e0->p->v.coord), 4);
            debug_print_chain(e0, cp_debug_ps_page_cnt);
            i++;
        }

        /* end page */
        cp_ps_page_end(cp_debug_ps);
    }
#endif
}

#else
#define debug_print_s(...) ((void)0)
#endif /* DEBUG */

/* ********************************************************************** */
/* Combine Lines into Polygons */

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
    return cp_vec2_lex_pt_cmp(&a->v.coord, &b->v.coord);
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
    void *user CP_UNUSED)
{
    event_t *a = CP_BOX_OF(_a, event_t, node_end);
    event_t *b = CP_BOX_OF(_b, event_t, node_end);
    return pt_cmp(a->p, b->p);
}

/**
 * Insert a vertex into the node_end structure.  Duplicates are OK
 * and will be handled later.
 */
static void end_insert(
    ctxt_t *c,
    event_t *e)
{
    LOG("insert %s\n", ev_str(e));
    (void)cp_dict_insert(&e->node_end, &c->end, pt_cmp_end_d, NULL, +1);
}

static bool q_contains(
    ctxt_t *c,
    event_t *e)
{
    return cp_dict_maybe_member_of(&e->node_q, c->q);
}

static bool s_contains(
    ctxt_t *c,
    event_t *e)
{
    return cp_dict_maybe_member_of(&e->node_s, c->s);
}

/**
 * Add an edge to the output edge.  Only right events are added.
 */
static void chain_add(
    ctxt_t *c,
    event_t *e)
{
    LOG("out:   %s (%p)\n", ev_str(e), e);

    event_t *o= e->other;

    /* the event should left and neither point should be s or q */
    assert(!e->left);
    assert(pt_cmp(e->p, o->p) >= 0);
    assert(!s_contains(c, e));
    assert(!q_contains(c, e));
    assert(!s_contains(c, o));
    assert(!q_contains(c, o));

    /*
     * This algorithm combines output edges into a polygon ring.  Because
     * we can have multiple edges meeting in a single point, we cannot
     * directly connect points as they come it; in some case, this would
     * create crossing paths, which we cannot have.
     *
     * Instead, we first add all points (both ends of each edge) to a
     * set ordered by point coordinates (c->end using node_end).  Left
     * and right vertices of each inserted edge are left singletons
     * (wrt. node_chain), i.e., the edges are defined by ->other,
     * and the next edge is found via a pair in (node_chain).
     * Identical points are in no particular order (we could sort them
     * now already, but we do not need the order for most of the point
     * pair, so comparing would be a waste at this point.  The data
     * structure will, in the end, have an even number of vertices at
     * each point coordinate.  Usually, it will have 2 unless vertices
     * coincide.
     *
     * When everything is inserted, we iterate the c->end data
     * structure and take out groups of equal points.  If there are 2,
     * they are connected into a chain.  For more than 2, the points
     * are sorted by absolute angle so that there is no edge between
     * adjacent vertices. Sorted this way, they can be connected
     * again.
     *
     * This second step will notice collapses of edges in the form
     * a-b-a, because the angle of the two a-b edges is equal.  Both
     * vertices of these edges are removed from the data structures.
     * (It may be that the countervertex is the same edge, as in a-b-c,
     * but there may also be two distinct vertices stemming from longer
     * collapsed chains, e.g. in  a-b-c-b-a.)
     *
     * In the last step, polygons are reconstructed from the chains
     * (in node_chain), each polygon is found by iterating
     * c->end (in node_end) again, marking what was already extracted.
     *
     * In total, this takes O(n log n) time with n edges found by the
     * algorithm.
     */

    /* make a singleton of the two end points */
    cp_ring_init(&e->node_chain);
    cp_ring_init(&o->node_chain);

    /* insert into c->end */
    end_insert(c, e);
    end_insert(c, o);
}

static void chain_merge(
    ctxt_t *c CP_UNUSED,
    event_t *e1,
    event_t *e2)
{
    assert(e1->p == e2->p);
    e1->p->path_cnt++;
    LOG("chain_merge: %s -- %s -- %s\n",
       pt_str(e1->other->p),
       pt_str(e1->p),
       pt_str(e2->other->p));

    cp_ring_pair(&e1->node_chain, &e2->node_chain);

    debug_print_s(c, "join", e1, e1->other, e2->other);
}

static cp_angle_t ev_atan2(
    event_t *e)
{
    /* We swap x and y in atan2 so that the touching end between -pi and +pi is
     * in the vertical, not horizontal.  This will produce more start/ends,
     * heuristically, compared to bends, which seems good for the triangulation
     * algorithm. */
    cp_angle_t a = atan2(
        e->p->v.coord.x - e->other->p->v.coord.x,
        e->p->v.coord.y - e->other->p->v.coord.y);

    /* identify -pi with +pi so that the angles are ordered equally.
     * Map -PI and -PI to -PI (not +PI), because in vertical lines, the
     * lower node compares smaller than the upper one, and so vertical+to_the_right
     * is not a start, but a bend, which is more brittle in triangulation.  Try to
     * avoid those kinds of edges in conflicting situations.
     */
    if (cp_eq(a, +CP_PI) || cp_eq(a, -CP_PI)) {
        a = -CP_PI;
    }

    return a;
}

static int cmp_atan2(event_t *a, event_t *b)
{
    assert(a->p == b->p);
    return cp_cmp(ev_atan2(a), ev_atan2(b));
}

static int cmp_atan2_p(
    event_t * const *a,
    event_t * const *b,
    void *u CP_UNUSED)
{
    return cmp_atan2(*a, *b);
}

static bool same_dir(event_t *e1, event_t *e2)
{
    /* atan2 is the same option, but it's measurably slow (~5%: 0.88s vs. 0.84s) */
#if 1
    return
        cp_vec2_in_line(
           &e1->other->p->v.coord,
           &e1->p->v.coord,
           &e2->other->p->v.coord) &&
        (
            cp_cmp(0, e1->other->p->v.coord.x - e1->p->v.coord.x) ==
            cp_cmp(0, e2->other->p->v.coord.x - e2->p->v.coord.x)
        ) &&
        (
            cp_cmp(0, e1->other->p->v.coord.y - e1->p->v.coord.y) ==
            cp_cmp(0, e2->other->p->v.coord.y - e2->p->v.coord.y)
        );
#else
    return cmp_atan2(e1, e2) == 0;
#endif
}

/**
 * Handle same point vertices */
static void chain_flush_vertex(
    ctxt_t *c)
{
    LOG("BEGIN: flush_vertex: %"CP_Z"u points\n", c->vert.size);
    assert(c->vert.size > 0);
    assert(((c->vert.size & 1) == 0) && "Odd number of edges meet in one point");

    /* sort by atan2() if we have more than 2 vertices */
    if (c->vert.size > 2) {
        /* avoid atan2 unless really needed, because it's slow */
        cp_v_qsort(&c->vert, 0, ~(size_t)0, cmp_atan2_p, NULL);
    }

    /* remove adjacent equal angles (both of the entries) */
    size_t o = 0;
    for (cp_v_each(i, &c->vert)) {
        event_t *e = cp_v_nth(&c->vert, i);
        /* equal to predecessor? => skip */
        if ((i > 0) && same_dir(e, cp_v_nth(&c->vert, i-1))) {
            continue;
        }
        /* equal to successor? => skip */
        if ((i < (c->vert.size - 1)) && same_dir(e, cp_v_nth(&c->vert, i+1))) {
            continue;
        }

        /* not equal: keep */
        cp_v_nth(&c->vert, o) = e;
        o++;
    }
    c->vert.size = o;

    /* join remaining edges in pairs */
    assert(((c->vert.size & 1) == 0) && "Odd number of edges meet in one point");
    for (size_t i = 0; i < c->vert.size; i += 2) {
        event_t *e1 = cp_v_nth(&c->vert, i);
        event_t *e2 = cp_v_nth(&c->vert, i+1);
        chain_merge(c, e1, e2);
    }
    LOG("END: flush_vertex\n");

    /*
     * In situations where there is a deadend path, the deadend is kept separated
     * by the above loops:
     *
     *    A
     *    |
     *    B===C===D
     *    |
     *    E
     *
     * This will connect A-B-E, but will not connect B-C or B-D.  So the above
     * sub-chain C--D will remain.  It may be connected into longer chains if
     * there are edge B--C, C--D, B--D.  The path_add_point3 will filter it out
     * by the collinear rule.  It may end up with short polies, however.
     */

    /* sweep */
    cp_v_clear(&c->vert, 8);
}

/**
 * Combine longer chains from c->end structure
 */
static void chain_combine(
    ctxt_t *c)
{
    LOG("BEGIN: chain_combine\n");
    /* init */
    cp_v_clear(&c->vert, 8); /* FIXME: temporary: should be in pool */

    /* iterate c->end for same points */
    for (cp_dict_each(_e, c->end)) {
        event_t *e = CP_BOX_OF(_e, event_t, node_end);
        if ((c->vert.size > 0) && (cp_v_last(&c->vert)->p != e->p)) {
            chain_flush_vertex(c);
        }
        cp_v_push(&c->vert, e);
    }
    if (c->vert.size > 0) {
        chain_flush_vertex(c);
    }
    LOG("END: chain_combine\n");
}

/**
 * Add a point to a path.
 * If necessary, allocate a new point */
static void path_add_point(
    ctxt_t *c,
    cp_csg2_poly_t *r,
    cp_csg2_path_t *p,
    stack_t *ps,
    point_t *q)
{
    /* possibly allocate a point */
    size_t pi = q->point_idx;
    if (pi == CP_SIZE_MAX) {
        q->point_idx = pi = r->point.size;
        cp_v_push(&r->point, q->v);
    }
    assert(pi < r->point.size);

    /* if the point is part of the current path already, then
     * create a new path. */
    size_t fi = q->face_idx;
    if (c->flatten && (fi < p->point_idx.size)) {
        size_t cnt = p->point_idx.size - fi;
        if (cnt >= 3) {
            /* make independent path for ring */
            cp_csg2_path_t *p2 = cp_v_push0(&r->path);
            cp_v_copy(&p2->point_idx, 0, &p->point_idx, fi, cnt);
            assert(p2->point_idx.size == cnt);
        }

        /* clear face_idx of ring points (except the shared point) */
        for (cp_size_each(i, cnt, 1)) {
            assert(ps->head != NULL);
            ps->head->face_idx = CP_SIZE_MAX;
            ps->head = ps->head->next;
        }
        assert(ps->head != NULL);

        /* cut off tail */
        cp_v_set_size(&p->point_idx, fi + 1);
    }
    else {
        /* append point to path */
        q->face_idx = p->point_idx.size;
        cp_v_push(&p->point_idx, pi);

        /* push head */
        q->next = ps->head;
        ps->head = q;
    }
}

static bool path_add_point3(
    ctxt_t *c,
    cp_csg2_poly_t *r,
    cp_csg2_path_t *p,
    stack_t *ps,
    event_t *prev,
    event_t *cur,
    event_t *next)
{
    LOG("point3: %p: (%s) -- %s -- (%s)\n",
        cur, pt_str(prev->p), pt_str(cur->p), pt_str(next->p));
    /* mark event used in polygon */
    assert(!cur->used);
    cur->used = true;

    if (c->all_points ||
        (cur->p->path_cnt > 1) ||
        !cp_vec2_in_line(&prev->p->v.coord, &cur->p->v.coord, &next->p->v.coord))
    {
        assert(!cp_vec2_eq(&prev->p->v.coord, &cur->p->v.coord));
        assert(!cp_vec2_eq(&next->p->v.coord, &cur->p->v.coord));
        path_add_point(c, r, p, ps, cur->p);
        return true;
    }

    return false;
}

/**
 * Construct the poly from the chains */
static void path_make(
    ctxt_t *c,
    cp_csg2_poly_t *r,
    event_t *e0)
{
    /* start at unused left points */
    if (!e0->left || e0->used || chain_other(e0)->used) {
        return;
    }

    event_t *e1 = e0->other;
    assert(!e1->left);
    /* e0 is a left edge, i.e., we have an orientation like this: e0--e1 */

    /* Make it so that in e0--e1, 'inside' is below. */
    if (!e1->in.below) {
        CP_SWAP(&e0, &e1);
    }

    /* Keep chain_other(ex)->other == ey by moving to other edge at e0->p. */
    e0 = chain_other(e0);
    event_t *ea = e0;
    event_t *eb = e1;
    event_t *ec = chain_other(e1)->other;
    assert(chain_other(ea)->other == eb);
    assert(chain_other(eb)->other == ec);
    if (ea == ec) {
        /* Too short. Longer chains of collinears are handled below. */
        return;
    }

    /* make a new path */
    cp_csg2_path_t p = {0};
    stack_t ps = {0};

    /* add points, removing collinear ones (if requested) */
    do {
        if (path_add_point3(c, r, &p, &ps, ea, eb, ec)) {
            ea = eb;
        }
        eb = ec;
        ec = chain_other(eb)->other;
    } while (ec != e0);
    if (path_add_point3(c, r, &p, &ps, ea, eb, e0)) {
        ea = eb;
    }
    path_add_point3(c, r, &p, &ps, ea, e0, e1);

    /* too short: throw away, else push */
    if (p.point_idx.size < 3) {
        cp_v_fini(&p.point_idx);
    }
    else {
        cp_v_push(&r->path, p);
    }
}

/**
 * Construct the poly from the chains */
static void poly_make(
    cp_csg2_poly_t *r,
    ctxt_t *c,
    cp_csg2_poly_t const  *t)
{
    CP_COPY_N_ZERO(r, obj, t->obj);

    /* iterate all points again */
    for (cp_dict_each(_e, c->end)) {
        event_t *e = CP_BOX_OF(_e, event_t, node_end);
        /* only start a poly at left nodes to get the orientation right (e->in.below). */
        /* only start at unused points */
        if (e->left && !e->used) {
            LOG("BEGIN: poly: %s\n", pt_str(e->p));
            path_make(c, r, e);
            LOG("END: poly\n");
        }
    }
}

/* ********************************************************************** */
/* CSG Algorithm */

/**
 * Compare a vec2 with a point in a dictionary.
 */
static int pt_cmp_d(
    cp_vec2_t *a,
    cp_dict_t *_b,
    void *user CP_UNUSED)
{
    point_t *b = CP_BOX_OF(_b, point_t, node_pt);
    return cp_vec2_lex_pt_cmp(a, &b->v.coord);
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
    cp_vec2_t const *_coord,
    cp_color_rgba_t const *color)
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

    point_t *p = CP_POOL_NEW(c->tmp, *p);
    p->v.coord = coord;
    p->v.loc = loc;
    p->v.color = *color;
    p->point_idx = CP_SIZE_MAX;
    p->face_idx = CP_SIZE_MAX;

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
    event_t *r = CP_POOL_NEW(c->tmp, *r);
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
    return cp_vec2_right_normal3_z(&a1->v.coord, &a2->v.coord, &b->v.coord);
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

#if NEW_COLLINEAR
static bool is_on_line(
    event_t const *l,
    point_t const *p)
{
    cp_vec2_t np;
    cp_vec2_nearest(&np, &l->p->v.coord, &l->line.dir, &p->v.coord);
    return cp_pt_eq(cp_vec2_dist(&np, &p->v.coord), 0);
}

/**
 * Segment order in S: generally bottom (small) to top (large)
 *
 * This is a bit weird: the seg_cmp() function only works correctly
 * when inserting into S, but will not correctly compare two elements
 * in S, because it expects one parameter to be the newly inserted
 * point defining the new position of the sweep line, i.e., the
 * reference point is the left point of the newly inserted segment.
 * This point and the line starting from there is compared to other
 * lines in S to decide whether the new line is above or below.
 * The newly inserted line's left point is alway in between left and right
 * points of lines in S (wrt. pt_cmp).  If the parameters to this
 * function are swapped, this relationship is not true anymore, and
 * this function may or may not work correctly -- it is not designed
 * to work well in that case.
 *
 * This was ported from a C++ Less() comparison, which seems to pass
 * the new element as second argument.  Our data structures pass the
 * new element as first argument, hence the swap of arguments and
 * result wrt. seg_cmp vs. seg_cmp_s.
 */
static int seg_cmp(event_t const *add, event_t const *old)
{
    /* only left edges are inserted into S */
    assert(old->left);
    assert(add->left);
    /* a segment cannot be added twice */
    assert(old != add);

    LOG("seg_cmp: %s vs %s\n", ev_str(old), ev_str(add));
    /* added point should be inside existing segments wrt. pt_cmp */
    assert(pt_cmp(add->p, old->p) >= 0);
    assert(pt_cmp(add->p, old->other->p) <= 0);

    /* first: determine whether add->p is on line */
    if (!is_on_line(old, add->p)) {
        int l_cmp = pt2_pt_cmp(old->p, old->other->p, add->p);
        assert(l_cmp != 0); /* cannot be collinear: we just checked that add->p is not on old */
        /* non-collinear. left point is above/below.
         * right point: we do not care here; intersection checking will care later */
        return -l_cmp;
    }

    /* second: determine whether a second point is on a line */
    if (is_on_line(old, add->other->p) ||
        ((old->other->p != add->p) && is_on_line(add, old->other->p)) ||
        ((old->p        != add->p) && is_on_line(add, old->p)))
    {
        LOG("seg_cmp: overlap: %d %d %d %d\n",
            is_on_line(old, add->p),
            is_on_line(old, add->other->p),
            is_on_line(add, old->p),
            is_on_line(add, old->other->p));
        return 0;
    }

    int r_cmp = pt2_pt_cmp(old->p, old->other->p, add->other->p);
    assert(r_cmp != 0); /* cannot be collinear: we just checked. */
    return -r_cmp;
}

#else /* !NEW_COLLINEAR */

static int seg_cmp_(event_t const *e1, event_t const *e2)
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
    return -seg_cmp_(e1,e2);
}

#endif /* !NEW_COLLINEAR */

/** dict version of ev_cmp for node_q */
static int ev_cmp_q(
    cp_dict_t *_e1,
    cp_dict_t *_e2,
    void *user CP_UNUSED)
{
    event_t *e1 = CP_BOX_OF(_e1, event_t, node_q);
    event_t *e2 = CP_BOX_OF(_e2, event_t, node_q);
    return ev_cmp(e1, e2);
}

/** dict version of seg_cmp for node_s */
static int seg_cmp_s(
    cp_dict_t *_e1,
    cp_dict_t *_e2,
    void *user CP_UNUSED)
{
    event_t *e1 = CP_BOX_OF(_e1, event_t, node_s);
    event_t *e2 = CP_BOX_OF(_e2, event_t, node_s);
    return seg_cmp(e1, e2);
}

static void q_insert(
    ctxt_t *c,
    event_t *e)
{
    assert(!q_contains(c, e));
    assert((pt_cmp(e->p, e->other->p) < 0) == e->left);
    cp_dict_insert(&e->node_q, &c->q, ev_cmp_q, NULL, 1);
}

static void q_remove(
    ctxt_t *c,
    event_t *e)
{
    assert(q_contains(c, e));
    cp_dict_remove(&e->node_q, &c->q);
}

static inline event_t *q_extract_min(ctxt_t *c)
{
    return CP_BOX0_OF(cp_dict_extract_min(&c->q), event_t, node_q);
}


/**
 * The seg_cmp function ultimately determines whether two
 * lines are collapsing and will compare them equal so that
 * cp_dict_insert fails and we can handle collapses.
 */
static event_t *s_insert(
    ctxt_t *c,
    event_t *add)
{
    assert(!s_contains(c, add));
    assert(add->left);
#if NEW_COLLINEAR
    cp_dict_t *_other = cp_dict_insert(&add->node_s, &c->s, seg_cmp_s, NULL, 0);
    return CP_BOX0_OF(_other, event_t, node_s);
#else
    (void)cp_dict_insert(&add->node_s, &c->s, seg_cmp_s, NULL, +1);
    return NULL;
#endif
}

static void s_remove(
    ctxt_t *c,
    event_t *e)
{
    assert(s_contains(c, e));
    cp_dict_remove(&e->node_s, &c->s);
    assert ((c->s == NULL) || (CP_BOX_OF(c->s, event_t, node_s)->left));
}

static void set_slope(
    event_t *e1)
{
    /* always compute the slope from the left point (this is used by the s_insert
     * collinearity test to sort the points on a line collapsing from two other lines. */
    if (!e1->left) {
        e1 = e1->other;
        assert(e1->left);
    }
    event_t *e2 = e1->other;

    cp_vec2_sub(&e1->line.dir, &e2->p->v.coord, &e1->p->v.coord);
    e1->line.swap = cp_lt(fabs(e1->line.dir.x), fabs(e1->line.dir.y));
    e1->line.a = LINE_Y(e1, &e1->line.dir) / LINE_X(e1, &e1->line.dir);
    e1->line.b = LINE_Y(e1, &e1->p->v.coord) - (e1->line.a * LINE_X(e1, &e1->p->v.coord));
    assert(cp_le(e1->line.a, +1));
    assert(cp_ge(e1->line.a, -1) ||
        CONFESS("a=%g (%g,%g--%g,%g)",
            e1->line.a, e1->p->v.coord.x, e1->p->v.coord.y, e2->p->v.coord.x, e2->p->v.coord.y));

    cp_vec2_unit(&e1->line.dir, &e1->line.dir);

    /* other direction edge is on the same line */
    e2->line = e1->line;
}

CP_UNUSED
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
    cp_vec2_loc_t *v1,
    cp_vec2_loc_t *v2,
    size_t poly_id)
{
    point_t *p1 = pt_new(c, v1->loc, &v1->coord, &v1->color);
    point_t *p2 = pt_new(c, v2->loc, &v2->coord, &v2->color);

    if (p1 == p2) {
        /* edge consisting of only one point (or two coordinates
         * closer than pt_epsilon collapsed) */
        return;
    }

    event_t *e1 = ev_new(c, v1->loc, p1, true,  NULL);
    e1->in.owner = ((size_t)1) << poly_id;

    event_t *e2 = ev_new(c, v2->loc, p2, false, e1);
    e2->in = e1->in;
    e1->other = e2;

    if (pt_cmp(e1->p, e2->p) > 0) {
        assert(!s_contains(c, e1));
        assert(!s_contains(c, e2));
        e1->left = false;
        e2->left = true;
    }

    set_slope(e1);

#ifndef NDEBUG
    /* check computation */
    cp_vec2_t g;
    get_coord_on_line(&g, e1, &e2->p->v.coord);
    assert(cp_vec2_eq(&g, &e2->p->v.coord));
    get_coord_on_line(&g, e2, &e1->p->v.coord);
    assert(cp_vec2_eq(&g, &e1->p->v.coord));
#endif

    /* Insert.  For 'equal' entries, order does not matter */
    q_insert(c, e1);
    q_insert(c, e2);
}

#ifndef NDEBUG
#  define divide_segment(c,e,p)  divide_segment_(__FILE__, __LINE__, c, e, p)
#else
#  define divide_segment_(f,l,c,e,p) divide_segment(c,e,p)
#endif

static void divide_segment_(
    char const *file CP_UNUSED,
    int line CP_UNUSED,
    ctxt_t *c,
    event_t *e,
    point_t *p)
{
    assert(p != e->p);
    assert(p != e->other->p);

    assert(e->left);
    event_t *o = e->other;

    assert(!s_contains(c, o));

    /*
     * Split an edge at a point p on that edge (we assume that p is correct -- no
     * check is done).
     *      p              p
     * e-------.       e--.l--.
     *  `-------o       `--r`--o
     */

    event_t *r = ev_new(c, p->v.loc, p, false, e);
    event_t *l = ev_new(c, p->v.loc, p, true,  o);

    /* relink buddies */
    o->other = l;
    e->other = r;
    assert(r->other == e);
    assert(l->other == o);

    /* copy in/out tracking -- the caller must set this up appropriately */
    r->in = e->in;
    l->in = o->in;

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
        assert(!s_contains(c, o));
        assert(!s_contains(c, l));
    }

    /* For e--r, if we encounter the same corner case, remove the edges from S
     * and put it back into Q -- this should work because the edges were adjacent,
     * we we can process them again. */
    if (ev_cmp(e, r) > 0) {
        r->left = true;
        e->left = false;
        if (s_contains(c, e)) {
            s_remove(c, e);
            q_insert(c, e);
        }
        assert(!s_contains(c, r));
        assert(!s_contains(c, e));
    }

#if 0
    /* copy edge slope and offset */
    l->line = r->line = e->line;
#else
    /* unfortunately, reset slope -- it seems impossible to cope with
     * corner cases otherwise */
    set_slope(l);
    set_slope(r);
#endif

    /* handle new events later */
    q_insert(c, l);
    q_insert(c, r);
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
    if (!dim_between(p0->v.coord.x, i.x, p0b->v.coord.x) ||
        !dim_between(p0->v.coord.y, i.y, p0b->v.coord.y) ||
        !dim_between(p1->v.coord.x, i.x, p1b->v.coord.x) ||
        !dim_between(p1->v.coord.y, i.y, p1b->v.coord.y))
    {
        return NULL;
    }

    /* Due to rounding, the relationship between eX->p and i may become different
     * from the one between eX->p and eX->other->p.  This will be handles in divide_segment
     * by removing and reinserting edges for reprocessing.
     */

    /* Finally, make a new point (or an old point -- pt_new will check whether we have
     * this already) */
    return pt_new(c, p0->v.loc, &i, &p0->v.color);
}

#if !NEW_COLLINEAR
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
    return coord_between(&a->v.coord, &b->v.coord, &c->v.coord);
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
#endif

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

static void redo_q_from_s(
    ctxt_t *c,
    event_t *el,
    point_t *ip)
{
    do {
        event_t *elp = s_prev(el);
        s_remove(c, el);
        q_insert(c, el);
        assert((elp == NULL) || elp->left);
        el = elp;
    } while ((el != NULL) && (el->p == ip));
}

static void ev_ignore(
    ctxt_t *c,
    event_t *e)
{
    e->in.owner = e->other->in.owner = 0;
#if NEW_COLLINEAR
    assert(!s_contains(c, e));
    assert(!s_contains(c, e->other));
#else
    if (s_contains(c, e)) {
        s_remove(c, e);
    }
    if (s_contains(c, e->other)) {
        s_remove(c, e->other);
    }
#endif
    if (q_contains(c, e)) {
        q_remove(c, e);
    }
    if (q_contains(c, e->other)) {
        q_remove(c, e->other);
    }
}

static event_t **add_sev(
    event_t **sev,
    event_t *el,
    event_t *eh)
{
    if (el->p == eh->p) {
        *(sev++) = NULL;
    }
    else if (ev_cmp(el, eh) > 0) {
        *(sev++) = eh;
        *(sev++) = el;
    }
    else {
        *(sev++) = el;
        *(sev++) = eh;
    }
    return sev;
}

/**
 * This returns a debug string describing what was done. */
static char const *check_intersection(
    ctxt_t *c,
    /** the lower edge in s */
    event_t *el,
    /** the upper edge in s */
    event_t *eh,
    /** whether we are finishing a right point */
    event_t *right)
{
    event_t *ol = el->other;
    event_t *oh = eh->other;
    assert( el->left);
    assert( eh->left);
    assert( s_contains(c, el));
    assert( s_contains(c, eh));
    assert(!ol->left);
    assert(!oh->left);
    assert(!s_contains(c, ol));
    assert(!s_contains(c, oh));

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

#if NEW_COLLINEAR
    unsigned u = 0;
#else
    unsigned u = ev4_overlap(el, ol, eh, oh);
#endif

    if ((u == 2) && (right != NULL) && (eh->p != el->p) && (right->p != el->p)) {
        /* BUG:
         * test32e.scad and test32b.scad trigger this.  This is
         * similar to the other test32.scad tests, but this has no
         * overlap, but a coincident point.  This happens in other
         * tests, too, without any consequent failure.  This needs
         * more debugging because it is more difficult to distinguish
         * when this fails and when it's ok.
         *
         * In this case, if there is an intersection, we must not round
         * it into el->p.
         *
         * The following ones are a different case (filtered by 'eh->p != el->p') with
         * three lines crossing on the left.  This works:
         *     chain1, test31b, test26j, test26k, test26l, uselessbox, linext1, linext7.
         */
        u = 0;
    }
    if ((u == 3) && (right != NULL)) {
        /* BUG:
         * linext5.scad triggered this in (WebGL) diff step (z=19.7+0.2).
         * The fix is to do nothing (do not collapse overlap).
         * Test32f.scad triggers exactly this at a larger scale.
         *
         * Tests test32*.scad have been added to further examine this,
         * and trigger more problems.  And indeed, doing nothing is not
         * always enough.
         *
         * The reason for the failure is an overlap of prev and next
         * lines: this cannot happen except due to rounding (at small
         * scales). => Change this into a potential intersection
         * instead (or keep edges as is).
         *
         * There is no need to check whether the right points of prev
         * and next coincide: this is not handled here and introduces
         * no new line or point.
         *
         * But there is still a danger: 'find_intersection' may
         * succeed and round 'ip' in such a way that the top line ends
         * up right of the current sweep point, which may invalidate
         * old decisions.
         *
         * We may have tried to handle this already by re-processing
         * prev and next, but this does not cut it: linext5 has an
         * intermediate vertical line that is already completely
         * processed that would also need reprocessing as it ends up
         * in the opposite side.
         */
        u = 0;
    }
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
                return "shared end";
            }

            char const *what = NULL;
            if (ip == el->p) {
                /* This means that we need to reclassify the upper line again (which
                 * we thought was below, but due to rounding, it now turns out to be
                 * completely above).  The easiest is to remove it again from S
                 * and throw it back into Q to try again later. */
                IFPSTRACE(what = "single intersection, redo below");
                redo_q_from_s(c, el, ip);
            }
            else if (ip != ol->p) {
                divide_segment(c, el, ip);
            }

            if (ip == eh->p) {
                /* Same corner case as above: we may have classified eh too early. */
                redo_q_from_s(c, eh, ip);
                IFPSTRACE(what = what ? "single intersection, redo both"
                                      : "single intersection, redo above");
            }
            else if (ip != oh->p) {
                divide_segment(c, eh, ip);
            }

            IFPSTRACE(what = what ? what : "single intersection");
            return what;
        }

        /* collinear means parallel here, i.e., no intersection */
        LOG("Rel: unrelated, parallel=%u (%s -- %s)\n", collinear, ev_str(el), ev_str(eh));
        return "non-intersecting";
    }

    assert(!right);

    /* check */
    assert(pt_cmp(el->p, ol->p) < 0);
    assert(pt_cmp(eh->p, oh->p) < 0);
    assert(pt_cmp(ol->p, eh->p) >= 0);
    assert(pt_cmp(oh->p, el->p) >= 0);

    LOG("overlap: %s with %s\n", ev_str(el), ev_str(eh));

    /* overlap */
    event_t *sev[4];
    event_t **sev_ptr = sev;
    sev_ptr = add_sev(sev_ptr, el, eh);
    sev_ptr = add_sev(sev_ptr, ol, oh);
    size_t sev_cnt = CP_PTRDIFF(sev_ptr, sev);
    assert(sev_cnt >= 2);
    assert(sev_cnt <= cp_countof(sev));

    size_t owner = (eh->in.owner ^ el->in.owner);
    size_t below = el->in.below;
    size_t above = below ^ owner;

    /* We do not need to care about resetting other->in.below, because it is !left
     * and is not part of S yet, and in.below will be reset upon insertion. */
    if (sev_cnt == 2) {
        LOG("Rel: complete overlap (%s -- %s)\n", ev_str(el), ev_str(eh));

        /*  eh.....oh
         *  el.....ol
         */
        assert(sev[0] == NULL);
        assert(sev[1] == NULL);
        eh->in.owner = oh->in.owner = owner;
        eh->in.below = below;

        assert(el->in.below == below);
        ev_ignore(c, el);
        return "complete overlap";
    }
    if (sev_cnt == 3) {
        LOG("Rel: overlap shared end (%s -- %s)\n", ev_str(el), ev_str(eh));

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
        event_t *sh  = sev[0] ? sev[0] : sev[2];
        event_t *shl = sev[0] ? sev[0] : sev[2]->other;
        sh->other->in.owner = owner;
        sh->other->in.below = below;
        if (shl == el) {
            assert((sev[1] == eh) || (sev[1] == oh));
            eh->in.below = above;
        }

        divide_segment(c, shl, sev[1]->p);

        ev_ignore(c, sev[1]);
        return "overlap shared end";
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
        LOG("Rel: mutual partial overlap (%s -- %s)\n", ev_str(el), ev_str(eh));

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
        return "mutual partial overlap";
    }

    LOG("Rel: inner overlap (%s -- %s)\n", ev_str(el), ev_str(eh));


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
    return "inner overlap";
}

#if NEW_COLLINEAR
static double dist_on_line(
    event_t *e,
    point_t const *p)
{
    cp_vec2_t w;
    cp_vec2_sub(&w, &p->v.coord, &e->p->v.coord);
    return cp_vec2_dot(&w, &e->line.dir);
}

static event_t *left_of(event_t *e)
{
    return e->left ? e : e->other;
}

static char const *collapse_divide(
    ctxt_t *c,
    event_t *a1,
    event_t *b1,
    char const *what)
{
    event_t *a2 = a1->other;
    a1->in.owner ^= b1->in.owner;

    assert(b1->p != a1->p);
    assert(b1->p != a2->p);
    divide_segment(c, left_of(a1), b1->p);
    ev_ignore(c, b1);

    if (!q_contains(c, a1)) {
        q_insert(c, a1);
    }
    if (!q_contains(c, a2)) {
        q_insert(c, a2);
    }

    return what;
}

/* The cases in this function are documented in doc/collcorner.fig */
static char const *collapse_edges(
    ctxt_t *c,
    event_t *a1,
    event_t *b1)
{
    event_t *a2 = a1->other;
    event_t *b2 = b1->other;
    /* will redo: get everything out of S; ends not in Q are added later if needed */
    assert(!s_contains(c,b1));
    assert( q_contains(c,a2));
    assert( q_contains(c,b2));
    s_remove(c, a1);

    /* check for coincident points => max 2 segments */
    if (a2->p == b2->p) {
       if (a1->p == b1->p) {
           /* one segment */
           a1->in.owner ^= b1->in.owner;
           ev_ignore(c, b1);
           q_insert(c, a1);
           return "a1b1==a2b2"; /*5b*/
       }
       return collapse_divide(c, a2, b1, "a1--b1==a2b2"); /*5a*/
    }

    if (a2->p == b1->p) {
        assert(a1->p != b2->p);
        if (dist_on_line(a1, b2->p) < 0) {
            return collapse_divide(c, b1, a1, "b2--a1==a2b1"); /*4b*/
        }
        return collapse_divide(c, a2, b2, "a1--b2==a2b1"); /*3b*/
    }

    assert(a1->p != b2->p);
    if (a1->p == b1->p) {
        if (dist_on_line(a1, a2->p) < dist_on_line(a1, b2->p)) {
            return collapse_divide(c, b1, a2, "a1b1==a2--b2"); /*2b*/
        }
        return collapse_divide(c, a1, b2, "a1b1==b2--a2"); /*1b*/
    }

    /* compute positions on unmodified line a */
    double dol_a2 = dist_on_line(a1, a2->p);
    double dol_b1 = dist_on_line(a1, b1->p);
    double dol_b2 = dist_on_line(a1, b2->p);

    /* no coincident points => three new segments => stem 1: split old line */
    divide_segment(c, a1, b1->p);
    event_t *a1i = a1->other;
    event_t *a2i = a2->other;

    if (dol_b2 < 0) {
        return collapse_divide(c, b1, a1, "b2--a1==b1--a2"); /*4a*/
    }

    if (dol_b2 < dol_b1) {
        return collapse_divide(c, a1i, b2, "a1--b2==b1--a2"); /*3a*/
    }

    if (dol_a2 < dol_b2) {
        return collapse_divide(c, b1, a2, "a1--b1==a2--b2"); /*2a*/
    }

    return collapse_divide(c, a2i, b1, "a1--b1==b2--a2"); /*1a*/
}
#endif

static void ev_left(
    ctxt_t *c,
    event_t *e)
{
    assert(!s_contains(c, e));
    assert(!s_contains(c, e->other));
    LOG("insert_s: %p (%p)\n", e, e->other);
#if NEW_COLLINEAR
    event_t *overlap = s_insert(c, e);
    if (overlap != NULL) {
        debug_print_s(c, "left before collapse", e, overlap, NULL);
        LOG("insert_s: merged edge\n");
        char const *what CP_UNUSED = collapse_edges(c, overlap, e);
        debug_print_s(c, "left after collapse: %s", e, overlap, NULL, what);
        return;
    }
#else
    s_insert(c, e);
#endif

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

#if HACK
    if ((prev != NULL) && (seg_cmp(e, prev) < 0)) {
        s_remove(c, e);
        q_insert(c, e);
        s_remove(c, prev);
        q_insert(c, prev);
        fprintf(stderr, "wrong order of cur and prev:\n  %s\n  %s\n", ev_str(e), ev_str(prev));
        return;
    }
    if ((next != NULL) && (seg_cmp(e, next) > 0)) {
        s_remove(c, e);
        q_insert(c, e);
        s_remove(c, next);
        q_insert(c, next);
        fprintf(stderr, "wrong order of cur and next:\n  %s\n  %s\n", ev_str(e), ev_str(next));
        return;
    }

    assert((prev == NULL) || seg_cmp(e, prev) > 0);
    assert((next == NULL) || seg_cmp(e, next) < 0);
#endif

    char const *ni CP_UNUSED = "NULL";
    if (next != NULL) {
        ni = check_intersection(c, e, next, NULL);
    }

    /* The previous 'check_intersection' may have kicked out 'e' from S due
     * to rounding, so check that e is still in S before trying to intersect.
     * If not, it is back in Q and we'll handle this later. */
    char const *pi CP_UNUSED = "NULL";
    if (prev != NULL) {
        if (s_contains(c, e)) {
            pi = check_intersection(c, prev, e, NULL);
        }
        else {
            pi = "new edge collapsed into next";
        }
    }

    debug_print_s(c, "left after intersect (n=%s, p=%s)", e, prev, next, ni, pi);
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
    assert(!e->left);
    event_t *sli = e->other;
    event_t *next = s_next(sli);
    event_t *prev = s_prev(sli);

    debug_print_s(c, "right before intersect", e, prev, next);

    /* first remove from s */
    LOG("remove_s: %p (%p)\n", e->other, e);
    s_remove(c, sli);
    assert(!s_contains(c, e));
    assert(!s_contains(c, e->other));

    /* now add to out */
    bool below_in = op_bitmap_get(c, sli->in.below);
    bool above_in = op_bitmap_get(c, sli->in.below ^ sli->in.owner);
    if (below_in != above_in) {
        assert(sli->in.owner != 0);
        e->in.below = e->other->in.below = below_in;
        chain_add(c, e);
    }

    char const *npi CP_UNUSED = "NULL";
    if ((next != NULL) && (prev != NULL)) {
        npi = check_intersection(c, prev, next, e);
    }

    debug_print_s(c, "right after intersect (np=%s)", e, prev, next, npi);
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

static bool csg2_op_csg2(
    op_ctxt_t *c,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg2_t *a);

static bool csg2_op_v_csg2(
    op_ctxt_t *c,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_v_obj_p_t *a)
{
    TRACE("n=%"CP_Z"u", a->size);
    assert(cp_mem_is0(o, sizeof(*o)));
    for (cp_v_each(i, a)) {
        cp_csg2_t *ai = cp_csg2_cast(*ai, cp_v_nth(a,i));
        if (i == 0) {
            if (!csg2_op_csg2(c, zi, o, ai)) {
                return false;
            }
        }
        else {
            cp_csg2_lazy_t oi = { 0 };
            if (!csg2_op_csg2(c, zi, &oi, ai)) {
                return false;
            }
            LOG("ADD\n");
            cp_csg2_op_lazy(c->opt, c->tmp, o, &oi, CP_OP_ADD);
        }
    }
    return true;
}

static bool csg2_op_add(
    op_ctxt_t *c,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg_add_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    return csg2_op_v_csg2(c, zi, o, &a->add);
}

static bool csg2_op_cut(
    op_ctxt_t *c,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg_cut_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    for (cp_v_each(i, &a->cut)) {
        cp_csg_add_t *b = cp_v_nth(&a->cut, i);
        if (i == 0) {
            if (!csg2_op_add(c, zi, o, b)) {
                return false;
            }
        }
        else {
            cp_csg2_lazy_t oc = {0};
            if (!csg2_op_add(c, zi, &oc, b)) {
                return false;
            }
            LOG("CUT\n");
            cp_csg2_op_lazy(c->opt, c->tmp, o, &oc, CP_OP_CUT);
        }
    }
    return true;
}

static bool csg2_op_xor(
    op_ctxt_t *c,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg_xor_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    for (cp_v_each(i, &a->xor)) {
        cp_csg_add_t *b = cp_v_nth(&a->xor, i);
        if (i == 0) {
            if (!csg2_op_add(c, zi, o, b)) {
                return false;
            }
        }
        else {
            cp_csg2_lazy_t oc = {0};
            if (!csg2_op_add(c, zi, &oc, b)) {
                return false;
            }
            LOG("XOR\n");
            cp_csg2_op_lazy(c->opt, c->tmp, o, &oc, CP_OP_XOR);
        }
    }
    return true;
}

static bool csg2_op_layer(
    op_ctxt_t *c,
    cp_csg2_lazy_t *o,
    cp_csg2_layer_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    if (a->root == NULL) {
        return true;
    }
    return csg2_op_add(c, a->zi, o, a->root);
}

static bool csg2_op_sub(
    op_ctxt_t *c,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg_sub_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    if (!csg2_op_add(c, zi, o, a->add)) {
        return false;
    }

    cp_csg2_lazy_t os = {0};
    if (!csg2_op_add(c, zi, &os, a->sub)) {
        return false;
    }
    LOG("SUB\n");
    cp_csg2_op_lazy(c->opt, c->tmp, o, &os, CP_OP_SUB);
    return true;
}

static bool csg2_op_stack(
    op_ctxt_t *c,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg2_stack_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));

    cp_csg2_layer_t *l = cp_csg2_stack_get_layer(a, zi);
    if (l == NULL) {
        return true;
    }
    if (zi != l->zi) {
        assert(l->zi == 0); /* not visited: must be empty */
        return true;
    }

    assert(zi == l->zi);
    return csg2_op_layer(c, o, l);
}

static bool csg2_op_csg2(
    op_ctxt_t *c,
    size_t zi,
    cp_csg2_lazy_t *o,
    cp_csg2_t *a)
{
    TRACE();
    assert(cp_mem_is0(o, sizeof(*o)));
    switch (a->type) {
    case CP_CSG2_POLY:
        csg2_op_poly(o, cp_csg2_cast(cp_csg2_poly_t, a));
        return true;

    case CP_CSG2_STACK:
        return csg2_op_stack(c, zi, o, cp_csg2_cast(cp_csg2_stack_t, a));

    case CP_CSG_ADD:
        return csg2_op_add(c, zi, o, cp_csg_cast(cp_csg_add_t, a));

    case CP_CSG_XOR:
        return csg2_op_xor(c, zi, o, cp_csg_cast(cp_csg_xor_t, a));

    case CP_CSG_SUB:
        return csg2_op_sub(c, zi, o, cp_csg_cast(cp_csg_sub_t, a));

    case CP_CSG_CUT:
        return csg2_op_cut(c, zi, o, cp_csg_cast(cp_csg_cut_t, a));
    }

    CP_DIE("2D object type");
    CP_ZERO(o);
    return false;
}

/**
 * This reuses the poly_t structure r->data[0], but does not destruct
 * any of its substructures, but will just overwrite the pointers to
 * them.  Any poly but r->data[0] will be left completely untouched.
 */
static void cp_csg2_op_poly(
    cp_pool_t *tmp,
    cp_csg2_poly_t *o,
    cp_csg2_lazy_t const *r,
    bool flatten)
{
    TRACE();
    /* make context */
    ctxt_t c = {
        .tmp = tmp,
        .comb = &r->comb,
        .comb_size = (1U << r->size),
        .flatten = flatten,
    };
    cp_list_init(&c.poly);

    /* initialise queue */
    for (cp_size_each(m, r->size)) {
        cp_csg2_poly_t *a = r->data[m];
        LOG("poly %"CP_Z"d: #path=%"CP_Z"u\n", m, a->path.size);
        for (cp_v_each(i, &a->path)) {
            cp_csg2_path_t *p = &cp_v_nth(&a->path, i);
            for (cp_v_each(j, &p->point_idx)) {
                cp_vec2_loc_t *pj = cp_csg2_path_nth(a, p, j);
                cp_vec2_loc_t *pk = cp_csg2_path_nth(a, p, cp_wrap_add1(j, p->point_idx.size));
                q_add_orig(&c, pj, pk, m);
            }
        }
    }
    LOG("start\n");

    /* run algorithm */
    size_t ev_cnt CP_UNUSED = 0;
    for (;;) {
        event_t *e = q_extract_min(&c);
        if (e == NULL) {
            break;
        }

        LOG("\nevent %"CP_Z"u: %s o=(0x%"CP_Z"x 0x%"CP_Z"x)\n",
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

    chain_combine(&c);
    poly_make(o, &c, r->data[0]);

    /* sweep */
    cp_v_fini(&c.vert);
}

static cp_csg2_poly_t *poly_sub(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_csg2_poly_t *a0,
    cp_csg2_poly_t *a1)
{
    size_t a0_point_sz CP_UNUSED = a0->point.size;
    size_t a1_point_sz CP_UNUSED = a1->point.size;

    cp_csg2_lazy_t o0;
    CP_ZERO(&o0);
    csg2_op_poly(&o0, a0);

    cp_csg2_lazy_t o1;
    CP_ZERO(&o1);
    csg2_op_poly(&o1, a1);

    cp_csg2_op_lazy(opt, tmp, &o0, &o1, CP_OP_SUB);
    assert(o0.size == 2);

    cp_csg2_poly_t *o = CP_CLONE(*o, a1);
    cp_csg2_op_poly(tmp, o, &o0, false);

    /* check that the originals really haven't changed */
    assert(a0->point.size == a0_point_sz);
    assert(a1->point.size == a1_point_sz);

    return o;
}

static void csg2_op_diff2_poly(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_csg2_poly_t *a0,
    cp_csg2_poly_t *a1)
{
    TRACE();
    if ((a0->point.size | a1->point.size) == 0) {
        return;
    }
    if (a0->point.size == 0) {
        a1->diff_below = a1;
        return;
    }
    if (a1->point.size == 0) {
        a0->diff_above = a0;
        return;
    }

    a0->diff_above = poly_sub(opt, tmp, a0, a1);
    a1->diff_below = poly_sub(opt, tmp, a1, a0);
}

static void csg2_op_diff2(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_csg2_t *a0,
    cp_csg2_t *a1)
{
    TRACE();
    cp_csg2_poly_t *p0 = cp_csg2_try_cast(*p0, a0);
    if (p0 == NULL) {
        return;
    }
    cp_csg2_poly_t *p1 = cp_csg2_try_cast(*p0, a1);
    if (p1 == NULL) {
        return;
    }
    csg2_op_diff2_poly(opt, tmp, p0, p1);
}

static void csg2_op_diff2_layer(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_csg2_layer_t *a0,
    cp_csg2_layer_t *a1)
{
    TRACE();

    /* Run diff even if one layer is empty because we did not triangulate
     * the layer to speed things up, so we need to ensure that there is a
     * above_diff/below_diff to be triangulated to avoid holes. */
    cp_csg2_poly_t empty = { .type = CP_CSG2_POLY };
    cp_csg2_t *pe = cp_csg2_cast(*pe, &empty);
    cp_csg2_t *p0 = pe;
    cp_csg2_t *p1 = pe;
    if ((a0 != NULL) && (cp_csg_add_size(a0->root) == 1)) {
        p0 = cp_csg2_cast(cp_csg2_t, cp_v_nth(&a0->root->add,0));
    }
    if ((a1 != NULL) && (cp_csg_add_size(a1->root) == 1)) {
        p1 = cp_csg2_cast(cp_csg2_t, cp_v_nth(&a1->root->add,0));
    }
    if (p0 == p1) {
        assert(p0 == pe);
        return;
    }
    csg2_op_diff2(opt, tmp, p0, p1);
    assert(empty.diff_above == NULL);
    assert(empty.diff_below == NULL);
}

static void csg2_op_diff_stack(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    size_t zi,
    cp_csg2_stack_t *a)
{
    TRACE();
    cp_csg2_layer_t *l0 = cp_csg2_stack_get_layer(a, zi);
    cp_csg2_layer_t *l1 = cp_csg2_stack_get_layer(a, zi + 1);
    csg2_op_diff2_layer(opt, tmp, l0, l1);
}

static void csg2_op_diff_csg2(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    size_t zi,
    cp_csg2_t *a)
{
    TRACE();
    /* only work on stacks, ignore anything else */
    switch (a->type) {
    case CP_CSG2_STACK:
        csg2_op_diff_stack(opt, tmp, zi, cp_csg2_cast(cp_csg2_stack_t, a));
        return;
    default:
        return;
    }
}

/**
 * Actually reduce a lazy poly to a single poly.
 *
 * The result is either empty (r->size == 0) or will have a single entry
 * (r->size == 1) stored in r->data[0].  If the result is empty, this
 * ensures that r->data[0] is NULL.
 *
 * Note that because lazy polygon structures have no dedicated space to store
 * a polygon, they must reuse the space of the input polygons, so applying
 * this function with more than 2 polygons in the lazy structure will reuse
 * space from the polygons for storing the result.
 */
static void cp_csg2_op_reduce(
    cp_pool_t *tmp,
    cp_csg2_lazy_t *r,
    bool flatten)
{
    TRACE();
    if (r->size == 0) {
        return;
    }
    if (!flatten && (r->size <= 1)) {
        return;
    }
    cp_csg2_op_poly(tmp, r->data[0], r, flatten);
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
 * Boolean operation on two lazy polygons.
 *
 * This does 'r = r op b'.
 *
 * Only the path information is used, not the triangles.
 *
 * \p r and/or \p b are reused and cleared to construct r.  This may happen
 * immediately or later in cp_csg2_op_reduce().
 *
 * Uses \p tmp for all temporary allocations (but not for constructing r).
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
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
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
            cp_csg2_op_reduce(tmp, r, false);
            assert(r->size <= 1);
        }
        else {
            cp_csg2_op_reduce(tmp, b, false);
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
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_csg2_tree_t *r,
    cp_csg2_tree_t *a,
    size_t zi)
{
    TRACE();
    cp_csg2_stack_t *s = cp_csg2_cast(*s, r->root);
    assert(zi < s->layer.size);

    op_ctxt_t c = {
        .opt = opt,
        .tmp = tmp,
    };

    cp_csg2_lazy_t ol;
    CP_ZERO(&ol);
    bool ok CP_UNUSED = csg2_op_csg2(&c, zi, &ol, a->root);
    assert(ok && "Unexpected object in tree.");
    cp_csg2_op_reduce(tmp, &ol, false);

    cp_csg2_poly_t *o = ol.data[0];
    if (o != NULL) {
        assert(o->point.size > 0);

        /* new layer */
        cp_csg2_layer_t *layer = cp_csg2_stack_get_layer(s, zi);
        assert(layer != NULL);
        cp_csg_add_init_perhaps(&layer->root, NULL);

        layer->zi = zi;

        cp_v_nth(&r->flag, zi) |= CP_CSG2_FLAG_NON_EMPTY;

        /* single polygon per layer */
        cp_v_push(&layer->root->add, cp_obj(o));
    }
}

/**
 * Reduce a set of 2D CSG items into a single polygon.
 *
 * This does not triangulate, but only create the path.
 *
 * The result is filled from root.  In the process, the elements in root are
 * cleared/reused, if necessary.
 *
 * If the result is empty. this either returns an empty
 * polygon, or NULL.  Which one is returned depends on what
 * causes the polygon to be empty.
 *
 * In case of an error, e.g. 3D objects that cannot be handled, this
 * assert-fails, so be sure to not pass anything this is unhandled.
 *
 * Runtime and space: see cp_csg2_op_add_layer.
 */
extern cp_csg2_poly_t *cp_csg2_flatten(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_v_obj_p_t *root)
{
    TRACE();
    op_ctxt_t c = {
        .opt = opt,
        .tmp = tmp,
    };

    cp_csg2_lazy_t ol;
    CP_ZERO(&ol);
    bool ok CP_UNUSED = csg2_op_v_csg2(&c, 0, &ol, root);
    assert(ok && "Unexpected object in tree.");
    cp_csg2_op_reduce(tmp, &ol, true);

    return ol.data[0];
}

/**
 * Diff a layer with the next and store the result in diff_above/diff_below.
 *
 * The tree must have been processed with cp_csg2_op_add_layer(),
 * and the layer ID must be in range.
 *
 * r is modified and a diff_below polygon is added.  The original polygons
 * are left untouched.
 *
 * Runtime and space: see cp_csg2_op_add_layer.
 */
extern void cp_csg2_op_diff_layer(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_csg2_tree_t *a,
    size_t zi)
{
    TRACE();
    cp_csg2_stack_t *s CP_UNUSED = cp_csg2_cast(*s, a->root);
    assert(zi < s->layer.size);

    csg2_op_diff_csg2(opt, tmp, zi, a->root);
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
    cp_csg2_stack_t *root = cp_csg2_new(*root, NULL);
    r->root = cp_csg2_cast(*r->root, root);
    r->thick = a->thick;
    r->opt = a->opt;
    r->root_xform = a->root_xform;

    size_t cnt = a->z.size;

    cp_csg2_stack_t *c = cp_csg2_cast(*c, r->root);
    cp_v_init0(&c->layer, cnt);

    cp_v_init0(&r->z, cnt);
    cp_v_copy_arr(&r->z, 0, &a->z, 0, cnt);

    cp_v_init0(&r->flag, cnt);
}
