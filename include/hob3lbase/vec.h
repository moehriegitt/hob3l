/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_VEC_H_
#define CP_VEC_H_

#include <hob3lbase/vec_tam.h>
#include <hob3lbase/qsort.h>

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

/* BEGIN MACRO * DO NOT EDIT, use 'mkmacro' instead. */

/**
 * Internal: Pointer to element in vector
 */
#define cp_v_nth_elem_(data,count,esz) \
    cp_v_nth_elem_1_(CP_GENSYM(_data), (data), (count), (esz))

#define cp_v_nth_elem_1_(data,_data,count,esz) \
    ({ \
        __typeof__(*_data) *data = _data; \
        (__typeof__(data))((size_t)data + cp_v_size_(count, esz)); \
    })

/**
 * Internal: Difference of indexes in a vector
 */
#define cp_v_ptrdiff_(esz,a,b_) \
    cp_v_ptrdiff_1_(CP_GENSYM(_a), CP_GENSYM(_b), CP_GENSYM(_esz), (esz), \
        (a), (b_))

#define cp_v_ptrdiff_1_(a,b,esz,_esz,_a,b_) \
    ({ \
        size_t esz = _esz; \
        __typeof__(*_a) *a = _a; \
        __typeof__(a) b = b_; \
        CP_PTRDIFF((char const *)a, (char const *)b) / esz; \
    })

/**
 * Clear vector.
 */
#define cp_v_init(vec) \
    cp_v_init_1_(CP_GENSYM(_vec), (vec))

#define cp_v_init_1_(vec,_vec) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_ZERO(vec); \
    }while(0)

/**
 * Initialises a vector with a given size and 0 contents.
 */
#define cp_v_init0(vec,sz) \
    cp_v_init0_1_(CP_GENSYM(_sz), CP_GENSYM(_vec), (vec), (sz))

#define cp_v_init0_1_(sz,vec,_vec,_sz) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        size_t sz = _sz; \
        assert(vec != NULL); \
        vec->data = CP_NEW_ARR(*vec->data, sz); \
        vec->size = sz; \
        if (cp_countof(vec->word) == 3) { \
            vec->word[2] = sz; \
        } \
    }while(0)

/**
 * Clear the vector and deallocate it, then zero it.
 */
#define cp_v_fini(vec) \
    cp_v_fini_1_(CP_GENSYM(_vec), (vec))

#define cp_v_fini_1_(vec,_vec) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        cp_v_fini_((cp_v_t*)vec); \
        CP_ZERO(vec); \
    }while(0)

/**
 * The size of the vector's data
 */
#define cp_v_esz_(vec) \
    cp_v_esz_1_(CP_GENSYM(_vec), (vec))

#define cp_v_esz_1_(vec,_vec) \
    ({ \
        __typeof__(*_vec) *vec = _vec; \
        sizeof(vec->data[0]); \
    })

/**
 * Clear the vector, i.e., set number of elements to 0.
 */
#define cp_v_clear(vec,size) \
    cp_v_clear_1_(CP_GENSYM(_vec), (vec), (size))

#define cp_v_clear_1_(vec,_vec,size) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        cp_v_clear_((cp_v_t*)vec, cp_v_esz_(vec), size); \
    }while(0)

/**
 * Set the size of the vector.
 * If it needs enlarging, the tail will be zeroed.
 */
#define cp_v_set_size(vec,size) \
    cp_v_set_size_1_(CP_GENSYM(_vec), (vec), (size))

#define cp_v_set_size_1_(vec,_vec,size) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        cp_v_set_size_((cp_v_t*)vec, cp_v_esz_(vec), size); \
    }while(0)

/**
 * Make sure the vector has a given size, possibly enlarging
 * by zeroing the tail.
 */
#define cp_v_ensure_size(vec,size) \
    cp_v_ensure_size_1_(CP_GENSYM(_vec), (vec), (size))

