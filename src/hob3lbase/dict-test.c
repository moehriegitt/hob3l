/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdlib.h>
#include <hob3lbase/dict.h>
#include <hob3lbase/alloc.h>
#include "hob3lbase-test.h"
#include "dict-test.h"

typedef struct {
    cp_dict_t node;
    size_t value;
    size_t sum;
} num_t;

static inline size_t get_sum(
    cp_dict_t *n_)
{
    if (n_ == NULL) {
        return 0;
    }
    num_t *n = CP_BOX_OF (n_, *n, node);
    return n->sum;
}

CP_UNUSED
static bool good_sum(
    cp_dict_t *a_)
{
    if (a_ == NULL) {
        return true;
    }
    num_t *a = CP_BOX_OF (a_, *a, node);
#if 0
    fprintf(stderr, "DEBUG: SUM: %p [ %p %p ]: %zu [ %zu %zu ] (%zu)\n",
        a_,
        a_->edge[0],
        a_->edge[1],
        a->value,
        get_sum(a_->edge[0]),
        get_sum(a_->edge[1]),
        a->sum);
#endif
    return a->sum == a->value + get_sum(a_->edge[0]) + get_sum(a_->edge[1]);
}

CP_UNUSED
static bool very_good_sum(
    cp_dict_t *a)
{
    if (a == NULL) {
        return true;
    }
    return good_sum(a) && very_good_sum(a->edge[0]) && very_good_sum(a->edge[1]);
}

static inline void update_sum(
    cp_dict_t *a_)
{
    if (a_ == NULL) {
        return;
    }
    num_t *a = CP_BOX_OF (a_, *a, node);
#if 0
    fprintf(stderr, "DEBUG: UPD: %p [ %p %p ]: %zu [ %zu %zu ] (%zu)\n",
        a_,
        a_->edge[0],
        a_->edge[1],
        a->value,
        get_sum(a_->edge[0]),
        get_sum(a_->edge[1]),
        a->sum);
#endif
    assert(good_sum(a_->edge[0]));
    assert(good_sum(a_->edge[1]));
    a->sum = a->value + get_sum(a_->edge[0]) + get_sum(a_->edge[1]);
}

static void my_aug_event(
    cp_dict_aug_t *aug CP_UNUSED,
    cp_dict_t *a_,
    cp_dict_t *b_,
    cp_dict_aug_type_t c)
{
    assert(a_ != NULL);
#if 0
    fprintf(stderr, "DEBUG: AUG: %p [ %p %p ], %p %s\n",
        a_,
        a_->edge[0],
        a_->edge[1],
        b_, cp_dict_str_aug_type(c));
#endif
    num_t *a = CP_BOX_OF (a_, *a, node);
    switch (c) {
    case CP_DICT_AUG_LEFT:
    case CP_DICT_AUG_RIGHT:
        /* for the black-height, we're updating based on internal
         * rbtree info, so for this, we need to check both children
         * at a rotation.  For data unrelated to the rbtree, we'd
         * need to update just b_ and a_. */
        update_sum(b_);
        update_sum(a_);
        break;

    case CP_DICT_AUG_NOP:
    case CP_DICT_AUG_ADD:
    case CP_DICT_AUG_JOIN:
        update_sum(a_);
        break;

    case CP_DICT_AUG_NOP2:
        update_sum(a_);
        update_sum(a_->parent);
        break;

    case CP_DICT_AUG_FINI:
        while (a_) {
            update_sum(a_);
            a_ = a_->parent;
        }
        break;

    case CP_DICT_AUG_CUT_SWAP:
        break;

    case CP_DICT_AUG_CUT_LEAF:
        update_sum(a_);
        break;

    case CP_DICT_AUG_SPLIT:
        fprintf(stderr, "DEBUG: CLR: %p [ %p %p ]: %" CP_FZU " %" CP_FZU "\n",
            a_,
            a_->edge[0],
            a_->edge[1],
            get_sum(a_->edge[0]),
            get_sum(a_->edge[1]));
        a->sum = a->value;
        break;
    }
}

