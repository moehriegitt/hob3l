/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <hob3lbase/arith.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/panic.h>
#include <hob3lbase/alloc.h>

static void v_grow(
    cp_alloc_t *m,
    cp_v_t *vec,
    size_t esz,
    size_t new_size)
{
    if (vec->alloc >= new_size) {
        return;
    }

    size_t new_alloc = vec->alloc;
    if (new_alloc < cp_v_min_alloc_()) {
        new_alloc = cp_v_min_alloc_();
    }

    while (new_alloc < new_size) {
        new_alloc *= 2;
        if (new_alloc > cp_v_max_size_(esz)) {
            cp_panic(NULL, 0, "Out of memory in cp_v_inflate_()\n");
        }
        assert(new_alloc > vec->alloc);
    }

    vec->data = cp_remalloc(m, vec->data, vec->alloc, new_alloc, esz);
    vec->alloc = new_alloc;
}

/**
 * Internal: Shallow delete a vector: remove all sub-structures, but not the
 * elements and not the pointer itself.  The structure will then
 * be a clean, empty vector.
 *
 * This function allows v == NULL.
 */
extern void cp_v_fini_(
    cp_alloc_t *m,
    cp_v_t *vec)
{
    if (vec == NULL)
        return;
    cp_free(m, vec->data);
}

/**
 * Internal: Shrink the underlying array to minimally \p size.
 * This does not modify the data or size, but only the allocation size.
 *
 * Currently, this is not implemented, but may be in the future.
 */
extern void cp_v_shrink_(
    cp_alloc_t *m CP_UNUSED,
    cp_v_t *vec CP_UNUSED,
    size_t esz CP_UNUSED,
    size_t new_size CP_UNUSED)
{
    /* nothing */
}

/**
 * Internal: Set \p vec to an empty vector.
 *
 * The \p pre_alloc parameter can be used to reduce re-allocation
 * when clearing a vector.  If vec->size is passed, no re-allocation
 * will take place at all.  If 0 is passed, an empty vector will
 * stay zeroed if it has not been initialised yet -- no alloc will
 * take place.
 */
extern void cp_v_clear_(
    cp_alloc_t *m,
    cp_v_t *vec,
    /** Element size */
    size_t esz,
    /**
     * Number of guaranteed pre-allocated elements (the actual number
     * allocated may be larger). */
    size_t pre_alloc)
{
    v_grow(m, vec, esz, pre_alloc);
    vec->size = 0;
}

/**
 * Internal: Ensures a minimum size of the vector.
 *
 * This is like cp_v_set_size_, except it never reduces the size.
 */
extern void cp_v_ensure_size_(
    cp_alloc_t *m,
    cp_v_t *vec,
    size_t esz,
    size_t new_size)
{
    if (vec->size < new_size) {
        v_grow(m, vec, esz, new_size);
        memset(
            cp_v_nth_elem_(vec->data, vec->size, esz),
            0,
            cp_v_size_(new_size - vec->size, esz));
        vec->size = new_size;
    }
}

/**
 * Internal: Sets the size of the vector.
 *
 * If the vector is shrunk, any elements beyond the given size are discarded.
 *
 * If the vector is grown, it is filled with NULL elements at the end.
 */
extern void cp_v_set_size_(
    cp_alloc_t *m,
    cp_v_t *vec,
    size_t esz,
    size_t new_size)
{
    if (vec->size < new_size) {
        v_grow(m, vec, esz, new_size);
        memset(
            cp_v_nth_elem_(vec->data, vec->size, esz),
            0,
            cp_v_size_(new_size - vec->size, esz));
    }
    vec->size = new_size;
}

/**
 * Internal: Insert a sequence of NULL into a vector at a given position.
 * Returns the pointer to the first element of the newly inserted area.
 */
extern void *cp_v_inflate_(
    cp_alloc_t *m,
    /** [IN/OUT] vector to append to */
    cp_v_t *vec,
    /** Element size */
    size_t esz,
    /** [IN] position where to insert (0..size) */
    size_t pos,
    /** [IN] number of elements to insert.  The elements will be
     * initialised with NULL. */
    size_t size)
{
    assert(vec != NULL);
    assert(pos <= vec->size);

    if (size == 0) {
        return NULL;
    }

    size_t new_size = vec->size + size;
    v_grow(m, vec, esz, new_size);

    size_t tail_size = vec->size - pos;

    void *start = cp_v_nth_elem_(vec->data, pos, esz);
    memmove(
        cp_v_nth_elem_(vec->data, pos + size, esz),
        start,
        cp_v_size_(tail_size, esz));

    memset(start, 0, cp_v_size_(size, esz));

    vec->size = new_size;

    return start;
}