#define cp_v_ensure_size_1_(vec,_vec,size) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        cp_v_ensure_size_((cp_v_t*)vec, cp_v_esz_(vec), size); \
    }while(0)

/**
 * Copy one element into the vector.
 */
#define cp_v_copy1(vec,pos,elem) \
    cp_v_copy1_1_(CP_GENSYM(_elem_p), CP_GENSYM(_vec), (vec), (pos), \
        (elem))

#define cp_v_copy1_1_(elem_p,vec,_vec,pos,elem) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        void const * elem_p = &elem; \
        cp_v_copy_arr_((cp_v_t*)vec, cp_v_esz_(vec), pos, elem_p, 1); \
    }while(0)

/**
 * Copy from one vector to another, overwriting.
 */
#define cp_v_copy(vec,pos,src,pos2,cnt) \
    cp_v_copy_1_(CP_GENSYM(_vec), CP_GENSYM(_vec2), (vec), (pos), (src), \
        (pos2), (cnt))

#define cp_v_copy_1_(vec,vec2,_vec,pos,src,pos2,cnt) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        __typeof__(*vec) const * vec2 = src; \
        assert(vec2 != NULL); \
        cp_v_copy_( \
            (cp_v_t*)vec, cp_v_esz_(vec), pos, (cp_v_t const *)vec2, pos2, cnt); \
    }while(0)

/**
 * Copy from one vector to another without resizing.
 */
#define cp_v_copy_arr(vec,pos,src,pos2,cnt) \
    cp_v_copy_arr_1_(CP_GENSYM(_cnt), CP_GENSYM(_pos), CP_GENSYM(_pos2), \
        CP_GENSYM(_vec), CP_GENSYM(_vec2), (vec), (pos), (src), (pos2), \
        (cnt))

#define cp_v_copy_arr_1_(cnt,pos,pos2,vec,vec2,_vec,_pos,src,_pos2,_cnt) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        size_t pos = _pos; \
        size_t pos2 = _pos2; \
        size_t cnt = _cnt; \
        assert(vec != NULL); \
        __typeof__(*vec) const * vec2 = src; \
        assert(vec2 != NULL); \
        assert(pos <= vec->size); \
        assert(cnt <= vec->size); \
        assert(pos + cnt <= vec->size); \
        assert(pos2 <= vec2->size); \
        assert(cnt <= vec2->size); \
        assert(pos2 + cnt <= vec2->size); \
        memmove(&vec->data[pos], &vec2->data[pos2], cp_v_esz_(vec) * cnt); \
    }while(0)

/**
 * Insert zero elements at a given position. */
#define cp_v_inflate(vec,pos,size) \
    cp_v_inflate_1_(CP_GENSYM(_vec), (vec), (pos), (size))

#define cp_v_inflate_1_(vec,_vec,pos,size) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        cp_v_inflate_((cp_v_t*)vec, cp_v_esz_(vec), pos, size); \
    }while(0)

/**
 * Insert multiple values from an array into the vector at a given position.
 */
#define cp_v_insert_arr(vec,pos,elem_,size) \
    cp_v_insert_arr_1_(CP_GENSYM(_elem), CP_GENSYM(_vec), (vec), (pos), \
        (elem_), (size))

#define cp_v_insert_arr_1_(elem,vec,_vec,pos,elem_,size) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        __typeof__(*vec->data) const * elem = elem_; \
        cp_v_insert_arr_((cp_v_t*)vec, cp_v_esz_(vec), pos, elem, size); \
    }while(0)

/**
 * Insert one value into the vector at a given position
 */
#define cp_v_insert1(vec,pos,elem_) \
    cp_v_insert1_1_(CP_GENSYM(_elem), CP_GENSYM(_vec), (vec), (pos), \
        (elem_))

#define cp_v_insert1_1_(elem,vec,_vec,pos,elem_) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        __typeof__(*vec->data) elem = elem_; \
        cp_v_insert_arr_((cp_v_t*)vec, cp_v_esz_(vec), pos, &elem, 1); \
    }while(0)

