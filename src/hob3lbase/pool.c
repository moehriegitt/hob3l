/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/base-def.h>
#include <hob3lbase/panic.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/pool.h>
#include <hob3lbase/arith.h>

/**
 * Align block to 4k pages
 */
#define BLOCK_ALIGN 0x1000

/**
 * Default size of allocation block */
#define BLOCK_SIZE_DEFAULT (1 * 1024 * 1024)

struct cp_pool_block {
    /**
     * Next allocator block in the lists we maintain.
     */
    cp_pool_block_t *next;

    /**
     * Number of objects in head[].
     */
    size_t heap_size;

    /**
     * Brk of heap: now elements are allocated from here.
     *
     * The top is at heap + heap_size.
     * The bottom is at heap[].
     */
    char *brk;

    /**
     * The memory to allocate from.  All memory between heap..brk will be
     * kept zero to make allocation O(1).
     */
    char heap[];
};

static void block_push(
    cp_pool_block_list_t *list,
    cp_pool_block_t *b)
{
    assert(b->next == NULL);
    b->next = list->head;
    list->head = b;
}

static cp_pool_block_t *block_pop(
    cp_pool_block_list_t *list)
{
    cp_pool_block_t *b = list->head;
    if (b != NULL) {
        list->head = b->next;
        b->next = NULL;
    }
    return b;
}

static void block_clear(
    cp_pool_block_t *b)
{
    char *heap_end = b->heap + b->heap_size;
    if (b->brk != heap_end) {
        memset(b->heap, 0, b->heap_size);
        b->brk = b->heap + b->heap_size;
    }
}

/**
 * Empty the allocator, i.e., throw away all content.
 *
 * This does not deallocate any block, it only clears the allocator
 * from all objects inside so that the whole allocated area can be
 * used again for more allocations.
 *
 * This also clears memory so that the cp_alloc() returns zeroed
 * objects again.
 */
extern void cp_pool_clear(
    cp_pool_t *a)
{
    for(;;) {
        cp_pool_block_t *b = block_pop(&a->used);
        if (b == NULL) {
            break;
        }
        block_clear(b);
        block_push(&a->free, b);
    }
}

static void block_list_fini(
    cp_pool_block_list_t *list)
{
    for(;;) {
        cp_pool_block_t *b = block_pop(list);
        if (b == NULL) {
            break;
        }
        CP_DELETE(b);
    }
}

/**
 * Throw away all blocks (and hence, all allocated objects) of the allocator.
 */
extern void cp_pool_fini(
    cp_pool_t *a)
{
    block_list_fini(&a->used);
    block_list_fini(&a->free);
}

static cp_pool_block_t *block_next(
    char const *file,
    int line,
    cp_pool_t *pool,
    size_t block_size)
{
    cp_pool_block_t *b = pool->free.head;
    if ((b != NULL) &&
        (b->heap_size < (block_size - sizeof(*b))))
    {
        /* discard free blocks: they have become too small */
        block_list_fini(&pool->free);
    }

    /* try to get free block */
    b = block_pop(&pool->free);
    if (b != NULL) {
        assert(b->next == NULL);
        return b;
    }

    /* allocate new block */
    b = cp_calloc_(file, line, &cp_alloc_global, block_size, 1);
    assert(block_size > sizeof(*b));

    b->heap_size = block_size - sizeof(*b);
    b->brk = b->heap + b->heap_size;
    assert(b->next == NULL);
    return b;
}

static void *try_block_calloc(
    cp_pool_block_t *a,
    size_t nmemb,
    size_t size1,
    size_t align)
{
    assert((size1 > 0) && "Objects of size 0 are not supported");
    assert(nmemb > 0);

    if (a == NULL) {
        return NULL;
    }
    if (nmemb > (a->heap_size / size1)) {
        return NULL;
    }

    size_t size = nmemb * size1;

    if (CP_MONUS(a->brk, a->heap) < size) {
        return NULL;
    }
    char *new_brk = a->brk - size;

    size_t align_diff = cp_align_down_diff((size_t)new_brk, align);
    if (CP_MONUS(new_brk, a->heap) < align_diff) {
        return NULL;
    }
    a->brk = new_brk - align_diff;

    assert(cp_mem_is0(a->brk, size));
    return a->brk;
}