/**
 * Internal: Overwrite part of a vector with values from a different array.
 *
 * The dst vector is grown as necessary.
 */
extern void cp_v_copy_arr_(
    cp_alloc_t *m,
    /** Destination of the copying */
    cp_v_t *dst,
    /** Element size */
    size_t esz,
    /** The position where to copy the elements.  Must be 0..size of dst. */
    size_t dst_pos,
    /** Source of the copying */
    void const *data,
    /** Number of elements to copy from data */
    size_t size)
{
    assert(dst != NULL);
    assert(dst_pos <= dst->size);
    if (size == 0) {
        return;
    }

    assert(data != NULL);

    /* possibly grow dst */
    size_t old_size = dst->size;
    size_t end_pos = dst_pos + size;
    if (end_pos > old_size) {
        void *old_data = dst->data;
        v_grow(m, dst, esz, end_pos);
        dst->size = end_pos;

        /* recompute index pointer if it pointed into the dst vector*/
        if (old_data &&
            (data >= old_data) &&
            (data <= cp_v_nth_elem_(old_data, old_size, esz)))
        {
            assert(
                cp_v_nth_elem_(data, size, esz) <=
                cp_v_nth_elem_(old_data, old_size, esz));
            data = ((char*)dst->data) + CP_MONUS((char const *)data, (char const *)old_data);
        }
    }

    memmove(cp_v_nth_elem_(dst->data, dst_pos, esz), data, cp_v_size_(size, esz));
}

/**
 * Internal: Copy (part of) the vector.
 *
 * Make a shallow copy of the vector cell and the array, but not of
 * each element.
 *
 * The dst vector is grown if necessary, e.g.
 *
 *    cp_v_copy_(x, x->size, x, 0, CP_SIZE_MAX, c)
 *
 * is equivalent to
 *
 *    cp_v_append_(x, x, c);
 */
extern void cp_v_copy_(
    cp_alloc_t *m,
    /** Destination of the copying. */
    cp_v_t *dst,
    /** Element size */
    size_t esz,
    /** The position where to copy the elements.  Must be 0..size of dst. */
    size_t dst_pos,
    /** Source of the copying */
    cp_v_t const *src,
    /**
     * Index of the first element to be copied.  For copying the whole
     * vector, pass 0.  Must be 0..size of src. */
    size_t src_pos,
    /**
     * Maximum number of elements to copy.  For copying the whole
     * vector, pass CP_SIZE_MAX.
     *
     * Note that fewer elements may be copied if the vector is
     * shorter.  This is not a bug, but a features. */
    size_t size)
{
    assert(src != NULL);
    assert(src_pos <= src->size);

    /* possibly cull size */
    if (size > src->size - src_pos) {
        size = src->size - src_pos;
    }

    /* use array copy */
    cp_v_copy_arr_(m, dst, esz, dst_pos, cp_v_nth_elem_(src->data, src_pos, esz), size);
}

/**
 * Internal: Insert an array of elements to a vector to a given position.
 * Returns the pointer to the first element of the newly inserted area.
 */
extern void *cp_v_insert_arr_(
    cp_alloc_t *m,
    /** [IN/OUT] vector to append to */
    cp_v_t *dst,
    /** [IN] Element size */
    size_t esz,
    /** [IN] position where to insert (0..size) */
    size_t dst_pos,
    /** [IN] array of elements to fill in */
    void const *data,
    /** [IN] number of elements to fill in */
    size_t size)
{
    assert(dst != NULL);

    if (size == 0) {
        return NULL;
    }

    assert(data != NULL);

    /* data in dst? => tricky situation, we might even split the
     * originally contiguous data by the inflation. */
    if ((dst->data != NULL) &&
        (data >= dst->data) &&
        (data <= cp_v_nth_elem_(dst->data, dst->size, esz)))
    {
        assert(cp_v_nth_elem_(data, size, esz) <= cp_v_nth_elem_(dst->data, dst->size, esz));
        size_t src_pos = cp_v_monus_(esz, data, dst->data);
        size_t src_end = src_pos + size;
        size_t dst_end = dst_pos + size;
        cp_v_inflate_(m, dst, esz, dst_pos, size);
        if (src_end <= dst_pos) {
            /* completely in first part */
            cp_v_copy_(m, dst, esz, dst_pos, dst, src_pos, size);
        }
        else
        if (dst_end <= src_pos) {
            /* completely in last part */
            cp_v_copy_(m, dst, esz, dst_pos, dst, src_pos+size, size);
        }
        else {
            /* first part before gap */
            assert(src_pos < dst_pos);
            size_t size1 = dst_pos - src_pos;
            assert(size1 < size);
            cp_v_copy_(m, dst, esz, dst_pos, dst, src_pos, size1);

            /* second part behind gap */
            assert(dst_end < src_end);
            size_t size2 = size - size1;
            assert(dst_end + size2 == src_end + size);
            cp_v_copy_(m, dst, esz, dst_pos+size1, dst, dst_end, size2);
        }
    }
    else {
        /* data arrays are disjoint => simple case */
        cp_v_inflate_(m, dst, esz, dst_pos, size);
        cp_v_copy_arr_(m, dst, esz, dst_pos, data, size);
    }

    return cp_v_nth_elem_(dst->data, dst_pos, esz);
}

