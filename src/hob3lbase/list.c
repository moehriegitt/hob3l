/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/list.h>

/* ********************************************************************** */
/* macros */
#ifdef CP_MACRO_

/**
 * Initialise a list.
 *
 * A list contains at least one node, which could be said
 * not to belong to the list (but that's a matter of definition).
 * The init function initialises the node by pointing next and prev
 * to itself, creating a ring of one node.
 */
extern macro void cp_list_init(val *x)
{
    assert(x != NULL);
    x->next = x;
    x->prev = x;
}

/**
 * Insert a list between p and p->next.
 * Split a list so that p becomes the predecessor of n.
 *
 *      ...a->n->b...    ...c->p->d...
 * =>   ...a->d...       ...c->p->n->b...
 *
 * It will hold that p->next == n and
 * OLD(p->next)->prev == OLD(n->prev).
 *
 * For insertion of q between p and p->prev, just reverse the argument
 * order, i.e., use 'chain(q, p)'.
 *
 * For removal from a list, pass the same node twice: 'chain(n, n)'.
 */
extern macro void cp_list_chain(val *p, val *n)
{
    assert(p != NULL);
    assert(n != NULL);
    __typeof__(*p) *_l_pn = p->next;
    __typeof__(*p) *_l_np = n->prev;
    n->prev = p;
    p->next = n;
    _l_pn->prev = _l_np;
    _l_np->next = _l_pn;
}

/**
 * Remove the node from the list.
 */
extern macro void cp_list_remove(val *q)
{
    cp_list_chain(q, q);
}

/**
 * Whether the node is part of a list.  Note that strictly speaking,
 * single nodes are lists of one element, but this only returns true
 * for lists of length > 1.  Often, one element of the list is used
 * as the sentinel, not counted as part of the list, so the membership
 * criterion is consistent with that model.
 */
extern macro val cp_list_is_member(val *n)
{
    assert(n != NULL);
    assert(n->next != NULL);
    assert(n->prev != NULL);
    (n != n->next);
}

#endif /*0*/