/**
 * Insert one vector into the other at a given position
 */
#define cp_v_insert(vec,pos,vec2) \
    cp_v_insert_1_(CP_GENSYM(_data_), CP_GENSYM(_vec), CP_GENSYM(_vec2), \
        (vec), (pos), (vec2))

#define cp_v_insert_1_(data_,vec,vec2,_vec,pos,_vec2) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        __typeof__(*_vec2) *vec2 = _vec2; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        assert(vec2 != NULL); \
        __typeof__(*vec->data) const * data_ = vec2->data; \
        cp_v_insert_arr_( \
            (cp_v_t*)vec, cp_v_esz_(vec), pos, data_, vec2->size); \
    }while(0)

/**
 * Remove elements from the vector
 */
#define cp_v_remove(vec,pos,size) \
    cp_v_remove_1_(CP_GENSYM(_vec), (vec), (pos), (size))

#define cp_v_remove_1_(vec,_vec,pos,size) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        cp_v_remove_((cp_v_t*)vec, cp_v_esz_(vec), pos, size); \
    }while(0)

/**
 * Reverse the order of elements in part of the vector.
 */
#define cp_v_reverse(vec,pos,size) \
    cp_v_reverse_1_(CP_GENSYM(_vec), (vec), (pos), (size))

#define cp_v_reverse_1_(vec,_vec,pos,size) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        cp_v_reverse_((cp_v_t*)vec, cp_v_esz_(vec), pos, size); \
    }while(0)

/**
 * Extract an element from the vector.
 */
#define cp_v_extract(vec,pos) \
    cp_v_extract_1_(CP_GENSYM(_elem), CP_GENSYM(_vec), (vec), (pos))

#define cp_v_extract_1_(elem,vec,_vec,pos) \
    ({ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        CP_NEED_ALLOC_(vec); \
        __typeof__(*vec->data) elem \
        cp_v_extract_(&elem, (cp_v_t*)vec, cp_v_esz_(vec), pos); \
        elem; \
    })

/**
 * Sort a portion of the vector.
 */
#define cp_v_qsort(vec,pos,size,cmp,user) \
    cp_v_qsort_1_(CP_GENSYM(_l_cmp), CP_GENSYM(_user), CP_GENSYM(_vec), \
        (vec), (pos), (size), (cmp), (user))

#define cp_v_qsort_1_(_l_cmp,user,vec,_vec,pos,size,cmp,_user) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        __typeof__(_user) user = _user; \
        assert(vec != NULL); \
        int (* _l_cmp)( \
            __typeof__(*vec->data) const *, \
            __typeof__(*vec->data) const *, \
            __typeof__(*user) *) = cmp; \
        cp_v_qsort_((cp_v_t*)vec, cp_v_esz_(vec), pos, size, \
            (int(*)(void const *, void const *, void *))_l_cmp, \
            (void *)(size_t)user); \
    }while(0)

/**
 * Binary search a key in the vector.
 */
#define cp_v_bsearch(key,vec,cmp,user) \
    cp_v_bsearch_1_(CP_GENSYM(_l_cmp), CP_GENSYM(_key), CP_GENSYM(_user), \
        CP_GENSYM(_vec), (key), (vec), (cmp), (user))

#define cp_v_bsearch_1_(_l_cmp,key,user,vec,_key,_vec,cmp,_user) \
    ({ \
        __typeof__(*_key) *key = _key; \
        __typeof__(*_vec) *vec = _vec; \
        __typeof__(*_user) *user = _user; \
        assert(vec != NULL); \
        int (*_l_cmp)( \
            __typeof__(*key) const *, \
            __typeof__(*vec->data) const *, \
            __typeof__(user)) = cmp; \
        cp_v_bsearch_( \
            (void const *)key, \
            (cp_v_t*)vec, cp_v_esz_(vec), \
            (int(*)(void const *, void const *, void *))_l_cmp, \
            (void*)(size_t)user); \
    })