/**
 * Allocate an array of elements from the allocator.
 *
 * If you don't know about the alignment, just pass 0 -- the
 * alignment will be derived from size by using the largest
 * power-of-2 factor in size.  Note: for this to work, it is vital
 * not to mix the nmemb and the align parameters!
 *
 * The returned memory is always zeroed.
 *
 * If nmemb is 0, this returns NULL.  I.e., NULL is not an indication
 * of an error, just an indication of an empty array that must not be
 * accessed.
 *
 * If nmemb > 0, this never returns NULL, but will assert fail in case
 * of it runs out of memory.
 *
 * size must not be 0.
 */
extern void *cp_pool_calloc(
    char const *file,
    int line,
    cp_pool_t *pool,
    size_t nmemb,
    size_t size1,
    size_t align)
{
    assert(pool != NULL);
    assert(size1 != 0);

    if (nmemb == 0) {
        return NULL;
    }

    for (size_t try = 0; try < 3; try++) {
        void *r = try_block_calloc(pool->used.head, nmemb, size1, align);
        if (r != NULL) {
            return r;
        }

        if (pool->block_size == 0) {
            pool->block_size = BLOCK_SIZE_DEFAULT;
        }

        /* possibly increase block size for next block */
        while ((10 * nmemb) > (pool->block_size / size1)) {
            size_t n = pool->block_size * 2;
            if (n < pool->block_size) {
                break;
            }
            pool->block_size = n;
        }

        assert(pool->block_size > 0);
        pool->block_size = cp_align_up(pool->block_size, BLOCK_ALIGN);

        /* get new block */
        cp_pool_block_t *b = block_next(file, line, pool, pool->block_size);
        block_push(&pool->used, b);

        assert(pool->used.head != NULL);
    }
    CP_DIE("allocator is broken");
}

static void *pool_calloc(
    cp_alloc_t *m,
    size_t a, size_t b)
{
    cp_pool_t *pool = CP_BOX_OF(m, *pool, alloc);
    return cp_pool_calloc(CP_FILE, CP_LINE, pool, a, b,
        cp_size_align(b | cp_alignof(max_align_t)));
}

static void *pool_malloc(cp_alloc_t *m, size_t a, size_t b)
{
    return pool_calloc(m, a, b);
}

static void pool_free(
    cp_alloc_t *m CP_UNUSED,
    void *p CP_UNUSED)
{
}

static void *pool_remalloc(
    cp_alloc_t *m,
    void *p,
    size_t ao, size_t an, size_t b)
{
    if (an <= ao) {
        return p;
    }
    void *q = pool_calloc(m, an, b);
    if (q != NULL) {
        size_t osz = ao * b;
        if (osz > 0) {
            memcpy(q, p, osz);
        }
    }
    return q;
}

static void *pool_recalloc(
    cp_alloc_t *m,
    void *p,
    size_t ao, size_t an, size_t b)
{
    if (an <= ao) {
        return p;
    }
    void *q = pool_calloc(m, an, b);
    if (q != NULL) {
        size_t osz = ao * b;
        if (osz > 0) {
            memcpy(q, p, osz);
        }
        size_t nsz = an * b;
        memset((char*)q + osz, 0, nsz - osz);
    }
    return q;
}

cp_alloc_t const cp_alloc_pool_ = {
    .x_malloc   = pool_malloc,
    .x_calloc   = pool_calloc,
    .x_remalloc = pool_remalloc,
    .x_recalloc = pool_recalloc,
    .x_free     = pool_free,
};
