/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <hob3lbase/dict.h>
#include <hob3lbase/list.h>
#include <hob3lbase/base-mat.h>
#include <hob3lbase/arith.h>
#include <hob3lbase/pool.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/panic.h>
#include <hob3lbase/obj.h>
#include <hob3lbase/bool-bitmap.h>
#include <hob3lop/op-sweep.h>
#include <hob3lop/op-trianglify.h>
#include <hob3lop/op-poly.h>
#include <hob3lop/op-ps.h>
#include <hob3l/csg.h>
#include <hob3l/csg2.h>
#include <hob3l/ps.h>
#include "internal.h"

/**
 * An unresolved polygon combination.
 *
 * This type is only used in this file.
 */
typedef struct {
    /**
     * Number of polygons to be combined (valid entries in \a data)
     * If this is >= 1, then either `result` or `data[0]` is valid,
     * with `result` the preference.
     */
    size_t size;

    /**
     * Polygons to be combined.
     *
     * This can be a CP_CSG2_POLY (directly from csg3) or CP_CSG2_VLINE2 (typically
     * from a 3D->2D slice operation) or CP_CSG2_SWEEP (from a flattening operation
     * in this file).
     *
     * The SWEEP is only used internally in this module for results.
     */
    cp_csg2_t *data[CP_BOOL_BITMAP_MAX_LAZY];

    /**
     * Boolean combination map to decide from a mask of inside bits for each
     * polygon whether the result is inside.  This is indexed bitwise with the
     * mask of bits.  The number of entries is (1U << size) bits. */
    cp_bool_bitmap_t comb;
} cp_csg2_lazy_t;

typedef cp_csg2_bool_mode_t mode_t;
typedef cp_csg2_lazy_t      lazy_t;

/**
 * Context for csg2_op_csg2 functions.
 */
typedef struct {
    cp_csg_opt_t const *opt;
    cp_pool_t *tmp;
} op_ctxt_t;

/* ********************************************************************* */

/**
 * This sets r->sweep by combining all the r->data[*] into one polygon.
 * It then resets r->size to 0 and clears r->sweep to NULL if empty.
 * Or if non-empty, sets r->size to 1 and returns without resetting r->sweep.
 *
 * If r->sweep is set when this is entered, then r->data[0] will be
 * overwritten by exporting r->sweep, then r->sweep will be deleted and
 * set anew.
 *
 * Note that because lazy polygon structures have no dedicated space to store
 * a polygon, they must reuse the space of the input polygons, so running this
 * may reuse space from the stored polygons.
 */
static void flatten_eager(
    cp_pool_t *tmp,
    lazy_t *r,
    mode_t mode)
{
    if (r->size == 0) {
        return;
    }
    if (r->size == 1) {
        assert(r->data[0] != NULL);
        if (r->data[0]->type == CP_CSG2_SWEEP) {
            return;
        }
        if (mode == CP_CSG2_BOOL_MODE_VLINE2) {
            if (r->data[0]->type == CP_CSG2_VLINE2) {
                return;
            }
        }
    }

    /* construct a new result */
    cq_sweep_t *sweep = cq_sweep_new(tmp, r->data[0]->loc, 0);

    /* add all polygons */
    for (cp_size_each(i, r->size)) {
        cp_csg2_t *pi = r->data[i];
        size_t member = ((size_t)1) << i;
        switch (pi->type) {
        default:
            assert(0 && "unexpected object type");

        case CP_CSG2_VLINE2:{
            cp_csg2_vline2_t *pi_vline2 = cp_csg2_cast(cp_csg2_vline2_t, pi);
            cq_sweep_add_v_line2(sweep, &pi_vline2->q, member);
            break;}

        case CP_CSG2_SWEEP:{
            cq_sweep_t *pi_sweep = (cq_sweep_t*)pi;
            cq_sweep_add_sweep(sweep, pi_sweep, member);
            cq_sweep_delete(pi_sweep);
            r->data[i] = NULL; /* this is gone, be sure to destroy it */
            break;}

        case CP_CSG2_POLY:{
            cp_csg2_poly_t *pi_poly = cp_csg2_cast(cp_csg2_poly_t, pi);
            cq_sweep_add_poly(sweep, &pi_poly->q, member);
            break;}
        }
    }

    /* run algorithms */
    cq_sweep_intersect(sweep);
    cq_sweep_reduce(sweep, &r->comb, (1U << r->size));

    /* evaluate and mark result */
    if (cq_sweep_empty(sweep)) {
        cq_sweep_delete(sweep);
        r->size = 0;
        return;
    }

    r->size = 1;
    r->data[0] = (cp_csg2_t*)sweep;
    r->comb.b[0] = 2;
}

