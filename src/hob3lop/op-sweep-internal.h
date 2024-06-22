/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CQ_OP_SWEEP_INTERNAL_H_
#define CQ_OP_SWEEP_INTERNAL_H_

/**
 * This implements a two phase line intersection algorithm.  It
 * handles any arbitrary (but correct) input of polygons, overlapping,
 * touching, self-intersecting, and is also arithmetically stable and
 * robust.
 *
 * Phase 1 searches all intersections and collapses overlapping lines
 * and splits lines that intersect in integer coordinates.  This is a
 * modified Bentley-Ottmann place sweep running with exact arithmetics
 * (using double precision fractionals and degree 3 multiplication and
 * degree 4 comparison (see the Boissonnat and Preparata paper on
 * robust plane sweep)).
 *
 * Phase 2 snap rounds the arrangement from phase 1 and produces a set
 * of output segments (based on a paper of de Berg et.al.).
 *
 * Phase 2 is implemented itself in two major phases plus a
 * finalisation.  Phase 2a handles positive slope edges, phase 2b
 * negative slope edges, and phase 2c finds equal edges from phases 2a
 * and 2b (can happen for both horizontal and vertical edges).
 *
 * Because it is somewhat difficult to do separately, and it's quite
 * easy to do it within this algorithm, and because the boolean
 * algorithm needs it, this algorithm associates each segment with a
 * bitmask to associate a segment with a set of polygons.  When edges
 * collapse, this algorithm merges the bitmasks.  Especially in phase
 * 2, this merging is hard to do efficiently externally -- it is
 * implemented directly here using an RB tree augmentation callback.
 *
 * Furthermore, because it is also relatively awkward to do this
 * externally or with callbacks, this also tracks the geometric
 * context of an edge.  For this, another bitmask is maintained.  This
 * bitmask can be used to track which polygons are entered and which
 * ones are 'left' when the edge is crossed.  This is also needed by
 * the boolean algorithm.
 *
 * This was originally adapted from Francisco Martinez del Rio (2011),
 * v1.4.1.  See: http://www4.ujaen.es/~fmartin/bool_op.html .  But
 * because that implementation was not stable or robust (because it
 * did not use exact arithmetics for the intersections, and I
 * struggled a lot to try to get it to work), this is a complete
 * rewrite from scratch, going back to Bentley-Ottmann, using exact
 * arithmetics (32 bit integer plus 64/64 bit fractional), and adding
 * the special cases based on several papers and ideas.
 *
 * The inside/outside idea is the same as described by Sean Conelly in his
 * polybooljs project, see: https://sean.cm/a/polygon-clipping-pt2
 *
 * This library modifies and extends the inside/outside idea by using
 * xor based bit masks instead, which may be less obvious, but also
 * allows the algorithm to handle polygons with self-overlapping edges
 * (they are resolved using a xor logic). The bitmasks allow
 * extension to more than 2 polygons.  The boolean function is stored
 * in a bitmask that maps in/out masks for multiple polygons to a
 * single bit.
 *
 * This implementation uses dictionaries (red-black trees) for almost
 * everything.  The dictionary implementation has several advanced
 * operations, e.g., join and split for the de Berg snap rounding,
 * and insertion before another node (without using the comparison
 * callback) for splitting a bundle.
 *
 * This algorithm takes as input a set of segments with a polygon ID
 * mask, i.e., not an ordered sequence of segments.  This algorithm
 * really does not need any original order, and the algorithm for the
 * 3D polyhedron slicer can be simpler if this accepts any set of
 * segments.
 *
 * The output of this is, again, a set of segments.  This is suitable
 * as input for the triangulation of Herter&Mehlhorn.
 *
 * TODO:
 *    * once debugged, 'bundle_t::free_next' can be in a union because its
 *      usage does not overlap with any phase.
 */

#include <hob3lbase/heap.h>
#include <hob3lbase/pool.h>
#include <hob3lbase/list.h>
#include <hob3lop/matq.h>
#include <hob3lop/gon.h>
#include <hob3lop/hedron.h>
#include <hob3lop/op-ps.h>
#include <hob3lop/op-sweep.h>

#define DEBUG_FREE 0

#define DEBUG 0

#if CQ_TRACE
#define PSPR(...) ((void)(cq_ps_info.f && fprintf(cq_ps_info.f, __VA_ARGS__)))
#else
#define PSPR(...) ((void)0)
#endif

#define PSPR_(...) do{ \
    PSPR(__VA_ARGS__); \
    fprintf(stderr, __VA_ARGS__); \
}while(0)

#ifdef NDEBUG
#undef  DEBUG
#define DEBUG 0
#endif

/* for valgrind, we can switch to calloc(), to ease memory debugging */
#if 0
#  define NEW(x)        ((__typeof__(x)*)calloc(1, sizeof(x)))
#  define POOL_NEW(m,x) NEW(x)
#else
#  define NEW(x)        CP_NEW(x)
#  define POOL_NEW(m,x) CP_POOL_NEW(m,x)
#endif

typedef enum {
    LEFT = 0,
    RIGT = 1,
} side_t;

/**
 * This represents an end of an edge.  Each point of an edge
 * is separate, and the same point shared by two edges is also
 * separate.  I.e., for a polygon touching itself in one edge,
 * there may be multiple entries.  The algorithm needs to
 * treat them specially.
 */
typedef struct vertex {
    /** the point */
    union {
        CQ_VEC2_T;
        cq_vec2_t vec2;
    };

    /** which end/side/head/tail of the edge: LEFT or RIGT */
    unsigned side;

    /** 'poly' and 'triangle' phases: point index in output poly vector */
    unsigned point_idx;

    /**
     * cell for data_t::agenda_vertex (phase 1 and 2)
     * cell for data_t::result (output phase)
     */
    cp_dict_t in_agenda;
} vertex_t;

/* Forward decls */
typedef struct edge edge_t;
typedef struct xing xing_t;

typedef struct {
    size_t data[4];
} edge_clear_t;

/**
 * An edge in the original (set of) polygon(s)
 * This is sorted so that v[0] is LEFT and e[1]
 * is RIGT (hence LEFT and RIGT).
 */
struct edge {
    union {
        vertex_t v[2];
        struct {
            vertex_t left, rigt;
        };
    };

    /**
     * intersect: cell for data_t::state
     * snaprnd:   cell for bundle_t::bundle
     * reduce:    cell for data_t::state
     * poly:      cell for data_t::state
     * triangle:  cell for data_t::state
     */
    cp_dict_t in_tree;

    union {
        /** data for phase 1, phase 2, reduce */
        struct {
            /** which polygons this belongs to (a bitmap implementing a set) */
            size_t member;

            /** used to point to the next free edge (after it has been consumed).
             * The nil value uses a sentinel (EDGE_NIL) so that if this is
             * non-NULL, it is consumed and free.   NULL means it is in use.
             * During the intersect phase, deleted edges need to be identifiable,
             * so the free pointer needs to be live together with other data
             * members.  Later (during triangle phase), the free point is only
             * used when an edge is deleted, so it can be shared with the data
             * from that phase.
             */
            edge_t *free_next;

