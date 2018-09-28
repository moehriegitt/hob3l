/* -*- Mode: C -*- */

#include <cpmat/list.h>

#define NEXT(n) (*get_ptr(n, offset_next))
#define PREV(n) (*get_ptr(n, offset_prev))

static void **get_ptr(void *a, size_t o)
{
    return (void**)((char*)a + o);
}

extern void __cp_list_swap(
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
        __cp_list_swap(b, a, offset_next, offset_prev);
        return;
    }

    if (nb == a) {
        if (na == b) {
            return;
        }
        __cp_list_swap(b, a, offset_next, offset_prev);
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