/* ********************************************************************* */

static void flatten_lazy_vline2(
    lazy_t *o,
    cp_csg2_vline2_t *a)
{
    assert(cp_mem_is0(o, sizeof(*o)));
    if (a->q.size > 0) {
        o->size = 1;
        o->data[0] = cp_csg2_cast(cp_csg2_t, a);
        o->comb.b[0] = 2; /* == 0b10 */
    }
}

static void flatten_lazy_poly(
    lazy_t *o,
    cp_csg2_poly_t *a)
{
    assert(cp_mem_is0(o, sizeof(*o)));
    if (a->q.point.size > 0) {
        o->size = 1;
        o->data[0] = cp_csg2_cast(cp_csg2_t, a);
        o->comb.b[0] = 2; /* == 0b10 */
    }
}

static void flatten_lazy_rec(
    op_ctxt_t *c,
    size_t zi,
    lazy_t *o,
    cp_csg2_t *a);

/**
 * Compute r = r op b, but run this lazily, possibly just building up r
 * and reducing it only if it gets too larger.
 * The actual reduction is them done by flatten_eager().
 */
static void flatten_lazy(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    lazy_t *r,
    lazy_t *b,
    cp_bool_op_t op);

static void flatten_lazy_v_csg2(
    op_ctxt_t *c,
    size_t zi,
    lazy_t *o,
    cp_v_obj_p_t *a)
{
    assert(cp_mem_is0(o, sizeof(*o)));
    for (cp_v_each(i, a)) {
        cp_csg2_t *ai = cp_csg2_cast(*ai, cp_v_nth(a,i));
        if (i == 0) {
            flatten_lazy_rec(c, zi, o, ai);
        }
        else {
            lazy_t oi = { 0 };
            flatten_lazy_rec(c, zi, &oi, ai);
            flatten_lazy(c->opt, c->tmp, o, &oi, CP_OP_ADD);
        }
    }
}

static void flatten_lazy_add(
    op_ctxt_t *c,
    size_t zi,
    lazy_t *o,
    cp_csg_add_t *a)
{
    assert(cp_mem_is0(o, sizeof(*o)));
    flatten_lazy_v_csg2(c, zi, o, &a->add);
}

static void flatten_lazy_cut(
    op_ctxt_t *c,
    size_t zi,
    lazy_t *o,
    cp_csg_cut_t *a)
{
    assert(cp_mem_is0(o, sizeof(*o)));
    for (cp_v_each(i, &a->cut)) {
        cp_csg_add_t *b = cp_v_nth(&a->cut, i);
        if (i == 0) {
            flatten_lazy_add(c, zi, o, b);
        }
        else {
            lazy_t oc = {0};
            flatten_lazy_add(c, zi, &oc, b);
            flatten_lazy(c->opt, c->tmp, o, &oc, CP_OP_CUT);
        }
    }
}

static void flatten_lazy_xor(
    op_ctxt_t *c,
    size_t zi,
    lazy_t *o,
    cp_csg_xor_t *a)
{
    assert(cp_mem_is0(o, sizeof(*o)));
    for (cp_v_each(i, &a->xor)) {
        cp_csg_add_t *b = cp_v_nth(&a->xor, i);
        if (i == 0) {
            flatten_lazy_add(c, zi, o, b);
        }
        else {
            lazy_t oc = {0};
            flatten_lazy_add(c, zi, &oc, b);
            flatten_lazy(c->opt, c->tmp, o, &oc, CP_OP_XOR);
        }
    }
}