            union {
                /** intersection phase (phase 1) */
                struct {
                    /** points to the crossing with prev edge */
                    xing_t *prev_xing;

                    /** points to the crossing with next edge */
                    xing_t *next_xing;
                };

                /** snap rounding (phase 2, pass 1 and pass 2) */
                struct {
                    /** which set the subtree belongs to (OR of all edges in that tree) */
                    size_t sum_member;
                };

                /** reduce phase  */
                struct {
                    size_t below;
                    bool keep;
                };
            };
        };

        /**
         * polygon phase
         * triangulation phase
         */
        struct {
            /** whether this edge runs backwards */
            bool back;

            /** inner edge: not part of outside */
            bool inner;

            /** whether LEFT and/or RIGT vertex are disabled/deleted in 'list' */
            bool v_dis[2];

            /** the list cell of the path the edge is part of */
            cp_list_t list;

            /** pointer to rigt_most vertex */
            vertex_t *rigt_most;
        };

        /**
         * For clearning */
        edge_clear_t clear;
    };
};

typedef CP_VEC_T(edge_t*) v_edge_p_t;

/**
 * Intersection point: 'crossing' */
struct xing {
    /** the point (exactly) */
    union {
        CQ_VEC2IF_T;
        cq_vec2if_t vec2if;
    };

    /**
     * cell for data_t::agenda_xing (phase 1 and phase 2) */
    cp_dict_t in_agenda;

    /** phase 1 data */
    struct {
        /**
         * phase 1: Edges that intersect in this crossing.
         *
         * We allow multiple, not just two, because we cannot exclude
         * this special case.  The edge list is not stored, but each
         * edge points back to this crossing if the crossing is imminent
         * for that edge (i.e., if there is not another crossing that
         * is handled before this one).
         *
         * The list of edges is needed at the crossing event.  At that
         * time, all edges that cross in this crossing are adjacent, and
         * this one of them must have added to the set after the last
         * edge was removed from it (none is removed now).
         *
         * We store one pointer to some of the edges that cross here.
         * The pointer is NULLed when this crossing becomes non-imminent,
         * and set when it becomes imminent.  Because becoming imminent
         * comes last before the crossing is processed, this pointer
         * must then be non-NULL.  It is OK when the pointer is
         * NULL for a while during the algorithm, because at last,
         * at the crossing, it will bet non-NULL.
         *
         * The list of edges that cross is extracted from the state
         * at the crossing: all crossing edges must be adjacent, so we
         * search for the top-most and bottom-most that point back here
         * to find the range of edges.
         *
         * We have two edges here for the second phase, which runs in
         * two phases: the algorithm uses some_edge in the first phase
         * and when processing the intersection, it sets some_edge_tb to
         * the bottom and top edges.  This way, one of them is guaranteed
         * to be set for pass 1 and pass 2 of phase 2.
         */
         union {
             edge_t *some_edge;
             edge_t *some_edge_tb[2];
         };
    };
};

/**
 * Vector of all crossings that were found */
typedef CP_VEC_T(xing_t*) v_xing_p_t;

typedef struct bundle bundle_t;

typedef struct {
    cp_dict_t *root;
    cp_dict_t *top;
    cp_dict_t *bot;
} dict_plus_t;

/**
 * A bundle with a bundle of edges.
 * This is used only in phase 2.
 */
struct bundle {
    struct {
        /* phase 2 data */
        struct {
            /**
             * The vector at which this bundle originates */
            union {
               CQ_VEC2_T;
               cq_vec2_t vec2;
            };

            /**
             * The root, minimum and maximum, of the tree of edges */
            dict_plus_t bundle;

            /** cell for data_t::state */
            cp_dict_t in_state;
        };

        /* free list data */
        struct {
            /* Next free bundle object.  The start of the free list is
             * data_t::free_bundle.
             * Uses BUNDLE_NIL (which is != NULL) as a sentinel when deleted.
             */
            bundle_t *free_next;
        };
    };
};

typedef cq_sweep_t data_t;

/**
 * Phase of the algorithm.
 * This is needed in some places when data structures are used
 * in different ways in different phases, e.g., in the debug
 * PS output, but also to distinguish the order of processing
 * in pixels that are vertically arranged.
 */
typedef enum {
    /* features */
    HAVE_FREE   = 0x0010,
    HAVE_TREE   = 0x0020,
    HAVE_XING   = 0x0040,
    HAVE_SUM    = 0x0080,
    HAVE_NEXT   = 0x0100,
    HAVE_BELOW  = 0x0200,
    HAVE_STATE  = 0x0400,
    HAVE_BUNDLE = 0x0800,
    HAVE_LIST   = 0x1000,
    HAVE_RESULT = 0x2000,
    HAVE_RM     = 0x4000, /* rigt_most */

    /* order */

    /** run: phase 1 (intersection) */
    INTERSECT = 0 | HAVE_FREE | HAVE_TREE | HAVE_XING | HAVE_STATE,

    /** run: phase 2, pass 1 (snap rounding, pos. slope edges) */
    SNAP_NORTH = 1 | HAVE_FREE | HAVE_TREE | HAVE_SUM | HAVE_BUNDLE | HAVE_RESULT,

    /** run: phase 2, pass 2 (snap rounding, neg. slope edges) */
    SNAP_SOUTH = 2 | HAVE_FREE | HAVE_TREE | HAVE_SUM | HAVE_BUNDLE | HAVE_RESULT,

    /** reduce: boolean operation */
    REDUCE = 3 | HAVE_FREE | HAVE_TREE | HAVE_BELOW | HAVE_STATE | HAVE_RESULT,

    /** poly: computation of a well-formed polygon (w/ ordered paths) */
    POLY = 4 | HAVE_TREE | HAVE_STATE | HAVE_LIST | HAVE_RESULT,

    /** triangle: computation of a triangulation */
    TRIANGLE = 5 | HAVE_TREE | HAVE_STATE | HAVE_LIST | HAVE_RESULT | HAVE_RM,
} phase_t;


#define CQ_SWEEP_OBJ \
    struct { \
        unsigned type; \
        cp_loc_t loc; \
    }

/**
 * Main working data structure */
struct cq_sweep {
    union {
        CQ_SWEEP_OBJ;
        CQ_SWEEP_OBJ obj;
    };

    /**
     * Pool for temporary memory during the algorithm. */
    cp_pool_t *tmp;

    /**
     * The set of edges.
     */
    v_edge_p_t edges[1];

    /**
     * The edge free list (we constantly discard and allocate new edges,
     * so this use used to speed that process up).
     */
    edge_t *free_edge;

    /**
     * List of freed bundles that can be reused.
     */
    bundle_t *free_bundle;

    /**
     * The crossings found in the run of the algorithm */
    v_xing_p_t xings[1];

    /**
     * An unused xing_t structure that can be used instead of
     * allocating a new one (this is in case we find a crossing
     * that already exists).  After using it, this must be NULLed. */
    xing_t *new_xing;

    /**
     * The agenda of vertices/events.  This has double the size of \p edges,
     * with one event for each side of an edge.
     * The agenda is a heap, a priority queue, so that we can extract and
     * also add new items to it.  We do not need a contiguous range, so
     * no dict needs to be used.
     *
     * Used in both phase 1 and phase 2.
     */
    cp_dict_t *agenda_vertex;

