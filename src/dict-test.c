/* -*- Mode: C -*- */

#include <stdlib.h>
#include <cpmat/dict.h>
#include <cpmat/alloc.h>
#include "test.h"

typedef struct {
    size_t value;
    cp_dict_t node;
} num_t;

static int cmp_size(size_t a, size_t b)
{
    return a < b ? -1 : a > b ? +1 : 0;
}

static size_t *num_value_ptr(
    cp_dict_t *_a)
{
    num_t *a = CP_BOX_OF(_a, num_t, node);
    return &a->value;
}

static size_t num_value(
    cp_dict_t *_a)
{
    return *num_value_ptr(_a);
}

static int cmp_num_f(
    size_t *a,
    cp_dict_t *_b,
    void *user __unused)
{
    return cmp_size(*a, num_value(_b));
}

static int cmp_num(
    cp_dict_t *_a,
    cp_dict_t *_b,
    void *user)
{
    return cmp_num_f(num_value_ptr(_a), _b, user);
}

static num_t *num_new(size_t x)
{
    num_t *r = CP_NEW(*r);
    r->value = x;
    return r;
}

static size_t dict_size(cp_dict_t *r)
{
    if (r == NULL) {
        return 0;
    }
    return dict_size(r->edge[0]) + dict_size(r->edge[1]) + 1;
}

static double drand(double r, double m, double n)
{
    return floor((r / m) * n);
}

static size_t irand(size_t n)
{
    return (size_t)lrint(drand((unsigned)rand(), RAND_MAX, cp_double(n)));
}

