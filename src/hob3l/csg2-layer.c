/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#define DEBUG 0

#include <hob3lbase/arith.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/base-mat.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/pool.h>
#include <hob3lbase/obj.h>
#include <hob3lop/op-slice.h>
#include <hob3l/gc.h>
#include <hob3l/csg.h>
#include <hob3l/csg2.h>
#include <hob3l/csg3.h>
#include "internal.h"

static void csg2_add_layer_poly(
    cp_pool_t *pool,
    double z,
    cp_v_obj_p_t *c,
    cp_csg3_poly_t const *d)
{
    /* FIXME: no temporary alloc yet (we could put 'r' and probably 'c' in the pool.
     * op_slice also needs a pool param, and we probably want to use an allocator
     * instead of a pool so that cp_v_push_alloc() can be used (with pool storage).
     */
    (void)pool;

    cp_csg2_vline2_t *r = cp_csg2_new(*r, d->loc);

    cq_slice_t slice;
    cq_slice_init(&slice, &r->q, z);
    for (cp_v_each(i, &d->face)) {
        cp_csg3_face_t *face = &cp_v_nth(&d->face, i);
        cq_slice_add_face(&slice, &face->point);
    }
    cq_slice_fini(&slice);


    if (r->q.size == 0) {
        CP_DELETE(r);
    }
    else {
        cp_v_push(c, cp_obj(r));
    }
}

static void csg2_add_layer_sphere(
    cp_pool_t *pool,
    double z,
    cp_v_obj_p_t *c,
    cp_csg3_sphere_t const *d)
{
    (void)pool; /* FIXME: pool for vectors (see above) */

    /* Mechanism for slicing an ellipse from the ellipsoid:
     * Problem: we want to cut the ellipsoid at the XZ plane defined by z.
     * The ellipsoid is given as a unit sphere mapped to the object coordinate
     * system using d->mat.
     *
     * Idea:
     *   * Map the cutting XZ plane into the unit sphere coordinate system using
     *     the inverse object coordinate matrix.  This can be done by mapping three
     *     points in the XY plane, e.g., [1,0,z], [0,0,z], and [0,1,z] and then
     *     reconstructing the plane normal from those points.
     *   * Rotate the unit sphere's coordinate system so that the mapped plane
     *     is parallel to z again, call this coordinate system Q.  This can be
     *     done using the *_rot_into_z() matrix.
     *   * In Q, compute the distance between the plane and the unit sphere center,
     *     which is easy because by the nature of this coordinate system, we just
     *     need to subtract the z coordinates.
     *   * Decide by distance whether the plane cuts the sphere, and compute the
     *     diameter of the circle in Q.  This can be done by basic trigonometry.
     *   * Nap the circle using the inverse matrix of Q back into the object
     *     coordinate system, then reduce the 3D mapping matrix to a 2D mapping
     *     matrix by eliminating the Z component.
     */

    /* step 1: compute the plane in d's coordinate system */
    cp_vec3_t oa = {{ 1, 0, z }};
    cp_vec3_t ob = {{ 0, 0, z }};
    cp_vec3_t oc = {{ 0, 1, z }};
    cp_vec3_t sa, sb, sc;
    cp_vec3w_xform(&sa, &d->mat->i, &oa);
    cp_vec3w_xform(&sb, &d->mat->i, &ob);
    cp_vec3w_xform(&sc, &d->mat->i, &oc);
    cp_vec3_t sn;
    if (!cp_vec3_right_normal3(&sn, &sa, &sb, &sc)) {
        /* coordinate system is too small/large: generate nothing */
        return;
    }

    /* step 2: compute coordinate system where sn points upright again */
    cp_mat3wi_t mq = {0};
    cp_mat3wi_rot_into_z(&mq, &sn);

    /* step 3: rotate plane base point around unit sphere */
    cp_vec3_t sq;
    cp_vec3w_xform(&sq, &mq.n, &sb);

    /* step 4: distance and radius of circle cutting the plane */
    cp_dim_t dist = fabs(sq.z);
    if (cp_ge(dist, 1)) {
        /* plane does not cut unit sphere */
        return;
    }
    cp_dim_t rad = sqrt(1 - (dist*dist));

    /* step 5: map 2D circle into so that we can compute it at [cos,sin,0] */

    /* (a) scale unit circle */
    cp_mat3wi_t mt;
    cp_mat3wi_scale(&mt, rad, rad, 1);

    /* (b) move circle into plane in Q */
    cp_mat3wi_t mm;
    cp_mat3wi_xlat(&mm, 0, 0, sq.z);
    cp_mat3wi_mul(&mt, &mm, &mt);

    /* (c) rotate into sphere coordinate system: apply mq inverse.
     * We do not need mq anymore, so we can invert in place */
    cp_mat3wi_inv(&mq, &mq);
    cp_mat3wi_mul(&mt, &mq, &mt);

    /* (d) map from unit sphere coordinate system into object coordinate system */
    cp_mat3wi_mul(&mt, d->mat, &mt);

    /* (e) extract 2D part of matrix */
    cp_mat2wi_t mt2;
    (void)cp_mat2wi_from_mat3wi(&mt2, &mt);

    /* approximate circle */
    size_t fn = d->_fn;
    cp_csg2_vline2_t *r = cp_csg2_new(*r, d->loc);
    cp_v_push(c, cp_obj(r));
    cp_v_init0(&r->q, fn);
    for (cp_circle_each(i, fn)) {
        cp_vec2_t ptf = { .x = i.cos, .y = i.sin };
        cp_vec2w_xform(&ptf, &mt2.n, &ptf);

        cq_vec2_t pti = cq_import_vec2(&ptf);

        cq_line2_t *p0 = &cp_v_nth(&r->q, i.idx);
        cq_line2_t *p1 = &cp_v_nth(&r->q, cp_wrap_add1(i.idx, fn));
        p0->a = pti;
        p1->b = pti;
    }
}

