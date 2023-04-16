/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * Allocator for temporary objects.
 *
 * This allocates large blocks, has a very fast 'alloc', but not free.
 * Deallocation can only be done by destructing the whole allocator.
 *
 * Alternatively to CP_POOL_* API, the macros from alloc.h with the
 * _ALLOC suffix can be used with pools by passing 'pool->alloc' as the
 * memory allocator pointer.  This also works for 'vec.h' stuff (with
 * _alloc suffix functions).
 */

#ifndef CP_POOL_H_
#define CP_POOL_H_

#include <hob3lbase/base-def.h>
#include <hob3lbase/pool_tam.h>

#define CP_POOL_NEW_ARR_PLUS(p, x, xp, n) \
    ((__typeof__(x)*)cp_pool_calloc(CP_FILE, CP_LINE, p, n, \
        sizeof(x) + (xp), \
        cp_alignof_minmax(x, xp, cp_alignof(max_align_t))))

#define CP_POOL_NEW_ARR(p, x, n)   CP_POOL_NEW_ARR_PLUS(p, x, 0,  n)
#define CP_POOL_NEW_PLUS(p, x, xp) CP_POOL_NEW_ARR_PLUS(p, x, xp, 1)

#define CP_POOL_NEW(p, x)          CP_POOL_NEW_PLUS(p, x, 0)

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
    size_t size1,
    size_t align);

extern cp_alloc_t const cp_alloc_pool_;

/**
 * Initialises a new allocator together with a first block of memory to
 * allocate from.
 */
static inline void cp_pool_init(
    /**
     * Pointer to a pool for initialisation.
     */
    cp_pool_t *pool)
{
    CP_ZERO(pool);
    *pool->alloc = cp_alloc_pool_;
}

#endif /*CP_POOL_H_ */
