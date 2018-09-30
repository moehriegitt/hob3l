/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <cpmat/arith.h>
#include <cpmat/vec.h>

extern void __cp_v_fini(
    cp_v_t *vec)
{
    if (vec == NULL)
        return;
    free(vec->data);
}

static void __grow(
    cp_v_t *vec,
    size_t esz,
    size_t new_size)
{
    if (vec->alloc >= new_size) {
        return;
    }

    size_t new_alloc = vec->alloc;
    if (new_alloc < __cp_v_min_alloc()) {
        new_alloc = __cp_v_min_alloc();
    }

    while (new_alloc < new_size) {
        new_alloc *= 2;
        if (new_alloc > __cp_v_max_size(esz)) {
            fprintf(stderr, "ERROR: Out of memory in __cp_v_inflate()\n");
            abort();
        }
        assert(new_alloc > vec->alloc);
    }

    vec->data = realloc(vec->data, __cp_v_size(new_alloc, esz));
    vec->alloc = new_alloc;
}

static void __shrink(
    cp_v_t *vec __unused,
    size_t esz __unused,
    size_t new_size __unused)
{
}

extern void __cp_v_clear(
    cp_v_t *vec,
    size_t esz,
    size_t pre_alloc)
{
    __grow(vec, esz, pre_alloc);
    __shrink(vec, esz, pre_alloc);
    vec->size = 0;
}

extern void __cp_v_ensure_size(
    cp_v_t *vec,
    size_t esz,
    size_t new_size)
{
    if (vec->size < new_size) {
        __grow(vec, esz, new_size);
        memset(
            __cp_v_nth_ptr(vec->data, vec->size, esz),
            0,
            __cp_v_size(new_size - vec->size, esz));
        vec->size = new_size;
    }
}

extern void __cp_v_set_size(
    cp_v_t *vec,
    size_t esz,
    size_t new_size)
{
    if (vec->size < new_size) {
        __grow(vec, esz, new_size);
        memset(
            __cp_v_nth_ptr(vec->data, vec->size, esz),
            0,
            __cp_v_size(new_size - vec->size, esz));
    }
    else {
        __shrink(vec, esz,new_size);
    }
    vec->size = new_size;
}

extern void *__cp_v_inflate(
    cp_v_t *vec,
    size_t esz,
    size_t pos,
    size_t size)
{
    assert(vec != NULL);
    assert(pos <= vec->size);

    if (size == 0) {
        return NULL;
    }

    size_t new_size = vec->size + size;
    __grow(vec, esz, new_size);

    size_t tail_size = vec->size - pos;

    void *start = __cp_v_nth_ptr(vec->data, pos, esz);
    memmove(
        __cp_v_nth_ptr(vec->data, pos + size, esz),
        start,
        __cp_v_size(tail_size, esz));

    memset(start, 0, __cp_v_size(size, esz));

    vec->size = new_size;

    return start;
}

extern void __cp_v_copy_arr(
    cp_v_t *dst,
    size_t esz,
    size_t dst_pos,
    void const *data,
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
        __grow(dst, esz, end_pos);
        dst->size = end_pos;

        /* recompute index pointer if it pointed into the dst vector*/
        if (old_data &&
            (data >= old_data) &&
            (data <= __cp_v_nth_ptr(old_data, old_size, esz)))
        {
            assert(
                __cp_v_nth_ptr(data, size, esz) <=
                __cp_v_nth_ptr(old_data, old_size, esz));
            data = ((char*)dst->data) + CP_PTRDIFF((char const *)data, (char const *)old_data);
        }
    }

    memmove(__cp_v_nth_ptr(dst->data, dst_pos, esz), data, __cp_v_size(size, esz));
}