/**
 * Internal: Remove and return the given element of the array.
 */
extern void cp_v_remove_(
    /** [IN/OUT] vector to append to */
    cp_v_t *vec,
    /** Element size */
    size_t esz,
    /** [IN] position where to insert (0..size-1) */
    size_t pos,
    /** [IN] number of elements to remove.  If size is greater than the
     * number that could be removed, the array is truncated from pos. */
    size_t size)
{
    assert(pos <= vec->size);
    if (size > vec->size - pos) {
        size = vec->size - pos;
    }

    size_t rest = vec->size - pos - size;
    memmove(
        cp_v_nth_elem_(vec->data, pos, esz),
        cp_v_nth_elem_(vec->data, pos + size, esz),
        cp_v_size_(rest, esz));

    vec->size -= size;
}

/**
 * Reverse a portion of the vector.
 */
extern void cp_v_reverse_(
    /** [IN/OUT] vector to reverse to */
    cp_v_t *vec,
    /** Element size */
    size_t esz,
    /** [IN] position where to start reversal (0..size-1) */
    size_t pos,
    /** [IN] number of elements to reverse. */
    size_t size)
{
    assert(pos <= vec->size);
    if (size > vec->size - pos) {
        size = vec->size - pos;
    }

    for (cp_size_each(i, size/2)) {
        size_t j = size - i - 1;
        cp_memswap(
           cp_v_nth_elem_(vec->data, pos + i, esz),
           cp_v_nth_elem_(vec->data, pos + j, esz),
           esz);
    }
}

/**
 * Internal: Remove and return the given element of the array. */
extern void cp_v_extract_(
    /** [OUT] the extracted element */
    void *out,
    /** [IN/OUT] vector to append to */
    cp_v_t *vec,
    /** Element size */
    size_t esz,
    /** [IN] position where to insert (0..size-1) */
    size_t pos)
{
    assert(pos < vec->size);
    memcpy(out, cp_v_nth_elem_(vec->data, pos, esz), esz);
    cp_v_remove_(vec, esz, pos, 1);
}

/**
 * Internal: Sort (sub-)array using qsort() */
extern void cp_v_qsort_(
    /** [IN/OUT] vector to append to */
    cp_v_t *vec,
    /** Element size */
    size_t esz,
    /** [IN] start position of sort */
    size_t pos,
    /** [IN] number of positions to sort, CP_SIZE_MAX for up to end */
    size_t size,
    /** [IN] comparison function.
     *  This receives two pointers to elements (not the elements
     *  themselves, but pointers to them). */
    int (*cmp)(void const *, void const *, void *user),
    /** [IN] user pointer, passed as third argument to cmp */
    void *user)
{
    assert(vec != NULL);
    assert(pos <= vec->size);
    if (size > vec->size - pos) {
        size = vec->size - pos;
    }
    if (size > 0) {
        cp_qsort_r(cp_v_nth_elem_(vec->data, pos, esz), size, esz, cmp, user);
    }
}

/**
 * Standard binary search function.
 *
 * Reentrant version with an additional user pointer */
extern size_t cp_bsearch(
    const void *key,
    const void *base,
    size_t nmemb,
    size_t size,
    int (*cmp)(void const *, void const *, void *user),
    void *user)
{
    assert(nmemb < (CP_SIZE_MAX / size));
    size_t a = 0;
    size_t b = nmemb;
    while (a < b) {
        size_t idx = a + ((b - a) / 2);
        const void *p = cp_v_nth_elem_(base, idx, size);
        int c = (*cmp)(key, p, user);
        if (c < 0) {
            b = idx;
        }
        else
        if (c > 0) {
            a = idx + 1;
        }
        else {
            return idx;
        }
    }

    return CP_SIZE_MAX;
}

/* ********************************************************************** */
/* heap functionality */

#define HEAP_PARENT(POS) (((POS) - 1) / 2) // 2->0, 1->0
#define HEAP_CHILD0(POS) (((POS) * 2) + 1) // 0->1

