/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * This implements non-directional rings including mirror nodes.
 *
 * A ring is a cyclic sequence that can be traversed in both
 * directions: a-b-c-a-b-c-a-b-c-...  It is denoted by a-b-c*.
 * Since this is non-directional, a-b-c-d* is equivalent to a-d-c-b*.
 *
 * A mirror node is a node where the sequence is reversed:
 * a-b-c-d-c-b-a...  It is denoted by a-b-c-d|
 * Again, due to being non-directional, a-b-c| is the same as c-b-a|
 */

#ifndef __CP_RING_H
#define __CP_RING_H

#include <assert.h>
#include <stdio.h>
#include <hob3lbase/def.h>
#include <hob3lbase/ring_tam.h>

/**
 * Iterate through a ring, assiging n to each node between b and a.
 *
 * This iterates all node starting with c=next(a,b), and continuing
 * in that direction, i.e., the next node is d=next(b,c), then
 * e=next(c,d), etc.  Nodes a and b are not iterated, i.e., the
 * iteration stops if next(u,v)==a.
 *
 * If a==b, this does not iterate any node either.
 *
 * This also stops at mirror nodes.
 *
 * E.g.
 *
 * For a-b-c-d-e-a-b..., each(n,a,b) iterates n={c,d,e}.
 * For a-b-c-d-e-d-c..., each(n,a,b) iterates n={c,d,e}.
 */
#define cp_ring_each(n,a,b) \
    cp_ring_t *__a = (a), \
        *__b = (b), \
        *__n = cp_ring_next(__a,__b), \
        *n = __n; \
    (n != __a); \
    __n = cp_ring_next(__b, n), \
        __n = (__n == __b ? __a : __n), \
        __b = n, \
        n = __n

/**
 * Cut a ring at a given pair, i.e., make each of the nodes an end.
 *
 * Note that we introduce no NULL or mirrors, but self-loops.
 *
 * Runtime: O(1)
 */
extern void cp_ring_cut(
    cp_ring_t *a,
    cp_ring_t *b);

/**
 * Join together two mirrow nodes.
 *
 * On two singletons, this makes a pair.  The ends of a pair
 * are always mirrors, so a pair is both a ring and a sequence
 * with mirrors and its ends.
 *
 * From three nodes, things become more interesting.  This function
 * will only take care of connecting two nodes, it will not create a
 * ring, e.g. it will not create a ring from a pair joined with a
 * singleton, but the result will be a sequence of three nodes with
 * two mirror nodes.  To make a ring, use 'cp_ring_insert_*'
 * functions.
 *
 * E.g.:
 *    with a* and b*, join(a, b) = a-b*
 *
 *    with a-b* and c*, join(b, c) = a-b-c|
 *
 *    with a-b-c* and d*, join(c, d) = ERROR (c is no mirror node)
 *
 *    with a-b-c| and d*, joint(c,d) = a-b-c-d|
 *
 * Runtime: O(1)
 *
 * Note: This prototype would not work for XOR lists.
 */
extern void cp_ring_join(
    cp_ring_t *a,
    cp_ring_t *b);

/**
 * Insert one ring into another one.
 *
 * This needs two edges a-b and u-v and will cut both
 * edges and reconnect a-u and b-v instead.
 *
 * To insert a node n between two nodes a-b, use
 * cp_ring_insert_between() or cp_ring_insert_after() instead.
 * To make a ring of two elements, use cp_ring_pair() instead.
 *
 * This function can both join and split rings.
 *
 * It can also swap two adjacent nodes a-b, by passing:
 * prev(a,b), a, b, next(a,b).
 *
 * Connecting to self means 'make a mirror' (only singletons
 * are connected to itself, so this broadens the definition).
 *
 * Examples for rewire(a,b,u,v):
 *
 * (a) reversal:
 *     x-a-b-u-v-z    => x-a  b-u    v-z  => x-a-u-b-v-z
 *     x-a-b-y-u-v-z  => x-a  b-y-u  v-z  => x-a-u-y-b-v-z
 *
 * (b1) split:
 *     x-a-b-v-u-z    => x-a  b-v    u-z  => x-a-u-z  b-v*
 *     x-a-b-y-v-u-z  => x-a  b-y-v  u-z  => x-a-u-z  b-y-v*
 *
 * (b2) singleton extraction: b==v, a!=u
 *     x-a-b-u-z      => x-a  b  u-z  => x-a-u-z  b*
 *         v
 *
 * (b3) singleton extraction: a==u, b!=v
 *     z-v-a-b-y      => z-v  a  b-y  => z-v-b-y  a*
 *         u
 *
 * (b4) a==u, b==v: a and b are a pair: split pair into singletons:
 *
 *     a-b*    => a* b*
 *
 * (b5) a==u, b==v: b is a mirror: cut off the mirror:
 *
 *     x-u-v-y   => x-a  b-y      => x>-a*  *b-<y
 *     x-a-b-y
 *
 * (b6) a==u, b==v: split the ring, make two mirrors:
 *
 *     x-u-v-y   => x-a  b-y      => x-a|  |b-y
 *     x-a-b-y
 *
 * (c) insertion: a==b (a is a singleton):
 *     a*  x-u-v-z    => x-u-a-v-z
 *
 * (d) make a pair: a==b, u==v:
 *     a*  u*   => a-u*
 *
 * (e) nop: b==u: (same for a==v) = reversal of singleton
 *
 *     x-a-b-v-z  => x-a  b  v-z   => x-a-b-v-z
 *
 * a and b must be neighbours (including a singleton).
 * u and v must be neighbours (including a singleton).
 * If a == u, then b must not be equal to v unless a-b is a pair
 * If b == v, then a must not be equal to u unless a-b is a pair
 *
 * Runtime: O(1).
 */