    /**
     * The minimum in agenda_vertex (phase 1 and phase 2) */
    cp_dict_t *agenda_vertex_min;

    /**
     * The comparison function to use for agenda_vertex.
     * This differs between phase 1 and phase 2.
     */
    int (*agenda_vertex_cmp)(
        cp_dict_t *a,
        cp_dict_t *b,
        data_t *user);

    /**
     * The agenda of crossings (phase 1 and phase 2) */
    cp_dict_t *agenda_xing;

    /**
     * The minimum in agenda_xing (phase 1 and phase 2) */
    cp_dict_t *agenda_xing_min;

    /**
     * The comparison function to use for agenda_xing.
     * This differes between phase 1 and phase 2 */
    int (*agenda_xing_cmp)(
        cp_dict_t *a,
        cp_dict_t *b,
        data_t *data);

    /**
     * The current set of edges crossing the scanline.
     *
     * phase 1: this contains edges
     * phase 2: this contains bundles
     */
    cp_dict_t *state;

    /**
     * Pass 1 is phase 1 and the first pass of phase 2.  Pass 2 is
     * the second pass of phase 2.
     * In pass 1, same x-coordinate vertices are compared by y
     * coordinate on the agenda to run up.  In pass 2, they run
     * down.  This is due to the different orientation of the
     * edge bundles in pass 2 (negative slope).
     */
    phase_t phase;

    /** phase 2 data */
    struct {
        /**
         * Output structure of edges.
         * This is sorted to find duplicates from pass 1 and pass 2.
         */
        cp_dict_t *result;
    };

#if CQ_TRACE
    int ps_line;
#endif
};

/* ********************************************************************** */

#define err_msg(err1_, loc1_, ...) \
    ({ \
        cp_err_t *err2_ = (err1_); \
        cp_loc_t loc2_  = (loc1_); \
        err2_->loc = loc2_; \
        err2_->loc2 = (cp_loc_t){0}; \
        cp_vchar_printf(&err2_->msg, __VA_ARGS__); \
    })

/* ********************************************************************** */

/**
 * Get the opposite end of an edge.
 * This only works if \p v points into an edge, to either edge_t::v[0] or
 * edge_t::v[1].  In this modules, all vertices do that.
 */
#define other_end(v) other_end_1_(CP_GENSYM(v_), v)
#define other_end_1_(v, v_) \
({ \
    __typeof__(*(v_)) *v = (v_); \
    (v + 1 - (v->side * 2)); \
})

/**
 * Get the left end of an edge based on some vector (either left or rigt):
 */
#define left_end(v) left_end_1_(CP_GENSYM(v1_),v)
#define left_end_1_(v, v_) \
({ \
    __typeof__(*(v_)) *v = (v_); \
    (v - v->side); \
})

/**
 * Get the rigt end of an edge based on some vector (either left or rigt):
 */
#define rigt_end(v) rigt_end_1_(CP_GENSYM(v2_),v)
#define rigt_end_1_(v, v_) \
({ \
    __typeof__(*(v_)) *v = (v_); \
    (v + (1 - v->side)); \
})

/**
 * Get the edge of a vertex */
#define edge_of(v) edge_of_1_(CP_GENSYM(v3_),v)
#define edge_of_1_(v, v_) \
({ \
    __typeof__(*(v_)) *v = (v_); \
    v = left_end(v); \
    CP_BOX_OF(v, edge_t, left); \
})

static inline int dim2_cmp(
    cq_dim_t ax, cq_dim_t ay,
    cq_dim_t bx, cq_dim_t by)
{
    int i = CP_CMP(ax, bx);
    if (i != 0) {
        return i;
    }
    return CP_CMP(ay, by);
}

static inline int vec2_cmp(
    cq_vec2_t const *a,
    cq_vec2_t const *b)
{
    return dim2_cmp(a->x, a->y, b->x, b->y);
}

static inline vertex_t *agenda_get_vertex(
    cp_dict_t *x)
{
    vertex_t *m = CP_BOX0_OF(x, *m, in_agenda);
    return m;
}

static inline edge_t *list_get_edge(
    cp_list_t *x)
{
    edge_t *e = CP_BOX0_OF(x, *e, list);
    return e;
}

static inline edge_t *tree_get_edge(
    cp_dict_t *x)
{
    edge_t *e = CP_BOX0_OF(x, *e, in_tree);
    return e;
}

static inline edge_t *bundle_get_root(
    bundle_t const *x)
{
    return tree_get_edge(x->bundle.root);
}

static inline edge_t *bundle_get_top(
    bundle_t const *x)
{
    return tree_get_edge(x->bundle.top);
}

static inline edge_t *bundle_get_bot(
    bundle_t const *x)
{
    return tree_get_edge(x->bundle.bot);
}

static inline bool state_edge_is_member(
    data_t *data,
    edge_t const *e)
{
    return cp_dict_may_contain(data->state, &e->in_tree);
}

static inline bool bundle_edge_is_member(
    bundle_t *bundle,
    edge_t const *e)
{
    return cp_dict_may_contain(bundle->bundle.root, &e->in_tree);
}

static inline bool state_bundle_is_member(
    data_t *data,
    bundle_t const *e)
{
    assert((data->phase == SNAP_NORTH) || (data->phase == SNAP_SOUTH));
    return cp_dict_may_contain(data->state, &e->in_state);
}

/* can only be used during the phases that HAVE_FREE */
static inline bool edge_is_deleted(
    data_t *data CP_UNUSED,
    edge_t const *e)
{
    assert(data->phase & HAVE_FREE);
    if (e->free_next != NULL) {
        return true;
    }
    assert(e->left.side == LEFT);
    assert(e->rigt.side == RIGT);
    return false;
}

/* returns true during phases that do not HAVE_FREE */
static inline bool edge_is_deleted_debug(
    data_t *data,
    edge_t const *e)
{
    if ((data->phase & HAVE_FREE) == 0) {
        return false;
    }
    return edge_is_deleted(data, e);
}

static inline bool bundle_is_deleted(
    bundle_t const *e)
{
    return (e->free_next != NULL);
}

static inline bool agenda_vertex_is_member(
    data_t *data,
    vertex_t const *v)
{
    assert(!edge_is_deleted_debug(data,edge_of(v)));
    return cp_dict_may_contain(data->agenda_vertex, &v->in_agenda);
}

static inline bool result_is_member(
    data_t *data,
    vertex_t const *v)
{
    assert(!edge_is_deleted_debug(data,edge_of(v)));
    return cp_dict_may_contain(data->result, &v->in_agenda);
}

static inline bundle_t *state_get_bundle(
    cp_dict_t *x)
{
    bundle_t *e = CP_BOX_OF(x, *e, in_state);
    return e;
}

/* ********************************************************************** */

extern int cq_sweep_tree_vertex_edge_cmp(
    vertex_t *v,
    cp_dict_t *e_,
    void *user CP_UNUSED);

extern int cq_sweep_bundle_vec2_edge_cmp(
    cq_vec2_t const *v,
    cp_dict_t *e_,
    void *user CP_UNUSED);