static void csg2_add_layer(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_t *c);

static void csg2_add_layer_v(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_v_obj_p_t *c)
{
    for (cp_v_each(i, c)) {
        csg2_add_layer(no, pool, r, zi, cp_csg2_cast(cp_csg2_t, cp_v_nth(c,i)));
    }
}

static void csg2_add_layer_add(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg_add_t *c)
{
    csg2_add_layer_v(no, pool, r, zi, &c->add);
}

static void csg2_add_layer_sub(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg_sub_t *c)
{
    bool add_no = false;
    csg2_add_layer_add(&add_no, pool, r, zi, c->add);
    if (add_no) {
        *no = true;
        csg2_add_layer_add(no, pool, r, zi, c->sub);
    }
}

static void csg2_add_layer_cut(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg_cut_t *c)
{
    for (cp_v_each(i, &c->cut)) {
        csg2_add_layer_add(no, pool, r, zi, cp_v_nth(&c->cut, i));
    }
}

static void csg2_add_layer_xor(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg_xor_t *c)
{
    for (cp_v_each(i, &c->xor)) {
        csg2_add_layer_add(no, pool, r, zi, cp_v_nth(&c->xor, i));
    }
}

static void csg2_add_layer_stack(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_stack_t *c)
{
    double z = cp_v_nth(&r->z, zi);

    cp_csg3_t const *d = c->csg3;

    cp_csg2_layer_t *l = cp_csg2_stack_get_layer(c, zi);
    if (l == NULL) {
        return;
    }

    cp_csg_add_init_perhaps(&l->root, d->loc);
    l->zi = zi;

    switch (d->type) {
    case CP_CSG3_SPHERE:
        csg2_add_layer_sphere(pool, z, &l->root->add, cp_csg3_cast(cp_csg3_sphere_t, d));
        break;

    case CP_CSG3_POLY:
        csg2_add_layer_poly(pool, z, &l->root->add, cp_csg3_cast(cp_csg3_poly_t, d));
        break;

    default:
        CP_DIE("3D object");
    }

    if (cp_csg_add_size(l->root) > 0) {
        *no = true;
    }

    return;
}

static void csg2_add_layer(
    bool *no,
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi,
    cp_csg2_t *c)
{
    switch (c->type) {
    case CP_CSG2_STACK:
        csg2_add_layer_stack(no, pool, r, zi, cp_csg2_cast(cp_csg2_stack_t, c));
        return;

    case CP_CSG_ADD:
        csg2_add_layer_add(no, pool, r, zi, cp_csg_cast(cp_csg_add_t, c));
        return;

    case CP_CSG_XOR:
        csg2_add_layer_xor(no, pool, r, zi, cp_csg_cast(cp_csg_xor_t, c));
        return;

    case CP_CSG_SUB:
        csg2_add_layer_sub(no, pool, r, zi, cp_csg_cast(cp_csg_sub_t, c));
        return;

    case CP_CSG_CUT:
        csg2_add_layer_cut(no, pool, r, zi, cp_csg_cast(cp_csg_cut_t, c));
        return;

    case CP_CSG2_POLY:
    case CP_CSG2_VLINE2:
    case CP_CSG2_SWEEP:
        CP_DIE("unexpected tree structure: objects should be inside stack");
    }

    CP_DIE("3D object type: %#x", c->type);
}

/* ********************************************************************** */
/* extern */

/**
 * If the stack has the given layer, return it.
 * Otherwise, return NULL.
 */
extern cp_csg2_layer_t *cp_csg2_stack_get_layer(
    cp_csg2_stack_t *c,
    size_t zi)
{
    if (! (zi >= c->idx0)) {
        return NULL;
    }
    size_t i = zi - c->idx0;
    if (! (i < c->layer.size)) {
        return NULL;
    }
    return &cp_v_nth(&c->layer, i);
}

/**
 * This generates stacks of polygon CSG2 trees.
 *
 * The tree must be either empty, or the root of the tree must have
 * type CP_CSG2_ADD.  If the result is non-empty, this will set the
 * root to a node of type CP_CSG2_ADD.
 *
 * In the polygons, only the 'path' entries are filled in, i.e.,
 * the 'triangle' entries are left empty.
 *
 * Uses \p pool for all temporary allocations (but not for constructing r).
 */
extern void cp_csg2_tree_add_layer(
    cp_pool_t *pool,
    cp_csg2_tree_t *r,
    size_t zi)
{
    assert(r->root != NULL);
    assert(r->root->type == CP_CSG2_ADD);
    assert(zi < r->z.size);
    bool no = false;
    csg2_add_layer_add(&no, pool, r, zi, cp_csg_cast(cp_csg_add_t, r->root));
}

/**
 * Return the layer thickness of a given layer.
 */
extern cp_dim_t cp_csg2_layer_thickness(
    cp_csg2_tree_t *r,
    size_t zi CP_UNUSED)
{
    /* Currently, all layers have uniform thickness. */
    return r->thick;
}
