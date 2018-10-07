/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <cpmat/def.h>
#include <cpmat/ring.h>
#include "test.h"
#include "ring-test.h"

#define TEST_ORDER1(a) \
    ({ \
        TEST_EQ(cp_ring_next(a,a), a); \
    })

#define TEST_ORDER2(a, b) \
    ({ \
        TEST_EQ(cp_ring_next(a,b), a); \
        TEST_EQ(cp_ring_next(b,a), b); \
    })

#define TEST_ORDER3(a, b, c) \
    ({ \
        TEST_EQ(cp_ring_next(a,b), c); \
        TEST_EQ(cp_ring_next(b,a), c); \
        TEST_EQ(cp_ring_next(b,c), a); \
        TEST_EQ(cp_ring_next(c,b), a); \
        TEST_EQ(cp_ring_next(a,c), b); \
        TEST_EQ(cp_ring_next(c,a), b); \
    })

#define TEST_ORDER4(a, b, c, d) \
    ({ \
        TEST_EQ(cp_ring_next(a,b), c); \
        TEST_EQ(cp_ring_next(b,a), d); \
        TEST_EQ(cp_ring_next(b,c), d); \
        TEST_EQ(cp_ring_next(c,b), a); \
        TEST_EQ(cp_ring_next(c,d), a); \
        TEST_EQ(cp_ring_next(d,c), b); \
        TEST_EQ(cp_ring_next(d,a), b); \
        TEST_EQ(cp_ring_next(a,d), c); \
    })

#define TEST_ORDER5(a, b, c, d, e) \
    ({ \
        TEST_EQ(cp_ring_next(a,b), c); \
        TEST_EQ(cp_ring_next(b,a), e); \
        TEST_EQ(cp_ring_next(b,c), d); \
        TEST_EQ(cp_ring_next(c,b), a); \
        TEST_EQ(cp_ring_next(c,d), e); \
        TEST_EQ(cp_ring_next(d,c), b); \
        TEST_EQ(cp_ring_next(d,e), a); \
        TEST_EQ(cp_ring_next(e,d), c); \
        TEST_EQ(cp_ring_next(e,a), b); \
        TEST_EQ(cp_ring_next(a,e), d); \
    })

#define TEST_ORDER3_MIRR(a, b, c) \
    ({ \
        TEST_EQ(cp_ring_next(a,b), c); \
        TEST_EQ(cp_ring_next(b,a), b); \
        TEST_EQ(cp_ring_next(b,c), b); \
        TEST_EQ(cp_ring_next(c,b), a); \
    })

#define TEST_ORDER4_MIRR(a, b, c, d) \
    ({ \
        TEST_EQ(cp_ring_next(a,b), c); \
        TEST_EQ(cp_ring_next(b,a), b); \
        TEST_EQ(cp_ring_next(b,c), d); \
        TEST_EQ(cp_ring_next(c,b), a); \
        TEST_EQ(cp_ring_next(c,d), c); \
        TEST_EQ(cp_ring_next(d,c), b); \
    })

#define TEST_ORDER5_MIRR(a, b, c, d, e) \
    ({ \
        TEST_EQ(cp_ring_next(a,b), c); \
        TEST_EQ(cp_ring_next(b,a), b); \
        TEST_EQ(cp_ring_next(b,c), d); \
        TEST_EQ(cp_ring_next(c,b), a); \
        TEST_EQ(cp_ring_next(c,d), e); \
        TEST_EQ(cp_ring_next(d,c), b); \
        TEST_EQ(cp_ring_next(d,e), d); \
        TEST_EQ(cp_ring_next(e,d), c); \
    })