extern void cq_sweep_bundle_aug_ev(
    cp_dict_aug_t *aug CP_UNUSED,
    cp_dict_t *a_,
    cp_dict_t *b_,
    cp_dict_aug_type_t c);

extern int cq_sweep_state_pixel_bundle_cmp(
    bundle_t const *a,
    cp_dict_t *b_,
    data_t *data CP_UNUSED);

extern int cq_sweep_state_bundle_bundle_cmp(
    bundle_t *a,
    cp_dict_t *b_,
    data_t *data CP_UNUSED);

extern int cq_sweep_agenda_vertex_phase1_cmp(
    cp_dict_t *a_,
    cp_dict_t *b_,
    data_t *user);

extern int cq_sweep_agenda_vertex_phase2_cmp(
    cp_dict_t *a_,
    cp_dict_t *b_,
    data_t *data);

extern int cq_sweep_agenda_xing_phase1_cmp(
    cp_dict_t *a_,
    cp_dict_t *b_,
    data_t *data);

extern int cq_sweep_agenda_xing_phase2_cmp(
    cp_dict_t *a_,
    cp_dict_t *b_,
    data_t *data);

/* ********************************************************************** */
/* 'edge' stuff */

static inline cq_dimw_t edge_sqr_len(
    edge_t *e)
{
    return cq_vec2_sqr_dist(&e->left.vec2, &e->rigt.vec2);
}

#define EDGE_NIL ((edge_t*)1)

#define EDGE_DELETE(data, e1_) \
    do { \
        edge_t **e2_ = &(e1_); \
        edge_delete(data, *e2_); \
        *e2_ = NULL; \
    }while(0)

#if DEBUG
static unsigned debug = 0;
#endif

static inline void edge_delete(
    data_t *data,
    edge_t *e)
{
    assert(e != NULL);
    assert(!edge_is_deleted_debug(data,e));
    assert(!state_edge_is_member(data, e));
    assert(!agenda_vertex_is_member(data, &e->left));
    assert(!agenda_vertex_is_member(data, &e->rigt));
#ifndef NDEBUG
    memset(e, 170, sizeof(*e));
#endif
    assert(data->free_edge != NULL);
    e->free_next = data->free_edge;
    data->free_edge = e;
    assert(data->free_edge != NULL);
}

static inline edge_t *edge_free_pop(
    data_t *data)
{
    edge_t *r = data->free_edge;
    assert(data->free_edge != NULL);
    if (r == EDGE_NIL) {
        return NULL;
    }
    assert(r->sum_member == 0xaaaaaaaaaaaaaaaa);
    data->free_edge = r->free_next;
    assert(data->free_edge != NULL);
    *r = (edge_t){};
    return r;
}

static inline edge_t *edge_new(
    data_t *data,
    cq_vec2_t const *left,
    cq_vec2_t const *rigt,
    size_t member,
    bool append)
{
    assert(vec2_cmp(left, rigt) < 0);

    edge_t *e = edge_free_pop(data);
    if (e == NULL) {
        e = POOL_NEW(data->tmp, *e);
        if (append) {
            cp_v_push_alloc(data->tmp->alloc, data->edges, e);
        }
    }

    e->v[0].side = LEFT;
    e->v[1].side = RIGT;
    assert(e->member == 0);
    assert(!edge_is_deleted_debug(data,e));
    assert(!agenda_vertex_is_member(data, &e->left));
    assert(!agenda_vertex_is_member(data, &e->rigt));

    assert(left_end(&e->v[0]) == &e->v[0]);
    assert(left_end(&e->v[1]) == &e->v[0]);
    assert(rigt_end(&e->v[0]) == &e->v[1]);
    assert(rigt_end(&e->v[1]) == &e->v[1]);
    assert(edge_of(&e->v[0]) == e);
    assert(edge_of(&e->v[1]) == e);

    e->left.vec2 = *left;
    e->rigt.vec2 = *rigt;
    e->member = member;
    return e;
}

/* Compare a vector with an edge with exact math, not considering any tolerance square.
 */
static inline int vec2_edge_cmp_exact(
    cq_vec2_t const *v,
    edge_t const *e)
{
    return CP_SIGN(cq_vec2_right_cross3_z(v, &e->rigt.vec2, &e->left.vec2));
}

/* Compare a vector with an edge, considering a tolerance square.
 */
static inline int vec2_edge_cmp_tolerant(
    cq_vec2_t const *v,
    edge_t const *e)
{
    return CP_SIGN(cq_vec2_cmp_edge_rnd(v, &e->rigt.vec2, &e->left.vec2));
}

/* ********************************************************************** */
/* 'result' data structure */

extern int cq_sweep_result_vertex_cmp(
    cp_dict_t *a_,
    cp_dict_t *b_,
    data_t *data CP_UNUSED);

static inline edge_t *result_insert(
    data_t *data,
    edge_t *n)
{
    assert(!cp_dict_is_member(&n->in_tree));

    cp_dict_t *o_ = cp_dict_insert(&n->left.in_agenda, &data->result,
        cq_sweep_result_vertex_cmp, data, 0);
    if (o_ != NULL) {
        vertex_t *v = agenda_get_vertex(o_);
        return edge_of(v);
    }

    o_ = cp_dict_insert(&n->rigt.in_agenda, &data->result,
        cq_sweep_result_vertex_cmp, data, 0);
    assert(o_ == NULL);
    return NULL;
}

static inline void result_remove(
    data_t *data,
    vertex_t *x)
{
    cp_dict_t *v = &x->in_agenda;
    assert(result_is_member(data, x));
    cp_dict_remove(v, &data->result);
}

/* ********************************************************************** */
/* shared:
 * phase 1 'state'
 * phase 2 'bundle'
 */

static inline edge_t *tree_edge_next(
    edge_t *edge)
{
    cp_dict_t *d = cp_dict_next(&edge->in_tree);
    edge_t *r = CP_BOX0_OF(d, *r, in_tree);
    return r;
}

static inline edge_t *tree_edge_prev(
    edge_t *edge)
{
    cp_dict_t *d = cp_dict_prev(&edge->in_tree);
    edge_t *r = CP_BOX0_OF(d, *r, in_tree);
    return r;
}

/* ********************************************************************** */
/* phase 1 'state' data structure: containing single edges */

static inline edge_t *state_edge_insert(
    data_t *data,
    vertex_t *left)
{
    assert(data->phase & HAVE_STATE);
    assert(!edge_is_deleted_debug(data,edge_of(left)));
    cq_assert(left->side == LEFT);
    edge_t *edge = edge_of(left);
    assert(!state_edge_is_member(data, edge));
    cp_dict_t *equ =
        cp_dict_insert_by(&edge->in_tree, left, &data->state,
            cq_sweep_tree_vertex_edge_cmp, NULL, 0);
    if (equ != NULL) {
        edge_t *othr = CP_BOX_OF(equ, *othr, in_tree);
        return othr;
    }
    assert(state_edge_is_member(data, edge));
    assert(data->state != NULL);
    return NULL;
}

