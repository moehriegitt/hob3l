/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/list.h>

#define NEXT(n) (*get_ptr(n, offset_next))
#define PREV(n) (*get_ptr(n, offset_prev))

static void **get_ptr(void *a, size_t o)
{
    return (void**)((char*)a + o);
}

/**
 * Internal function to swap two elements.
 *
 * Use cp_list_swap() instead.
 */
extern void cp_list_swap_(
    void *a, void *b, size_t offset_next, size_t offset_prev)
{
    /* same */
    if (a == b) {
        return;
    }

    void *na = NEXT(a);
    void *pa = PREV(a);
    void *nb = NEXT(b);
    void *pb = PREV(b);
    if (na == a) {
        if (nb == b) {
            return;
        }
        cp_list_swap_(b, a, offset_next, offset_prev);
        return;
    }

    if (nb == a) {
        if (na == b) {
            return;
        }
        cp_list_swap_(b, a, offset_next, offset_prev);
        return;
    }

    assert(na != a);
    assert(nb != a);

    if (na == b) {
        assert(pb == a);
        /* BEFORE: pa->a->b->nb */
        /* AFTER:  pa->b->a->nb */
        PREV(a) = b;
        NEXT(a) = nb;
        PREV(b) = pa;
        NEXT(b) = a;
        PREV(nb) = a;
        NEXT(pa) = b;
        return;
    }

    /* BEFORE: pa->a->na  pb->b->nb */
    /* AFTER:  pa->b->na  pb->a->nb */
    PREV(b) = pa;
    NEXT(b) = na;
    PREV(na) = b;
    NEXT(pa) = b;

    if (nb == b) {
        assert(pb == b);
        /* BEFORE: pa->a->na  b */
        /* AFTER:  pa->b->na  a */
        PREV(a) = NEXT(a) = a;
        return;
    }

    PREV(a) = pb;
    NEXT(a) = nb;
    PREV(nb) = a;
    NEXT(pb) = a;
}

/* ********************************************************************** */
/* macros */
#ifdef CP_MACRO_

/**
 * Insert a list between p and p->next.
 *
 * It will hold that p->next == n and
 * OLD(p->next)->prev == OLD(n->prev).
 *
 * For insertion of q between p and p->prev, just reverse the argument
 * order, i.e., use 'insert(q, p)'.
 */
extern macro void cp_list_insert(val *p, val *n)
{
    assert(p != n);
    assert(p != NULL);
    assert(n != NULL);
    __typeof__(*p) *_l_p_neigh = p->next;
    __typeof__(*p) *_l_n_neigh = n->prev;
    n->prev = p;
    p->next = n;
    _l_p_neigh->prev = _l_n_neigh;
    _l_n_neigh->next = _l_p_neigh;
}

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
 * Split a list so that p becomes the predecessor of n.
 *
 * Splitting is only allowed if the split list has
 * more than two nodes.
 *
 * p and n can be the same, in which case this removes
 * the node from the other list.
 */
extern macro void cp_list_split(val *p, val *n)
{
    assert(n != NULL);
    assert(p != NULL);
    __typeof__(*n) *_l_np = n->prev;
    __typeof__(*n) *_l_pn = p->next;
    _l_pn->prev = _l_np;
    _l_np->next = _l_pn;
    p->next = n;
    n->prev = p;
}

/**
 * Remove the node from the list.
 */
extern macro void cp_list_remove(val *q)
{
    cp_list_split(q, q);
}

/**
 * Swap two nodes in a list or between two lists
 */
extern macro void cp_list_swap(val *q, val *p)
{
    assert(q != NULL);
    assert(p != NULL);
    cp_list_swap_(
        q,
        p,
        CP_PTRDIFF((char*)&q->next, (char*)q),
        CP_PTRDIFF((char*)&q->prev, (char*)q));
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
