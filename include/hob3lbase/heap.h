/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_HEAP_H_
#define CP_HEAP_H_

#include <stddef.h>
#include <hob3lbase/vec.h>

/**
 * The index value representing that the element is
 * not in the heap.  This can be used to initialise
 * the index of an element before putting it in
 * the heap.
 * This library resets the idx to this value when an
 * element is removed.
 */
#define CP_HEAP_NO_IDX  ((size_t)-1)

/**
 * The heap type.
 */
typedef CP_VEC_T(size_t*) cp_heap_t;

/**
 * Internal: make a heap.
 */
extern void cp_heap_make_(
    cp_heap_t *vec,
    int (*cmp)(size_t const *, size_t const *, void *user),
    void *user);

/**
 * This can be used to update the position in the heap after a heap
 * element's priority has changed.
 */
extern void cp_heap_update(
    cp_heap_t *heap,
    int (*cmp)(size_t const *a, size_t const *b, void *user),
    void *user,
    size_t idx);

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
    size_t idx);

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
    size_t *x);

/**
 * Extract the minimum element from the heap.
 * If the heap is empty, this returns NULL.
 */
extern size_t *cp_heap_extract(
    cp_heap_t *heap,
    int (*cmp)(size_t const *, size_t const *, void *user),
    void *user);

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
    size_t *x);

/**
 * The minimum element of the heap.
 * If the heap is empty, this returns NULL.
 */
extern size_t *cp_heap_min(
    cp_heap_t *heap);

/**
 * Whether the element is part of a heap */
static inline bool cp_heap_is_member(
    size_t *x)
{
    return *x != CP_HEAP_NO_IDX;
}

#endif /* CP_HEAP_H_ */