static inline void state_edge_insert_successfully(
    data_t *data,
    vertex_t *left)
{
    edge_t *othr CP_UNUSED = state_edge_insert(data, left);
    assert(othr == NULL);
}

static inline void state_edge_swap(
    data_t *data,
    edge_t *a,
    edge_t *b)
{
    assert(data->phase & HAVE_STATE);
    assert(a != b);
    assert(!edge_is_deleted_debug(data,a));
    assert(!edge_is_deleted_debug(data,b));
    assert(state_edge_is_member(data, a));
    assert(state_edge_is_member(data, b));
    cp_dict_swap_update_root(&data->state, &a->in_tree, &b->in_tree);
}

/**
 * Like state_edge_swap, but checks that a is in state and b is not in state */
static inline void state_edge_replace(
    data_t *data,
    edge_t *a,
    edge_t *b)
{
    assert(data->phase & HAVE_STATE);
    assert(a != b);
    assert(!edge_is_deleted_debug(data,a));
    assert(!edge_is_deleted_debug(data,b));
    assert( state_edge_is_member(data, a));
    assert(!state_edge_is_member(data, b));
    cp_dict_swap_update_root(&data->state, &a->in_tree, &b->in_tree);
}

static inline void state_edge_remove(
    data_t *data,
    edge_t *edge)
{
    assert(data->phase & HAVE_STATE);
    assert(!edge_is_deleted_debug(data,edge));
    assert(data->state != NULL);
    assert(state_edge_is_member(data, edge));
    cp_dict_t *e = &edge->in_tree;
    cp_dict_remove(e, &data->state);
    assert(!state_edge_is_member(data, edge));
}

/* ********************************************************************** */
/* phase 2: 'bundle' stuff */

#define BUNDLE_NIL ((bundle_t*)1)

static cp_dict_aug_t bundle_aug = { .event = cq_sweep_bundle_aug_ev };

#define BUNDLE_DELETE(data, e1_) \
    do { \
        bundle_t **e2_ = &(e1_); \
        bundle_delete(data, *e2_); \
        *e2_ = NULL; \
    }while(0)

static inline void bundle_delete(
    data_t *data,
    bundle_t *b)
{
    assert(b != NULL);
    assert(b->bundle.root == NULL);
    assert(!bundle_is_deleted(b));
    assert(!state_bundle_is_member(data, b));
#ifndef NDEBUG
    memset(b, 170, sizeof(*b));
#endif
    assert(data->free_bundle != NULL);
    b->free_next = data->free_bundle;
    data->free_bundle = b;
    assert(data->free_bundle != NULL);
}

static inline bundle_t *bundle_free_pop(
    data_t *data)
{
    bundle_t *r = data->free_bundle;
    assert(data->free_bundle != NULL);
    if (r == BUNDLE_NIL) {
        return NULL;
    }
    data->free_bundle = r->free_next;
    assert(data->free_bundle != NULL);
    *r = (bundle_t){};
    return r;
}

static inline bundle_t *bundle_new(
    data_t *data,
    cq_dim_t x,
    cq_dim_t y)
{
    bundle_t *e = bundle_free_pop(data);
    if (e == NULL) {
        e = POOL_NEW(data->tmp, *e);
    }
    assert(!bundle_is_deleted(e));
    e->vec2 = CQ_VEC2(x, y);
    return e;
}

/* split a bundle */
static inline cp_dict_t *bundle_split(
    bundle_t *bundle,
    bundle_t const *target,
    unsigned back)
{
    cp_dict_t *min = NULL;
    cp_dict_split_aug(&min, &bundle->bundle.root, bundle->bundle.root, &target->vec2,
        cq_sweep_bundle_vec2_edge_cmp, NULL, back, &bundle_aug);
    /* update min/max */
    bundle->bundle.bot = cp_dict_min(bundle->bundle.root);
    bundle->bundle.top = cp_dict_max(bundle->bundle.root);
    return min;
}

static inline cp_dict_t *bundle_join(
    cp_dict_t *a,
    cp_dict_t *b)
{
    return cp_dict_join2_aug(a, b, &bundle_aug);
}

static inline void bundle_edge_new(
    data_t *data,
    cq_vec2_t const *left,
    cq_vec2_t const *rigt,
    cp_dict_t *bundle)
{
    int i = vec2_cmp(left, rigt);
    assert(i != 0);
    if (i > 0) {
        CP_SWAP(&left, &rigt);
    }

    edge_t *root = tree_get_edge(bundle);
    edge_t *n = edge_new(data, left, rigt, root->sum_member, false);
    edge_t *o = result_insert(data, n);
    if (o != NULL) {
        o->member ^= n->member;
        assert(!cp_dict_is_member(&n->in_tree));
        EDGE_DELETE(data, n);
    }
}

static inline size_t cq_sweep_bundle_get_sum_member(
    cp_dict_t *n_)
{
    if (n_ == NULL) {
        return 0;
    }
    edge_t *n = tree_get_edge(n_);
    return n->sum_member;
}

static inline void cq_sweep_bundle_update_sum_member(
    cp_dict_t *a_)
{
    if (a_ == NULL) {
        return;
    }
    edge_t *a = tree_get_edge(a_);
    a->sum_member = a->member ^
        cq_sweep_bundle_get_sum_member(a_->edge[0]) ^
        cq_sweep_bundle_get_sum_member(a_->edge[1]);
}

static inline void cq_sweep_bundle_update_sum_member_rec(
    cp_dict_t *a_)
{
        while (a_) {
            cq_sweep_bundle_update_sum_member(a_);
            a_ = a_->parent;
        }
}

static inline void bundle_edge_swap(
    bundle_t *bundle,
    edge_t *a,
    edge_t *b)
{
    assert(a != b);
    assert(bundle_edge_is_member(bundle, a));
    assert(bundle_edge_is_member(bundle, b));
    cp_dict_swap_update_root(&bundle->bundle.root, &a->in_tree, &b->in_tree);
    cq_sweep_bundle_update_sum_member_rec(&a->in_tree);
    cq_sweep_bundle_update_sum_member_rec(&b->in_tree);
}

static inline void bundle_edge_insert(
    bundle_t *bundle,
    edge_t *e)
{
    cq_assert(e->left.side == LEFT);
    assert(!bundle_edge_is_member(bundle, e));
    cp_dict_t *equ =
        cp_dict_insert_by_aug(&e->in_tree, &e->left, &bundle->bundle.root,
            cq_sweep_tree_vertex_edge_cmp, NULL, 0, &bundle_aug);
    edge_t *othr CP_UNUSED = CP_BOX0_OF(equ, *othr, in_tree);
    assert(othr == NULL);
}

static inline void bundle_edge_remove(
    bundle_t *bundle,
    edge_t *edge)
{
    assert(bundle->bundle.root != NULL);
    assert(bundle_edge_is_member(bundle, edge));
    cp_dict_t *e = &edge->in_tree;
    cp_dict_remove_aug(e, &bundle->bundle.root, &bundle_aug);
    assert(!bundle_edge_is_member(bundle, edge));
}

/* ********************************************************************** */
/* phase 2 'state' data structure: containing bundles */

