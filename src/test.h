/* -*- Mode: C -*- */

#ifndef CP_TEST_H_
#define CP_TEST_H_

#include <stdio.h>

#define TEST_CHECK_(cond, cond_str) \
    ({ \
        if (!(cond)) { \
            cp_test_fail_(__FILE__, __LINE__, "Test failed: ", cond_str); \
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
        TEST_CHECK_(__res == (val), "(" #expr ") == " #val); \
    })

#define TEST_NE(expr, val) \
    ({ \
        fprintf(stderr, "%s:%d: X %-20s!= %s\n", __FILE__, __LINE__, #val, #expr); \
        __typeof__(expr) __res = (expr); \
        TEST_CHECK_(__res != (val), "(" #expr ") != " #val); \
    })

#define TEST_FEQ(expr, val) \
    ({ \
        fprintf(stderr, "%s:%d: X %-20s===%s\n", __FILE__, __LINE__, #val, #expr); \
        __typeof__(expr) __res = (expr); \
        TEST_CHECK_(cp_eq(__res, val), "(" #expr ") === " #val); \
    })

#define TEST_FNE(expr, val) \
    ({ \
        fprintf(stderr, "%s:%d: X %-20s!==%s\n", __FILE__, __LINE__, #val, #expr); \
        __typeof__(expr) __res = (expr); \
        TEST_CHECK_(!cp_eq(__res, val), "(" #expr ") !== " #val); \
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
extern void cp_test_fail_(
    char const *file,
    int line,
    char const *msg1,
    char const *msg2);

#endif /* CP_TEST_H_ */
