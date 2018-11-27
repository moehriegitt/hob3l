/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_LIST_H_
#define CP_LIST_H_

#include <stddef.h>
#include <assert.h>
#include <hob3lbase/list_tam.h>

/* BEGIN MACRO * DO NOT EDIT, use 'mkmacro' instead. */

/**
 * Insert a list between p and p->next.
 *
 * It will hold that p->next == n and
 * OLD(p->next)->prev == OLD(n->prev).
 *
 * For insertion of q between p and p->prev, just reverse the argument
 * order, i.e., use 'insert(q, p)'.
 */
#define cp_list_insert(p,n) \
    cp_list_insert_1_(CP_GENSYM(_l_n_neighIG), CP_GENSYM(_l_p_neighIG), \
        CP_GENSYM(_nIG), CP_GENSYM(_pIG), (p), (n))

#define cp_list_insert_1_(_l_n_neigh,_l_p_neigh,n,p,_p,_n) \
    do{ \
        __typeof__(*_p) *p = _p; \
        __typeof__(*_n) *n = _n; \
        assert(p != n); \
        assert(p != NULL); \
        assert(n != NULL); \
        __typeof__(*p) *_l_p_neigh = p->next; \
        __typeof__(*p) *_l_n_neigh = n->prev; \
        n->prev = p; \
        p->next = n; \
        _l_p_neigh->prev = _l_n_neigh; \
        _l_n_neigh->next = _l_p_neigh; \
    }while(0)

/**
 * Initialise a list.
 *
 * A list contains at least one node, which could be said
 * not to belong to the list (but that's a matter of definition).
 * The init function initialises the node by pointing next and prev
 * to itself, creating a ring of one node.
 */
#define cp_list_init(x) \
    cp_list_init_1_(CP_GENSYM(_xBA), (x))

#define cp_list_init_1_(x,_x) \
    do{ \
        __typeof__(*_x) *x = _x; \
        assert(x != NULL); \
        x->next = x; \
        x->prev = x; \
    }while(0)

/**
 * Split a list so that p becomes the predecessor of n.
 *
 * Splitting is only allowed if the split list has
 * more than two nodes.
 *
 * p and n can be the same, in which case this removes
 * the node from the other list.
 */
#define cp_list_split(p,n) \
    cp_list_split_1_(CP_GENSYM(_l_npMN), CP_GENSYM(_l_pnMN), \
        CP_GENSYM(_nMN), CP_GENSYM(_pMN), (p), (n))

#define cp_list_split_1_(_l_np,_l_pn,n,p,_p,_n) \
    do{ \
        __typeof__(*_p) *p = _p; \
        __typeof__(*_n) *n = _n; \
        assert(n != NULL); \
        assert(p != NULL); \
        __typeof__(*n) *_l_np = n->prev; \
        __typeof__(*n) *_l_pn = p->next; \
        _l_pn->prev = _l_np; \
        _l_np->next = _l_pn; \
        p->next = n; \
        n->prev = p; \
    }while(0)

/**
 * Remove the node from the list.
 */
#define cp_list_remove(q) \
    cp_list_remove_1_(CP_GENSYM(_qFT), (q))

#define cp_list_remove_1_(q,_q) \
    do{ \
        __typeof__(*_q) *q = _q; \
        cp_list_split(q, q); \
    }while(0)

/**
 * Swap two nodes in a list or between two lists
 */
#define cp_list_swap(q,p) \
    cp_list_swap_1_(CP_GENSYM(_pED), CP_GENSYM(_qED), (q), (p))

#define cp_list_swap_1_(p,q,_q,_p) \
    do{ \
        __typeof__(*_q) *q = _q; \
        __typeof__(*_p) *p = _p; \
        assert(q != NULL); \
        assert(p != NULL); \
        cp_list_swap_( \
            q, \
            p, \
            CP_PTRDIFF((char*)&q->next, (char*)q), \
            CP_PTRDIFF((char*)&q->prev, (char*)q)); \
    }while(0)

/**
 * Whether the node is part of a list.  Note that strictly speaking,
 * single nodes are lists of one element, but this only returns true
 * for lists of length > 1.  Often, one element of the list is used
 * as the sentinel, not counted as part of the list, so the membership
 * criterion is consistent with that model.
 */
#define cp_list_is_member(n) \
    cp_list_is_member_1_(CP_GENSYM(_nBF), (n))

#define cp_list_is_member_1_(n,_n) \
    ({ \
        __typeof__(*_n) *n = _n; \
        assert(n != NULL); \
        assert(n->next != NULL); \
        assert(n->prev != NULL); \
        (n != n->next); \
    })

/* END MACRO */

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
    cp_list_each_1_(CP_GENSYM(_n), i, (n))

#define cp_list_each_1_(_n, i, n) \
    __typeof__(*n) *_n = n, *i = _n ->next; \
    i != _n; \
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
    cp_list_each_rev_1_(CP_GENSYM(_n), i, (n))

#define cp_list_each_rev_1_(_n, i, n) \
    __typeof__(*n) *_n = n, *i = _n ->prev; \
    i != _n; \
    i = i->prev

/**
 * Internal function to swap two elements.
 *
 * Use cp_list_swap() instead.
 */
extern void cp_list_swap_(
    void *a, void *b, size_t offset_next, size_t offset_prev);

#endif /* CP_LIST_H_ */