static void cp_v_heap_up_(
    cp_v_t *vec,
    size_t esz,
    int (*cmp)(void const *, void const *, void *user),
    void *user,
    size_t pos)
{
    assert(vec != NULL);
    assert(pos < vec->size);
    void *q = cp_v_nth_elem_(vec->data, pos, esz);
    while (pos > 0) {
        pos = HEAP_PARENT(pos);
        void *p = cp_v_nth_elem_(vec->data, pos, esz);
        if (cmp(p, q, user) <= 0) {
            break;
        }

        cp_memswap(p, q, esz);
        q = p;
    }
}

static void cp_v_heap_down_(
    cp_v_t *vec,
    size_t esz,
    int (*cmp)(void const *, void const *, void *user),
    void *user,
    size_t pos)
{
    assert(vec != NULL);
    assert(pos < vec->size);
    size_t size = vec->size;
    void *q = cp_v_nth_elem_(vec->data, pos, esz);
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
        void *c = cp_v_nth_elem_(vec->data, pos, esz);

        /* check whether second child is smaller */
        if (pos + 1 < size) {
            void *d = cp_v_nth_elem_(vec->data, pos + 1, esz);
            if (cmp(d, c, user) < 0) {
                c = d;
                pos++;
            }
        }

        /* check whether parent is smaller than smallest child */
        if (cmp(q, c, user) <= 0) {
            break;
        }

        cp_memswap(c, q, esz);
        q = c;
    }
}

/**
 * Internal: update the priority of a given entry.
 */
extern void cp_v_heap_update_(
    cp_v_t *vec,
    size_t esz,
    int (*cmp)(void const *, void const *, void *user),
    void *user,
    size_t pos)
{
    cp_v_heap_up_  (vec, esz, cmp, user, pos);
    cp_v_heap_down_(vec, esz, cmp, user, pos);
}

/**
 * Internal: make a heap.
 */
extern void cp_v_heap_make_(
    cp_v_t *vec,
    size_t esz,
    int (*cmp)(void const *, void const *, void *user),
    void *user)
{
    size_t size = vec->size;
    if (size < 2) {
        return;
    }
    for (vec->size = 2; vec->size <= size; vec->size++) {
        cp_v_heap_up_(vec, esz, cmp, user, vec->size-1);
    }
}

/**
 * Internal: extract an element from the heap.  It will be
 * in the element past the end of the vector (in elemen
 * vec->size).
 */
extern void cp_v_heap_extract_(
    cp_v_t *vec,
    size_t esz,
    int (*cmp)(void const *, void const *, void *user),
    void *user)
{
    void *p = cp_v_nth_elem_(vec->data, 0, esz);
    void *q = cp_v_nth_elem_(vec->data, vec->size-1, esz);
    vec->size--;
    if (vec->size > 0) {
        cp_memswap(p, q, esz);
        cp_v_heap_update_(vec, esz, cmp, user, 0);
    }
}

/* ********************************************************************** */
/* macros */
#ifdef CP_MACRO_

/**
 * Internal: Pointer to element in vector
 */
extern macro val cp_v_nth_elem_(val *data, val count, val esz)
{
    (__typeof__(data))((size_t)data + cp_v_size_(count, esz));
}

/**
 * Internal: Difference of indexes in a vector
 */
extern macro val cp_v_monus_(size_t esz, val *a, val b_)
{
    __typeof__(a) b = b_;
    CP_MONUS((char const *)a, (char const *)b) / esz;
}

/**
 * Clear vector.
 */
extern macro void cp_v_init(val *vec)
{
    assert(vec != NULL);
    CP_ZERO(vec);
}

/**
 * Initialises a vector with a given size and 0 contents.
 */
extern macro void cp_v_init0_alloc(
    cp_alloc_t *m, val *vec, size_t sz)
{
    assert(vec != NULL);
    vec->data = CP_NEW_ARR_ALLOC(m, *vec->data, sz);
    vec->size = sz;
    if (cp_countof(vec->word) == 3) {
        vec->word[2] = sz;
    }
}

/**
 * Initialises a vector with a given size and 0 contents.
 */
extern macro void cp_v_init0(val *vec, size_t sz)
{
    cp_v_init0_alloc(&cp_alloc_global, vec, sz);
}

/**
 * Clear the vector and deallocate it, then zero it.
 */
extern macro void cp_v_fini_alloc(
    cp_alloc_t *m, val *vec)
{
    cp_v_fini_(m, (cp_v_t*)vec);
    CP_ZERO(vec);
}

/**
 * Clear the vector and deallocate it, then zero it.
 */
extern macro void cp_v_fini(val *vec)
{
    cp_v_fini_alloc(&cp_alloc_global, vec);
}

/**
 * The size of the vector's data
 */
extern macro val cp_v_esz_(val vec)
{
    sizeof(vec->data[0]);
}