extern void cp_ring_rewire(
    cp_ring_t *a,
    cp_ring_t *b,
    cp_ring_t *u,
    cp_ring_t *v);

/**
 * Internal: Swap two nodes a and b.
 *
 * This is, maybe surprisingly, the most complex operation of this
 * library and will not be done inline.
 *
 * na must be a neighbour of a (it does not matter which one).
 * nb must be a neighbour of b (it does not matter which one).
 *
 * a is allowed to be equal to na.
 * b is allowed to be equal to nb.
 *
 * Runtime: O(1).
 */
extern void cp_ring_swap2(
    cp_ring_t *a,
    cp_ring_t *na,
    cp_ring_t *b,
    cp_ring_t *nb);

/**
 * Internal: Set both neighbours
 */
static inline void __cp_ring_set_both(
    cp_ring_t *u,
    cp_ring_t *a,
    cp_ring_t *b)
{
    u->n[0] = a;
    u->n[1] = b;
}

/**
 * Internal: Replace one neighbour
 */
static inline void __cp_ring_replace(
    cp_ring_t *a,
    cp_ring_t *o,
    cp_ring_t *n)
{
    assert((a->n[0] == o) || (a->n[1] == o));
    a->n[a->n[1] == o] = n;
}

/**
 * Internal: Get the reference to a neighbour without any checks.
 */
static inline size_t __cp_ring_ref_raw(
    cp_ring_t const *a,
    cp_ring_t const *u)
{
    return a->n[1] == u;
}

/**
 * Internal: Set a neighbour */
static inline void __cp_ring_set(
    cp_ring_t *a,
    size_t i,
    cp_ring_t *u)
{
    a->n[i] = u;
}

/**
 * Internal: Get other edge
 */
static inline cp_ring_t *__cp_ring_get_buddy(
    cp_ring_t const *a __unused,
    size_t i)
{
    return a->n[!i];
}

/**
 * Internal: Get the reference to a neighbour.
 *
 * This can be used to access the corresponding neighbour using
 * __cp_ring_(get|set)_neigh().
 */
static inline size_t __cp_ring_ref(
    cp_ring_t const *a,
    cp_ring_t const *u)
{
    size_t i = __cp_ring_ref_raw(a, u);
    assert((a->n[i] == u) && "expected neighbours");
    assert(((u->n[0] == a) || (u->n[1] == a)) && "expected neighbours");
    return i;
}

/**
 * Initialise a self-circular ring.
 */
static inline void cp_ring_init(
    cp_ring_t *c)
{
    __cp_ring_set_both(c, c, c);
}

/**
 * Get the next node after the edge a-b.
 *
 * I.e., for a sequence a-b-c, when paramters a,b are
 * passed, return c.
 *
 * To get the previous one, use next(b,a).
 * (For convenience, there is also cp_ring_prev()).
 *
 * a and b must be neighbours.
 * b must not be an end (use cp_ring_try_next instead).
 *
 * Runtime: O(1).
 */
static inline cp_ring_t *cp_ring_next(
    cp_ring_t const *a,
    cp_ring_t const *b)
{
    return __cp_ring_get_buddy(b, __cp_ring_ref(b,a));
}

/**
 * Get the next node after the edge a-b.
 *
 * I.e., for a sequence a-b-c, when paramters b,c are
 * passed, return a.
 *
 * See cp_ring_next().
 *
 * a and b must be neighbours.
 * a must not be an end (see cp_ring_try_prev() instead).
 *
 * Runtime: O(1).
 */
static inline cp_ring_t *cp_ring_prev(
    cp_ring_t const *a,
    cp_ring_t const *b)
{
    return cp_ring_next(b,a);
}

/**
 * Make a pair of two rings.
 *
 * Both a and b must be singleton.
 *
 * Runtime: O(1).
 */
