/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/arith.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/alloc.h>
#include <hob3l/gc.h>
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
        __typeof__(*(_r)) *__r = (_r); \
        __r->type = (_type); \
        __r->loc = (_loc); \
    })

static cp_csg2_t *csg2_tree_from_csg3(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg3_t const *d);

static void csg2_tree_from_v_csg3(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_v_csg2_p_t *c,
    cp_v_csg3_p_t const *d)
{
    cp_v_ensure_size(c, d->size);
    for (cp_v_each(i, d)) {
        cp_v_nth(c,i) = csg2_tree_from_csg3(r, s, cp_v_nth(d,i));
    }
}

static cp_csg2_t *csg2_tree_from_csg3_add(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg3_add_t const *d)
{
    cp_csg2_t *_c = cp_csg2_new(CP_CSG2_ADD, d->loc);
    cp_csg2_add_t *c = cp_csg2_add(_c);
    csg2_tree_from_v_csg3(r, s, &c->add, &d->add);
    return _c;
}

static cp_csg2_t *csg2_tree_from_csg3_sub(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg3_sub_t const *d)
{
    cp_csg2_t *_c = cp_csg2_new(CP_CSG2_SUB, d->loc);
    cp_csg2_sub_t *c = cp_csg2_sub(_c);
    cp_csg2_add_init_perhaps(&c->add, c->loc);
    cp_csg2_add_init_perhaps(&c->sub, c->loc);
    csg2_tree_from_v_csg3(r, s, &c->add.add, &d->add.add);
    csg2_tree_from_v_csg3(r, s, &c->sub.add, &d->sub.add);
    return _c;
}

static cp_csg2_t *csg2_tree_from_csg3_cut(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg3_cut_t const *d)
{
    cp_csg2_t *_c = cp_csg2_new(CP_CSG2_CUT, d->loc);
    cp_csg2_cut_t *c = cp_csg2_cut(_c);

    cp_v_init0(&c->cut, d->cut.size);
    for (cp_v_each(i, &c->cut)) {
        cp_csg2_t *_q = csg2_tree_from_csg3_add(r, s, cp_v_nth(&d->cut, i));
        cp_v_nth(&c->cut, i) = cp_csg2_add(_q);
    }
    return _c;
}

static cp_csg2_t *csg2_tree_from_csg3_obj(
    cp_range_t const *s,
    cp_csg3_t const *d)
{
    cp_csg2_t *_c = cp_csg2_new(CP_CSG2_STACK, d->loc);
    cp_csg2_stack_t *c = cp_csg2_stack(_c);

    c->csg3 = d;
    c->idx0 = 0;
    cp_v_init0(&c->layer, s->cnt);
    assert(c->layer.size == s->cnt);

    return _c;
}

static cp_csg2_t *csg2_tree_from_csg3(
    cp_csg2_tree_t *r,
    cp_range_t const *s,
    cp_csg3_t const *d)
{
    switch (d->type) {
    case CP_CSG3_SPHERE:
    case CP_CSG3_CYL:
    case CP_CSG3_POLY:
    case CP_CSG2_CIRCLE:
    case CP_CSG2_POLY:
        return csg2_tree_from_csg3_obj(s, d);

    case CP_CSG3_ADD:
        return csg2_tree_from_csg3_add(r, s, cp_csg3_cast(_add, d));

    case CP_CSG3_SUB:
        return csg2_tree_from_csg3_sub(r, s, cp_csg3_cast(_sub, d));

    case CP_CSG3_CUT:
        return csg2_tree_from_csg3_cut(r, s, cp_csg3_cast(_cut, d));
    }

    CP_DIE("3D object type");
}

/* ********************************************************************** */
/* extern */

/**
 * Initialise a cp_csg2_add_t object unless it is initialised
 * already.
 *
 * For this to work, the data must be zeroed first, then this
 * function can be used to initialise it, if it is not yet
 * initialised.
 */
extern void cp_csg2_add_init_perhaps(
    cp_csg2_add_t *r,
    cp_loc_t loc)
{
    assert((r->type == 0) || (r->type == CP_CSG2_ADD));
    if (r->type == CP_CSG2_ADD) {
        return;
    }
    CP_CSG2_INIT(r, CP_CSG2_ADD, loc);
}

/**
 * Internal: allocate a new CSG2 object.
 */
extern cp_csg2_t *__cp_csg2_new(
    char const *file,
    int line,
    cp_csg2_type_t type,
    cp_loc_t loc)
{
    static const size_t size[] = {
        [CP_CSG2_CIRCLE]   = sizeof(cp_csg2_circle_t),
        [CP_CSG2_POLY]     = sizeof(cp_csg2_poly_t),
        [CP_CSG2_ADD]      = sizeof(cp_csg2_add_t),
        [CP_CSG2_SUB]      = sizeof(cp_csg2_sub_t),
        [CP_CSG2_CUT]      = sizeof(cp_csg2_cut_t),
        [CP_CSG2_STACK]    = sizeof(cp_csg2_stack_t),
    };
    assert(type < cp_countof(size));
    assert(size[type] != 0);
    cp_csg2_t *r = cp_calloc(file, line, 1, size[type]);
    CP_CSG2_INIT(r, type, loc);
    return r;
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
    cp_csg2_tree_opt_t const *o)
{
    r->root = cp_csg2_new(CP_CSG2_ADD, d->root->loc);
    r->thick = s->step;
    r->opt = *o;

    cp_v_init0(&r->flag, s->cnt);

    cp_v_init0(&r->z, s->cnt);
    for (cp_size_each(zi, s->cnt)) {
        cp_v_nth(&r->z, zi) = s->min + (s->step * cp_f(zi));
    }

    if (d->root == NULL) {
        return;
    }

    csg2_tree_from_v_csg3(r, s, &cp_csg2_add(r->root)->add, &d->root->add);
}