/**
 * Clear the vector, i.e., set number of elements to 0.
 *
 * This never shrinks (reallocates) the array, this must be done
 * manually using cp_v_shrink().
 */
extern macro void cp_v_clear_alloc(
    cp_alloc_t *m, val *vec, val size)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    cp_v_clear_(m, (cp_v_t*)vec, cp_v_esz_(vec), size);
}

/**
 * Clear the vector, i.e., set number of elements to 0.
 *
 * This never shrinks (reallocates) the array, this must be done
 * manually using cp_v_shrink().
 */
extern macro void cp_v_clear(
    val *vec, val size)
{
    cp_v_clear_alloc(&cp_alloc_global, vec, size);
}

/**
 * Does not modify the vector, but possibly shrinks the
 * allocation size to a minimum of \p size.  Use 0
 * to allow shrinking to a complete deallocation.
 *
 * Shrinking is never done automatically but must
 * be done manually.
 *
 * Currently, this functionality is not implemented,
 * but is a nop.
 */
extern macro void cp_v_shrink_alloc(
    cp_alloc_t *m, val *vec, val size)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    cp_v_shrink_(m, (cp_v_t*)vec, cp_v_esz_(vec), size);
}

/**
 * Does not modify the vector, but possibly shrinks the
 * allocation size to a minimum of \p size.  Use 0
 * to allow shrinking to a complete deallocation.
 *
 * Shrinking is never done automatically but must
 * be done manually.
 *
 * Currently, this functionality is not implemented,
 * but is a nop.
 */
extern macro void cp_v_shrink(
    val *vec, val size)
{
    cp_v_shrink_alloc(&cp_alloc_global, vec, size);
}

/**
 * Set the size of the vector.
 * If it needs enlarging, the tail will be zeroed.
 * This will never shrink (reallocate) the array of the vector.
 */
extern macro void cp_v_set_size_alloc(
    cp_alloc_t *m, val *vec, val size)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    cp_v_set_size_(m, (cp_v_t*)vec, cp_v_esz_(vec), size);
}

/**
 * Set the size of the vector.
 * If it needs enlarging, the tail will be zeroed.
 * This will never shrink (reallocate) the array of the vector.
 */
extern macro void cp_v_set_size(
    val *vec, val size)
{
    cp_v_set_size_alloc(&cp_alloc_global, vec, size);
}

/**
 * Make sure the vector has a given size, possibly enlarging
 * by zeroing the tail.
 */
extern macro void cp_v_ensure_size_alloc(
    cp_alloc_t *m, val *vec, val size)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    cp_v_ensure_size_(m, (cp_v_t*)vec, cp_v_esz_(vec), size);
}

/**
 * Make sure the vector has a given size, possibly enlarging
 * by zeroing the tail.
 */
extern macro void cp_v_ensure_size(
    val *vec, val size)
{
    cp_v_ensure_size_alloc(&cp_alloc_global, vec, size);
}

/**
 * Copy one element into the vector.
 */
extern macro void cp_v_copy1_alloc(
    cp_alloc_t *m, val *vec, val pos, val elem)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    void const *elem_p = &elem;
    cp_v_copy_arr_(m, (cp_v_t*)vec, cp_v_esz_(vec), pos, elem_p, 1);
}

/**
 * Copy one element into the vector.
 */
extern macro void cp_v_copy1(
    val *vec, val pos, val elem)
{
    cp_v_copy1_alloc(&cp_alloc_global, vec, pos, elem);
}

/**
 * Copy from one vector to another, overwriting.
 */
extern macro void cp_v_copy_alloc(
    cp_alloc_t *m, val *vec, val pos, val src, val pos2, val cnt)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    __typeof__(*vec) const *vec2 = src;
    assert(vec2 != NULL);
    cp_v_copy_(m,
        (cp_v_t*)vec, cp_v_esz_(vec), pos, (cp_v_t const *)vec2, pos2, cnt);
}

/**
 * Copy from one vector to another, overwriting.
 */
extern macro void cp_v_copy(
    val *vec, val pos, val src, val pos2, val cnt)
{
    cp_v_copy_alloc(&cp_alloc_global, vec, pos, src, pos2, cnt);
}

/**
 * Copy from one vector to another without resizing.
 */
extern macro void cp_v_copy_arr(val *vec, size_t pos, val src, size_t pos2, size_t cnt)
{
    assert(vec != NULL);
    __typeof__(*vec) const *vec2 = src;
    assert(vec2 != NULL);
    assert(pos <= vec->size);
    assert(cnt <= vec->size);
    assert(pos + cnt <= vec->size);
    assert(pos2 <= vec2->size);
    assert(cnt <= vec2->size);
    assert(pos2 + cnt <= vec2->size);
    memmove(&vec->data[pos], &vec2->data[pos2], cp_v_esz_(vec) * cnt);
}

