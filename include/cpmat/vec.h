/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_VEC_H
#define __CP_VEC_H

#include <cpmat/vec_tam.h>
#include <cpmat/qsort.h>

static inline size_t __cp_v_min_alloc(void)
{
    return 4;
}

static inline size_t __cp_v_max_size(size_t esz)
{
    return CP_MAX_OF((size_t)0) / esz;
}

static inline size_t __cp_v_size(size_t count, size_t esz)
{
    assert(count <= __cp_v_max_size(esz));
    return count * esz;
}

#define __cp_v_nth_ptr(data, count, esz) \
    ((__typeof__(data))((size_t)(1?(data):((void*)0)) + __cp_v_size(count, esz)))

#define __cp_v_ptrdiff(esz, a, b) \
    (CP_PTRDIFF((char const *)(1?(a):((void*)0)), (char const *)(1?(b):((void*)0))) / (esz))

/**
 * Overwrite part of a vector with values from a different array.
 *
 * The dst vector is grown as necessary.
 */
extern void __cp_v_copy_arr(
    /**
     * Destination of the copying */
    cp_v_t *dst,

    /**
     * Element size */
    size_t esz,

    /**
     * The position where to copy the elements.  Must be 0..size of dst. */
    size_t dst_pos,

    /**
     * Source of the copying */
    void const *data,

    /**
     * Number of elements to copy from data
     */
    size_t size);

/**
 * Copy (part of) the vector.
 *
 * Make a shallow copy of the vector cell and the array, but not of
 * each element.
 *
 * The dst vector is grown if necessary, e.g.
 *
 *    __cp_v_copy(x, x->size, x, 0, CP_SIZE_MAX, c)
 *
 * is equivalent to
 *
 *    __cp_v_append(x, x, c);
 */
extern void __cp_v_copy(
    /**
     * Destination of the copying. */
    cp_v_t *dst,

    /**
     * Element size */
    size_t esz,

    /**
     * The position where to copy the elements.  Must be 0..size of dst. */
    size_t dst_pos,

    /**
     * Source of the copying */
    cp_v_t const *src,

    /**
     * Index of the first element to be copied.  For copying the whole
     * vector, pass 0.  Must be 0..size of src.
     */
    size_t src_pos,

    /**
     * Maximum number of elements to copy.  For copying the whole
     * vector, pass CP_SIZE_MAX.
     *
     * Note that fewer elements may be copied if the vector is
     * shorter.  This is not a bug, but a features.
     */
    size_t cnt);


/**
 * Shallow delete a vector: remove all sub-structures, but not the
 * elements and not the pointer itself.  The structure will then
 * be a clean, empty vector.
 *
 * This function allows v == NULL.
 */
extern void __cp_v_fini(
    cp_v_t *v);

/**
 * Set \p vec to an empty vector.
 *
 * The \p pre_alloc parameter can be used to reduce re-allocation
 * when clearing a vector.  If vec->size is passed, no re-allocation
 * will take place at all.  If 0 is passed, an empty vector will
 * stay zeroed if it has not been initialised yet -- no alloc will
 * take place.
 */
extern void __cp_v_clear(
    cp_v_t *vec,

    /**
     * Element size */
    size_t esz,

    /**
     * Number of guaranteed pre-allocated elements (the actual number
     * allocated may be larger). */
    size_t pre_alloc);

/**
 * Sets the size of the vector.
 *
 * If the vector is shrunk, any elements beyond the given size are discarded.
 *
 * If the vector is grown, it is filled with NULL elements at the end.
 */
extern void __cp_v_set_size(
    cp_v_t *vec,
    /**
     * Element size */
    size_t esz,

    size_t size);

/**
 * Ensures a minimum size of the vector.
 *
 * This is like __cp_v_set_size, except it never shrinks the vector.
 */
extern void __cp_v_ensure_size(
    cp_v_t *vec,
    /**
     * Element size */
    size_t esz,

    size_t min_size);

/**
 * Insert a sequence of NULL into a vector at a given position.
 * Returns the pointer to the first element of the newly inserted area.
 */
extern void *__cp_v_inflate(
    /** [IN/OUT] vector to append to */
    cp_v_t *vec,
    /**
     * Element size */
    size_t esz,

    /** [IN] position where to insert (0..size) */
    size_t pos,

    /** [IN] number of elements to insert.  The elements will be
     * initialised with NULL. */
    size_t size);

/**
 * Insert an array of elements to a vector to a given position.
 * Returns the pointer to the first element of the newly inserted area.
 */
