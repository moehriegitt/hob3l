/* -*- Mode: C -*- */

/* print in SCAD format */

#include <csg2plane/csg2.h>
#include <cpmat/arith.h>
#include <cpmat/vec.h>
#include <cpmat/mat.h>
#include <csg2plane/gc.h>
#include "internal.h"

static void v_csg2_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    cp_v_csg2_p_t *r);

static void union_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    cp_v_csg2_p_t *r)
{
    v_csg2_put_stl(s, t, d, r);
}

static void sub_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    cp_csg2_sub_t *r)
{
    /* FIXME: this output format cannot do SUB, only UNION, so ignore
     * the SUB part.  This should actually be an error... */
    union_put_stl (s, t, d + IND, &r->add.add);
}

static void cut_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    cp_csg2_cut_t *r)
{
    /* FIXME: this output format cannot do CUT, only UNION, so ignore
     * all but the first part.  This should actually be an error... */
    if (r->cut.size > 0) {
        union_put_stl(s, t, d + IND, &r->cut.data[0]->add);
    }
}

static void poly_put_stl(
    cp_stream_t *s,
    int d,
    cp_csg2_poly_t *r)
{
    (void)s;
    (void)d;
    (void)r;
}

#if 0
static void layer_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    cp_csg2_layer_t *r)
{
    (void)s;
    (void)t;
    (void)d;
    (void)r;
}
#endif

static void stack_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    cp_csg2_stack_t *r)
{
    (void)s;
    (void)t;
    (void)d;
    (void)r;
}

static void csg2_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    cp_csg2_t *r)
{
    switch (r->type) {
    case CP_CSG2_ADD:
        /* not directly iterated */
        assert(0);
        return;

    case CP_CSG2_SUB:
        sub_put_stl(s, t, d, &r->sub);
        return;

    case CP_CSG2_CUT:
        cut_put_stl(s, t, d, &r->cut);
        return;

    case CP_CSG2_POLY:
        poly_put_stl(s, d, &r->poly);
        return;

    case CP_CSG2_STACK:
        stack_put_stl(s, t, d, &r->stack);
        return;

    case CP_CSG2_CIRCLE:
        assert(0);
        return;
    }

    assert(0);
}

static void v_csg2_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t,
    int d,
    cp_v_csg2_p_t *r)
{
    for (cp_v_each(i, r)) {
        csg2_put_stl(s, t, d, r->data[i]);
    }
}

extern void cp_csg2_tree_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t)
{
    if (t->root != NULL) {
        csg2_put_stl(s, t, 0, t->root);
    }
}
