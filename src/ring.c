/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/ring.h>

/**
 * Cut a ring at a given pair, i.e., make each of the nodes an end.
 *
 * Note that we introduce no NULL or mirrors, but self-loops.
 *
 * Runtime: O(1)
 */
extern void cp_ring_cut(
    cp_ring_t *a,
    cp_ring_t *b)
{
    cp_ring_t *pa = cp_ring_prev(a,b);
    cp_ring_t *nb = cp_ring_next(a,b);
    if (pa == b) {
        pa = a;
    }
    if (nb == a) {
        nb = b;
    }
    cp_ring_set_both_(a, pa, pa);
    cp_ring_set_both_(b, nb, nb);
}

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
    cp_ring_t *b)
{
    assert((a->n[0] == a->n[1]) && "expected a mirror node");
    assert((b->n[0] == b->n[0]) && "expected a mirror node");
    a->n[0] = b;
    b->n[0] = a;
    if (a->n[1] == a) {
        a->n[1] = b;
    }
    if (b->n[1] == b) {
        b->n[1] = a;
    }
}

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
    cp_ring_t *v)
{
    if ((a == u) && (b == v)) {
        /* Split pair into singleton is allowed (and correctly done by
         * the code below).
         *
         *   a-b*  =>  a* b*
         *   u-v*
         */

        /* Note that this can also not remove non-trivial mirror nodes,
         * because it is not described by the parameters ('connect a with u
         * and b with v'):
         *
         *   x-a-b|  =>  x-?   a*  b*
         *     u-v
         */

         if (cp_ring_is_end(b)) {
             CP_SWAP(&a, &b);
             CP_SWAP(&u, &v);
         }
         /* x-b-a|
          */
         cp_ring_t *x = cp_ring_prev(b,a);
         if (x != a) {
             cp_ring_set_both_(a, a, a);
             cp_ring_set_both_(b, x, x);
             return;
         }
    }

    if (a == b) {
        cp_ring_set_both_(a, u, v);
    }
    else {
        size_t ia = cp_ring_ref_(a,b);
        size_t ib = cp_ring_ref_(b,a);
        cp_ring_set_(a, ia, u);
        cp_ring_set_(b, ib, v);
    }

    if (u == v) {
        cp_ring_set_both_(u, a, b);
    }
    else {
        size_t iu = cp_ring_ref_(u,v);
        size_t iv = cp_ring_ref_(v,u);
        cp_ring_set_(u, iu, a);
        cp_ring_set_(v, iv, b);
    }
}

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
    cp_ring_t *nb)
{
    /* trivial */
    if (a == b) {
        return;
    }

    /* symmetry */
    if (na == a) {
        if (nb == b) {
            return;
        }
        CP_SWAP(&a,  &b);
        CP_SWAP(&na, &nb);
    }

    cp_ring_t *pa = cp_ring_prev(a, na);
    cp_ring_t *pb = cp_ring_prev(b, nb);
    if (nb == a) {
        if (na == b) {
            if (na == pa) {
                return;
            }
            CP_SWAP(&na, &pa);
        }
        CP_SWAP(&a,  &b);
        CP_SWAP(&na, &nb);
    }

    /* prepare */
    assert(na != a);
    assert(nb != a);

    /* swap a<->b in outer neighbours */
    cp_ring_replace_(pa, a, b);
    cp_ring_replace_(nb, b, a);

    /* adjacent */
    if (na == b) {
        assert(pb == a);
        /* If pa == nb, the following code is a NOP. */
        /* And that's correct: the direction is not important, so
         * a-b-c* is equal to a-c-b* */

        /* BEFORE: pa->a->b->nb */
        /* AFTER:  pa->b->a->nb */
        cp_ring_set_both_(a, b, nb);
        cp_ring_set_both_(b, a, pa);
        return;
    }

    /* generic */

    /* The following code also works for the special case nb == b == pb */

    /* BEFORE: pa->a->na  b */
    /* AFTER:  pa->b->na  a */

    /* BEFORE: pa->a->na  pb->b->nb */
    /* AFTER:  pa->b->na  pb->a->nb */
    cp_ring_replace_(na, a, b);
    cp_ring_replace_(pb, b, a);
    cp_ring_set_both_(a, pb, nb);
    cp_ring_set_both_(b, pa, na);
}
