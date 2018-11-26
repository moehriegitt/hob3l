/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_LIST_H_
#define CP_LIST_H_

#include <stddef.h>
#include <assert.h>
#include <hob3lbase/list_tam.h>

/**
 * Iterate the list.
 *
 * The node n itself is excluded from the iteration.
 *
 * The iteration is robust against modification of the list, but not
 * against deallocation of the iterated element.
 *
 * The list will not terminate if the start node is removed from the
 * list while iterating.
 */
#define cp_list_each(i, n) \
    __typeof__(*(n)) *__n = (n), *i = __n ->next; \
    i != __n; \
    i = i->next

/**
 * Iterate the list in reverse direction
 *
 * The node n itself is excluded from the iteration.
 *
 * The iteration is robust against modification of the list, but not against
 * deallocation of the iterated element.
 *
 * The list will not terminate if the start node is removed from the
 * list while iterating.
 */
#define cp_list_each_rev(i, n) \
    __typeof__(*(n)) *__n = (n), *i = __n ->prev; \
    i != __n; \
    i = i->prev

/**
 * Initialise a list.
 *
 * A list contains at least one node, which could be said
 * not to belong to the list (but that's a matter of definition).
 * The init function initialises the node by pointing next and prev
 * to itself, creating a ring of one node.
 */
#define cp_list_init(x) \
    ({ \
        __typeof__(*(x)) *__x = (x); \
        assert(__x != NULL); \
        __x->next = __x; \
        __x->prev = __x; \
    })

/**
 * Insert a list between p and p->next.
 *
 * It will hold that p->next == n and
 * OLD(p->next)->prev == OLD(n->prev).
 *
 * For insertion of q between p and p->prev, just reverse the argument
 * order, i.e., use 'insert(q, p)'.
 */
#define cp_list_insert(p, n) \
    ({ \
        __typeof__(*(p)) *__p = (p); \
        __typeof__(*(p)) *__n = (n); \
        assert(__p != __n); \
        assert(__p != NULL); \
        assert(__n != NULL); \
        __typeof__(*(p)) *__p_neigh = __p->next; \
        __typeof__(*(p)) *__n_neigh = __n->prev; \
        __n->prev = __p; \
        __p->next = __n; \
        __p_neigh->prev = __n_neigh; \
        __n_neigh->next = __p_neigh; \
    })

/**
 * Split a list so that p becomes the predecessor of n.
 *
 * Splitting is only allowed if the split list has
 * more than two nodes.
 *
 * p and n can be the same, in which case this removes
 * the node from the other list.
 */
#define cp_list_split(p, n) \
    ({ \
        __typeof__(*(n)) *__n = (n); \
        __typeof__(*(n)) *__p = (p); \
        assert(__n != NULL); \
        assert(__p != NULL); \
        __typeof__(*(n)) *__np = __n->prev; \
        __typeof__(*(n)) *__pn = __p->next; \
        __pn->prev = __np; \
        __np->next = __pn; \
        __p->next = __n; \
        __n->prev = __p; \
    })

/**
 * Remove the node from the list.
 */
#define cp_list_remove(q) \
    ({ \
        __typeof__(*(q)) *__q = (q); \
        cp_list_split(__q, __q); \
    })

/**
 * Swap two nodes in a list or between two lists
 */
#define cp_list_swap(q, p) \
    ({ \
        __typeof__(*(q)) *__q = (q); \
        __typeof__(*(q)) *__p = (p); \
        assert(__q != NULL); \
        assert(__p != NULL); \
        cp_list_swap_(\
            __q, \
            __p, \
            CP_PTRDIFF((char*)&__q->next, (char*)__q), \
            CP_PTRDIFF((char*)&__q->prev, (char*)__q)); \
    })

/**
 * Whether the node is part of a list.  Note that strictly speaking,
 * single nodes are lists of one element, but this only returns true
 * for lists of length > 1.  Often, one element of the list is used
 * as the sentinel, not counted as part of the list, so the membership
 * criterion is consistent with that model.
 */
#define cp_list_is_member(n) \
    ({ \
        __typeof__(*(n)) *__n = (n); \
        assert(__n != NULL); \
        assert(__n->next != NULL); \
        assert(__n->prev != NULL); \
        (__n != __n->next); \
    })

/**
 * Internal function to swap two elements.
 *
 * Use cp_list_swap() instead.
 */
extern void cp_list_swap_(
    void *a, void *b, size_t offset_next, size_t offset_prev);

#endif /* CP_LIST_H_ */
