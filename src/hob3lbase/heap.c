/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <hob3lbase/arith.h>
#include <hob3lbase/heap.h>

#define HEAP_PARENT(POS) (((POS) - 1) / 2) // 2->0, 1->0
#define HEAP_CHILD0(POS) (((POS) * 2) + 1) // 0->1

static inline void cp_heap_swap_(
    cp_heap_t *vec,
    size_t **p,
    size_t **q)
{
    CP_SWAP(p, q);
    **q = CP_MONUS(q, vec->data);
    **p = CP_MONUS(p, vec->data);
}

static void cp_heap_up_(
    cp_heap_t *vec,
    int (*cmp)(size_t const *, size_t const *, void *user),
    void *user,
    size_t pos)
{
    assert(vec != NULL);
    assert(pos < vec->size);
    size_t **q = cp_v_nth_ptr(vec, pos);
    while (pos > 0) {
        pos = HEAP_PARENT(pos);
        size_t **p = cp_v_nth_ptr(vec, pos);
        if (cmp(*p, *q, user) <= 0) {
            break;
        }

        cp_heap_swap_(vec, p, q);
        q = p;
    }
}

static void cp_heap_down_(
    cp_heap_t *vec,
    int (*cmp)(size_t const *, size_t const *, void *user),
    void *user,
    size_t pos)
{
    assert(vec != NULL);
    assert(pos < vec->size);
    size_t size = vec->size;
    size_t **q = cp_v_nth_ptr(vec, pos);
    for (;;) {
        /*   2                -> good
         * 4   7
         *
         *   9         4      -> proceed (swap with smaller child)
         * 4   7     9   7
         */
        pos = HEAP_CHILD0(pos);
        if (pos >= size) {
            break;
        }
        size_t **c = cp_v_nth_ptr(vec, pos);

        /* check whether second child is smaller */
        if (pos + 1 < size) {
            size_t **d = cp_v_nth_ptr(vec, pos + 1);
            if (cmp(*d, *c, user) < 0) {
                c = d;
                pos++;
            }
        }

        /* check whether parent is smaller than smallest child */
        if (cmp(*q, *c, user) <= 0) {
            break;
        }

        cp_heap_swap_(vec, c, q);
        q = c;
    }
}

/**
 * Internal: make a heap.
 */
extern void cp_heap_make_(
    cp_heap_t *vec,
    int (*cmp)(size_t const *, size_t const *, void *user),
    void *user)
{
    /* set initial indices */
    for (cp_v_each(i, vec)) {
         *vec->data[i] = i;
    }

    /* 0 or 1 sized vectors are valid heaps */
    size_t size = vec->size;
    if (size < 2) {
        return;
    }

    /* establish heap structure */
    for (vec->size = 2; vec->size <= size; vec->size++) {
        cp_heap_up_(vec, cmp, user, vec->size-1);
    }
}

/**
 * This can be used to update the position in the heap after a heap
 * element's priority has changed.
 */
extern void cp_heap_update(
    cp_heap_t *heap,
    int (*cmp)(size_t const *a, size_t const *b, void *user),
    void *user,
    size_t idx)
{
    cp_heap_up_  (heap, cmp, user, idx);
    cp_heap_down_(heap, cmp, user, idx);
}

/**
 * Remove an element from the heap.
 *
 * If idx is 0, this removes the minimum.  It is then similar to
 * cp_heap_extract(), except that the heap must not be empty.
 */
extern size_t *cp_heap_remove(
    cp_heap_t *heap,
    int (*cmp)(size_t const *a, size_t const *b, void *user),
    void *user,
    size_t idx)
{
    assert(idx < heap->size);

    size_t **q = cp_v_nth_ptr(heap, heap->size-1);
    heap->size--;
    if (heap->size != idx) {
        assert(heap->size > 0);
        size_t **p = cp_v_nth_ptr(heap, idx);
        cp_heap_swap_(heap, p, q);
        cp_heap_update(heap, cmp, user, idx);
    }

    **q = CP_HEAP_NO_IDX;
    return *q;
}

/**
 * Replace an element in the heap by another one.
 *
 * This is faster than first removing and then inserting,
 * as the heap needs to be updated only once instead of twice.
 *
 * If the same element is extracted as is inserted, then this
 * is like cp_heap_update() returns NULL.
 *
 * If idx is 0, this replaces the minimum.
 *
 * If idx is CP_HEAP_NO_IDX, then this is like cp_heap_insert(),
 * and it returns NULL.
 *
 * If x is NULL, then this is like cp_heap_remove().
 *
 * This can be used to insert or update, if invoked as '..., *x, x)'
 * is passed, if the element is initialised properly with
 * CP_HEAP_NO_IDX.
 */
extern size_t *cp_heap_replace(
    cp_heap_t *heap,
    int (*cmp)(size_t const *a, size_t const *b, void *user),
    void *user,
    size_t idx,
    size_t *x)
{
    if (idx == CP_HEAP_NO_IDX) {
        cp_heap_insert(heap, cmp, user, x);
        return NULL;
    }
    assert(idx < heap->size);
    if (x == NULL) {
        return cp_heap_remove(heap, cmp, user, idx);
    }

    size_t **q = cp_v_nth_ptr(heap, idx);
    size_t *r = *q;
    if (r == x) {
        cp_heap_update(heap, cmp, user, idx);
        return NULL;
    }
    assert(*r == idx);

    *q = x;
    *x = idx;
    *r = CP_HEAP_NO_IDX;
    cp_heap_update(heap, cmp, user, idx);

    assert(*r == CP_HEAP_NO_IDX);
    return r;
}

/**
 * Extract the minimum element from the heap.
 * If the heap is empty, this returns NULL.
 */
extern size_t *cp_heap_extract(
    cp_heap_t *heap,
    int (*cmp)(size_t const *, size_t const *, void *user),
    void *user)
{
    if (heap->size == 0) {
        return NULL;
    }
    return cp_heap_remove(heap, cmp, user, 0);
}

/**
 * Insert a new element into the heap.
 * The insertion is done via a pointer to an embedded 'size_t', which will
 * held current for the position within the heap so that direct access
 * to the heap is possible for each element in the heap.
 */
extern void cp_heap_insert(
    cp_heap_t *heap,
    int (*cmp)(size_t const *a, size_t const *b, void *user),
    void *user,
    size_t *x)
{
    size_t **q = cp_v_push(heap, x);
    size_t idx = heap->size - 1;
    **q = idx;
    cp_heap_update(heap, cmp, user, idx);
}

/**
 * The minimum element of the heap.
 * If the heap is empty, this returns NULL.
 */
extern size_t *cp_heap_min(
    cp_heap_t *heap)
{
    assert(heap != NULL);
    if (heap->size == 0) {
        return NULL;
    }
    return heap->data[0];
}