static inline void state_bundle_insert(
    data_t *data,
    bundle_t *bundle)
{
    assert(data->phase >= SNAP_NORTH);
    assert(!bundle_is_deleted(bundle));
    assert(!state_bundle_is_member(data, bundle));
    cp_dict_insert_by(&bundle->in_state, bundle, &data->state,
        cq_sweep_state_bundle_bundle_cmp, data, 0);
    assert(state_bundle_is_member(data, bundle));
    assert(data->state != NULL);
}

static inline void state_bundle_insert_at(
    data_t *data,
    bundle_t *bundle,
    bundle_t *pos,
    unsigned dir)
{
    assert(data->phase >= SNAP_NORTH);
    assert(!bundle_is_deleted(bundle));
    assert(!state_bundle_is_member(data, bundle));
    cp_dict_insert_at(&bundle->in_state, &pos->in_state, dir, &data->state);
    assert(state_bundle_is_member(data, bundle));
    assert(data->state != NULL);
}

static inline void state_bundle_remove(
    data_t *data,
    bundle_t *bundle)
{
    assert(data->phase >= SNAP_NORTH);
    assert(!bundle_is_deleted(bundle));
    assert(data->state != NULL);
    assert(state_bundle_is_member(data, bundle));
    cp_dict_t *e = &bundle->in_state;
    cp_dict_remove(e, &data->state);
    assert(!state_bundle_is_member(data, bundle));
}

/** find the start of an iteration of bundles crossing a pixel */
static inline bundle_t *state_bundle_find_bot(
    data_t *data,
    bundle_t const *pixel)
{
    assert(data->phase >= SNAP_NORTH);
    assert(!bundle_is_deleted(pixel));
    assert(!state_bundle_is_member(data, pixel));
    cp_dict_t *x = cp_dict_find(pixel, data->state,
        cq_sweep_state_pixel_bundle_cmp, data, -2);
    bundle_t *r = CP_BOX0_OF(x, *r, in_state);
    return r;
}

static inline bundle_t *state_bundle_next(
    bundle_t *bundle)
{
    assert(!bundle_is_deleted(bundle));
    cp_dict_t *d = cp_dict_next(&bundle->in_state);
    bundle_t *r = CP_BOX0_OF(d, *r, in_state);
    assert((r == NULL) || !bundle_is_deleted(r));
    return r;
}

static inline bundle_t *state_bundle_prev(
    bundle_t *bundle)
{
    assert(!bundle_is_deleted(bundle));
    cp_dict_t *d = cp_dict_prev(&bundle->in_state);
    bundle_t *r = CP_BOX0_OF(d, *r, in_state);
    assert((r == NULL) || !bundle_is_deleted(r));
    return r;
}

/* ********************************************************************** */
/* 'agenda_vertex' data structure */

static inline vertex_t *agenda_vertex_min(
    data_t *data)
{
    return agenda_get_vertex(data->agenda_vertex_min);
}

static inline void agenda_vertex_update_min(
    data_t *data)
{
    data->agenda_vertex_min = cp_dict_min(data->agenda_vertex);
}

static inline void agenda_vertex_insert(
    data_t *data,
    vertex_t *x)
{
    assert(!agenda_vertex_is_member(data, x));
    cp_dict_insert_update(
        &x->in_agenda,
        &data->agenda_vertex, &data->agenda_vertex_min, NULL,
        data->agenda_vertex_cmp, data, -1);
    assert(data->agenda_vertex_min == cp_dict_min(data->agenda_vertex));
}

static inline void agenda_vertex_remove(
    data_t *data,
    vertex_t *x)
{
    cp_dict_t *v = &x->in_agenda;
    assert(agenda_vertex_is_member(data, x));
    cp_dict_remove(v, &data->agenda_vertex);
    agenda_vertex_update_min(data);
}

/**
 * Change the position of a vertex on the agenda if necessary.
 *
 * The vertex must be on the agenda.
 */
static inline void agenda_vertex_update(
    data_t *data,
    vertex_t *x)
{
    /* simply remove and re-add */
    cp_dict_t *v = &x->in_agenda;
    assert(agenda_vertex_is_member(data, x));
    cp_dict_remove(v, &data->agenda_vertex);
    cp_dict_insert(&x->in_agenda, &data->agenda_vertex, data->agenda_vertex_cmp, data, -1);
    agenda_vertex_update_min(data);
}

static inline vertex_t *agenda_vertex_extract_min(
    data_t *data)
{
    cp_dict_t *m = cp_dict_extract_update_min(&data->agenda_vertex, &data->agenda_vertex_min);
    assert(m != NULL);
    assert(data->agenda_vertex_min == cp_dict_min(data->agenda_vertex));
    vertex_t *r = CP_BOX_OF(m, *r, in_agenda);
    return r;
}

/* ********************************************************************** */
/* 'agenda_xing' data structure */

static inline xing_t *agenda_xing_min(
    data_t *data)
{
    xing_t *m = CP_BOX0_OF(data->agenda_xing_min, *m, in_agenda);
    return m;
}

static inline void agenda_xing_update_min(
    data_t *data)
{
    data->agenda_xing_min = cp_dict_min(data->agenda_xing);
}

static inline xing_t *agenda_xing_extract_min(
    data_t *data)
{
    cp_dict_t *m = cp_dict_extract_update_min(&data->agenda_xing, &data->agenda_xing_min);
    assert(m != NULL);
    assert(data->agenda_xing_min == cp_dict_min(data->agenda_xing));
    xing_t *r = CP_BOX_OF(m, *r, in_agenda);
    return r;
}

static inline xing_t *agenda_xing_insert(
    data_t *data,
    xing_t *e)
{
    cp_dict_t *f_ = cp_dict_insert_update(
        &e->in_agenda,
        &data->agenda_xing, &data->agenda_xing_min, NULL,
        data->agenda_xing_cmp, data, 0);
    xing_t *f = CP_BOX0_OF(f_, *f, in_agenda);
    return f;
}

static inline void xing_new(
    data_t *data,
    cq_vec2if_t const *it,
    edge_t *prev,
    edge_t *next)
{
    assert(prev->next_xing == NULL);
    assert(next->prev_xing == NULL);

    /* use existing new xing, or make a new one */
    xing_t *e = data->new_xing ? data->new_xing : POOL_NEW(data->tmp, *e);
    data->new_xing = NULL;

    /* init new element */
    e->vec2if = *it;

    /* try to insert into agenda */
    xing_t *f = agenda_xing_insert(data, e);
    if (f == NULL) {
        /* remember new crossing */
        cp_v_push_alloc(data->tmp->alloc, data->xings, e);
    }
    else {
        /* set 'e' as empty xing node and use 'f' instead */
        data->new_xing = e;
        e = f;
    }
    assert(data->agenda_xing_min == cp_dict_min(data->agenda_xing));

    /* insert edges */
    assert(prev->next_xing == NULL);
    assert(next->prev_xing == NULL);
    prev->next_xing = e;
    next->prev_xing = e;
    e->some_edge = next;
}

/**
 * Remove a crossing when edges become non-adjacent.
 */