static void show(cp_ring_t *base, cp_ring_t *a, cp_ring_t *b)
{
    fprintf(stderr, "RING: ");
    fprintf(stderr, "%"_Pz"u--", CP_PTRDIFF(a, base));
    if (a != b) {
        fprintf(stderr, "%"_Pz"u--", CP_PTRDIFF(b, base));
        for (cp_ring_each(n,a,b)) {
            fprintf(stderr, "%"_Pz"u--", CP_PTRDIFF(n, base));
            if (cp_ring_is_mirr(n)) {
                fprintf(stderr, "|");
            }
        }
    }
    fprintf(stderr, "\n");
}

/**
 * Unit tests for ring data structure.
 */
extern void cp_ring_test(void)
{
    cp_ring_t n[5];
    TEST_VOID(CP_ZERO(&n));

    for (cp_arr_each(i, n)) {
        TEST_VOID(cp_ring_init(&n[i]));
        TEST_ORDER1(&n[i]);
    }

    TEST_VOID(cp_ring_swap2(&n[0], &n[0], &n[0], &n[0]));

    TEST_ORDER1(&n[0]);

    TEST_VOID(cp_ring_swap2(&n[0], &n[0], &n[1], &n[1]));

    TEST_ORDER1(&n[0]);
    TEST_ORDER1(&n[1]);

    TEST_VOID(cp_ring_pair(&n[0], &n[1]));

    TEST_ORDER2(&n[0], &n[1]);

    TEST_VOID(cp_ring_swap(&n[0], &n[1]));

    TEST_ORDER2(&n[0], &n[1]);

    TEST_VOID(cp_ring_insert_between(&n[0], &n[2], &n[1]));

    show(&n[0], &n[0], &n[2]);
    TEST_ORDER3(&n[0], &n[2], &n[1]);

    TEST_VOID(cp_ring_remove2(&n[1], &n[2]));

    show(&n[0], &n[0], &n[2]);
    TEST_ORDER2(&n[0], &n[2]);
    TEST_ORDER1(&n[1]);

    TEST_VOID(cp_ring_insert_between(&n[0], &n[3], &n[2]));

    show(&n[0], &n[0], &n[3]);
    TEST_ORDER3(&n[0], &n[3], &n[2]);

    /* insert([0]--2, [1]--3) == 0--1--3--2 */
    TEST_VOID(cp_ring_insert_between(&n[0], &n[1], &n[3]));

    show(&n[0], &n[0], &n[1]);
    TEST_ORDER4(&n[0], &n[1], &n[3], &n[2]);

    TEST_VOID(cp_ring_swap(&n[1], &n[2]));

    show(&n[0], &n[0], &n[2]);
    TEST_ORDER4(&n[0], &n[2], &n[3], &n[1]);

    TEST_VOID(cp_ring_swap(&n[1], &n[3]));
    show(&n[0], &n[0], &n[2]);
    TEST_ORDER4(&n[0], &n[2], &n[1], &n[3]);

    TEST_VOID(cp_ring_rewire(&n[0], &n[2], &n[3], &n[1]));

    show(&n[0], &n[0], &n[3]);
    show(&n[0], &n[1], &n[2]);
    TEST_ORDER2(&n[0], &n[3]);
    TEST_ORDER2(&n[1], &n[2]);

    TEST_VOID(cp_ring_rewire(&n[0], &n[3], &n[3], &n[0]));
    TEST_ORDER2(&n[0], &n[3]);

    TEST_VOID(cp_ring_rewire(&n[0], &n[3], &n[0], &n[3]));
    TEST_ORDER1(&n[0]);
    TEST_ORDER1(&n[3]);

    TEST_VOID(cp_ring_pair(&n[0], &n[3]));
    show(&n[0], &n[0], &n[3]);
    TEST_ORDER2(&n[0], &n[3]);

    TEST_VOID(cp_ring_remove(&n[1]));
    TEST_ORDER1(&n[1]);
    TEST_ORDER1(&n[2]);

    TEST_VOID(cp_ring_insert_after(&n[3], &n[0], &n[1]));
    show(&n[0], &n[0], &n[1]);
    TEST_ORDER3(&n[0], &n[1], &n[3]);

    TEST_VOID(cp_ring_insert_after(&n[0], &n[1], &n[2]));

    show(&n[0], &n[0], &n[1]);
    TEST_ORDER4(&n[0], &n[1], &n[2], &n[3]);

    TEST_VOID(cp_ring_rewire(&n[0], &n[1], &n[2], &n[3]));

    show(&n[0], &n[0], &n[2]);
    TEST_ORDER4(&n[0], &n[2], &n[1], &n[3]);

    TEST_VOID(cp_ring_insert_after(&n[1], &n[3], &n[4]));

    TEST_ORDER5(&n[0], &n[2], &n[1], &n[3], &n[4]);

    TEST_VOID(cp_ring_swap_pair(&n[2], &n[1]));

    TEST_ORDER5(&n[0], &n[1], &n[2], &n[3], &n[4]);

    TEST_VOID(cp_ring_cut(&n[2], &n[3]));
    TEST_VOID(cp_ring_cut(&n[0], &n[4]));

    show(&n[0], &n[0], &n[1]);
    show(&n[0], &n[3], &n[4]);
    TEST_ORDER3_MIRR(&n[0], &n[1], &n[2]);
    TEST_ORDER2(&n[3], &n[4]);

    TEST_VOID(cp_ring_join(&n[2], &n[4]));
    show(&n[0], &n[0], &n[1]);
    TEST_ORDER5_MIRR(&n[0], &n[1], &n[2], &n[4], &n[3]);
    TEST_EQ(cp_ring_is_mirr(&n[0]), true);
    TEST_EQ(cp_ring_is_mirr(&n[1]), false);
    TEST_EQ(cp_ring_is_mirr(&n[2]), false);
    TEST_EQ(cp_ring_is_mirr(&n[4]), false);
    TEST_EQ(cp_ring_is_mirr(&n[3]), true);

    TEST_VOID(cp_ring_cut(&n[4], &n[3]));
    show(&n[0], &n[0], &n[1]);
    TEST_ORDER4_MIRR(&n[0], &n[1], &n[2], &n[4]);

    TEST_VOID(cp_ring_remove(&n[4]));
    show(&n[0], &n[0], &n[1]);
    TEST_ORDER1(&n[4]);
    TEST_ORDER3_MIRR(&n[0], &n[1], &n[2]);

    TEST_VOID(cp_ring_join(&n[4], &n[4]));
    show(&n[0], &n[4], &n[4]);
    TEST_ORDER1(&n[4]);

    TEST_VOID(cp_ring_join(&n[0], &n[2]));
    show(&n[0], &n[0], &n[1]);
    TEST_ORDER3(&n[0], &n[1], &n[2]);

    TEST_VOID(cp_ring_remove(&n[2]));
    show(&n[0], &n[0], &n[1]);
    TEST_ORDER2(&n[0], &n[1]);

    TEST_VOID(cp_ring_cut(&n[0], &n[1]));
    TEST_ORDER1(&n[0]);
    TEST_ORDER1(&n[1]);

    TEST_VOID(cp_ring_pair(&n[0], &n[1]));
    show(&n[0], &n[0], &n[1]);
    TEST_VOID(cp_ring_insert_after(&n[0], &n[1], &n[2]));
    show(&n[0], &n[0], &n[1]);
    TEST_ORDER3(&n[0], &n[1], &n[2]);

    TEST_VOID(cp_ring_cut(&n[2], &n[0]));
    show(&n[0], &n[0], &n[1]);
    TEST_ORDER3_MIRR(&n[0], &n[1], &n[2]);

    TEST_VOID(cp_ring_cut(&n[1], &n[2]));
    show(&n[0], &n[0], &n[1]);
    TEST_ORDER2(&n[0], &n[1]);
    TEST_ORDER1(&n[2]);

    TEST_VOID(cp_ring_join(&n[1], &n[2]));
    show(&n[0], &n[0], &n[1]);
    TEST_ORDER3_MIRR(&n[0], &n[1], &n[2]);
}