/**
 * Compute the index of a pointer into the vector.
 */
#define cp_v_idx(vec,ptr_) \
    cp_v_idx_1_(CP_GENSYM(_idx), CP_GENSYM(_ptr), CP_GENSYM(_vec), (vec), \
        (ptr_))

#define cp_v_idx_1_(idx,ptr,vec,_vec,ptr_) \
    ({ \
        __typeof__(*_vec) *vec = _vec; \
        __typeof__(*vec->data) const * ptr = ptr_; \
        assert(vec != NULL); \
        assert(ptr >= vec->data); \
        assert(ptr <= (vec->data + vec->size)); \
        size_t idx = CP_PTRDIFF(ptr, vec->data); \
        assert(idx < vec->size); \
        idx; \
    })

/**
* Initialises the vector with a copy of the given data.
*/
#define cp_v_init_with(vec,arr_,size) \
    cp_v_init_with_1_(CP_GENSYM(_arr), CP_GENSYM(_size), CP_GENSYM(_vec), \
        (vec), (arr_), (size))

#define cp_v_init_with_1_(arr,size,vec,_vec,arr_,_size) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        size_t size = _size; \
        cp_v_init0(vec, size); \
        __typeof__(*vec->data) const * arr = arr_; \
        assert(arr != NULL); \
        memcpy(vec->data, arr, sizeof(*arr) * size); \
    }while(0)

/**
 * Delete the vector and its contents.
 * Additional to cp_v_fini(), this also deletes the vector itself.
 */
#define cp_v_delete(vec) \
    cp_v_delete_1_(CP_GENSYM(_vec), (vec))

#define cp_v_delete_1_(vec,_vec) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        cp_v_fini(*vec); \
        CP_FREE(*vec); \
    }while(0)

/**
 * Remove the last element from the vector and return it.
 */
#define cp_v_pop(vec) \
    cp_v_pop_1_(CP_GENSYM(_vec), (vec))

#define cp_v_pop_1_(vec,_vec) \
    ({ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        cp_v_extract(vec, vec->size - 1); \
    })

/**
 * Append a zeroed element at the end of a vector and return a pointer to it.
 */
#define cp_v_push0(vec) \
    cp_v_push0_1_(CP_GENSYM(_vec), (vec))

#define cp_v_push0_1_(vec,_vec) \
    ({ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        cp_v_inflate(vec, vec->size, 1); \
        &vec->data[vec->size-1]; \
    })

/**
 * Append multiple elements to the end of a vector.
 */
#define cp_v_append_arr(vec,elem,size) \
    cp_v_append_arr_1_(CP_GENSYM(_size), CP_GENSYM(_vec), (vec), (elem), \
        (size))

#define cp_v_append_arr_1_(size,vec,_vec,elem,_size) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        __typeof__(_size) size = _size; \
        assert(vec != NULL); \
        cp_v_insert_arr(vec, vec->size, elem, size); \
    }while(0)

/**
 * Append a single element to the end of a vector and return a pointer to it.
 */
#define cp_v_push(vec,elem) \
    cp_v_push_1_(CP_GENSYM(_vec), (vec), (elem))

#define cp_v_push_1_(vec,_vec,elem) \
    ({ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        cp_v_insert1(vec, vec->size, elem); \
        &vec->data[vec->size-1]; \
    })

/**
 * Append a vector to another vector.
 */
#define cp_v_append(vec,vec2) \
    cp_v_append_1_(CP_GENSYM(_vec), (vec), (vec2))

#define cp_v_append_1_(vec,_vec,vec2) \
    do{ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        cp_v_insert(vec, vec->size, vec2); \
    }while(0)

/**
 * Reference to last element of vector
 */
#define cp_v_last(vec) \
    cp_v_last_1_(CP_GENSYM(_vec), (vec))