extern void cp_dict_test(void)
{
    cp_dict_t *r = NULL;
    TEST_EQ(
        cp_dict_find((size_t[1]){50}, r, cmp_num_f, NULL),
        NULL);

    TEST_EQ(
        cp_dict_insert(&num_new(50)->node, &r, cmp_num, NULL, 0),
        NULL);

    cp_dict_t *r50;
    TEST_NE(
        (r50 = cp_dict_find((size_t[1]){50}, r, cmp_num_f, NULL)),
        NULL);

    TEST_EQ(num_value(r50), 50);
    TEST_EQ(dict_size(r), 1);

    TEST_EQ(
        cp_dict_insert(&num_new(20)->node, &r, cmp_num, NULL, 0),
        NULL);
    TEST_EQ(dict_size(r), 2);

    TEST_EQ(
        cp_dict_insert(&num_new(60)->node, &r, cmp_num, NULL, 0),
        NULL);
    TEST_EQ(dict_size(r), 3);

    TEST_EQ(
        cp_dict_insert(&num_new(70)->node, &r, cmp_num, NULL, 0),
        NULL);
    TEST_EQ(dict_size(r), 4);

    TEST_EQ(
        cp_dict_insert(&num_new(80)->node, &r, cmp_num, NULL, 0),
        NULL);
    TEST_EQ(dict_size(r), 5);

    TEST_EQ(
        cp_dict_insert(&num_new(90)->node, &r, cmp_num, NULL, 0),
        NULL);
    TEST_EQ(dict_size(r), 6);

    cp_dict_t *r20;
    TEST_NE((r20 = cp_dict_min(r)), NULL);
    TEST_EQ(num_value(r20), 20);

    TEST_NE((r50 = cp_dict_next(r20)), NULL);
    TEST_EQ(num_value(r50), 50);

    cp_dict_t *r60;
    TEST_NE((r60 = cp_dict_next(r50)), NULL);
    TEST_EQ(num_value(r60), 60);

    cp_dict_t *r70;
    TEST_NE((r70 = cp_dict_next(r60)), NULL);
    TEST_EQ(num_value(r70), 70);

    cp_dict_t *r80;
    TEST_NE((r80 = cp_dict_next(r70)), NULL);
    TEST_EQ(num_value(r80), 80);

    cp_dict_t *r90;
    TEST_NE((r90 = cp_dict_next(r80)), NULL);
    TEST_EQ(num_value(r90), 90);

    TEST_VOID(cp_dict_remove(r60, &r));
    TEST_NE(r, NULL);
    TEST_EQ(dict_size(r), 5);

    TEST_EQ(cp_dict_next(r60), NULL);
    TEST_EQ(cp_dict_prev(r60), NULL);

    TEST_NE((r70 = cp_dict_next(r50)), NULL);
    TEST_EQ(num_value(r70), 70);

    TEST_VOID(cp_dict_remove(r, &r));
    TEST_NE(r, NULL);
    TEST_EQ(dict_size(r), 4);

    TEST_VOID(cp_dict_remove(r, &r));
    TEST_NE(r, NULL);
    TEST_EQ(dict_size(r), 3);

    TEST_VOID(cp_dict_remove(r, &r));
    TEST_NE(r, NULL);
    TEST_EQ(dict_size(r), 2);

    TEST_VOID(cp_dict_remove(r, &r));
    TEST_NE(r, NULL);
    TEST_EQ(dict_size(r), 1);

    TEST_VOID(cp_dict_remove(r, &r));
    TEST_EQ(r, NULL);
    TEST_EQ(dict_size(r), 0);

    cp_dict_insert(r20, &r, cmp_num, NULL, 0);
    cp_dict_insert(r50, &r, cmp_num, NULL, 0);
    cp_dict_insert(r60, &r, cmp_num, NULL, 0);
    cp_dict_insert(r70, &r, cmp_num, NULL, 0);
    cp_dict_insert(r80, &r, cmp_num, NULL, 0);
    cp_dict_insert(r90, &r, cmp_num, NULL, 0);
    TEST_EQ(dict_size(r), 6);
    TEST_EQ((cp_dict_remove(r20,&r),dict_size(r)), 5);
    TEST_EQ((cp_dict_remove(r50,&r),dict_size(r)), 4);
    TEST_EQ((cp_dict_remove(r60,&r),dict_size(r)), 3);
    TEST_EQ((cp_dict_remove(r70,&r),dict_size(r)), 2);
    TEST_EQ((cp_dict_remove(r80,&r),dict_size(r)), 1);
    TEST_EQ((cp_dict_remove(r90,&r),dict_size(r)), 0);

    cp_dict_insert(r60, &r, cmp_num, NULL, 0);
    cp_dict_insert(r90, &r, cmp_num, NULL, 0);
    cp_dict_insert(r80, &r, cmp_num, NULL, 0);
    cp_dict_insert(r20, &r, cmp_num, NULL, 0);
    cp_dict_insert(r70, &r, cmp_num, NULL, 0);
    cp_dict_insert(r50, &r, cmp_num, NULL, 0);
    TEST_EQ(dict_size(r), 6);
    TEST_EQ((cp_dict_remove(r80,&r),dict_size(r)), 5);
    TEST_EQ((cp_dict_remove(r60,&r),dict_size(r)), 4);
    TEST_EQ((cp_dict_remove(r70,&r),dict_size(r)), 3);
    TEST_EQ((cp_dict_remove(r90,&r),dict_size(r)), 2);
    TEST_EQ((cp_dict_remove(r20,&r),dict_size(r)), 1);
    TEST_EQ((cp_dict_remove(r50,&r),dict_size(r)), 0);

    cp_dict_t *a[] = { r20, r50, r60, r70, r80, r90 };
    for (cp_size_each(i, 10)) {
        for (cp_size_each(k, 2*cp_countof(a))) {
            size_t u = irand(cp_countof(a));
            size_t v = irand(cp_countof(a));
            CP_SWAP(&a[u], &a[v]);
        }

        fprintf(stderr, "INSERT:");
        for (cp_arr_each(k, a)) {
            fprintf(stderr, " %"_Pz"u", CP_BOX_OF(a[k], num_t, node)->value);
            cp_dict_insert(a[k], &r, cmp_num, NULL, 0);
        }
        fprintf(stderr, "\n");
        TEST_EQ(dict_size(r), cp_countof(a));

        for (cp_size_each(k, 2*cp_countof(a))) {
            size_t u = irand(cp_countof(a));
            size_t v = irand(cp_countof(a));
            CP_SWAP(&a[u], &a[v]);
        }
        fprintf(stderr, "REMOVE:");
        for (cp_arr_each(k, a)) {
            fprintf(stderr, " %"_Pz"u", CP_BOX_OF(a[k], num_t, node)->value);
        }
        fprintf(stderr, "\n");
        for (cp_arr_each(k, a)) {
            TEST_EQ((cp_dict_remove(a[k],&r),dict_size(r)), cp_countof(a)-k-1);
        }
    }
}