/**
 * Insert zero elements at a given position. */
extern macro void cp_v_inflate_alloc(
    cp_alloc_t *m, val *vec, val pos, val size)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    cp_v_inflate_(m, (cp_v_t*)vec, cp_v_esz_(vec), pos, size);
}

/**
 * Insert zero elements at a given position. */
extern macro void cp_v_inflate(val *vec, val pos, val size)
{
    cp_v_inflate_alloc(&cp_alloc_global, vec, pos, size);
}

/**
 * Insert multiple values from an array into the vector at a given position.
 */
extern macro void cp_v_insert_arr_alloc(
    cp_alloc_t *m, val *vec, val pos, val elem_, val size)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    __typeof__(*vec->data) const *elem = elem_;
    cp_v_insert_arr_(m, (cp_v_t*)vec, cp_v_esz_(vec), pos, elem, size);
}

/**
 * Insert multiple values from an array into the vector at a given position.
 */
extern macro void cp_v_insert_arr(
    val *vec, val pos, val elem_, val size)
{
    cp_v_insert_arr_alloc(&cp_alloc_global, vec, pos, elem_, size);
}

/**
 * Insert one value into the vector at a given position
 */
extern macro void cp_v_insert1_alloc(
    cp_alloc_t *m, val *vec, val pos, val elem_)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    __typeof__(*vec->data) elem = elem_;
    cp_v_insert_arr_(m, (cp_v_t*)vec, cp_v_esz_(vec), pos, &elem, 1);
}

/**
 * Insert one value into the vector at a given position
 */
extern macro void cp_v_insert1(
    val *vec, val pos, val elem_)
{
    cp_v_insert1_alloc(&cp_alloc_global, vec, pos, elem_);
}

/**
 * Insert one vector into the other at a given position
 */
extern macro void cp_v_insert_alloc(
    cp_alloc_t *m, val *vec, val pos, val *vec2)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    assert(vec2 != NULL);
    __typeof__(*vec->data) const *data_ = vec2->data;
    cp_v_insert_arr_(m,
        (cp_v_t*)vec, cp_v_esz_(vec), pos, data_, vec2->size);
}

/**
 * Insert one vector into the other at a given position
 */
extern macro void cp_v_insert(
    val *vec, val pos, val *vec2)
{
    cp_v_insert_alloc(&cp_alloc_global, vec, pos, vec2);
}

/**
 * Remove elements from the vector
 * This will never shrink (reallocate) the array of the vector.
 */
extern macro void cp_v_remove(val *vec, val pos, val size)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    cp_v_remove_((cp_v_t*)vec, cp_v_esz_(vec), pos, size);
}

/**
 * Reverse the order of elements in part of the vector.
 */
extern macro void cp_v_reverse(val *vec, val pos, val size)
{
    assert(vec != NULL);
    cp_v_reverse_((cp_v_t*)vec, cp_v_esz_(vec), pos, size);
}

/**
 * Extract an element from the vector.
 * This will never shrink (reallocate) the array of the vector.
 */
extern macro val cp_v_extract(val *vec, val pos)
{
    assert(vec != NULL);
    CP_NEED_ALLOC_(vec);
    __typeof__(*vec->data) elem;
    cp_v_extract_(&elem, (cp_v_t*)vec, cp_v_esz_(vec), pos);
    elem;
}

/**
 * Sort a portion of the vector.
 */
extern macro void cp_v_qsort(val *vec, val pos, val size, val cmp, val user)
{
    assert(vec != NULL);
    int (* _l_cmp)(
        __typeof__(*vec->data) const *,
        __typeof__(*vec->data) const *,
        __typeof__(*user) *) = cmp;
    cp_v_qsort_((cp_v_t*)vec, cp_v_esz_(vec), pos, size,
        (int(*)(void const *, void const *, void *))_l_cmp,
        (void *)(size_t)user);
}

/**
 * Binary search a key in the vector.
 */
extern macro val cp_v_bsearch(val *key, val *vec, val cmp, val *user)
{
    assert(vec != NULL);
    int (*_l_cmp)(
        __typeof__(*key) const *,
        __typeof__(*vec->data) const *,
        __typeof__(user)) = cmp;
    cp_v_bsearch_(
        (void const *)key,
        (cp_v_t*)(size_t)vec, cp_v_esz_(vec),
        (int(*)(void const *, void const *, void *))_l_cmp,
        (void*)(size_t)user);
}

/**
 * Compute the index of a pointer into the vector.
 */