static cp_dict_aug_t my_aug[1] = {{ .event = my_aug_event }};

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
    void *user CP_UNUSED)
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
    r->value = r->sum = x;
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

static size_t dump_dict_rec(
    cp_dict_t *n,
    int ind)
{
    if (n == NULL) {
        return 0;
    }
    size_t h1 = dump_dict_rec(n->edge[0], ind+1);
    size_t h = h1 + !cp_dict_is_red(n);

    printf("%*s%p %c %" CP_FZU " (%" CP_FZU "%s)\n",
        4*ind, "", n, cp_dict_is_red(n)?'R':'B', cp_dict_black_height(n), h,
        cp_dict_black_height(n) == h ? "" : " !ERR#");

#ifndef NDEBUG
    size_t h2 = dump_dict_rec(n->edge[1], ind+1);
    assert(h1 == h2);
#endif

    assert(h == cp_dict_black_height(n));
    return h;
}

static void dump_dict(
    cp_dict_t *n)
{
    printf("--\n");
    dump_dict_rec(n, 0);
    printf("--\n");
}

static int num_cmp(
    size_t *a,
    cp_dict_t *b_,
    void *user CP_UNUSED)
{
    num_t *b = CP_BOX_OF(b_, *b, node);
    return CP_CMP(*a, b->value);
}

static void join3_test(
    size_t n1,
    size_t n2)
{
    size_t o = 0;
    cp_dict_t *l = NULL;
    for (cp_size_each(i, n1)) {
        TEST_VOID(l = cp_dict_join3_aug(l, &num_new(o++)->node, NULL, my_aug));
        assert(very_good_sum(l));
    }
    TEST_EQ(dict_size(l), n1);

    cp_dict_t *m = &num_new(o++)->node;
    assert(very_good_sum(m));

    o += n2;
    size_t o2 = o;
    cp_dict_t *r = NULL;
    for (cp_size_each(i, n2)) {
        TEST_VOID(r = cp_dict_join3_aug(NULL, &num_new(--o2)->node, r, my_aug));
        assert(very_good_sum(r));
    }
    TEST_EQ(dict_size(r), n2);

    TEST_VOID(r = cp_dict_join3_aug(l, m, r, my_aug));
    TEST_EQ(dict_size(r), n1+n2+1);
    TEST_EQ(o, n1+n2+1);

    /* check that the numbers are consecutive in the tree */
    size_t u = 0;
    for (cp_dict_each(i, r)) {
        num_t *n = CP_BOX_OF(i, *n, node);
        TEST_EQ(u, n->value);
        u++;
    }
    TEST_EQ(u, n1+n2+1);

    /* try to split and join a few times */
    for (size_t j = 0; j <= 10; j++) {
        size_t pivot = ((n1 + n2 + 1) * j) / 10;
        cp_dict_t *ll = NULL;
        cp_dict_t *rr = NULL;
        cp_dict_split_aug(&ll, &rr, r, &pivot, num_cmp, NULL, 1, my_aug);
        assert(very_good_sum(ll));
        assert(very_good_sum(rr));

        u = 0;
        for (cp_dict_each(i, ll)) {
            num_t *n = CP_BOX_OF(i, *n, node);
            assert(u == n->value);
            u++;
        }
        TEST_EQ(u, pivot);
        for (cp_dict_each(i, rr)) {
            num_t *n = CP_BOX_OF(i, *n, node);
            assert(u == n->value);
            u++;
        }
        TEST_EQ(u, n1+n2+1);

        /* merge them back */
        r = cp_dict_join2_aug(ll, rr, my_aug);
        assert(very_good_sum(r));
    }
}

static void check_seq(
    cp_dict_t *r,
    size_t size CP_UNUSED,
    size_t const *data)
{
    size_t i = 0;
    for (cp_dict_each(v_, r)) {
        num_t *v = CP_BOX_OF(v_, *v, node);
        assert(i < size);
        fprintf(stderr, "%s:%d: r[%" CP_FZU "]: expect %" CP_FZU " == %" CP_FZU "\n",
            __FILE__, __LINE__, i, v->value, data[i]);
        assert(v->value == data[i]);
        i++;
    }
    assert(i == size);
}