extern void *__cp_v_insert_arr(
    /** [IN/OUT] vector to append to */
    cp_v_t *vec,
    /**
     * Element size */
    size_t esz,

    /** [IN] position where to insert (0..size) */
    size_t pos,
    /** [IN] array of elements to fill in */
    void const *data,
    /** [IN] number of elements to fill in */
    size_t size);

/**
 * Remove and return the given element of the array.
 */
extern void __cp_v_remove(
    /** [IN/OUT] vector to append to */
    cp_v_t *vec,
    /**
     * Element size */
    size_t esz,

    /** [IN] position where to insert (0..size-1) */
    size_t pos,
    /** [IN] number of elements to remove.  If size is greater than the
     * number that could be removed, the array is truncated from pos.
     */
    size_t size);

/**
 * Remove and return the given element of the array.
 */
extern void __cp_v_extract(
    /** [OUT] the extracted element */
    void *out,
    /** [IN/OUT] vector to append to */
    cp_v_t *vec,
    /**
     * Element size */
    size_t esz,

    /** [IN] position where to insert (0..size-1) */
    size_t pos);

/**
 * Reverse a portion of the vector.
 */
extern void __cp_v_reverse(
    /** [IN/OUT] vector to reverse to */
    cp_v_t *vec,
    /**
     * Element size */
    size_t esz,

    /** [IN] position where to start reversal (0..size-1) */
    size_t pos,
    /** [IN] number of elements to reverse. */
    size_t size);

/**
 * Sort (sub-)array using qsort()
 *
 */
extern void __cp_v_qsort(
    /**
     * [IN/OUT] vector to append to */
    cp_v_t *vec,
    /**
     * Element size */
    size_t esz,

    /**
     * [IN] start position of sort */
    size_t pos,
    /**
     * [IN] end position of sort, CP_SIZE_MAX for up to end */
    size_t size,
    /**
     * [IN] comparison function.
     * This receives two pointers to elements (not the elements
     * themselves, but pointers to them).
     */
    int (*cmp)(void const *, void const *, void *user),
    /** [IN] user pointer, passed as third argument to cmp */
    void *user);

extern size_t cp_bsearch(
    const void *key,
    const void *base,
    size_t nmemb,
    size_t size,
    int (*cmp)(void const *, void const *, void *user),
    void *user);

/**
 * Search for something in the vector using bsearch().
 * Returns the index (not the pointer, to avoid problems
 * with 'const' casting) if found, or 'CP_SIZE_MAX' on error,
 * which is greater than the size of the vector.
 */
static inline size_t __cp_v_bsearch(
    /** [IN] The pointer to the key to search */
    void const *key,
    /** [IN] The vector */
    cp_v_t *vec,
    /**
     * Element size */
    size_t esz,

    /**
     * [IN] comparison function.  The first parameter
     * is 'key, the second will be a pointer to an array
     * entry (like in sort).
     * Note that althought the third argument is not
     * strictly necessary because arbitrary user data
     * could be passed via the first argument, a user
     * pointer is implemented to have the same prototype
     * as for the cmp function in cp_v_qsort() (if the
     * key is the same).
     */
    int (*cmp)(void const *, void const *, void *user),
    /**
     * [IN] user pointer, passed as third argement to cmp */
    void *user)
{
    return cp_bsearch(key, vec->data, vec->size, esz, cmp, user);
}


/*
 * (Semi) type-safe and convenience macro layer.
 *
 * We cannot really check that the vector pointer passed in is actually
 * constructed using CP_VEC_T().
 *
 * Some of the macros that do not change the structure and that do not
 * depend on this size of the vector can also be used for CP_ARR_T()
 * declared types (which misses the alloc slot).
 */

#define __CP_NEED_ALLOC(vec) \
    ((void)(vec)->alloc)

#define cp_v_init(vec) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL); \
        CP_ZERO(__vec); \
    })

/**
 * Initialises a vector with a given size and 0 contents */
#define cp_v_init0(_vec, _size) \
    ({ \
        __typeof__(*(_vec)) *__vec = (_vec); \
        size_t __size = (_size); \
        assert(__vec != NULL); \
        CP_CALLOC_ARR(__vec->data, __size); \
        __vec->size = __size; \
        if (cp_countof(__vec->word) == 3) { \
            __vec->word[2] = __size; \
        } \
    })

#define cp_v_fini(vec) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        __cp_v_fini((cp_v_t*)__vec); \
        CP_ZERO(__vec); \
    })

#define __cp_v_esz(vec) (sizeof((vec)->data[0]))

#define cp_v_clear(vec, size) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __cp_v_clear((cp_v_t*)__vec, __cp_v_esz(__vec), size); \
    })