static inline void xing_split(
    edge_t *prev,
    edge_t *next)
{
    if (prev && prev->next_xing) {
        prev->next_xing->some_edge = NULL;
        prev->next_xing = NULL;
    }
    if (next && next->prev_xing) {
        next->prev_xing->some_edge = NULL;
        next->prev_xing = NULL;
    }
}

/**
 * Move crossings from one edge to another */
static inline void xing_move(
    edge_t *e,
    edge_t *o)
{
    assert(e != NULL);
    assert(o != NULL);
    assert(e->next_xing == NULL);
    assert(e->prev_xing == NULL);
    if (o->next_xing) {
        e->next_xing = o->next_xing;
        e->next_xing->some_edge = e;
        o->next_xing = NULL;
        assert(cq_vec2_vec2if_cmp(&e->left.vec2, &e->next_xing->vec2if) < 0);
        assert(cq_vec2_vec2if_cmp(&e->rigt.vec2, &e->next_xing->vec2if) > 0);
    }
    if (o->prev_xing) {
        e->prev_xing = o->prev_xing;
        e->prev_xing->some_edge = e;
        o->prev_xing = NULL;
        assert(cq_vec2_vec2if_cmp(&e->left.vec2, &e->prev_xing->vec2if) < 0);
        assert(cq_vec2_vec2if_cmp(&e->rigt.vec2, &e->prev_xing->vec2if) > 0);
    }
}

/**
 * Clear any crossings that are right of the right end of o
 */
static inline void xing_clear_beyond(
    edge_t *e)
{
    if (e->next_xing &&
        cq_vec2_vec2if_cmp(&e->rigt.vec2, &e->next_xing->vec2if) <= 0)
    {
        edge_t *f = tree_edge_next(e);
        assert(f->prev_xing == e->next_xing);
        e->next_xing->some_edge = NULL;
        e->next_xing = NULL;
        f->prev_xing = NULL;
    }

    if (e->prev_xing &&
        cq_vec2_vec2if_cmp(&e->rigt.vec2, &e->prev_xing->vec2if) <= 0)
    {
        edge_t *f = tree_edge_prev(e);
        assert(f->next_xing == e->prev_xing);
        e->prev_xing->some_edge = NULL;
        e->prev_xing = NULL;
        f->next_xing = NULL;
    }
}

/* ********************************************************************** */
/* phases 'poly' and 'triangle' */

static inline void list_edge_init(
    data_t *data CP_UNUSED,
    edge_t *e)
{
    assert(data->phase & HAVE_LIST);
    cp_list_init(&e->list);
}

/** For START event */
static inline void list_edge_init2(
    data_t *data CP_UNUSED,
    /* the lower edge */
    edge_t *a,
    /* the upper edge */
    edge_t *b)
{
    assert(data->phase & HAVE_LIST);
    assert(a->back != b->back);
    assert(cq_vec2_eq(&a->left.vec2, &b->left.vec2));
    list_edge_init(data, a);
    list_edge_init(data, b);
    cp_list_chain(&a->list, &b->list);
}

/** For BEND event */
static inline void list_edge_insert(
    data_t *data CP_UNUSED,
    /* the left edge */
    edge_t *a,
    /* the right edge */
    edge_t *b)
{
    assert(data->phase & HAVE_LIST);
    assert(a->list.next != &a->list); /* a is a non-singleton */
    assert(a->list.prev != &a->list);
    assert(b->list.next == &b->list); /* b is a singleton (a new edge) */
    assert(b->list.prev == &b->list);
    assert(a->back == b->back);
    assert(cq_vec2_eq(&a->rigt.vec2, &b->left.vec2));
    /* cp_list_chain(a,b) inserts b between a and a->next
     * After the operation, a->next == b.
     *
     * Note that this is a list of edges.
     * If !'back' (i.e., 'forward'), then 'a->next' (i.e., a->edge[0]) points to the
     * edge further right (i.e., 'b').  If 'back', then 'a->prev' will point to 'b'.
     */
    if (a->back) {
        cp_list_chain(&b->list, &a->list);
        assert(a->list.prev == &b->list);
    }
    else {
        cp_list_chain(&a->list, &b->list);
        assert(a->list.next == &b->list);
    }
}

static inline void list_edge_remove(
    edge_t *e)
{
    cp_list_remove(&e->list);
}

/** For one case of an END event */
static inline void list_edge_merge(
    data_t *data CP_UNUSED,
    /* the lower edge */
    edge_t *a,
    /* the upper edge */
    edge_t *b)
{
    assert(data->phase & HAVE_LIST);
    assert(a->list.next != &a->list);
    assert(a->list.prev != &a->list);
    assert(b->list.next != &b->list);
    assert(b->list.prev != &b->list);
    assert(a->back != b->back);
    assert(cq_vec2_eq(&a->rigt.vec2, &b->rigt.vec2));
    /* the edges will connect at the right vertex */

    /**
     * If 'a->back', then a->prev will be b.
     * If 'b->back', then a->next will be b.
     */
    if (a->back) {
        cp_list_chain(&b->list, &a->list);
        assert(a->list.prev == &b->list);
    }
    else {
        cp_list_chain(&a->list, &b->list);
        assert(a->list.next == &b->list);
    }
}

/**
 * Plain chain operation, used a START conditions in some algorithms */
static inline void list_edge_chain(
    data_t *data CP_UNUSED,
    /* the lower edge */
    edge_t *a,
    /* the upper edge */
    edge_t *b)
{
    assert(data->phase & HAVE_LIST);
    cp_list_chain(&a->list, &b->list);
    assert(a->list.next == &b->list);
}

static inline vertex_t *list_edge_get_end(
    edge_t *e,
    /** 0=source, 1=target */
    unsigned target)
{
    return &e->v[e->back ^ target];
}

static inline edge_t *list_edge_step(edge_t *e, unsigned dir)
{
    cq_assert(dir <= 1);
    return list_get_edge(e->list.edge[dir]);
}

static inline edge_t *list_edge_next(edge_t *e)
{
    return list_edge_step(e, 0);
}

static inline edge_t *list_edge_prev(edge_t *e)
{
    return list_edge_step(e, 1);
}

/* ********************************************************************** */

static inline bool phase_south(
    data_t *data)
{
    return data->phase == SNAP_SOUTH;
}

/* ********************************************************************** */

#define CQ_PT_INVAL (-1U)

/* The following few functions establish a way of viewing the
 * edge list as a vertex list, as needed for the triangulation
 * algorithm.  It is stored as an edge list because there is
 * already memory space at an edge, and vertices are stored
 * inside an edge anyway.  By doing it this way, we need less
 * list cells.  Also, we need to store the vertices of each
 * edge as a separate structure (two edges cannot just point
 * to a vertex), because at improper starts, the same vertex
 * is split and put into separate paths.
 *
 * The focus in the triangulation algorithm is on vertices,
 * however.  The triangulation subroutine moves along a path
 * and converts ears into triangles, deleting a vertex.  This
 * needs to be replicated.
 */

/**
 * Get the buddy vertex of the next edge, or NULL.
 * The buddy is the vertex opposite in the
 * connected edge.
 */