#define CHECK_SEQ(r, ...) \
({ \
    size_t data_[] = { __VA_ARGS__ }; \
    check_seq(r, cp_countof(data_), data_); \
})

static void insert_test2(void)
{
    cp_dict_t *r = NULL;

    cp_dict_t *n[10];
    for (cp_arr_each(i, n)) {
        n[i] = &num_new((i + 1) * 10)->node;
    }
    for (cp_arr_each(i, n)) {
        if (i & 1) { continue; }
        cp_dict_insert(n[i], &r, cmp_num, NULL, 0);
    }

    CHECK_SEQ(r, 10, 30, 50, 70, 90);

    cp_dict_ref_t ref = { .parent = n[2], .child = 1 };
    cp_dict_insert_ref(n[3], &ref, &r);
    CHECK_SEQ(r, 10, 30, 40, 50, 70, 90);

    ref = (cp_dict_ref_t){ .parent = n[2], .child = 0 };
    cp_dict_insert_ref(n[1], &ref, &r);
    CHECK_SEQ(r, 10, 20, 30, 40, 50, 70, 90);

    ref = (cp_dict_ref_t){ .parent = n[8], .child = 0 };
    cp_dict_insert_ref(n[7], &ref, &r);
    CHECK_SEQ(r, 10, 20, 30, 40, 50, 70, 80, 90);

    cp_dict_insert_at(n[5], n[6], 0, &r);
    CHECK_SEQ(r, 10, 20, 30, 40, 50, 60, 70, 80, 90);

    cp_dict_insert_at(n[9], n[8], 1, &r);
    CHECK_SEQ(r, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100);
}

/**
 * Unit tests for dictionary data structure
 */