#define cp_v_set_size(vec, size) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __cp_v_set_size((cp_v_t*)__vec, __cp_v_esz(__vec), size); \
    })

#define cp_v_ensure_size(vec, size) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __cp_v_ensure_size((cp_v_t*)__vec, __cp_v_esz(__vec), size); \
    })

#define cp_v_copy1(vec,pos,elem) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __typeof__(*(vec)->data) __elem = (elem); \
        __cp_v_copy_arr(\
            (cp_v_t*)__vec, __cp_v_esz(__vec), pos, (void const *)(size_t)&__elem, 1); \
    })

#define cp_v_copy(vec,pos,vec2,pos2,cnt) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __typeof__(*(vec)) const *__vec2 = (vec2); \
        assert(__vec2 != NULL);  \
        __cp_v_copy(\
            (cp_v_t*)__vec, __cp_v_esz(__vec), pos, (cp_v_t const *)__vec2, pos2, cnt); \
    })

#define cp_v_copy_arr(vec,pos,vec2,pos2,cnt) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __typeof__(*(vec)) const *__vec2 = (vec2); \
        assert(__vec2 != NULL);  \
        size_t __pos = (pos); \
        size_t __pos2 = (pos2); \
        size_t __cnt = (cnt); \
        assert(__pos <= __vec->size); \
        assert(__cnt <= __vec->size); \
        assert(__pos + __cnt <= __vec->size); \
        assert(__pos2 <= __vec2->size); \
        assert(__cnt <= __vec2->size); \
        assert(__pos2 + __cnt <= __vec2->size); \
        memmove(&__vec->data[__pos], &__vec2->data[__pos2], __cp_v_esz(__vec) * __cnt); \
    })

#define cp_v_inflate(vec,pos,size) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __cp_v_inflate((cp_v_t*)__vec, __cp_v_esz(__vec), pos, size); \
    })

#define cp_v_insert_arr(vec,pos,elem,size) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __typeof__(*(vec)->data) const *__elem = (elem); \
        __cp_v_insert_arr(\
            (cp_v_t*)__vec, __cp_v_esz(__vec), pos, (void const *)(size_t)__elem, size); \
    })

#define cp_v_insert1(vec,pos,elem) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __typeof__(*(vec)->data) __elem = (elem); \
        __cp_v_insert_arr(\
            (cp_v_t*)__vec, __cp_v_esz(__vec), pos, (void const *)(size_t)&__elem, 1); \
    })

#define cp_v_insert(vec,pos,vec2) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __typeof__(*(vec2)) const *__vec2 = (vec2); \
        assert(__vec2 != NULL);  \
        __typeof__(*(vec)->data) const *__data = __vec2->data; \
        __cp_v_insert_arr(\
            (cp_v_t*)__vec, __cp_v_esz(__vec), pos, (void const *)(size_t)__data, __vec2->size); \
    })

#define cp_v_remove(vec,pos,size) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __cp_v_remove((cp_v_t*)__vec, __cp_v_esz(__vec), pos, size); \
    })

#define cp_v_reverse(vec,pos,size) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL); \
        __cp_v_reverse((cp_v_t*)__vec, __cp_v_esz(__vec), pos, size); \
    })

#define cp_v_extract(vec,pos) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        __CP_NEED_ALLOC(__vec); \
        __typeof__(*(vec)->data) __elem; \
        __cp_v_extract(&__elem, (cp_v_t*)__vec, __cp_v_esz(__vec), pos); \
        __elem; \
    })

#define cp_v_qsort(vec, pos, size, cmp, user) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        assert(__vec != NULL);  \
        int (*__cmp)( \
            __typeof__(*__vec->data) const *, \
            __typeof__(*__vec->data) const *, \
            __typeof__(*(user)) *) = (cmp); \
        __cp_v_qsort((cp_v_t*)__vec, __cp_v_esz(__vec), pos, size, \
            (int(*)(void const *, void const *, void *))__cmp, \
            (void *)(size_t)(user)); \
    })

#define cp_v_bsearch(key, vec, cmp, user) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        __typeof__(*(key)) const *__key = (key); \
        assert(__vec != NULL); \
        int (*__cmp)( \
            __typeof__(__key), \
            __typeof__(*__vec->data) const *, \
            __typeof__(*(user)) *) = (cmp); \
        __cp_v_bsearch( \
            (void const *)__key, \
            (cp_v_t*)__vec, __cp_v_esz(__vec), \
            (int(*)(void const *, void const *, void *))__cmp, \
            (void*)(size_t)(user)); \
    })