static void flatten_lazy_layer(
    op_ctxt_t *c,
    lazy_t *o,
    cp_csg2_layer_t *a)
{
    assert(cp_mem_is0(o, sizeof(*o)));
    if (a->root == NULL) {
        return;
    }
    flatten_lazy_add(c, a->zi, o, a->root);
}

static void flatten_lazy_sub(
    op_ctxt_t *c,
    size_t zi,
    lazy_t *o,
    cp_csg_sub_t *a)
{
    assert(cp_mem_is0(o, sizeof(*o)));
    flatten_lazy_add(c, zi, o, a->add);

    lazy_t os = {0};
    flatten_lazy_add(c, zi, &os, a->sub);
    flatten_lazy(c->opt, c->tmp, o, &os, CP_OP_SUB);
}

static void flatten_lazy_stack(
    op_ctxt_t *c,
    size_t zi,
    lazy_t *o,
    cp_csg2_stack_t *a)
{
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
    flatten_lazy_layer(c, o, l);
}

static void flatten_lazy_rec(
    op_ctxt_t *c,
    size_t zi,
    lazy_t *o,
    cp_csg2_t *a)
{
    assert(cp_mem_is0(o, sizeof(*o)));
    switch (a->type) {
    case CP_CSG2_SWEEP:
        assert(0 && "no cq_sweep support here, expected v_line2 or poly internally");
        return;

    case CP_CSG2_POLY:
        flatten_lazy_poly(o, cp_csg2_cast(cp_csg2_poly_t, a));
        return;

    case CP_CSG2_VLINE2:
        flatten_lazy_vline2(o, cp_csg2_cast(cp_csg2_vline2_t, a));
        return;

    case CP_CSG2_STACK:
        flatten_lazy_stack(c, zi, o, cp_csg2_cast(cp_csg2_stack_t, a));
        return;

    case CP_CSG_ADD:
        flatten_lazy_add(c, zi, o, cp_csg_cast(cp_csg_add_t, a));
        return;

    case CP_CSG_XOR:
        flatten_lazy_xor(c, zi, o, cp_csg_cast(cp_csg_xor_t, a));
        return;

    case CP_CSG_SUB:
        flatten_lazy_sub(c, zi, o, cp_csg_cast(cp_csg_sub_t, a));
        return;

    case CP_CSG_CUT:
        flatten_lazy_cut(c, zi, o, cp_csg_cast(cp_csg_cut_t, a));
        return;
    }

    CP_DIE("2D object type");
}

/**
 * Boolean operation on two lazy polygons.
 *
 * This does 'r = r op b'.
 *
 * Only the path information is used, not the triangles.
 *
 * \p r and/or \p b are reused and cleared to construct r.  This may happen
 * immediately or later in flatten_eager().
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
static void flatten_lazy(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    lazy_t *r,
    lazy_t *b,
    cp_bool_op_t op)
{
    assert(opt->max_simultaneous >= 2);
    size_t max_sim = cp_min(opt->max_simultaneous, cp_countof(r->data));
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
            flatten_eager(tmp, r, CP_CSG2_BOOL_MODE_VLINE2);
            assert(r->size <= 1);
        }
        else {
            flatten_eager(tmp, b, CP_CSG2_BOOL_MODE_VLINE2);
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

    cp_bool_bitmap_repeat(&r->comb, r->size, b->size);
    cp_bool_bitmap_spread(&b->comb, b->size, r->size);

    r->size += b->size;

    cp_bool_bitmap_combine(&r->comb, &b->comb, r->size, op);

#ifndef NDEBUG
    /* clear with garbage to trigger bugs when accessed */
    memset(b, 170, sizeof(*b));
#endif
}

/* ********************************************************************** */
/* extern */