static inline vertex_t *vertex_buddy(
    vertex_t *v)
{
    edge_t *e = edge_of(v);
    /* LEFT(0) vertex on FORE(0) edge: step PREV(1)
     * RIGT(1) vertex on FORE(0) edge: step NEXT(0)
     * LEFT(0) vertex on BACK(1) edge: step NEXT(0)
     * RIGT(1) vertex on BACK(1) edge: step PREV(1)
     * => step !(side ^ back)
     */
    unsigned dir = !(v->side ^ e->back);
    e = list_edge_step(e, dir);

    /* FORE(0) edge after NEXT(0) step: return LEFT(0)
     * BACK(0) edge after NEXT(0) step: return RIGT(1)
     * FORE(0) edge after PREV(1) step: return RIGT(1)
     * BACK(0) edge after PREV(1) step: return LEFT(0)
     * => return dir ^ back;
     */
    vertex_t *w = &e->v[dir ^ e->back];
    return w;
}

/**
 * Get the buddy vertex of the next edge, or NULL.
 *
 * This returns a vertex that has the same coordinates,
 * but is on an adjacent edge on the opposite side of
 * this vector's side (after normalising the direction
 * based on the pointiness of the edge).
 */
static inline vertex_t *vertex_eq_buddy(
    vertex_t *v)
{
    vertex_t *w = vertex_buddy(v);
    if (!cq_vec2_eq(&v->vec2, &w->vec2)) {
        return NULL;
    }

    return w;
}

/**
 * Get next vertex based on the edge list.
 *
 * This skips vertices that are disabled.
 * This does not skip duplicate vertices of adjacent edges.
 *
 * This reacts to the 'back' boolean to reverse the order of
 * vertices on an edge.
 *
 * 'dir' == 0: steps to next
 * 'dir' == 1: steps to prev
 */
static inline vertex_t *vertex_list_step_raw(vertex_t *v, unsigned dir)
{
    cq_assert(dir <= 1);
    edge_t *e = edge_of(v);
    if (v->side ^ e->back ^ dir) {
        /* For dir == 0:
         *   RIGT vertex at FORE edge,
         *   LEFT vertex at BACK edge
         *   => v is the DST end of e
         *   => return SRC(NEXT(e))
         *   Here, SRC is LEFT or RIGT based on whether NEXT(e) is fore or back.
         *   NEXT(e) is FORE => return LEFT(NEXT(e))
         *   NEXT(e) is BACK => return RIGT(NEXT(e))
         *
         * For dir == 1:
         *   LEFT vertex at FORE edge,
         *   RIGT vertex at BACK edge
         *   => v is the SRC end of e
         *   => return DST(NEXT(e))
         *   Here, DST is RIGT or LEFT based on whether NEXT(e) is fore or back.
         *   NEXT(e) is FORE => return RIGT(NEXT(e))
         *   NEXT(e) is BACK => return LEFT(NEXT(e))
         *
         */
        e = list_edge_step(e, dir);
        return &e->v[e->back ^ dir];
    }
    /* For dir == 0:
     *    LEFT vertex at forward edge,
     *    RIGT vertex at backward edge
     *    => v is the SRC end of e
     *    => return other end
     * For dir == 1:
     *    RIGT vertex at forward edge,
     *    LEFT vertex at backward edge
     *    => v is the DST end of e
     *    => return other end
     */
    return other_end(v);
}

/**
 * Whether a vertex is valid (i.e., is left of the scan line) */
static inline bool vertex_valid(vertex_t *v)
{
    return (v != NULL) && (v->point_idx != CQ_PT_INVAL);
}

/**
 * Same as vertex_list_step_raw(), but skips disabled vertices.
 */
static inline vertex_t *vertex_list_step(vertex_t *v, unsigned dir)
{
    assert(vertex_valid(v));
    for (cq_each_assert_max(3)) {
        v = vertex_list_step_raw(v, dir);
        if (!vertex_valid(v)) {
            return v;
        }

        edge_t *e = edge_of(v);
        if (!e->v_dis[v->side]) {
            return v;
        }

        /* if both sides are disabled, why is the edge still in the list? */
        assert(!e->v_dis[!v->side]);
    }
}

/**
 * Same as vertex_list_step(), but skips equal vertices.
 *
 * This does not mark skipped vertices as disabled, because the
 * double-vertex is needed at improper starts to break up the one
 * loop into two.
 */
static inline vertex_t *vertex_list_step_neq(vertex_t *v, unsigned dir)
{
    cq_assert(dir <= 1);
    vertex_t *o = v;
    for (cq_each_assert_max(2)) {
        vertex_t *w = vertex_list_step(v, dir);
        if (o == w) {
            return NULL; /* !! */
        }
        if (w->point_idx != v->point_idx) { // !cq_vec2_eq(&w->vec2, &v->vec2)) {
            return w;
        }
        v = w;
    }
}

static inline vertex_t *vertex_list_next_raw(vertex_t *v)
{
    return vertex_list_step_raw(v, 0);
}

static inline vertex_t *vertex_list_next(vertex_t *v)
{
    return vertex_list_step(v, 0);
}

static inline vertex_t *vertex_list_next_neq(vertex_t *v)
{
    return vertex_list_step_neq(v, 0);
}

static inline vertex_t *vertex_list_prev_raw(vertex_t *v)
{
    return vertex_list_step_raw(v, 1);
}

static inline vertex_t *vertex_list_prev(vertex_t *v)
{
    return vertex_list_step(v, 1);
}

static inline vertex_t *vertex_list_prev_neq(vertex_t *v)
{
    return vertex_list_step_neq(v, 1);
}

/* ********************************************************************** */
/* point index handling */

static inline void point_idx_init(
    cq_vec2_t *last_pt,
    unsigned *idx)
{
    *last_pt = (cq_vec2_t){};
    *idx = 0;
}

static inline unsigned point_idx_get(
    cq_vec2_t *last_pt,
    unsigned *idx,
    cq_csg2_poly_t *r,
    cq_vec2_t *t_vec2)
{
    for (;;) {
        if (*idx >= r->point.size) {
            *idx = ~0U;
            break;
        }

        *last_pt = cq_import_vec2(&cp_v_nth(&r->point, *idx).coord);
        if (cq_vec2_eq(last_pt, t_vec2)) {
            return *idx;
        }
        (*idx)++;
    }

    if ((r->point.size == 0) || !cq_vec2_eq(last_pt, t_vec2)) {
        *last_pt = *t_vec2;
        cp_v_push(&r->point, ((cp_vec2_loc_t){ .coord = cq_export_vec2(last_pt) }));
    }
    assert(r->point.size > 0);
    assert(r->point.size < ~0U);
    return (r->point.size - 1) & ~0U;
}

/* ********************************************************************** */

#if CQ_TRACE

extern void cq_sweep_trace_end_page(
    data_t *data);

extern void cq_sweep_trace_begin_page(
    data_t *data,
    vertex_t *i,
    xing_t *q,
    bundle_t *b,
    cq_csg2_poly_t *r);

#else /* !CQ_TRACE */

#define cq_sweep_trace_end_page(...)   ((void)0)
#define cq_sweep_trace_begin_page(...) ((void)0)

#endif /* !CQ_TRACE */

#endif /* CQ_OP_SWEEP_INTERNAL_H_ */