static inline void cp_ring_pair(
    cp_ring_t *a,
    cp_ring_t *b)
{
    assert((cp_ring_next(a,a) == a) && "expected a singleton");
    assert((cp_ring_next(b,b) == b) && "expected a singleton");
    cp_ring_rewire(a, a, b, b);
}

/**
 * Insert a node between two others.
 *
 * This needs an edge a-c and a singleton b and will
 * connect them into a-b-c.  The order of parameters
 * is the desired order of the nodes in the resulting
 * ring.
 *
 * a and c must be neighbours.
 * b must be singleton.
 *
 * Runtime: O(1).
 */
static inline void cp_ring_insert_between(
    cp_ring_t *a,
    cp_ring_t *b,
    cp_ring_t *c)
{
    cp_ring_rewire(b,b,a,c);
}

/**
 * Insert a node after two others.
 *
 * This inserts a node c after an edge a-b, i.e.,
 * it yields a-b-c.  The order of parameters
 * is the desired order of the nodes in the resulting
 * ring.
 *
 * a and b must be neighbours.
 * c must be singleton.
 *
 * Runtime: O(1).
 */
static inline void cp_ring_insert_after(
    cp_ring_t *a,
    cp_ring_t *b,
    cp_ring_t *c)
{
    cp_ring_insert_between(b, c, cp_ring_next(a,b));
}

/**
 * Insert a node before two others.
 *
 * This inserts a node c before an edge a-b, i.e.,
 * it yields c-a-b.  The order of parameters
 * is the desired order of the nodes in the resulting
 * ring.
 *
 * a and b must be neighbours.
 * c must be singleton.
 *
 * Runtime: O(1).
 */
static inline void cp_ring_insert_before(
    cp_ring_t *c,
    cp_ring_t *a,
    cp_ring_t *b)
{
    cp_ring_insert_between(cp_ring_prev(a,b), c, a);
}

/**
 * Swap a pair of nodes.
 *
 * If a is equal to b, this is a NOP.
 *
 * Runtime: O(1).
 */
static inline void cp_ring_swap_pair(
    cp_ring_t *a,
    cp_ring_t *b)
{
    cp_ring_rewire(cp_ring_prev(a,b), a, b, cp_ring_next(a,b));
}

/**
 * Internal: Remove a node from a ring.
 *
 * The extracted node becomes a singleton.
 *
 * na must be a neighbour of a, it does not matter which one.
 *
 * Runtime: O(1).
 */
static inline void cp_ring_remove2(
    cp_ring_t *a,
    cp_ring_t *na)
{
    cp_ring_rewire(a, na, a, cp_ring_next(na, a));
}

/**
 * Remove a node from a ring.
 *
 * The extracted node becomes a singleton.
 *
 * Runtime: O(1).
 *
 * Note: This prototype would not work for XOR lists.
 */
static inline void cp_ring_remove(
    cp_ring_t *a)
{
    cp_ring_rewire(a, a->n[0], a, a->n[1]);
}

/**
 * Swap two nodes a and b.
 *
 * This is, maybe surprisingly, the most complex operation of this
 * library and will not be done inline.
 *
 * Runtime: O(1).
 *
 * Note: This prototype would not work for XOR lists.
 */
static inline void cp_ring_swap(
    cp_ring_t *a,
    cp_ring_t *b)
{
    cp_ring_swap2(a, a->n[0], b, b->n[0]);
}

/**
 * Whether a ring is a singleton
 *
 * Runtime: O(1)
 */
static inline bool cp_ring_is_singleton(
    cp_ring_t *a)
{
    return (a->n[0] == a) && (a->n[1] == a);
}

/**
 * Whether two nodes are a pair.
 *
 * Runtime: O(1)
 */
static inline bool cp_ring_is_pair(
    cp_ring_t *a,
    cp_ring_t *b)
{
    return
        (a->n[0] == b) &&
        (a->n[1] == b) &&
        (b->n[0] == a) &&
        (b->n[1] == a);
}

/**
 * Whether a node is part of a pair.
 *
 * Runtime: O(1)
 */
static inline bool cp_ring_is_moiety(
    cp_ring_t *a)
{
    return cp_ring_is_pair(a, a->n[0]);
}

/**
 * Whether a ring is an end or mirror (including singletons).
 *
 * Note that all singleton and pair nodes are ends.  Only
 * in rings of length 3 or more mirrors and non-mirrors can
 * be distinguished.
 *
 * Runtime: O(1)
 */
static inline bool cp_ring_is_end(
    cp_ring_t const *a)
{
    return (a->n[0] == a->n[1]);
}

/**
 * Get one of the two neighbours of the node.
 *
 * Note: it is unspecified which one you'll get -- the ring is unordered.
 *
 * Runtime: O(1)
 */
static inline cp_ring_t *cp_ring_step(
    cp_ring_t *a,
    unsigned i)
{
    assert(i <= 1);
    return a->n[i];
}

#endif /* __CP_RING_H */
