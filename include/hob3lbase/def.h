/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_DEF_H
#define __CP_DEF_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <math.h>

#include <hob3lbase/arch.h>
#include <hob3lbase/float.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#define cp_static_assert(x) _Static_assert(x,#x)

/* some assumptions about the environment */
cp_static_assert(sizeof(short)     == 2);
cp_static_assert(sizeof(int)       == 4);
cp_static_assert(sizeof(long long) == 8);
cp_static_assert(sizeof(void*)     == sizeof(size_t));

#define CP_IND 2

#define __CP_STRINGIFY2(x) #x
#define __CP_STRINGIFY1(x) __CP_STRINGIFY2(x)
#define CP_STRINGIFY(x)    __CP_STRINGIFY1(x)

/**
 * Make a bit mask of all ones for a given type, be sure to
 * only return a positive result, i.e., return the maximum value
 * for a given type. */
#define CP_MAX_OF(x) \
    ((__typeof__(x))(((~(1ULL << ((sizeof(x)*8)-1))) << (((0?(x):0)-1) > 0)) | 1))

cp_static_assert(CP_MAX_OF(0)    == 0x7fffffff);
cp_static_assert(CP_MAX_OF(0U)   == 0xffffffff);
cp_static_assert(CP_MAX_OF(0LL)  == 0x7fffffffffffffff);
cp_static_assert(CP_MAX_OF(0ULL) == 0xffffffffffffffff);

#define CP_SIZE_MAX (~(size_t)0)

/**
 * Boolean operation
 *
 * Used for the low-level algorithm.
 */
typedef enum {
    CP_OP_CUT = 0,
    CP_OP_XOR = 1,
    CP_OP_SUB = 2,
    CP_OP_ADD = 3,
} cp_bool_op_t;

/**
 * For some historic reason, a - b returns a signed type in C.  I do
 * not want that, I want size_t, and I will compile with -Wconversion,
 * so make that happen:
 */
#define CP_PTRDIFF(a,b) \
    ({ \
        assert((a) >= (b)); \
        (size_t)((a) - (b)); \
    })

/* ** #define ** */

#define __CP_CONCAT3(x,y) x##y
#define __CP_CONCAT2(x,y) __CP_CONCAT3(x,y)
#define CP_CONCAT(x,y) __CP_CONCAT2(x,y)

#define cp_is_pow2(x) \
    ({ \
        __typeof__(x) __x = (x); \
        ((__x != 0) && ((__x & (__x - 1)) == 0)); \
    })

#define cp_offsetof(T,F) (__builtin_offsetof(__typeof__(T),F))

#define cp_alignof(X) (__alignof__(X))

#define cp_countof(a) (sizeof(a)/sizeof((a)[0]))

/**
 * Zero a structure and return the given pointer.
 *
 * This works for whole arrays, provided a pointer to an array
 * is passed.  If an array is passed as it, this will be reinterpreted
 * by C to be a pointer to the first element, so only the first
 * element is cleared.  This is a problem of the C language, sorry.
 */
#define CP_ZERO(obj) memset(obj, 0, sizeof(*(obj)))

/**
 * Assign the prefix, zero the reset of the structure.
 *
 * This is for structs with a prefix that is not to be cleared, but initialised
 * from something else.
 */
#define CP_COPY_N_ZERO(obj, prefix, prefix_value) \
    ({ \
        __typeof__(*(obj)) *__obj = (obj); \
        cp_static_assert(cp_offsetof(__typeof__(*__obj), prefix) == 0); \
        __obj->prefix = (prefix_value); \
        size_t __psz = sizeof(__obj->prefix); \
        (void)memset(((char*)obj) + __psz, 0, sizeof(*(obj)) - __psz); \
    })

#ifdef __COUNTER__
#  define CP_GENSYM(name) CP_CONCAT(name, __COUNTER__)
#else
#  define CP_GENSYM(name) CP_CONCAT(name, __LINE__)
#endif

/**
 * Swap two structures.
 *
 * This works for whole arrays, provided a pointer to an array
 * is passed.  If an array is passed as it, this will be reinterpreted
 * by C to be a pointer to the first element, so only the first
 * element is cleared.  This is a problem of the C language, sorry.
 */
#define CP_SWAP(x,y) do{ \
        __typeof__(*(x)) *__xp = (x); \
        __typeof__(*(y)) *__yp = (y); \
        if (__xp != __yp) { \
            __typeof__(*(x)) __h = *__xp; \
            *__xp = *__yp; \
            *__yp = __h; \
        } \
    }while(0)

#define CP_ROUNDUP_DIV(a,b) \
    ({ \
        __typeof(a) __a = (a); \
        __typeof(b) __b = (b); \
        (__a + (__b - 1)) / __b; \
    })

/**
 * Bit-clear, &~ operation without sign conversion issues
 */
#define CP_BIC(a,b) (((__typeof__((a)|(b)))(a)) & (~((__typeof__((a)|(b)))(b))))

