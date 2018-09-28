/* -*- Mode: C -*- */

#include <cpmat/def.h>
#include <cpmat/list.h>
#include <cpmat/panic.h>
#include <cpmat/alloc.h>
#include <cpmat/pool.h>

/**
 * Align block to 4k pages
 */
#define BLOCK_ALIGN 0x1000

/**
 * Default size of allocation block */
#define BLOCK_SIZE_DEFAULT (1 * 1024 * 1024)

struct cp_pool_block {
    /**
     * The allocator blocks are in a ring.  This is the next one.
     */
    cp_pool_block_t *prev;

    /**
     *The allocator blocks are in a ring.  This is the prev one.
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

static void block_clear(
    cp_pool_block_t *b)
{
    char *heap_end = b->heap + b->heap_size;
    if (b->brk != heap_end) {
        memset(b->heap, 0, b->heap_size);
        b->brk = b->heap + b->heap_size;
    }
}

extern void cp_pool_clear(
    cp_pool_t *a)
{
    if (a->cur != NULL) {
        block_clear(a->cur);
        for (cp_list_each(i, a->cur)) {
            block_clear(i);
        }
    }
}

extern void cp_pool_fini(
    cp_pool_t *a)
{
    if (a->cur != NULL) {
        cp_pool_block_t *p = NULL;
        for (cp_list_each(i, a->cur)) {
            CP_FREE(p);
            p = i;
        }
        CP_FREE(p);
        CP_FREE(a->cur);
    }
}

static cp_pool_block_t *block_alloc(
    char const *file,
    int line,
    size_t block_size)
{
    block_size = cp_align_up(block_size, BLOCK_ALIGN);

    cp_pool_block_t *r = cp_calloc(file, line, block_size, 1);
    assert(block_size > sizeof(*r));

    r->heap_size = block_size - sizeof(*r);
    r->brk = r->heap + r->heap_size;
    cp_list_init(r);
    return r;
}

static void *try_block_calloc(
    char const *file,
    int line,
    cp_pool_block_t *a,
    size_t nmemb,
    size_t size,
    size_t align)
{
    assert((size > 0) && "Objects of size 0 are not supported");
    assert(nmemb > 0);

    if (nmemb > (a->heap_size / size)) {
        cp_panic(file, line, "Out of memory: large allocation: %"_Pz"u * %"_Pz"u > %"_Pz"u",
            nmemb, size, a->heap_size);
    }

    if (CP_PTRDIFF(a->brk, a->heap) < size) {
        return NULL;
    }
    a->brk -= size;

    size_t align_diff = cp_align_down_diff((size_t)a->brk, align);
    if (CP_PTRDIFF(a->brk, a->heap) < align_diff) {
        return NULL;
    }
    a->brk -= align_diff;

    return a->brk;
}

extern void *cp_pool_calloc(
    char const *file,
    int line,
    cp_pool_t *pool,
    size_t nmemb,
    size_t size,
    size_t align)
{
    if (nmemb == 0) {
        return NULL;
    }

    if (pool->cur != NULL) {
        void *r = try_block_calloc(file, line, pool->cur, nmemb, size, align);
        if (r != NULL) {
            return r;
        }
    }

    if (pool->block_size == 0) {
        pool->block_size = BLOCK_SIZE_DEFAULT;
    }

    cp_pool_block_t *b = block_alloc(file, line, pool->block_size);
    if (pool->cur != NULL) {
        assert(b->heap_size == pool->cur->heap_size);
        cp_list_insert(b, pool->cur);
    }
    pool->cur = b;

    void *r = try_block_calloc(file, line, pool->cur, nmemb, size, align);
    assert(r != NULL);

    return r;
}
