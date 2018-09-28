/* -*- Mode: C -*- */

#include <cpmat/ring.h>

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
    __cp_ring_set_both(a, pa, pa);
    __cp_ring_set_both(b, nb, nb);
}

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

         if (cp_ring_is_mirr(b)) {
             CP_SWAP(&a, &b);
             CP_SWAP(&u, &v);
         }
         /* x-b-a|
          */
         cp_ring_t *x = cp_ring_prev(b,a);
         if (x != a) {
             __cp_ring_set_both(a, a, a);
             __cp_ring_set_both(b, x, x);
             return;
         }
    }

    if (a == b) {
        __cp_ring_set_both(a, u, v);
    }
    else {
        size_t ia = __cp_ring_ref(a,b);
        size_t ib = __cp_ring_ref(b,a);
        __cp_ring_set(a, ia, u);
        __cp_ring_set(b, ib, v);
    }

    if (u == v) {
        __cp_ring_set_both(u, a, b);
    }
    else {
        size_t iu = __cp_ring_ref(u,v);
        size_t iv = __cp_ring_ref(v,u);
        __cp_ring_set(u, iu, a);
        __cp_ring_set(v, iv, b);
    }
}

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
    __cp_ring_replace(pa, a, b);
    __cp_ring_replace(nb, b, a);

    /* adjacent */
    if (na == b) {
        assert(pb == a);
        /* If pa == nb, the following code is a NOP. */
        /* And that's correct: the direction is not important, so
         * a-b-c* is equal to a-c-b* */

        /* BEFORE: pa->a->b->nb */
        /* AFTER:  pa->b->a->nb */
        __cp_ring_set_both(a, b, nb);
        __cp_ring_set_both(b, a, pa);
        return;
    }

    /* generic */

    /* The following code also works for the special case nb == b == pb */

    /* BEFORE: pa->a->na  b */
    /* AFTER:  pa->b->na  a */

    /* BEFORE: pa->a->na  pb->b->nb */
    /* AFTER:  pa->b->na  pb->a->nb */
    __cp_ring_replace(na, a, b);
    __cp_ring_replace(pb, b, a);
    __cp_ring_set_both(a, pb, nb);
    __cp_ring_set_both(b, pa, na);
}
