/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#define DEBUG 0

#include <hob3lbase/arith.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/alloc.h>
#include <hob3l/gc.h>
#include <hob3l/obj.h>
#include <hob3l/csg.h>
#include <hob3l/csg2.h>
#include <hob3l/csg3.h>
#include <hob3l/ps.h>
#include "internal.h"

/**
 * Manual initialisation of CSG2 objects.
 *
 * Note: This does not zero the object, this has to be done before (with the
 * right size of the corresponding struct type).
 */
#define CP_CSG2_INIT(_r, _type, _loc) \
    ({ \
        __typeof__(*(_r)) *_r = (_r); \
        _r->type = (_type); \
        _r->loc = (_loc); \
    })

static cp_csg2_t *csg2_tree_from_csg3(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg3_t const *d);

static void csg2_tree_from_v_csg3(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_v_obj_p_t *c,
    cp_v_obj_p_t const *d)
{
    TRACE();
    cp_v_ensure_size(c, d->size);
    for (cp_v_each(i, d)) {
        cp_v_nth(c,i) = cp_obj(csg2_tree_from_csg3(r, s, cp_csg3_cast(cp_csg3_t, cp_v_nth(d,i))));
    }
}

static cp_csg_add_t *csg2_tree_from_csg3_add(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg_add_t const *d)
{
    TRACE();
    cp_csg_add_t *c = cp_csg_new(*c, d->loc);
    csg2_tree_from_v_csg3(r, s, &c->add, &d->add);
    return c;
}

static cp_csg_sub_t *csg2_tree_from_csg3_sub(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg_sub_t const *d)
{
    TRACE();
    cp_csg_sub_t *c = cp_csg_new(*c, d->loc);
    c->add = csg2_tree_from_csg3_add(r, s, d->add);
    c->sub = csg2_tree_from_csg3_add(r, s, d->sub);
    return c;
}

static cp_csg_cut_t *csg2_tree_from_csg3_cut(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg_cut_t const *d)
{
    TRACE();
    cp_csg_cut_t *c = cp_csg_new(*c, d->loc);
    cp_v_init0(&c->cut, d->cut.size);
    for (cp_v_each(i, &c->cut)) {
        cp_v_nth(&c->cut, i) = csg2_tree_from_csg3_add(r, s, cp_v_nth(&d->cut, i));
    }
    return c;
}

static cp_csg_xor_t *csg2_tree_from_csg3_xor(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg_xor_t const *d)
{
    TRACE();
    cp_csg_xor_t *c = cp_csg_new(*c, d->loc);
    cp_v_init0(&c->xor, d->xor.size);
    for (cp_v_each(i, &c->xor)) {
        cp_v_nth(&c->xor, i) = csg2_tree_from_csg3_add(r, s, cp_v_nth(&d->xor, i));
    }
    return c;
}

static cp_csg2_t *csg2_tree_from_csg3_obj(
    cp_range_t const *s,
    cp_csg3_t const *d)
{
    TRACE();
    cp_csg2_stack_t *c = cp_csg2_new(*c, d->loc);

    c->csg3 = d;
    c->idx0 = 0;
    cp_v_init0(&c->layer, s->cnt);
    assert(c->layer.size == s->cnt);

    return cp_csg2_cast(cp_csg2_t, c);
}

static cp_csg2_t *csg2_tree_from_csg3(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg3_t const *d)
{
    TRACE("obj=%s", d->loc);
    switch (d->type) {
    case CP_CSG3_SPHERE:
    case CP_CSG3_POLY:
    case CP_CSG2_POLY:
        return csg2_tree_from_csg3_obj(s, d);

    case CP_CSG_ADD: {
        cp_csg_add_t const *dx = cp_csg_cast(*dx, d);
        return cp_csg2_cast(cp_csg2_t, csg2_tree_from_csg3_add(r, s, dx));
        }
    case CP_CSG_XOR: {
        cp_csg_xor_t const *dx = cp_csg_cast(*dx, d);
        return cp_csg2_cast(cp_csg2_t, csg2_tree_from_csg3_xor(r, s, dx));
        }
    case CP_CSG_SUB: {
        cp_csg_sub_t const *dx = cp_csg_cast(*dx, d);
        return cp_csg2_cast(cp_csg2_t, csg2_tree_from_csg3_sub(r, s, dx));
        }

    case CP_CSG_CUT: {
        cp_csg_cut_t const *dx = cp_csg_cast(*dx, d);
        return cp_csg2_cast(cp_csg2_t, csg2_tree_from_csg3_cut(r, s, dx));
        }
    }

    CP_DIE("3D object type");
}

/* ********************************************************************** */
/* extern */

/**
 * Initialise a cp_csg_add_t object unless it is initialised
 * already.
 *
 * For this to work, the data must be zeroed first, then this
 * function can be used to initialise it, if it is not yet
 * initialised.
 */
extern void cp_csg_add_init_perhaps(
    cp_csg_add_t **r,
    cp_loc_t loc)
{
    if (*r != NULL) {
        assert((*r)->type == CP_CSG_ADD);
        return;
    }
    *r = cp_csg_new(**r, loc);
}

/**
 * Initialises a CSG2 structure with a tree derived from a CSG3
 * structure, and reserves, for each simple object in the tree, an
 * array of layers of size layer_cnt.
 *
 * This assumes a freshly zeroed r to be initialised.
 */
extern void cp_csg2_tree_from_csg3(
    cp_csg2_tree_t *r,
    cp_csg3_tree_t const *d,
    cp_range_t const *s,
    cp_csg_opt_t const *o)
{
    TRACE();
    assert(d != NULL);
    cp_csg_add_t *root = cp_csg_new(*root, d->root ? d->root->loc : NULL);
    r->root = cp_csg2_cast(*r->root, root);
    r->thick = s->step;
    r->opt = o;

    cp_v_init0(&r->flag, s->cnt);

    cp_v_init0(&r->z, s->cnt);
    for (cp_size_each(zi, s->cnt)) {
        cp_v_nth(&r->z, zi) = s->min + (s->step * cp_f(zi));
    }

    if (d->root == NULL) {
        return;
    }

    csg2_tree_from_v_csg3(r, s, &cp_csg_cast(cp_csg_add_t, r->root)->add, &d->root->add);
}