#define cp_v_last_1_(vec,_vec) \
    (*({ \
        __typeof__(*_vec) *vec = _vec; \
        assert(vec != NULL); \
        &vec->data[vec->size - 1]; \
    }))

/**
 * Reference to last but ith element of vector
 */
#define cp_v_last_but(vec,i) \
    cp_v_last_but_1_(CP_GENSYM(_i), CP_GENSYM(_vec), (vec), (i))

#define cp_v_last_but_1_(i,vec,_vec,_i) \
    (*({ \
        __typeof__(*_vec) *vec = _vec; \
        size_t i = _i; \
        assert(vec != NULL); \
        assert(i < vec->size); \
        &vec->data[vec->size - 1 - i]; \
    }))

/**
 * Pointer to an element of the vector, or NULL if index is out of range.
 */
#define cp_v_nth_ptr0(vec,i) \
    cp_v_nth_ptr0_1_(CP_GENSYM(_i), CP_GENSYM(_vec), (vec), (i))

#define cp_v_nth_ptr0_1_(i,vec,_vec,_i) \
    ({ \
        __typeof__(*_vec) *vec = _vec; \
        size_t i = _i; \
        ((vec != NULL) && (i < vec->size)) ? &vec->data[i] : NULL; \
    })

/**
 * Pointer to an element of the vector.
 */
#define cp_v_nth_ptr(vec,i) \
    cp_v_nth_ptr_1_(CP_GENSYM(_i), CP_GENSYM(_vec), (vec), (i))

#define cp_v_nth_ptr_1_(i,vec,_vec,_i) \
    ({ \
        __typeof__(*_vec) *vec = _vec; \
        size_t i = _i; \
        assert(vec != NULL); \
        assert((i < vec->size) || \
            (fprintf(stderr, "ERR: i=%"CP_Z"u, vec->size=%"CP_Z"u\n", \
                i, vec->size),0)); \
        &vec->data[i]; \
    })

/**
 * Reference to an element of the vector.
 */
#define cp_v_nth(vec,i) \
    (*( \
        cp_v_nth_ptr((vec), (i)) \
    ))

/**
 * Extract a bit from an integer vector.
 */
#define cp_v_bit_get(vec,i) \
    cp_v_bit_get_1_(CP_GENSYM(_i), CP_GENSYM(_ib), CP_GENSYM(_ik), \
        CP_GENSYM(_iv), CP_GENSYM(_vec), (vec), (i))

#define cp_v_bit_get_1_(i,ib,ik,iv,vec,_vec,_i) \
    ({ \
        __typeof__(*_vec) *vec = _vec; \
        size_t i = _i; \
        size_t ib = i / (8 * sizeof(*vec->data)); \
        assert(ib < vec->size); \
        size_t ik = i % (8 * sizeof(*vec->data)); \
        size_t iv = ((size_t)1) << ik; \
        ((vec->data[ib] & iv) != 0); \
    })

/**
 * Set a bit in an integer vector.
 */
#define cp_v_bit_set(vec,i,n) \
    cp_v_bit_set_1_(CP_GENSYM(_i), CP_GENSYM(_ib), CP_GENSYM(_ik), \
        CP_GENSYM(_iv), CP_GENSYM(_n), CP_GENSYM(_vec), (vec), (i), (n))

#define cp_v_bit_set_1_(i,ib,ik,iv,n,vec,_vec,_i,_n) \
    ({ \
        __typeof__(*_vec) *vec = _vec; \
        size_t i = _i; \
        size_t n = _n; \
        size_t ib = i / (8 * sizeof(*vec->data)); \
        assert(ib < vec->size); \
        size_t ik = i % (8 * sizeof(*vec->data)); \
        size_t iv = ((size_t)1) << ik; \
        vec->data[ib] = (vec->data[ib] | iv) ^ (n ? 0 : iv); \
    })

/* END MACRO */

#define CP_NEED_ALLOC_(vec) \
    ((void)(vec)->alloc)