#define cp_v_idx(vec, ptr) \
    ({ \
        __typeof__(*(vec)) *__vec = (vec); \
        __typeof__(*__vec->data) const *__ptr = (ptr); \
        assert(__vec != NULL); \
        assert(__ptr >= __vec->data); \
        assert(__ptr <= (__vec->data + __vec->size)); \
        size_t __idx = CP_PTRDIFF(__ptr, __vec->data); \
        assert(__idx < __vec->size); \
        __idx; \
    })

/* The following use different local variable names so they
 * are not equal to those of the invoked macros */
/*
 * Sets the vector with a copy of the given data. */
#define cp_v_init_with(_vec, _arr, _size) \
    ({ \
        __typeof__(*(_vec)) *__vecA = (_vec); \
        size_t __sizeA = (_size); \
        cp_v_init0(__vecA, __sizeA); \
        __typeof__(*(__vecA->data)) const *__arrA = (_arr); \
        assert(__arrA != NULL); \
        memcpy(__vecA->data, __arrA, sizeof(*__arrA) * __sizeA); \
    })

#define cp_v_delete(vec) \
    ({ \
        __typeof__(*(vec)) *__vecA = &(vec); \
        cp_v_fini(*__vecA); \
        CP_FREE(*__vecA); \
    })

#define cp_v_pop(vec) \
    ({ \
        __typeof__(*(vec)) *__vecA = (vec); \
        assert(__vecA != NULL); \
        cp_v_extract(__vecA, __vecA->size-1); \
    })

#define cp_v_push0(vec) \
    ({ \
        __typeof__(*(vec)) *__vecA = (vec); \
        assert(__vecA != NULL); \
        cp_v_inflate(__vecA, __vecA->size, 1); \
        &__vecA->data[__vecA->size-1]; \
    })

#define cp_v_append_arr(vec,elem,size) \
    ({ \
        __typeof__(*(vec)) *__vecA = (vec); \
        assert(__vecA != NULL);  \
        cp_v_insert_arr(__vecA, __vecA->size, elem, size); \
    })

#define cp_v_push(vec,elem) \
    ({ \
        __typeof__(*(vec)) *__vecA = (vec); \
        assert(__vecA != NULL);  \
        cp_v_insert1(__vecA, __vecA->size, elem); \
        &__vecA->data[__vecA->size-1]; \
    })

#define cp_v_append(vec,vec2) \
    ({ \
        __typeof__(*(vec)) *__vecA = (vec); \
        assert(__vecA != NULL);  \
        cp_v_insert(__vecA, __vecA->size, vec2); \
    })

#define cp_v_last(vec) \
    (*({ \
        __typeof__(*(vec)) * __vecB = (vec); \
        assert(__vecB != NULL); \
        &__vecB->data[__vecB->size - 1]; \
    }))

#define cp_v_last_but(vec, i) \
    (*({ \
        __typeof__(*(vec)) * __vecB = (vec); \
        size_t __iB = (i); \
        assert(__vecB != NULL); \
        assert(__iB < __vecB->size); \
        &__vecB->data[__vecB->size - 1 - __iB]; \
    }))

#define cp_v_nth(vec, i) \
    (*({ \
        __typeof__(*(vec)) * __vecB = (vec); \
        size_t __iB = (i); \
        assert(__vecB != NULL); \
        assert((__iB < __vecB->size) || \
            (fprintf(stderr, "ERR: __iB=%"_Pz"u, __vecB->size=%"_Pz"u\n", \
                __iB, __vecB->size),0)); \
        &__vecB->data[__iB]; \
    }))

#define cp_v_bit_get(vec, i) \
    ({ \
        __typeof__(*(vec)) * __vecC = (vec); \
        size_t __i = i; \
        size_t __ib = __i / (8 * sizeof(*__vecC->data)); \
        assert(__ib < __vecC->size); \
        size_t __ik = __i % (8 * sizeof(*__vecC->data)); \
        size_t __iv = (((size_t)1) << __ik); \
        ((__vecC->data[__ib] & __iv) != 0); \
    })

#define cp_v_bit_set(vec, i, n) \
    ({ \
        __typeof__(*(vec)) * __vecC = (vec); \
        size_t __i = i; \
        size_t __ib = __i / (8 * sizeof(*__vecC->data)); \
        assert(__ib < __vecC->size); \
        size_t __ik = __i % (8 * sizeof(*__vecC->data)); \
        size_t __iv = (((size_t)1) << __ik); \
        __vecC->data[__ib] = (__vecC->data[__ib] | __iv) ^ ((n) ? 0 : __iv); \
    })

#endif /* __CP_VEC_H */
