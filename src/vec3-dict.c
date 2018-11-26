/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/dict.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/pool.h>
#include <hob3l/vec3-dict.h>

static int cmp_vec3(
    cp_vec3_t const *a,
    cp_dict_t *_b,
    void *u CP_UNUSED)
{
    cp_vec3_dict_node_t *b = CP_BOX_OF(_b, *b, node);
    return cp_vec3_lex_cmp(a, &b->coord);
}

/**
 * Insert or find a vec3 in the given dictionary.
 *
 * Returns the insertion position, starting at 0.
 */
extern cp_vec3_dict_node_t *cp_vec3_dict_insert(
    cp_pool_t *pool,
    cp_vec3_dict_t *dict,
    cp_vec3_t const *v,
    cp_loc_t loc)
{
    cp_dict_ref_t ref;
    cp_vec3_dict_node_t *n =
        CP_BOX0_OF(cp_dict_find_ref(&ref, v, dict->root, cmp_vec3, NULL, 0), *n, node);
    if (n == NULL) {
        n = CP_POOL_NEW(pool, *n);
        n->coord = *v;
        n->idx = dict->size;
        n->loc = loc;
        dict->size++;
        cp_dict_insert_ref(&n->node, &ref, &dict->root);
    }
    return n;
}
