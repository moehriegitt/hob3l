/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* difference between two layers for better JS export */
#if 0
/**
 * FIXME: Disable for now until implemented correctly.
 * This is not perfect: we actually want a XOR
 * between the layers with triangulation, and the original polygon
 * should be reflexcted by the vertex order of each triangle.
 * (Before if a triangle survives for the upper layer, the triangle is
 * visible when viewed from the bottom, and for a lower layer, the
 * triangle is visible when viewed from the top.)
 */
static cp_csg2_poly_t *poly_sub(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_csg2_poly_t *a0,
    cp_csg2_poly_t *a1)
{
    size_t a0_point_sz CP_UNUSED = a0->point.size;
    size_t a1_point_sz CP_UNUSED = a1->point.size;

    lazy_t o0;
    CP_ZERO(&o0);
    csg2_op_poly(&o0, a0);

    lazy_t o1;
    CP_ZERO(&o1);
    csg2_op_poly(&o1, a1);

    flatten_lazy(opt, tmp, &o0, &o1, CP_OP_SUB);
    assert(o0.size == 2);

    ctxt_t c[1];
    ctxt_op_poly(c, tmp, &o0, false);

    cp_csg2_poly_t *o = cp_csg2_new(*o, NULL);
    cq_sweep_poly(c->sweep, o);

    ctxt_fini(c);

    /* check that the originals really haven't changed */
    assert(a0->point.size == a0_point_sz);
    assert(a1->point.size == a1_point_sz);

    return o;
}
#endif

#if 0 /* FIXME: XOR layer */
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
#endif

#if 0 /* FIXME: XOR layer */
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
#endif

#if 0 /* FIXME: XOR layer */
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
#endif

#if 0 /* FIXME: XOR layer */
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
#endif

#if 0 /* FIXME: XOR layer */
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
#endif

/**
 * Diff a layer with the next and store the result in diff_above/diff_below.
 *
 * The tree must have been processed with cp_csg2_op_flatten_layer(),
 * and the layer ID must be in range.
 *
 * r is modified and a diff_below polygon is added.  The original polygons
 * are left untouched.
 *
 * Runtime and space: see cp_csg2_op_flatten_layer.
 */
extern void cp_csg2_op_diff_layer(
    cp_csg_opt_t const *opt,
    cp_pool_t *tmp,
    cp_csg2_tree_t *a,
    size_t zi)
{
#if 0 /* FIXME: XOR layer */
    TRACE();
    cp_csg2_stack_t *s CP_UNUSED = cp_csg2_cast(*s, a->root);
    assert(zi < s->layer.size);

    csg2_op_diff_csg2(opt, tmp, zi, a->root);
#endif
}

