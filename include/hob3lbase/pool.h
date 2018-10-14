/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * Allocator for temporary objects.
 *
 * This allocates large blocks, has a very fast 'alloc', but not free.
 * Deallocation can only be done by destructing the whole allocator.
 */

#ifndef __CP_POOL_H
#define __CP_POOL_H

#include <hob3lbase/def.h>
#include <hob3lbase/pool_tam.h>

#define CP_POOL_NEW_ARR(p, x, n) \
    ((__typeof__(x)*)cp_pool_calloc(CP_FILE, CP_LINE, p, n, sizeof(x), cp_alignof(x)))

#define CP_POOL_NEW(p, x)           CP_POOL_NEW_ARR(p, x, 1)
#define CP_POOL_CALLOC_ARR(p, x, n) ((x) = CP_POOL_NEW_ARR(p, *(x), n))
#define CP_POOL_CALLOC(p, x)        CP_POOL_CALLOC_ARR(p, x, 1)

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
    cp_pool_t *a);

/**
 * Throw away all blocks (and hence, all allocated objects) of the allocator.
 */
extern void cp_pool_fini(
    cp_pool_t *a);

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
    size_t size,
    size_t align);

/**
 * Initialises a new allocator together with a first block of memory to
 * allocate from.
 */
static inline void cp_pool_init(
    /**
     * Pointer to a pool for initialisation.
     */
    cp_pool_t *pool,

    /**
     * Block size of each block to allocate.  By making this large,
     * less overhead occurs if a large object does not fit anymore.
     * If this is small, there is less waste if a block is not used
     * completely.
     *
     * Just pass 0 for a sensible default.
     */
    size_t block_size)
{
    CP_ZERO(pool);
    pool->block_size = block_size;
}

#endif /*__CP_POOL_H */
