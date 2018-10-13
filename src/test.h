/* -*- Mode: C -*- */

#ifndef __CP_TEST_H
#define __CP_TEST_H

#include <stdio.h>

#define __TEST_CHECK(cond, cond_str) \
    ({ \
        if (!(cond)) { \
            __cp_test_fail(__FILE__, __LINE__, "Test failed: ", cond_str); \
        } \
    })

#define TEST_VOID(expr) \
    ({ \
        fprintf(stderr, "%s:%d: X %-20s   %s\n", __FILE__, __LINE__, "", #expr); \
        (expr); \
    })

#define TEST_EQ(expr, val) \
    ({ \
        fprintf(stderr, "%s:%d: X %-20s== %s\n", __FILE__, __LINE__, #val, #expr); \
        __typeof__(expr) __res = (expr); \
        __TEST_CHECK(__res == (val), "(" #expr ") == " #val); \
    })

#define TEST_NE(expr, val) \
    ({ \
        fprintf(stderr, "%s:%d: X %-20s!= %s\n", __FILE__, __LINE__, #val, #expr); \
        __typeof__(expr) __res = (expr); \
        __TEST_CHECK(__res != (val), "(" #expr ") != " #val); \
    })

#define TEST_FEQ(expr, val) \
    ({ \
        fprintf(stderr, "%s:%d: X %-20s===%s\n", __FILE__, __LINE__, #val, #expr); \
        __typeof__(expr) __res = (expr); \
        __TEST_CHECK(cp_eq(__res, val), "(" #expr ") === " #val); \
    })

#define TEST_FNE(expr, val) \
    ({ \
        fprintf(stderr, "%s:%d: X %-20s!==%s\n", __FILE__, __LINE__, #val, #expr); \
        __typeof__(expr) __res = (expr); \
        __TEST_CHECK(!cp_eq(__res, val), "(" #expr ") !== " #val); \
    })

#define TEST_RUN(test) \
    ({ \
        fprintf(stderr, "%s:%d: R %s\n", __FILE__, __LINE__, #test); \
        (test); \
    })

/**
 * Function to be called when a test fails.
 */
__attribute__((noreturn))
extern void __cp_test_fail(
    char const *file,
    int line,
    char const *msg1,
    char const *msg2);

#endif /* __CP_TEST_H */
