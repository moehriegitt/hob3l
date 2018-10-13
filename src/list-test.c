/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <cpmat/def.h>
#include <cpmat/list.h>
#include "test.h"
#include "list-test.h"

typedef struct num num_t;

struct num {
    size_t value; /* currently unused */
    num_t *next, *prev;
};

#define TEST_ORDER1(a) \
    ({ \
        TEST_EQ((a)->next, (a)); \
        TEST_EQ((a)->prev, (a)); \
    })

#define TEST_ORDER2(a, b) \
    ({ \
        TEST_EQ((a)->next, (b)); \
        TEST_EQ((b)->prev, (a)); \
        TEST_EQ((b)->next, (a)); \
        TEST_EQ((a)->prev, (b)); \
    })

#define TEST_ORDER3(a, b, c) \
    ({ \
        TEST_EQ((a)->next, (b)); \
        TEST_EQ((b)->prev, (a)); \
        TEST_EQ((b)->next, (c)); \
        TEST_EQ((c)->prev, (b)); \
        TEST_EQ((c)->next, (a)); \
        TEST_EQ((a)->prev, (c)); \
    })

#define TEST_ORDER4(a, b, c, d) \
    ({ \
        TEST_EQ((a)->next, (b)); \
        TEST_EQ((b)->prev, (a)); \
        TEST_EQ((b)->next, (c)); \
        TEST_EQ((c)->prev, (b)); \
        TEST_EQ((c)->next, (d)); \
        TEST_EQ((d)->prev, (c)); \
        TEST_EQ((d)->next, (a)); \
        TEST_EQ((a)->prev, (d)); \
    })

static void show(num_t *base, num_t *start)
{
    num_t *n = start;
    fprintf(stderr, "LIST: ");
    for(;;) {
        fprintf(stderr, "%"_Pz"u->", CP_PTRDIFF(n, base));
        if (n->next == start) {
            break;
        }
        assert(n->prev->next == n);
        assert(n->next->prev == n);
        n = n->next;
    }
    fprintf(stderr, "\n");
}

/**
 * Unit tests for list data structure
 */
extern void cp_list_test(void)
{
    num_t n[5] = {{0}};

    TEST_EQ(n[0].next, NULL);
    TEST_EQ(n[0].prev, NULL);

    TEST_VOID(cp_list_init(&n[0]));

    TEST_EQ(n[0].next, &n[0]);
    TEST_EQ(n[0].prev, &n[0]);

    TEST_VOID(cp_list_swap(&n[0], &n[0]));

    TEST_EQ(n[0].next, &n[0]);
    TEST_EQ(n[0].prev, &n[0]);

    TEST_VOID(cp_list_init(&n[1]));

    TEST_VOID(cp_list_swap(&n[0], &n[1]));

    TEST_EQ(n[0].next, &n[0]);
    TEST_EQ(n[0].prev, &n[0]);
    TEST_EQ(n[1].next, &n[1]);
    TEST_EQ(n[1].prev, &n[1]);

    TEST_VOID(cp_list_insert(&n[0], &n[1]));

    TEST_EQ(n[0].next, &n[1]);
    TEST_EQ(n[0].prev, &n[1]);
    TEST_EQ(n[1].next, &n[0]);
    TEST_EQ(n[1].prev, &n[0]);

    TEST_VOID(cp_list_swap(&n[0], &n[1]));

    TEST_EQ(n[0].next, &n[1]);
    TEST_EQ(n[0].prev, &n[1]);
    TEST_EQ(n[1].next, &n[0]);
    TEST_EQ(n[1].prev, &n[0]);

    TEST_VOID(cp_list_init(&n[2]));

    TEST_VOID(cp_list_insert(&n[0], &n[2]));

    TEST_EQ(n[0].next, &n[2]);
    TEST_EQ(n[0].prev, &n[1]);
    TEST_EQ(n[1].next, &n[0]);
    TEST_EQ(n[1].prev, &n[2]);
    TEST_EQ(n[2].next, &n[1]);
    TEST_EQ(n[2].prev, &n[0]);

    TEST_VOID(cp_list_remove(&n[1]));

    TEST_EQ(n[0].next, &n[2]);
    TEST_EQ(n[0].prev, &n[2]);
    TEST_EQ(n[2].next, &n[0]);
    TEST_EQ(n[2].prev, &n[0]);

    TEST_EQ(n[1].next, &n[1]);
    TEST_EQ(n[1].prev, &n[1]);

    TEST_VOID(cp_list_init(&n[3]));

    TEST_VOID(cp_list_insert(&n[1], &n[3]));
    TEST_EQ(n[1].next, &n[3]);
    TEST_EQ(n[1].prev, &n[3]);

    /* insert([0]--2, [1]--3) == 0--1--3--2 */
    TEST_VOID(cp_list_insert(&n[0], &n[1]));

    show(&n[0], &n[0]);
    TEST_ORDER4(&n[0], &n[1], &n[3], &n[2]);

    TEST_VOID(cp_list_swap(&n[1], &n[2]));

    show(&n[0], &n[0]);
    TEST_ORDER4(&n[0], &n[2], &n[3], &n[1]);

    TEST_VOID(cp_list_swap(&n[1], &n[3]));
    show(&n[0], &n[0]);
    TEST_ORDER4(&n[0], &n[2], &n[1], &n[3]);

    TEST_VOID(cp_list_split(&n[0], &n[3]));
    show(&n[0], &n[0]);
    show(&n[0], &n[1]);
    TEST_ORDER2(&n[0], &n[3]);
    TEST_ORDER2(&n[1], &n[2]);

    TEST_VOID(cp_list_insert(&n[0], &n[1]));
    show(&n[0], &n[0]);
    TEST_ORDER4(&n[0], &n[1], &n[2], &n[3]);

    TEST_VOID(cp_list_split(&n[0], &n[2]));
    show(&n[0], &n[0]);
    show(&n[0], &n[1]);
    TEST_ORDER3(&n[0], &n[2], &n[3]);
    TEST_ORDER1(&n[1]);
}