extern macro val cp_v_idx(val *vec, val ptr_)
{
    __typeof__(*vec->data) const *ptr = ptr_;
    assert(vec != NULL);
    assert(ptr >= vec->data);
    assert(ptr <= (vec->data + vec->size));
    size_t idx = CP_MONUS(ptr, vec->data);
    assert(idx < vec->size);
    idx;
}

/**
* Initialises the vector with a copy of the given data.
*/
extern macro void cp_v_init_with_alloc(
    cp_alloc_t *m, val *vec, val arr_, size_t size)
{
    cp_v_init0_alloc(m, vec, size);
    __typeof__(*vec->data) const *arr = arr_;
    assert(arr != NULL);
    memcpy(vec->data, arr, sizeof(*arr) * size);
}

/**
* Initialises the vector with a copy of the given data.
*/
extern macro void cp_v_init_with(
    val *vec, val arr_, size_t size)
{
    cp_v_init_with_alloc(&cp_alloc_global, vec, arr_, size);
}

/**
 * Delete the vector and its contents.
 * Additional to cp_v_fini(), this also deletes the vector itself.
 */
extern macro void cp_v_delete_alloc(
    cp_alloc_t *m, val *vec)
{
    cp_v_fini_alloc(m, *vec);
    CP_DELETE_ALLOC(m, *vec);
}

/**
 * Delete the vector and its contents.
 * Additional to cp_v_fini(), this also deletes the vector itself.
 */
extern macro void cp_v_delete(
    val *vec)
{
    cp_v_delete_alloc(&cp_alloc_global, vec);
}

/**
 * Remove the last element from the vector and return it.
 * This will never shrink (reallocate) the array of the vector.
 */
extern macro val cp_v_pop(val *vec)
{
    assert(vec != NULL);
    cp_v_extract(vec, vec->size - 1);
}

/**
 * Append a zeroed element at the end of a vector and return a pointer to it.
 */
extern macro val cp_v_push0_alloc(
    cp_alloc_t *m, val *vec)
{
    assert(vec != NULL);
    cp_v_inflate_alloc(m, vec, vec->size, 1);
    &vec->data[vec->size-1];
}

/**
 * Append a zeroed element at the end of a vector and return a pointer to it.
 */
extern macro val cp_v_push0(
    val *vec)
{
    cp_v_push0_alloc(&cp_alloc_global, vec);
}

/**
 * Append multiple elements to the end of a vector.
 */
extern macro void cp_v_append_arr_alloc(
    cp_alloc_t *m, val *vec, val elem, val size)
{
    assert(vec != NULL);
    cp_v_insert_arr_alloc(m, vec, vec->size, elem, size);
}

/**
 * Append multiple elements to the end of a vector.
 */
extern macro void cp_v_append_arr(
    val *vec, val elem, val size)
{
    cp_v_append_arr_alloc(&cp_alloc_global, vec, elem, size);
}

/**
 * Append a single element to the end of a vector and return a pointer to it.
 */
extern macro val cp_v_push_alloc(
    cp_alloc_t *m, val *vec, val elem)
{
    assert(vec != NULL);
    cp_v_insert1_alloc(m, vec, vec->size, elem);
    &vec->data[vec->size-1];
}

/**
 * Append a single element to the end of a vector and return a pointer to it.
 */
extern macro val cp_v_push(
    val *vec, val elem)
{
    cp_v_push_alloc(&cp_alloc_global, vec, elem);
}

/**
 * Append a vector to another vector.
 */
extern macro void cp_v_append_alloc(
    cp_alloc_t *m, val *vec, val vec2)
{
    assert(vec != NULL);
    cp_v_insert_alloc(m, vec, vec->size, vec2);
}

/**
 * Append a vector to another vector.
 */
extern macro void cp_v_append(val *vec, val vec2)
{
    cp_v_append_alloc(&cp_alloc_global, vec, vec2);
}

/**
 * Pointer to last but ith element of vector
 */
extern macro val cp_v_lastp_but(val *vec, size_t i)
{
    assert(vec != NULL);
    assert(i < vec->size);
    &vec->data[vec->size - 1 - i];
}

/**
 * Reference to last but ith element of vector
 */
extern macro ref cp_v_last_but(val *vec, size_t i)
{
    cp_v_lastp_but(vec, i);
}

/**
 * Pointer to last element of vector
 */
extern macro val cp_v_lastp(val vec)
{
    cp_v_lastp_but(vec,0);
}

/**
 * Reference to last element of vector
 */
extern macro val cp_v_last(val vec)
{
    cp_v_last_but(vec,0);
}

/**
 * Pointer to an element of the vector, or NULL if index is out of range.
 */
extern macro val cp_v_nth_ptr0(val *vec, size_t i)
{
    ((vec != NULL) && (i < vec->size)) ? &vec->data[i] : NULL;
}

/**
 * An element of the vector, or 0 if index is out of range.
 */
