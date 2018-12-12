/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_DEF_H_
#define CP_DEF_H_

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <math.h>

#include <hob3lbase/arch.h>
#include <hob3lbase/float.h>

#define CP_UNUSED      __attribute__((__unused__))
#define CP_NORETURN    __attribute__((__noreturn__))
#define CP_PRINTF(X,Y) __attribute__((__format__(__printf__,X,Y)))
#define CP_VPRINTF(X)  __attribute__((__format__(__printf__,X,0)))

#define CP_STATIC_ASSERT(x) _Static_assert(x,#x)

/* some assumptions about the environment */
CP_STATIC_ASSERT(sizeof(short)     == 2);
CP_STATIC_ASSERT(sizeof(int)       == 4);
CP_STATIC_ASSERT(sizeof(long long) == 8);
CP_STATIC_ASSERT(sizeof(void*)     == sizeof(size_t));

/**
 * Make a bit mask of all ones for a given type, be sure to
 * only return a positive result, i.e., return the maximum value
 * for a given type. */
#define CP_MAX_OF(x) \
    ((__typeof__(x))(((~(1ULL << ((sizeof(x)*8)-1))) << ((((__typeof__(x))0)-1) > 0)) | 1))

CP_STATIC_ASSERT(CP_MAX_OF(0)    == 0x7fffffff);
CP_STATIC_ASSERT(CP_MAX_OF(0U)   == 0xffffffff);
CP_STATIC_ASSERT(CP_MAX_OF(0LL)  == 0x7fffffffffffffff);
CP_STATIC_ASSERT(CP_MAX_OF(0ULL) == 0xffffffffffffffff);

CP_STATIC_ASSERT(CP_MAX_OF(short)    == 0x7fff);
CP_STATIC_ASSERT(CP_MAX_OF(int)      == 0x7fffffff);
CP_STATIC_ASSERT(CP_MAX_OF(unsigned) == 0xffffffff);

#define CP_SIZE_MAX (~(size_t)0)

CP_STATIC_ASSERT(CP_MAX_OF(size_t) == CP_SIZE_MAX);

/**
 * For some historic reason, a - b returns a signed type in C.  I do
 * not want that, I want size_t, and I will compile with -Wconversion,
 * so make that happen:
 */
#define CP_PTRDIFF(a,b) \
    CP_PTRDIFF_1_(CP_GENSYM(_a), CP_GENSYM(_b), (a), (b))

#define CP_PTRDIFF_1_(a, b, _a, _b) \
    ({ \
        __typeof__(*_a) const *a = _a; \
        __typeof__(*_b) const *b = _b; \
        assert(a >= b); \
        (size_t)(a - b); \
    })

/* ** #define ** */

#define CP_IND 2

#define CP_STRINGIFY_2_(x) #x
#define CP_STRINGIFY_1_(x) CP_STRINGIFY_2_(x)
#define CP_STRINGIFY(x)    CP_STRINGIFY_1_(x)

#define CP_CONCAT_3_(x,y) x##y
#define CP_CONCAT_2_(x,y) CP_CONCAT_3_(x,y)
#define CP_CONCAT(x,y) CP_CONCAT_2_(x,y)

#define cp_is_pow2(x) \
    ({ \
        __typeof__(x) _x = (x); \
        ((_x != 0) && ((_x & (_x - 1)) == 0)); \
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
        __typeof__(*(obj)) *_obj = (obj); \
        CP_STATIC_ASSERT(cp_offsetof(*_obj, prefix) == 0); \
        _obj->prefix = (prefix_value); \
        size_t _psz = sizeof(_obj->prefix); \
        (void)memset(((char*)obj) + _psz, 0, sizeof(*(obj)) - _psz); \
    })

#define CP_GENSYM(name) CP_CONCAT(name, __LINE__)