/**
 * Internal: Shallow delete a vector: remove all sub-structures, but not the
 * elements and not the pointer itself.  The structure will then
 * be a clean, empty vector.
 *
 * This function allows v == NULL.
 */
extern void cp_v_fini_(
    cp_v_t *vec);

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
    cp_v_t *vec,
    /** Element size */
    size_t esz,
    /**
     * Number of guaranteed pre-allocated elements (the actual number
     * allocated may be larger). */
    size_t pre_alloc);

/**
 * Internal: Ensures a minimum size of the vector.
 *
 * This is like cp_v_set_size_, except it never shrinks the vector.
 */
extern void cp_v_ensure_size_(
    cp_v_t *vec,
    size_t esz,
    size_t new_size);

/**
 * Internal: Sets the size of the vector.
 *
 * If the vector is shrunk, any elements beyond the given size are discarded.
 *
 * If the vector is grown, it is filled with NULL elements at the end.
 */
extern void cp_v_set_size_(
    cp_v_t *vec,
    size_t esz,
    size_t new_size);

/**
 * Internal: Insert a sequence of NULL into a vector at a given position.
 * Returns the pointer to the first element of the newly inserted area.
 */
extern void *cp_v_inflate_(
    /** [IN/OUT] vector to append to */
    cp_v_t *vec,
    /** Element size */
    size_t esz,
    /** [IN] position where to insert (0..size) */
    size_t pos,
    /** [IN] number of elements to insert.  The elements will be
     * initialised with NULL. */
    size_t size);

/**
 * Internal: Overwrite part of a vector with values from a different array.
 *
 * The dst vector is grown as necessary.
 */
extern void cp_v_copy_arr_(
    /** Destination of the copying */
    cp_v_t *dst,
    /** Element size */
    size_t esz,
    /** The position where to copy the elements.  Must be 0..size of dst. */
    size_t dst_pos,
    /** Source of the copying */
    void const *data,
    /** Number of elements to copy from data */
    size_t size);

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
    size_t size);

/**
 * Internal: Insert an array of elements to a vector to a given position.
 * Returns the pointer to the first element of the newly inserted area.
 */
extern void *cp_v_insert_arr_(
    /** [IN/OUT] vector to append to */
    cp_v_t *dst,
    /** [IN] Element size */
    size_t esz,
    /** [IN] position where to insert (0..size) */
    size_t dst_pos,
    /** [IN] array of elements to fill in */
    void const *data,
    /** [IN] number of elements to fill in */
    size_t size);

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
    size_t size);

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
    size_t size);

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
    size_t pos);

/**
 * Internal: Sort (sub-)array using qsort() */
extern void cp_v_qsort_(
    /** [IN/OUT] vector to append to */
    cp_v_t *vec,
    /** Element size */
    size_t esz,
    /** [IN] start position of sort */
    size_t pos,
    /** [IN] end position of sort, CP_SIZE_MAX for up to end */
    size_t size,
    /** [IN] comparison function.
     *  This receives two pointers to elements (not the elements
     *  themselves, but pointers to them). */
    int (*cmp)(void const *, void const *, void *user),
    /** [IN] user pointer, passed as third argument to cmp */
    void *user);

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
    void *user);

static inline size_t cp_v_min_alloc_(void)
{
    return 4;
}

static inline size_t cp_v_max_size_(size_t esz)
{
    return CP_MAX_OF((size_t)0) / esz;
}

static inline size_t cp_v_size_(size_t count, size_t esz)
{
    assert(count <= cp_v_max_size_(esz));
    return count * esz;
}

/**
 * Search for something in the vector using bsearch().
 * Returns the index (not the pointer, to avoid problems
 * with 'const' casting) if found, or 'CP_SIZE_MAX' on error,
 * which is greater than the size of the vector.
 */
static inline size_t cp_v_bsearch_(
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

#endif /* CP_VEC_H_ */