extern macro val cp_v_nth0(val *vec, size_t i)
{
    ((vec != NULL) && (i < vec->size)) ? vec->data[i] : 0;
}

/**
 * Pointer to an element of the vector.
 */
extern macro val cp_v_nth_ptr(val *vec, size_t i)
{
    assert(vec != NULL);
    assert((i < vec->size) ||
        (fprintf(stderr, "ERR: i=%"CP_Z"u, vec->size=%"CP_Z"u\n",
            i, vec->size),0));
    &vec->data[i];
}

/**
 * Reference to an element of the vector.
 */
extern macro ref cp_v_nth(val vec, val i)
{
    cp_v_nth_ptr(vec, i);
}

/**
 * Extract a bit from an integer vector.
 */
extern macro val cp_v_bit_get(val *vec, size_t i)
{
    size_t ib = i / (8 * sizeof(*vec->data));
    assert(ib < vec->size);
    size_t ik = i % (8 * sizeof(*vec->data));
    size_t iv = ((size_t)1) << ik;
    ((vec->data[ib] & iv) != 0);
}

/**
 * Set a bit in an integer vector.
 */
extern macro val cp_v_bit_set(val *vec, size_t i, size_t n)
{
    size_t ib = i / (8 * sizeof(*vec->data));
    assert(ib < vec->size);
    size_t ik = i % (8 * sizeof(*vec->data));
    size_t iv = ((size_t)1) << ik;
    vec->data[ib] = (vec->data[ib] | iv) ^ (n ? 0 : iv);
}

/**
 * Convert an array into a heap.
 */
extern macro val cp_v_heap_make(val *vec, val cmp, val *user)
{
    assert(vec != NULL);
    int (*_l_cmp)(
        __typeof__(*vec->data) const *,
        __typeof__(*vec->data) const *,
        __typeof__(user)) = cmp;
    cp_v_heap_make_(
        (cp_v_t*)vec, cp_v_esz_(vec),
        (int(*)(void const *, void const *, void *))_l_cmp,
        (void *)(size_t)user);
}

/**
 * Heap function: update the priority of the given element.
 */
extern macro val cp_v_heap_update(val *vec, val cmp, val *user, val pos)
{
    assert(vec != NULL);
    int (*_l_cmp)(
        __typeof__(*vec->data) const *,
        __typeof__(*vec->data) const *,
        __typeof__(user)) = cmp;
    cp_v_heap_update_(
        (cp_v_t*)vec, cp_v_esz_(vec),
        (int(*)(void const *, void const *, void *))_l_cmp,
        (void *)(size_t)user,
        pos);
}

/**
 * Insert a new element into the heap.
 *
 * This is done by pushing the element to the heap and then updating
 * the new last element.
 */
extern macro val cp_v_heap_insert_alloc(
    cp_alloc_t *m, val *vec, val cmp, val *user, val elem)
{
    assert(vec != NULL);
    cp_v_push_alloc(m, vec, elem);
    cp_v_heap_update(vec, cmp, user, vec->size-1);
}

/**
 * Insert a new element into the heap.
 *
 * This is done by pushing the element to the heap and then updating
 * the new last element.
 */
extern macro val cp_v_heap_insert(val *vec, val cmp, val *user, val elem)
{
    cp_v_heap_insert_alloc(&cp_alloc_global, vec, cmp, user, elem);
}

/**
 * Extract an element from a heap.
 *
 * This is done by swapping the first with the last element, then
 * shrinking the array by one, then updating the first, and then
 * returning the element past the end.
 *
 * The element is not actually gone from the array, but removed
 * from the heap structure.  This returns a pointer to it.
 */
extern macro val cp_v_heap_extract_ptr(val *vec, val cmp, val *user)
{
    assert(vec != NULL);
    assert(vec->size > 0);
    int (*_l_cmp)(
        __typeof__(*vec->data) const *,
        __typeof__(*vec->data) const *,
        __typeof__(user)) = cmp;
    cp_v_heap_extract_((cp_v_t*)vec, cp_v_esz_(vec),
        (int(*)(void const *, void const *, void *))_l_cmp,
        (void *)(size_t)user);
    &vec->data[vec->size];
}

/**
 * Extract an element from a heap.
 *
 * Just like cp_v_heap_extract_ptr(), but returns the element
 * itself, not the pointer to it.
 */
extern macro val cp_v_heap_extract(val *vec, val cmp, val *user)
{
    *cp_v_heap_extract_ptr(vec, cmp, user);
}

/**
 * The minimum element of the heap
 */
extern macro val cp_v_heap_min(val *vec)
{
    assert(vec != NULL);
    assert(vec->size > 0);
    &vec->data[0];
}

#endif /*0*/