extern void cp_dict_test(void)
{
    cp_dict_t *r = NULL;
    TEST_EQ(
        cp_dict_find((size_t[1]){50}, r, cmp_num_f, NULL, 0),
        NULL);

    TEST_EQ(
        cp_dict_insert(&num_new(50)->node, &r, cmp_num, NULL, 0),
        NULL);

    cp_dict_t *r50;
    TEST_NE(
        (r50 = cp_dict_find((size_t[1]){50}, r, cmp_num_f, NULL, 0)),
        NULL);

    TEST_EQ(num_value(r50), 50);
    TEST_EQ(dict_size(r), 1);
    dump_dict(r);

    TEST_EQ(
        cp_dict_insert(&num_new(20)->node, &r, cmp_num, NULL, 0),
        NULL);
    TEST_EQ(dict_size(r), 2);
    dump_dict(r);

    TEST_EQ(
        cp_dict_insert(&num_new(60)->node, &r, cmp_num, NULL, 0),
        NULL);
    TEST_EQ(dict_size(r), 3);
    dump_dict(r);

    TEST_EQ(
        cp_dict_insert(&num_new(70)->node, &r, cmp_num, NULL, 0),
        NULL);
    TEST_EQ(dict_size(r), 4);
    dump_dict(r);

    TEST_EQ(
        cp_dict_insert(&num_new(80)->node, &r, cmp_num, NULL, 0),
        NULL);
    TEST_EQ(dict_size(r), 5);
    dump_dict(r);

    TEST_EQ(
        cp_dict_insert(&num_new(90)->node, &r, cmp_num, NULL, 0),
        NULL);
    TEST_EQ(dict_size(r), 6);
    dump_dict(r);

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

    /* do the same inserts with join3() */
    TEST_VOID(r = cp_dict_join3(r, &num_new(50)->node, NULL));
    dump_dict(r);
    TEST_EQ(dict_size(r), 1);

    TEST_VOID(r = cp_dict_join3(r, &num_new(20)->node, NULL));
    dump_dict(r);
    TEST_EQ(dict_size(r), 2);

    TEST_VOID(r = cp_dict_join3(r, &num_new(60)->node, NULL));
    dump_dict(r);
    TEST_EQ(dict_size(r), 3);

    TEST_VOID(r = cp_dict_join3(r, &num_new(70)->node, NULL));
    dump_dict(r);
    TEST_EQ(dict_size(r), 4);

    TEST_VOID(r = cp_dict_join3(r, &num_new(80)->node, NULL));
    dump_dict(r);
    TEST_EQ(dict_size(r), 5);

    TEST_VOID(r = cp_dict_join3(r, &num_new(90)->node, NULL));
    dump_dict(r);
    TEST_EQ(dict_size(r), 6);

    TEST_NE(cp_dict_extract_min(&r), NULL);
    TEST_NE(cp_dict_extract_min(&r), NULL);
    TEST_NE(cp_dict_extract_min(&r), NULL);
    TEST_NE(cp_dict_extract_min(&r), NULL);
    TEST_NE(cp_dict_extract_min(&r), NULL);
    TEST_NE(cp_dict_extract_min(&r), NULL);
    TEST_EQ(cp_dict_extract_min(&r), NULL);

    /* do insert again */
    cp_dict_insert(r20, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(r50, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(r60, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(r70, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(r80, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(r90, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(&num_new(100)->node, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(&num_new(101)->node, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(&num_new(102)->node, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(&num_new(52)->node, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(&num_new(62)->node, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(&num_new(42)->node, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(&num_new(32)->node, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(&num_new(22)->node, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(&num_new(12)->node, &r, cmp_num, NULL, 0);
    dump_dict(r);
    TEST_EQ(dict_size(r), 15);
    TEST_EQ((cp_dict_remove(r20,&r),dict_size(r)), 14);
    TEST_EQ((cp_dict_remove(r50,&r),dict_size(r)), 13);
    TEST_EQ((cp_dict_remove(r60,&r),dict_size(r)), 12);
    TEST_EQ((cp_dict_remove(r70,&r),dict_size(r)), 11);
    TEST_EQ((cp_dict_remove(r80,&r),dict_size(r)), 10);
    TEST_EQ((cp_dict_remove(r90,&r),dict_size(r)), 9);
    TEST_EQ((cp_dict_remove(cp_dict_min(r),&r),dict_size(r)), 8);
    TEST_EQ((cp_dict_remove(cp_dict_min(r),&r),dict_size(r)), 7);
    TEST_EQ((cp_dict_remove(cp_dict_min(r),&r),dict_size(r)), 6);
    TEST_EQ((cp_dict_remove(cp_dict_min(r),&r),dict_size(r)), 5);
    TEST_EQ((cp_dict_remove(cp_dict_min(r),&r),dict_size(r)), 4);
    TEST_EQ((cp_dict_remove(cp_dict_min(r),&r),dict_size(r)), 3);
    TEST_EQ((cp_dict_remove(cp_dict_min(r),&r),dict_size(r)), 2);
    TEST_EQ((cp_dict_remove(cp_dict_min(r),&r),dict_size(r)), 1);
    TEST_EQ((cp_dict_remove(cp_dict_min(r),&r),dict_size(r)), 0);
    dump_dict(r);

    cp_dict_insert(r60, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(r90, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(r80, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(r20, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(r70, &r, cmp_num, NULL, 0);
    dump_dict(r);
    cp_dict_insert(r50, &r, cmp_num, NULL, 0);
    dump_dict(r);
    TEST_EQ(dict_size(r), 6);
    TEST_EQ((cp_dict_remove(r80,&r),dict_size(r)), 5);
    TEST_EQ((cp_dict_remove(r60,&r),dict_size(r)), 4);
    TEST_EQ((cp_dict_remove(r70,&r),dict_size(r)), 3);
    TEST_EQ((cp_dict_remove(r90,&r),dict_size(r)), 2);
    TEST_EQ((cp_dict_remove(r20,&r),dict_size(r)), 1);
    TEST_EQ((cp_dict_remove(r50,&r),dict_size(r)), 0);
    dump_dict(r);

    /* other insert tests */
    insert_test2();

    /* more join3 tests */
    join3_test(0, 10);
    join3_test(1, 9);
    join3_test(2, 20);
    join3_test(3, 15);
    join3_test(5, 15);
    join3_test(8, 15);
    join3_test(12, 15);
    join3_test(15, 15);
    join3_test(1, 1);
    join3_test(5, 5);
    join3_test(20, 20);
    join3_test(200, 200);
    join3_test(100, 1000);
    join3_test(1, 1000);
    join3_test(100, 1000);

    /* more tests */
    size_t cnt = 4;
    for (cp_size_each(i, cnt)) {
        cp_dict_t *n = &num_new(i)->node;
        cp_dict_insert_aug(n, &r, cmp_num, NULL, 0, my_aug);
        dump_dict(r);
        TEST_EQ(dict_size(r), i+1);
        assert(very_good_sum(r));
    }
    for (cp_size_each(i, cnt)) {
        TEST_EQ(dict_size(r), cnt-i);
        cp_dict_t *n = cp_dict_min(r);
        cp_dict_remove_aug(n, &r, my_aug);
        dump_dict(r);
        assert(very_good_sum(r));
    }

    for (cp_size_each(i, 1000)) {
        cp_dict_t *n = &num_new(i)->node;
        cp_dict_insert_aug(n, &r, cmp_num, NULL, 0, my_aug);
        TEST_EQ(dict_size(r), i+1);
        assert(very_good_sum(r));
    }
    for (cp_size_each(i, 1000)) {
        TEST_EQ(dict_size(r), 1000-i);
        cp_dict_t *n = cp_dict_min(r);
        cp_dict_remove_aug(n, &r, my_aug);
        assert(very_good_sum(r));
    }

    for (cp_size_each(i, 1000)) {
        cp_dict_t *n = &num_new(255 & (size_t)rand())->node;
        cp_dict_insert_aug(n, &r, cmp_num, NULL, -1, my_aug);
        TEST_EQ(dict_size(r), i+1);
        assert(very_good_sum(r));
    }
    for (cp_size_each(o, 10)) {
        for (cp_size_each(i, 500)) {
            cp_dict_t *n = cp_dict_min(r);
            cp_dict_remove_aug(n, &r, my_aug);
            assert(very_good_sum(r));
        }
        for (cp_size_each(i, 500)) {
            cp_dict_t *n = &num_new(255 & (size_t)rand())->node;
            cp_dict_insert_aug(n, &r, cmp_num, NULL, -1, my_aug);
            assert(very_good_sum(r));
        }
    }
    for (cp_size_each(i, 100)) {
        cp_dict_t *n = cp_dict_min(r);
        cp_dict_remove(n, &r);
    }
    for (cp_size_each(i, 100)) {
        cp_dict_t *n = &num_new(255 & (size_t)rand())->node;
        cp_dict_insert(n, &r, cmp_num, NULL, -1);
    }
    size_t uu = 0;
    for (cp_size_each(i, 1000)) {
        TEST_EQ(dict_size(r), 1000-i);
        cp_dict_t *n = cp_dict_min(r);
        cp_dict_remove(n, &r);
        num_t *nn = CP_BOX_OF(n, *nn, node);
        TEST_LE(uu, nn->value);
        uu = nn->value;
    }

    cp_dict_t *a[] = { r20, r50, r60, r70, r80, r90 };
    for (cp_size_each(i, 10)) {
        for (cp_size_each(k, 2*cp_countof(a))) {
            size_t u = irand(cp_countof(a));
            size_t v = irand(cp_countof(a));
            CP_SWAP(&a[u], &a[v]);
        }

        fprintf(stderr, "INSERT:");
        for (cp_arr_each(k, a)) {
            fprintf(stderr, " %"CP_Z"u", CP_BOX_OF(a[k], num_t, node)->value);
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
            fprintf(stderr, " %"CP_Z"u", CP_BOX_OF(a[k], num_t, node)->value);
        }
        fprintf(stderr, "\n");
        for (cp_arr_each(k, a)) {
            TEST_EQ((cp_dict_remove(a[k],&r),dict_size(r)), cp_countof(a)-k-1);
        }
    }
}