/**
 * Swap two structures.
 *
 * This works for whole arrays, provided a pointer to an array
 * is passed.  If an array is passed as it, this will be reinterpreted
 * by C to be a pointer to the first element, so only the first
 * element is cleared.  This is a problem of the C language, sorry.
 */
#define CP_SWAP(x,y) do{ \
        __typeof__(*(x)) *_xp = (x); \
        __typeof__(*(y)) *_yp = (y); \
        if (_xp != _yp) { \
            __typeof__(*(x)) _h = *_xp; \
            *_xp = *_yp; \
            *_yp = _h; \
        } \
    }while(0)

#define CP_ROUNDUP_DIV(a,b) \
    ({ \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        (_a + (_b - 1)) / _b; \
    })

/**
 * Bit-clear, &~ operation without sign conversion issues
 */
#define CP_BIC(a,b) (((__typeof__((a)|(b)))(a)) & (~((__typeof__((a)|(b)))(b))))

/**
 * If c, then set bit b in a, otherwise delete bit b in a
 */
#define CP_BIT_COPY(a,b,c) ((c) ? ((a) | (b)) : CP_BIC(a,b))

/* Helper to gensym local symbols for cp_size_each_1_ */
#define cp_size_each_2_(_skipZ, _n, i,n,skipA,skipZ) \
    size_t i = (skipA), \
         _skipZ = (skipZ), \
         _n = (n); \
    i + _skipZ < _n; \
    i++

/**
 * Helper macro to allow cp_range_each to have optional arguments.
 */
#define cp_size_each_1_(i,n,skipA,skipZ,...) \
    cp_size_each_2_( \
        CP_GENSYM(_skipZ), CP_GENSYM(_n), i, n, skipA, skipZ)

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
#define cp_size_each(i,...) cp_size_each_1_(i, __VA_ARGS__, 0, 0)

/**
 * Helper macro for cp_arr_each.
 */
#define cp_arr_each_1_(i,v,skipA,skipZ,...) \
    cp_size_each(i, cp_countof(v), skipA, skipZ)

/**
 * Iterator expression for each index of an array.
 *
 * See cp_size_each() for details.
 */
#define cp_arr_each(i,...) cp_arr_each_1_(i, __VA_ARGS__, 0, 0)

/**
 * Address of a surrouning container of an embedded substructure.
 *
 * P must not be NULL.  See CP_BOX0_OF() instead.
 */
#define CP_BOX_OF(P,T,F) \
    CP_BOX_OF_1_(CP_GENSYM(_p), (P), T, F)

#define CP_BOX_OF_1_(_p,P,T,F) \
    ({ \
        __typeof__(*P) *_p = P; \
        assert(_p != NULL); \
        ((__typeof__(T)*)(((size_t)_p) - cp_offsetof(__typeof__(T),F))); \
    })

/**
 * Version of CP_BOX_OF() that maps NULL to NULL.
 */
#define CP_BOX0_OF(P,T,F) \
    ({ \
        __typeof__(*(P)) *_pA = (P); \
        (_pA == NULL ? NULL : CP_BOX_OF(_pA, T, F)); \
    })

/* To make object IDs unique to catch bugs, we define an offset
 * for each object type enum here. */
#define CP_TYPE_MASK       0xff00
#define CP_TYPE2_MASK      0xf000

#define CP_SYN_VALUE_TYPE  0x1100

#define CP_SYN_STMT_TYPE   0x2100

#define CP_SCAD_TYPE       0x3000
#define CP_SCAD_REC_TYPE   0x3100

#define CP_CSG_TYPE        0x4000
#define CP_CSG2_TYPE       0x4100
#define CP_CSG3_TYPE       0x4200

/** Type ID that is never given to any object */
#define CP_ABSTRACT       0xffff

#ifndef NDEBUG
#  define CP_FILE __FILE__
#  define CP_LINE __LINE__
#else
#  define CP_FILE NULL
#  define CP_LINE 0
#endif

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

#endif /* CP_MAT_H_ */