/**
 * Add a layer to a tree by reducing it from another tree.
 *
 * This runs the algorithm in such a way that the new layer is suitable
 * for output in various formats (e.g., STL).  The resulting layer has
 * type cp_csg2_poly_t (while the input layer leaves are expected
 * to be in cp_csg2_vline2_t format from the cp_csg2_tree_add_layer() step).
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
extern bool cp_csg2_op_flatten_layer(
    cp_err_t *err,
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_csg2_tree_t *r,
    cp_csg2_tree_t *a,
    size_t zi)
{
    cp_csg2_stack_t *s = cp_csg2_cast(*s, r->root);
    assert(zi < s->layer.size);

    op_ctxt_t c = {
        .opt = opt,
        .tmp = tmp,
    };

    assert(a->root != NULL);
    cp_loc_t loc = a->root->loc;
    lazy_t ol = {};
    flatten_lazy_rec(&c, zi, &ol, a->root);
    flatten_eager(tmp, &ol, CP_CSG2_BOOL_MODE_TRI);

    if (ol.size == 0) {
        return true;
    }
    assert(ol.size == 1);
    assert(ol.data[0] != NULL);
    assert(ol.data[0]->type == CP_CSG2_SWEEP);
    cq_sweep_t *sweep = (cq_sweep_t*)ol.data[0];
    ol.data[0] = NULL;

    cp_csg2_poly_t *o = cp_csg2_new(*o, loc);
    if (!cq_sweep_trianglify(err, sweep, &o->q)) {
        return false;
    }
    cq_sweep_delete(sweep);

    assert(o->point.size > 0);

    /* new layer */
    cp_csg2_layer_t *layer = cp_csg2_stack_get_layer(s, zi);
    assert(layer != NULL);
    cp_csg_add_init_perhaps(&layer->root, NULL);

    layer->zi = zi;

    cp_v_nth(&r->flag, zi) |= CP_CSG2_FLAG_NON_EMPTY;

    /* single polygon per layer */
    cp_v_push(&layer->root->add, cp_obj(o));

    return true;
}

/**
 * Reduce a set of 2D CSG items into a single polygon.
 *
 * This does not triangulate, but only create the path.  It is meant for
 * use within the SCAD/CSG3 framework (see csg3.c).
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
 * Runtime and space: see cp_csg2_op_flatten_layer.
 */
extern cp_csg2_poly_t *cp_csg2_flatten(
    cp_err_t *err,
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_v_obj_p_t *root,
    cp_csg2_bool_mode_t mode)
{
    assert((err->msg.size == 0) && "there was an error before");
    op_ctxt_t c = {
        .opt = opt,
        .tmp = tmp,
    };

    cp_loc_t loc = NULL;
    if (root->size == 0) {
        return NULL;
    }
    loc = root->data[0]->loc;

    lazy_t ol = {};
    flatten_lazy_v_csg2(&c, 0, &ol, root);
    flatten_eager(tmp, &ol, mode);

    if (ol.size == 0) {
        return NULL;
    }
    assert(ol.size == 1);
    assert(ol.data[0] != NULL);
    assert(ol.data[0]->type == CP_CSG2_SWEEP);
    cq_sweep_t *sweep = (cq_sweep_t*)ol.data[0];
    ol.data[0] = NULL;

#ifdef PSTRACE
    char psfn[32];
    snprintf(psfn, sizeof(psfn), "debug.ps");
    cq_ps_doc_begin(psfn);
    cq_vec2_minmax_t bb2 = CQ_VEC2_MINMAX_INIT;
    cq_sweep_minmax(&bb2, sweep);
    cq_ps_init(&bb2);
#endif

    cp_csg2_poly_t *o = cp_csg2_new(*o, loc);
    switch (mode) {
    default:
        assert(0 && "Illegal value");
        break;

    case CP_CSG2_BOOL_MODE_PATH:
        if (!cq_sweep_poly(err, sweep, &o->q)) {
            return NULL;
        }
        break;

    case CP_CSG2_BOOL_MODE_TRI:
        if (!cq_sweep_trianglify(err, sweep, &o->q)) {
            return NULL;
        }
        break;
    }
    cq_sweep_delete(sweep);

#ifdef PSTRACE
    cq_ps_doc_end();
#endif

    assert(o->point.size > 0);
    return o;
}

/**
 * Initialise a tree for cp_csg2_op_flatten_layer() operations.
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