extern void __cp_v_copy(
    cp_v_t *dst,
    size_t esz,
    size_t dst_pos,
    cp_v_t const *src,
    size_t src_pos,
    size_t size)
{
    assert(src != NULL);
    assert(src_pos <= src->size);

    /* possibly cull size */
    if (size > src->size - src_pos) {
        size = src->size - src_pos;
    }

    /* use array copy */
    __cp_v_copy_arr(dst, esz, dst_pos, __cp_v_nth_ptr(src->data, src_pos, esz), size);
}

extern void *__cp_v_insert_arr(
    cp_v_t *dst,
    size_t esz,
    size_t dst_pos,
    void const *data,
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
        (data <= __cp_v_nth_ptr(dst->data, dst->size, esz)))
    {
        assert(__cp_v_nth_ptr(data, size, esz) <= __cp_v_nth_ptr(dst->data, dst->size, esz));
        size_t src_pos = __cp_v_ptrdiff(esz, data, dst->data);
        size_t src_end = src_pos + size;
        size_t dst_end = dst_pos + size;
        __cp_v_inflate(dst, esz, dst_pos, size);
        if (src_end <= dst_pos) {
            /* completely in first part */
            __cp_v_copy(dst, esz, dst_pos, dst, src_pos, size);
        }
        else
        if (dst_end <= src_pos) {
            /* completely in last part */
            __cp_v_copy(dst, esz, dst_pos, dst, src_pos+size, size);
        }
        else {
            /* first part before gap */
            assert(src_pos < dst_pos);
            size_t size1 = dst_pos - src_pos;
            assert(size1 < size);
            __cp_v_copy(dst, esz, dst_pos, dst, src_pos, size1);

            /* second part behind gap */
            assert(dst_end < src_end);
            size_t size2 = size - size1;
            assert(dst_end + size2 == src_end + size);
            __cp_v_copy(dst, esz, dst_pos+size1, dst, dst_end, size2);
        }
    }
    else {
        /* data arrays are disjoint => simple case */
        __cp_v_inflate(dst, esz, dst_pos, size);
        __cp_v_copy_arr(dst, esz, dst_pos, data, size);
    }

    return __cp_v_nth_ptr(dst->data, dst_pos, esz);
}

extern void __cp_v_remove(
    cp_v_t *vec,
    size_t esz,
    size_t pos,
    size_t size)
{
    assert(pos <= vec->size);
    if (size > vec->size - pos) {
        size = vec->size - pos;
    }

    size_t rest = vec->size - pos - size;
    memmove(
        __cp_v_nth_ptr(vec->data, pos, esz),
        __cp_v_nth_ptr(vec->data, pos + size, esz),
        __cp_v_size(rest, esz));

    vec->size -= size;
    __shrink(vec, esz, vec->size);
}

extern void __cp_v_reverse(
    cp_v_t *vec,
    size_t esz,
    size_t pos,
    size_t size)
{
    assert(pos <= vec->size);
    if (size > vec->size - pos) {
        size = vec->size - pos;
    }

    for (cp_size_each(i, size/2)) {
        size_t j = size - i - 1;
        cp_memswap(
           __cp_v_nth_ptr(vec->data, pos + i, esz),
           __cp_v_nth_ptr(vec->data, pos + j, esz),
           esz);
    }

    __shrink(vec, esz, vec->size);
}

extern void __cp_v_extract(
    void *out,
    cp_v_t *vec,
    size_t esz,
    size_t pos)
{
    assert(pos < vec->size);
    memcpy(out, __cp_v_nth_ptr(vec->data, pos, esz), esz);
    __cp_v_remove(vec, esz, pos, 1);
}

extern void __cp_v_qsort(
    cp_v_t *vec,
    size_t esz,
    size_t pos,
    size_t size,
    int (*cmp)(void const *, void const *, void *),
    void *user)
{
    assert(vec != NULL);
    assert(pos <= vec->size);
    if (size > vec->size - pos) {
        size = vec->size - pos;
    }
    cp_qsort_r(__cp_v_nth_ptr(vec->data, pos, esz), size, esz, cmp, user);
}

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
        const void *p = __cp_v_nth_ptr(base, idx, size);
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