/**
 * If c, then set bit b in a, otherwise delete bit b in a
 */
#define CP_BIT_COPY(a,b,c) ((c) ? ((a) | (b)) : CP_BIC(a,b))

/* Helper to gensym local symbols for __cp_size_each */
#define __cp_size_each_aux(__skipZ, __n, i,n,skipA,skipZ) \
    size_t i = (skipA), \
         __skipZ = (skipZ), \
         __n = (n); \
    i + __skipZ < __n; \
    i++

/**
 * Helper macro to allow cp_range_each to have optional arguments.
 */
#define __cp_size_each(i,n,skipA,skipZ,...) \
    __cp_size_each_aux( \
        CP_GENSYM(__skipZ), CP_GENSYM(__n), i, n, skipA, skipZ)

/**
 * Iterator expression (for 'for') for a size_t iterator.
 *
 * This iterators or a C style 0-based range, i.e., from [0..n)
 * or [0..n-1].  From and back may be restricted by a given
 * number of entries so that in total, the range [skipA..n-skipB)
 * is iterated.
 *
 * \p i is the name of the iterator variable.  It will be newly
 * declared inside the loop.  There is no way to use an external
 * iteration variable.
 *
 * \p n is the number of elements size of the range.
 *
 * \p skipA is the number of steps to skip at the beginning.
 * Use 0 to start at the first step of the range.
 *
 * \p skipZ is the number of steps to skip at the end.  Use 0
 * to iterate up to the last step of the range.
 *
 * It skipA + skipZ is greater or equal to the number of steps
 * \p n, no iteration will be done at all.
 *
 * The macro takes care to only evalaute n, skipA and skipZ once.
 *
 * skipA and skipZ are optional.  If missing, 0 is assumed for either
 * of the two.
 */
#define cp_size_each(i,...) __cp_size_each(i, __VA_ARGS__, 0, 0)

/**
 * Helper macro for cp_arr_each.
 */
#define __cp_arr_each(i,v,skipA,skipZ,...) \
    cp_size_each(i, cp_countof(v), skipA, skipZ)

/**
 * Iterator expression for each index of an array.
 *
 * See cp_size_each() for details.
 */
#define cp_arr_each(i,...) __cp_arr_each(i, __VA_ARGS__, 0, 0)

/**
 * Address of a surrouning container of an embedded substructure.
 *
 * P must not be NULL.  See CP_BOX0_OF() instead.
 */
#define CP_BOX_OF(P,T,F) \
    ({ \
        __typeof__(*(P)) *__p = (P); \
        assert(__p != NULL); \
        ((T*)(((size_t)__p) - cp_offsetof(T,F))); \
    })

/**
 * Version of CP_BOX_OF() that maps NULL to NULL.
 */
#define CP_BOX0_OF(P,T,F) \
    ({ \
        __typeof__(*(P)) *__pA = (P); \
        (__pA == NULL ? NULL : CP_BOX_OF(__pA, T, F)); \
    })

/* To make object IDs unique to catch bugs, we define an offset
 * for each object type enum here. */
#define CP_TYPE_MASK      0xff00
#define CP_TYPE2_MASK     0xf000

#define CP_SYN_VALUE_TYPE 0x1100

#define CP_SYN_STMT_TYPE  0x2100

#define CP_SCAD_TYPE      0x3100

#define CP_CSG_TYPE       0x4000
#define CP_CSG2_TYPE      0x4100
#define CP_CSG3_TYPE      0x4200

/** Type ID that is never given to any object */
#define CP_ABSTRACT       0xffff

#ifndef NDEBUG
#  define CP_FILE __FILE__
#  define CP_LINE __LINE__
#else
#  define CP_FILE NULL
#  define CP_LINE 0
#endif

static inline bool strequ(char const *a, char const *b)
{
    return strcmp(a, b) == 0;
}

static inline bool strnequ(char const *a, char const *b, size_t n)
{
    return strncmp(a, b, n) == 0;
}

/**
 * Returns NULL if needle is not a prefix of haystack.
 * Returns the pointer to haystack+strlen(needle) otherwise.
 */
static inline char const *is_prefix(char const *haystack, char const *needle)
{
    size_t len = strlen(needle);
    if (strnequ(haystack, needle, len)) {
        return haystack + len;
    }
    return NULL;
}

static inline size_t cp_align_down(size_t n, size_t a)
{
    assert((a != 0) && "Alignment is zero");
    assert(((a & (a-1)) == 0) && "Alignment is not a power of 2");
    return n & -a;
}

static inline size_t cp_align_down_diff(size_t n, size_t a)
{
    return n - cp_align_down(n,a);
}

static inline size_t cp_align_up(size_t n, size_t a)
{
    return cp_align_down(n + a - 1, a);
}

static inline size_t cp_align_up_diff(size_t n, size_t a)
{
    return cp_align_up(n,a) - n;
}

#endif /* __CP_MAT_H */
