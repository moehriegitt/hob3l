/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#define CP_DICT_SHOW 0

#include <assert.h>
#include <string.h>

#if CP_DICT_SHOW
#include <stdio.h>
#endif

#include <hob3lbase/dict.h>

/*
 * Enable assertions that add a significant slowdown, maybe even
 * make the time complexity worse.
 * For debugging.
 */
#if 1
#define SLOW_ASSERT(x) assert(x)
#else
#define SLOW_ASSERT(x) ((void)x)
#endif

/**
 * Whether we allow a red root node.
 * This is necessary for bulk operations.
 */
#define ALLOW_RED_ROOT 1

#define COLOUR_MASK  1
#define BLACK        0
#define RED          1

#define HEIGHT_MASK  (~1UL)
#define HEIGHT_INC   2

extern char const *cp_dict_str_aug_type(
    cp_dict_aug_type_t t)
{
    static char const *const str[] = {
    [1 + CP_DICT_AUG_LEFT]     = "LEFT",
    [1 + CP_DICT_AUG_RIGHT]    = "RIGHT",
    [1 + CP_DICT_AUG_NOP]      = "NOP",
    [1 + CP_DICT_AUG_NOP2]     = "NOP2",
    [1 + CP_DICT_AUG_FINI]     = "FINI",
    [1 + CP_DICT_AUG_ADD]      = "ADD",
    [1 + CP_DICT_AUG_CUT_SWAP] = "CUT_SWAP",
    [1 + CP_DICT_AUG_CUT_LEAF] = "CUT_LEAF",
    [1 + CP_DICT_AUG_JOIN]     = "JOIN",
    [1 + CP_DICT_AUG_SPLIT]    = "SPLIT",
    };
    size_t in_bounds = t < cp_countof(str);
    return str[(1 + t) & -in_bounds];
}

static inline cp_dict_t *cp_dict_parent(
    cp_dict_t *n)
{
    return n->parent;
}

/* the height multiplied by 2 (the absolute value is not
 * important for the algorithm, just the relative value). */
static inline size_t cp_dict_height2(
    cp_dict_t *e)
{
    return (e == NULL) ? 0 : (e->stat & HEIGHT_MASK);
}

/**
 * Return the black height of the given node
 *
 * This is internal information and it exposes the implementation
 * (red-black trees).  But it may be interesting for debugging.
 * Do not use this productively in algorithms, but only in
 * debugging, benchmarking, testing, etc.
 */
extern size_t cp_dict_black_height(
    cp_dict_t *n)
{
    return cp_dict_height2(n) >> 1;
}

static inline bool cp_dict_red(
    cp_dict_t *e)
{
    return (e != NULL) && (e->stat & RED);
}

/**
 * Return whether the node is RED
 *
 * This is internal information and it exposes the implementation
 * (red-black trees).  But it may be interesting for debugging.
 * Do not use this productively in algorithms, but only in
 * debugging, benchmarking, testing, etc.
 */
extern size_t cp_dict_is_red(
    cp_dict_t *n)
{
    return cp_dict_red(n);
}

static inline void cp_dict_set_colour(
    cp_dict_t *e,
    bool colour)
{
    assert(e != NULL);
    e->stat = (e->stat | 1) ^ !colour;
}

static inline void cp_dict_set_RED(
    cp_dict_t *e)
{
    assert(e != NULL);
    e->stat |= RED;
}

static inline void cp_dict_set_BLACK(
    cp_dict_t *e)
{
    assert(e != NULL);
    e->stat &= ~(size_t)COLOUR_MASK;
}

static inline void cp_dict_INC_set_BLACK(
    cp_dict_t *e)
{
    assert(e != NULL);
    assert(cp_dict_red(e));
    e->stat++;
    assert(!cp_dict_red(e));
}

static inline void cp_dict_ensure_BLACK(
    cp_dict_t *e)
{
    assert(e != NULL);
    if (cp_dict_red(e)) {
        cp_dict_INC_set_BLACK(e);
    }
    assert(!cp_dict_red(e));
}

static inline void cp_dict_DEC_set_RED(
    cp_dict_t *e)
{
    assert(e != NULL);
    assert(!cp_dict_red(e));
    e->stat--;
    assert(cp_dict_red(e));
}

static inline void cp_dict_INC(
    cp_dict_t *e)
{
    assert(e != NULL);
    e->stat += 2;
}

static inline void cp_dict_DEC(
    cp_dict_t *e)
{
    assert(e != NULL);
    assert(e->stat >= 2);
    e->stat -= 2;
}

static inline void cp_dict_set_RED_LEAF(
    cp_dict_t *e)
{
    assert(e != NULL);
    e->stat = RED; /* height == 0, colour == RED */
}

static inline void cp_dict_set_BLACK_LEAF(
    cp_dict_t *e)
{
    assert(e != NULL);
    e->stat = 2; /* height == 1, colour == BLACK */
}

static inline void cp_dict_set_RED_same_depth(
    cp_dict_t *e,
    cp_dict_t *l)
{
    assert(e != NULL);
    e->stat = cp_dict_height2(l) | RED; /* same depth as c, make RED */
}

static inline void cp_dict_INC_set_BLACK_if_needed(
    cp_dict_t *e)
{
    if (cp_dict_red(e) &&
        (cp_dict_red(e->edge[0]) || cp_dict_red(e->edge[1])))
    {
        cp_dict_INC_set_BLACK(e);
    }
}

static inline void cp_dict_set_child(
    cp_dict_t *r,
    unsigned i,
    cp_dict_t *n)
{
    assert(i < cp_countof(r->edge));
    r->edge[i] = n;
}

static inline void cp_dict_set_child_and_parent(
    cp_dict_t *r,
    unsigned i,
    cp_dict_t *n)
{
    cp_dict_set_child(r, i, n);
    if (n != NULL) {
        n->parent = r;
    }
}

/**
 * Invoke a possible augmentation callback. */
static inline void augment(
    cp_dict_aug_t *aug,
    cp_dict_t *main,
    cp_dict_t *aux,
    cp_dict_aug_type_t type)
{
    if ((aug != NULL) && (main != NULL)) {
        assert(aug->event != NULL);
        aug->event(aug, main, aux, type);
    }
}

#ifndef NDEBUG

CP_UNUSED
static bool good_tree(
    cp_dict_t *p,
    bool check_col)
{
    if (p == NULL) {
        return true;
    }
    if (cp_dict_red(p)) {
        if (check_col && (cp_dict_red(p->edge[0]) || cp_dict_red(p->edge[1]))) {
            return false;
        }
        if (cp_dict_height2(p) != cp_dict_height2(p->edge[0])) {
            return false;
        }
        if (cp_dict_height2(p) != cp_dict_height2(p->edge[1])) {
            return false;
        }
    }
    else {
        if (cp_dict_height2(p) != (cp_dict_height2(p->edge[0]) + 2)) {
            return false;
        }
        if (cp_dict_height2(p) != (cp_dict_height2(p->edge[1]) + 2)) {
            return false;
        }
    }
    return true;
}

static bool good_children(
    cp_dict_t *p)
{
    if (p == NULL) {
        return false;
    }
    return good_tree(p->edge[0], true) && good_tree(p->edge[1], true);
}

#if CP_DICT_SHOW
static size_t show_tree_rec(
    cp_dict_t *n,
    int ind)
{
    if (n == NULL) {
        fprintf(stderr, "%*s%012zx %c%zu\n",
            4*ind, "", (size_t)n, cp_dict_is_red(n)?'R':'B', cp_dict_black_height(n));
        return 0;
    }
    size_t h1 = show_tree_rec(n->edge[0], ind+1);
    size_t h = h1 + !cp_dict_is_red(n);

    bool bad_color = cp_dict_red(n) && (cp_dict_red(n->edge[0]) || cp_dict_red(n->edge[1]));
    size_t hc0 = cp_dict_black_height(n->edge[0]) + !cp_dict_red(n);
    size_t hc1 = cp_dict_black_height(n->edge[1]) + !cp_dict_red(n);
    bool good_h = (h == hc0) && (h == hc1);

    fprintf(stderr, "%*s%012zx %c%zu (%zu)%s\n",
        4*ind, "", (size_t)n, cp_dict_is_red(n)?'R':'B', cp_dict_black_height(n), h,
        bad_color ? "!ERR_COL#" : good_h && (cp_dict_black_height(n) == h) ? "" : " !ERR_H#");
    size_t h2 = show_tree_rec(n->edge[1], ind+1);
    (void)h2;
    return h;
}
#endif

static inline void show_tree(
    cp_dict_t *p CP_UNUSED)
{
#if CP_DICT_SHOW
    fprintf(stderr, "TREE:\n");
    show_tree_rec(p, 0);
#endif
}

CP_UNUSED
static size_t get_black_height(
    cp_dict_t *p)
{
    if (!good_tree(p, true)) {
        return -0UL;
    }
    if (p == NULL) {
        return 0;
    }
    return get_black_height(p->edge[0]) + !cp_dict_red(p);
}

CP_UNUSED
static bool very_good_tree(
    cp_dict_t *p)
{
    return get_black_height(p) == cp_dict_black_height(p);
}

#endif

static inline void cp_dict_collapse_edge(
    cp_dict_t *r,
    unsigned i,
    cp_dict_t *e)
{
    assert(i < cp_countof(r->edge));
    if ((e != NULL) && cp_dict_red(r->edge[i])) {
        cp_dict_set_RED(e);
    }
    r->edge[i] = e;
}

/**
 * Start to iterate.
 * dir=0 finds the first element, dir=1 finds the last.
 *
 * Time complexity:  O(log n), amortized in iteration
 * together with cp_dict_step(): O(1),
 *
 * Stack complexity: O(1)
 */
extern cp_dict_t *cp_dict_start(
    cp_dict_t *n,
    unsigned dir)
{
    cp_dict_t *p = n;
    while (n != NULL) {
        p = n;
        n = cp_dict_child(n, dir);
    }
    return p;
}

/**
 * Get the root node of a tree from an arbitrary node.
 *
 * This can be used if the root point is not stored for some reason,
 * to find the root for any node in the tree.
 *
 * Time complexity: O(log n) (for bottom-up parent search)
 *
 * Stack complexity: O(1)
 */
extern cp_dict_t *cp_dict_root(
    cp_dict_t *n)
{
    if (n != NULL) {
        while (n->parent != NULL) {
            n = cp_dict_parent(n);
        }
    }
    return n;
}

/**
 * Iterate a tree: do one step.
 * dir=0 does a step forward, dir=1 does a step backward.
 *
 * Time complexity:  O(log n), amortized in iteration: O(1),
 * on minimum or maximum: O(1).
 *
 * Stack complexity: O(1)
 */
extern cp_dict_t *cp_dict_step(
    cp_dict_t *n,
    unsigned dir)
{
    assert(n != NULL);
    if (cp_dict_child(n, !dir) != NULL) {
        return cp_dict_start(cp_dict_child(n, !dir), dir);
    }

    cp_dict_t *p = cp_dict_parent(n);
    while ((p != NULL) && (n == cp_dict_child(p, !dir))) {
        n = p;
        p = cp_dict_parent(p);
    }
    return p;
}

static void rb_rotate(
    cp_dict_aug_t *aug,
    cp_dict_t **root,
    unsigned dir,
    cp_dict_t *x)
{
    cp_dict_t *y = x->edge[!dir];

    x->edge[!dir] = y->edge[dir];

    if (y->edge[dir] != NULL) {
        y->edge[dir]->parent = x;
    }

    y->parent = x->parent;

    if (x->parent == NULL) {
        *root = y;
    }
    else {
        x->parent->edge[cp_dict_idx(x->parent,x)] = y;
    }

    y->edge[dir] = x;
    x->parent = y;

    augment(aug, y, x, CP_DICT_AUG_LEFT + dir);
}

static void _balance_insert(
    cp_dict_aug_t *aug,
    cp_dict_t **root,
    cp_dict_t *x)
{
    while ((x != *root) &&
           (!ALLOW_RED_ROOT || x->parent->parent) &&
           cp_dict_red(x->parent))
    {
        assert(cp_dict_red(x));
        unsigned side = cp_dict_idx(x->parent->parent, x->parent);
        cp_dict_t *y = x->parent->parent->edge[!side];
        if (cp_dict_red(y)) {
            /*      (Q)B1        =>     (Q)R1
             *     /    \              /    \
             *   (y)R0   (P)R0       (y)B1  (P)B1
             *             \                  \
             *             (x)R0              (x)R0
             */
            cp_dict_INC_set_BLACK(x->parent);
            cp_dict_INC_set_BLACK(y);
            cp_dict_set_RED(x->parent->parent);

            augment(aug, x->parent, x, CP_DICT_AUG_NOP2);
            x = x->parent->parent;
        }
        else {
            if (x == x->parent->edge[!side]) {
                /*    (Q)B        =>    (Q)B
                 *   /    \            /    \
                 * (y)B    (P)R      (y)B    (x)R
                 *         /                    \
                 *        (x)R                   (P)R
                 */
                x = x->parent;
                rb_rotate(aug, root, side, x);
            }

            /*       (Q)B1      =>      (Q)R0      =>      (P)B1
             *      /    \             /    \             /    \
             *    (y)B0  (P)R0       (y)B0   (P)B1      (Q)R0  (x)R0
             *             \                   \        /
             *             (x)R0               (x)R0   (y)B0
             */
            cp_dict_INC_set_BLACK(x->parent);
            if (!cp_dict_red(x->parent->parent)) {
                cp_dict_DEC_set_RED(x->parent->parent);
            }
            rb_rotate(aug, root, !side, x->parent->parent);
            assert(!cp_dict_red(x->parent));

            x = x->parent;
            break;
        }
    }

    augment(aug, x->parent, x, CP_DICT_AUG_FINI);
}

/**
 * Internal, type-unsafe variant of cp_dict_find_ref().
 *
 * Do not use, use cp_dict_find_ref() or cp_dict_find() instead.
 *
 * Time complexity: O(log n)
 *
 * Stack complexity: O(1)
 */
extern cp_dict_t *cp_dict_find_ref_(
    cp_dict_ref_t *ref,
    void *idx,
    cp_dict_t *n,
    cp_dict_cmp_t_ cmp,
    void *user,
    int duplicate)
{
    unsigned i = 1;
    unsigned pa = 0;
    cp_dict_t *e = NULL;
    cp_dict_t *p = n;
    while (n != NULL) {
        int d = cmp(idx, n, user);
        if (d == 0) {
            e = n;
            d = duplicate;
        }
        if (d == 0) {
            pa |= 4;
            break;
        }
        p = n;
        i = d > 0;
        pa |= (i + 1);
        n = cp_dict_child(n, i);
    }

    assert(i <= 1);
    if (ref != NULL) {
        ref->parent = p;
        ref->child = i;
        ref->path = pa;
    }

    return ((duplicate & 3) == 2) ? e : n;
}

/**
 * Insert a node into a predefined location in the tree, then rebalance.
 *
 * In contrast to find + insert, this avoids one search operation.
 *
 * This does not search for the location, so no cmp function
 * is needed, but it uses the \p ref argument for direct
 * insertion.
 *
 * The \p ref argument can be retrieved by a using cp_dict_find_ref().
 *
 * There is no principle problem modifying the tree between finding
 * the reference and doing the insertion, however, the insertion
 * position is relative to a parent node, and it may be that after
 * inserting something else, cp_dict_find_ref() would find a different
 * node, because, e.g., by the given cmp criterion, the new key is
 * between the reference and \p nnew, so this may not be what you
 * want.  OTOH, it may be exactly what you want because you do want to
 * control the insertion manually.  Also, when the reference node is
 * removed from a tree after finding the reference, then effectively a
 * new tree is started by this insertion -- again, this works in
 * principle, but might not be what you want.
 *
 * The reference child index determines the direction of insertion:
 * if i is 0, this inserts a node smaller than the reference node, if
 * i is 1, this inserts a node larger than the reference node.
 *
 * If the reference/parent node is NULL, this assumes an imaginary
 * node that is outside of the tree, representing a node larger than
 * the absolute maximum or smaller than the absolute minimum that can
 * be inserted.  In this case, the child index is interpreted the same
 * way as with a normal node: it defines at which end of the tree this
 * assumes the reference node: if child is 0, the this inserts a new
 * maximum (i.e., a node smaller than the absolute maximum).  If child
 * is 1, this inserts a new minimum.
 *
 * See cp_dict_insert_ref() for a function without augmentation callback.
 *
 * Time complexity: O(log n) (for bottom-up rebalance)
 *
 * Stack complexity: O(1)
 */
extern void cp_dict_insert_ref_aug(
    cp_dict_t *node,
    cp_dict_ref_t const *ref,
    cp_dict_t **root,
    cp_dict_aug_t *aug)
{
    assert(node != NULL);
    assert(ref != NULL);
    assert(root != NULL);
    assert((root == NULL) || !cp_dict_may_contain(*root, node));
    assert(node->parent == NULL);
    assert(node->edge[0] == NULL);
    assert(node->edge[1] == NULL);
    assert(!cp_dict_red(node)); /* nodes outside of a tree are black */

    cp_dict_t *p = ref->parent;
    unsigned i = ref->child;

    /* Insert minimum or maximum node: there could have been an
     * insertion since the find_ref, so be sure we update p and i.
     */
    if (p == NULL) {
        i = !i;
        p = cp_dict_start(*root, i);
    }

    /* insert initial node */
    if (p == NULL) {
        assert(!cp_dict_red(node));
        *root = node;
        if (ALLOW_RED_ROOT) {
            cp_dict_set_RED_LEAF(node);
        }
        else {
            cp_dict_set_BLACK_LEAF(node);
        }
        SLOW_ASSERT(very_good_tree(node));
        return;
    }

    /* inner node? */
    if (p->edge[i] != NULL) {
        /* find leaf in given insertion direction */
        i = !i;
        p = cp_dict_step(p, i);
        assert(p != NULL);
    }

    /* leaf */
    assert(p->edge[i] == NULL);
    node->parent = p;
    node->parent->edge[i] = node;
    cp_dict_set_RED_LEAF(node);
    augment(aug, node, NULL, CP_DICT_AUG_ADD);

    /* Rebalance */
    cp_dict_t *r = *root;
    _balance_insert(aug, &r, node);
    cp_dict_ensure_BLACK(r);
    *root = r;
    assert(ALLOW_RED_ROOT || !cp_dict_red(r));
    SLOW_ASSERT(very_good_tree(r));
}

static inline unsigned char idx(
    cp_dict_t *parent,
    cp_dict_t *child)
{
    assert((cp_dict_child(parent,0) == child) || (cp_dict_child(parent,1) == child));
    return cp_dict_child(parent,1) == child;
}

static inline void swap_remove_u(
    cp_dict_aug_t *aug,
    cp_dict_t **root,
    int *ip,
    unsigned *m,
    /** node to be removed */
    cp_dict_t *c,
    /** next(node), the node to swap with */
    cp_dict_t *d)
{
    /* Left child must be NULL here, since the stand-in node is a
     * successor of a 2-node. */
    assert(d->edge[0] == NULL);

    augment(aug, c, d, CP_DICT_AUG_CUT_SWAP);

    /* The stand-in node for removal may have a right child. */
    cp_dict_t *e = cp_dict_child(d,1);

    /* start by assuming c is not the parent of d.
     * f is d's father, e is the only edge of d. */
    cp_dict_t *father = d->parent;

    /* return color of removed node */
    *m = cp_dict_red(d);

    /* copy over all parent, children, colours */
    *d = *c;

    /* i == 1 iff c is the parent of d (fig: (b))
     * In this case, e's new parent (usually f) is actually d */
    unsigned char i = (father == c);
    *ip = i;
    if (i) {
        father = d;
    }

    /* Cut off the node below f, it is always NULL.  Keep the colour
     * (could produce a red NULL edge).
     * Note the usage of i: it is the edge index.  This is due to how
     * cp_dict_next() moves down: if it moves only one step, it moves right,
     * otherwise left.  And if it moves only one step, c is parent of d.
     * For case (2), this is redundant.
     */
    cp_dict_collapse_edge(father, i, e);
    if (e != NULL) {
        e->parent = father;
    }

    /* d's pointers are all correct now, so set the buddy pointers around d */
    if (d->parent != NULL) {
        assert(c->parent == d->parent);
        cp_dict_set_child(d->parent, idx(c->parent, c), d);
    }

    if (d->edge[0] != NULL) {
        cp_dict_child(d,0)->parent = d;
    }

    if (d->edge[1] != NULL) {
        cp_dict_child(d,1)->parent = d;
    }

    /* return the collapsed edge */
    *root = father;
}

static inline void remove_u(
    cp_dict_aug_t *aug,
    cp_dict_t **root,
    int *ip,
    unsigned *m,
    cp_dict_t *c)
{
    /* fig: (D) */
    assert(c != NULL);
    /* a 2-node has to be swapped with its next node */
    if ((c->edge[0] != NULL) && (c->edge[1] != NULL)) {
        swap_remove_u(aug, root, ip, m, c, cp_dict_next(c));
        return;
    }

    *m = cp_dict_red(c);

    /* fig: (a) */
    /* get non-null child and parent */
    cp_dict_t *b = c->edge[!c->edge[0]];
    *root = c->parent;
    if (b != NULL) {
        b->parent = c->parent;
    }

    /* possibly we're done */
    if (*root == NULL) {
        *root = b;
        assert(*ip < 0);
        return;
    }

    /* skip node c */
    assert(c->parent == *root);
    unsigned char i = idx(*root, c);
    *ip = i;
    cp_dict_collapse_edge(*root, i, b);
}

static inline void balance_remove(
    cp_dict_aug_t *aug,
    cp_dict_t **root,
    cp_dict_t *p,
    unsigned i)
{
    cp_dict_t *x = p->edge[i];
    while (!cp_dict_red(x)) {
        cp_dict_t *w = p->edge[!i];
        assert(good_children(p) || (show_tree(p),0));
        if (cp_dict_red(w)) {
            /*      (p)B2      =>      (p)R2      =>    (w)B2
             *     /   \              /   \            /   \
             *  (x)B0+  (w)R1      (x)B0+  (w)B1     (p)R1
             *                                      /   \
             *                                    (x)B0  (q)  =: w
             */
            cp_dict_INC_set_BLACK(w);
            cp_dict_DEC_set_RED(p);
            rb_rotate(aug, root, i, p);
            w = p->edge[!i];
        }

        if (w != NULL) {
            assert(!cp_dict_red(w));
            if (!cp_dict_red(w->edge[!i]) && !cp_dict_red(w->edge[i])) {
                /*      (p)?        =>     (p)?+
                 *     /    \             /    \
                 *  (x)B0+  (w)B1       (x)B0  (w)R0
                 *         /   \              /   \
                 *       (r)B0 (q)B0       (r)B0  (q)B0
                 */
                cp_dict_DEC_set_RED(w);
                cp_dict_DEC(p);
                assert(good_tree(p, false) || (show_tree(p),0));
            }
            else {
                if (!cp_dict_red(w->edge[!i])) {
                    /*      (p)?         =>      (p)?       =>      (p)?
                     *     /   \                /   \              /   \
                     *  (x)B0+ (w)B1         (x)B0+ (w)R0       (x)B0+ (r)B1  =: w
                     *        /   \                /   \                  \
                     *     (r)R0  (q)B0         (r)B1  (q)B0              (w)R0
                     *                                                       \
                     *                                                       (q)B0
                     */
                    cp_dict_INC_set_BLACK(w->edge[i]);
                    cp_dict_DEC_set_RED(w);
                    rb_rotate(aug, root, !i, w);
                    assert(good_tree(w, true));
                    w = p->edge[!i];
                }
                if (cp_dict_red(p)) {
                    /*      (p)R1   =>     (p)B1      =>      (w)R1
                     *     /   \          /   \              /   \
                     *  (x)B0+ (w)B1   (x)B0+ (w)R1        (p)B1 (q)B1
                     *           \              \          /
                     *           (q)R0          (q)B1    (x)B0
                     */
                    cp_dict_set_RED(w);
                    cp_dict_set_BLACK(p);
                }
                else {
                    /*      (p)B2   =>     (p)B1      =>      (w)B2
                     *     /   \          /   \              /   \
                     *  (x)B0+ (w)B1   (x)B0+ (w)B2        (p)B1 (q)B1
                     *           \              \          /
                     *           (q)R0          (q)B1    (x)B0
                     */
                    cp_dict_INC(w);
                    cp_dict_DEC(p);
                }
                cp_dict_INC_set_BLACK(w->edge[!i]);
                rb_rotate(aug, root, i, p);
                assert(good_tree(w, true) || (show_tree(w),0));
                augment(aug, w, p, CP_DICT_AUG_FINI);
                return;
            }
        }

        augment(aug, p, x, CP_DICT_AUG_NOP);
        x = p;
        p = p->parent;
        if (p == NULL) {
            break;
        }

        i = idx(p, x);
    }

    augment(aug, p, x, CP_DICT_AUG_FINI);

    if (cp_dict_red(x)) {
        cp_dict_INC_set_BLACK(x);
        return;
    }

    if (!ALLOW_RED_ROOT) {
        cp_dict_ensure_BLACK(x);
    }
}

/**
 * Remove a node from the tree.
 *
 * If the root changes, this will update *root.
 *
 * This function does not read *root; it is a pure output
 * parameter.  In some situations, the caller might not
 * know the root when removing a key, in which case it is
 * OK to initialise *root to NULL.  If it changes to non-NULL,
 * the caller then knows that the root was updated.
 *
 * Because root is a pure output parameter, it may even be NULL.
 * In some special cases, e.g. when iterating and restructing
 * a tree at the same time, the root may not be needed anymore
 * be the caller, and to simplify the code, this function
 * accepts root=NULL.
 *
 * See cp_dict_remove() for a function without augmentation callback.
 *
 * Time complexity: O(log n) (for bottom-up rebalance)
 *
 * Stack complexity: O(1)
 */
extern void cp_dict_remove_aug(
    cp_dict_t *c,
    cp_dict_t **root,
    cp_dict_aug_t *aug)
{
    assert(c != NULL);
    assert((root == NULL) || cp_dict_may_contain(*root, c));
    assert((root == NULL) || (cp_dict_root(c) == *root));

    /* if we remove the root, we remember a child pointer for
     * resetting root if necessary */
    cp_dict_t *z = NULL;
    if (c->parent == NULL) {
        z = c->edge[0] ? c->edge[0] : c->edge[1];
    }

    cp_dict_t *p;
    unsigned red;
    int i = -1;
    remove_u(aug, &p, &i, &red, c);
    cp_dict_init(c);
    augment(aug, p, c, CP_DICT_AUG_CUT_LEAF);

    if (i < 0) {
        if (root != NULL) {
            *root = p;
        }
        if (p != NULL) {
            assert(p->parent == NULL);
            if (!ALLOW_RED_ROOT) {
                cp_dict_ensure_BLACK(p);
            }
        }
        augment(aug, p, NULL, CP_DICT_AUG_FINI);
        SLOW_ASSERT(very_good_tree(cp_dict_root(p)));
        return;
    }

    cp_dict_t *r = NULL;
    if (!red) {
        balance_remove(aug, &r, p, i & 1);
    }
    else {
        augment(aug, p, NULL, CP_DICT_AUG_FINI);
    }

    if (root != NULL) {
        if (r != NULL) {
            assert(r->parent == NULL);
            assert(ALLOW_RED_ROOT || !cp_dict_red(r));
            *root = r;
        }
        else
        if (z != NULL) {
            r = cp_dict_root(z);
            assert(r->parent == NULL);
            assert(ALLOW_RED_ROOT || !cp_dict_red(r));
            *root = r;
        }
    }
    SLOW_ASSERT(very_good_tree(cp_dict_root(r)));
}

static inline void swap_update_child(
    cp_dict_t *a,
    cp_dict_t *b)
{
    cp_dict_t *p = a->parent;
    if (p != NULL) {
        cp_dict_set_child(p, cp_dict_idx(p, b), a);
    }
}

static inline void swap_update_parent(
    cp_dict_t *a,
    unsigned i)
{
    cp_dict_t *c = cp_dict_child(a, i);
    if (c != NULL) {
        c->parent = a;
    }
}

/**
 * Swap two nodes from same or different tree.
 *
 * This can also exchange a node that's in the tree by one that is not.
 *
 * Note that this does not update the root pointer, but this may
 * be necessary if the root node might be swapped.  For this, there
 * are cp_dict_swap_update_root() and cp_dict_swap_update_root2().
 *
 * This does not callback any augmentation, because there is no
 * balancing and there is also no information whether the nodes are
 * in the same or in different trees.
 *
 * Time complexity: O(1).
 *
 * Stack complexity: O(1)
 */
extern void cp_dict_swap(
    cp_dict_t *a,
    cp_dict_t *b)
{
    assert(a != NULL);
    assert(b != NULL);
    if (a == b) {
        return;
    }

    /* swap nodes in tree */
    CP_SWAP(a, b);

    /* handle special case of two siblings */
    cp_dict_t *p = a->parent;
    if (p && (p == b->parent)) {
        assert((p->edge[0] == a) || (p->edge[0] == b));
        assert((p->edge[1] == a) || (p->edge[1] == b));
        assert(a->parent != a);
        assert(b->parent != b);
        CP_SWAP(&p->edge[0], &p->edge[1]);
    }
    else {
        /* handle if one is the child of the other */
        if (a->parent == a) {
            a->parent = b;
        }
        if (b->parent == b) {
            b->parent = a;
        }

        /* update parent's child pointers */
        swap_update_child(a, b);
        swap_update_child(b, a);
    }

    /* update children's parent pointers */
    swap_update_parent(a, 0);
    swap_update_parent(a, 1);
    swap_update_parent(b, 0);
    swap_update_parent(b, 1);
}

/**
 * Swap two nodes, also updating root.
 *
 * This function can be used to swap two nodes in one tree, or
 * a node in a tree and a node that is not in a tree, and it will
 * correctly update the root if that is swapped.
 *
 * This does not callback any augmentation, because there is no
 * balancing and there is also no information whether the nodes are
 * in the same or in different trees.
 *
 * Time complexity: O(1)
 *
 * Stack complexity: O(1)
 */
extern void cp_dict_swap_update_root(
    cp_dict_t **r,
    cp_dict_t *a,
    cp_dict_t *b)
{
    cp_dict_swap(a, b);
    if (a == *r) {
        *r = b;
    }
    else
    if (b == *r) {
        *r = a;
    }
}

/**
 * Swap two nodes, also updating roots of two trees.
 *
 * Additional to what cp_dict_swap_update_root() can do,
 * this also allows swapping nodes between two trees,
 * and both root pointers will be updated, if necessary.
 *
 * This does not callback any augmentation, because there is no
 * balancing and there is also no information whether the nodes are
 * in the same or in different trees.
 *
 * Time complexity: O(1)
 *
 * Stack complexity: O(1)
 */
extern void cp_dict_swap_update_root2(
    cp_dict_t **r1,
    cp_dict_t **r2,
    cp_dict_t *a,
    cp_dict_t *b)
{
    cp_dict_swap(a, b);
    if (a == *r1) {
        *r1 = b;
    }
    else
    if (b == *r1) {
        *r1 = a;
    }

    if (r1 != r2) {
        if (a == *r2) {
            *r2 = b;
        }
        else
        if (b == *r2) {
            *r2 = a;
        }
    }
}

/**
 * Join two trees and a single element in between into a single tree.
 *
 * The trees and element are joined in order: l, m, r.
 *
 * m must not be NULL, but it must be a single element that is not
 * in any other tree.
 *
 * This merges the trees, i.e., the input trees will be restructures
 * and become part of the whole tree.
 *
 * See cp_dict_join3() for a function without augmentation callback.
 *
 * Time complexity: O(abs(height(l) - height(r))) = O(log n)
 *
 * Stack complexity: O(1)
 */
CP_WUR
extern cp_dict_t *cp_dict_join3_aug(
    cp_dict_t *l,
    cp_dict_t *m,
    cp_dict_t *r,
    cp_dict_aug_t *aug)
{
    assert(m != NULL);
    assert(!cp_dict_is_member(m));
    assert((l == NULL) || (l->parent == NULL));
    assert((r == NULL) || (r->parent == NULL));

    /* make l the larger tree */
    unsigned i = (cp_dict_height2(l) < cp_dict_height2(r));
    if (i) {
        CP_SWAP(&l, &r);
    }

    /* if r is smaller and red, make it black */
    if ((cp_dict_height2(l) > cp_dict_height2(r)) &&
        cp_dict_red(r))
    {
        cp_dict_INC_set_BLACK(r); /* might make it equal height */
    }

    if (cp_dict_height2(l) == cp_dict_height2(r)) {
        /* m becomes the new root with two equal depths trees left and right */
        cp_dict_set_child_and_parent(m, i,  l);
        cp_dict_set_child_and_parent(m, !i, r);
        cp_dict_set_RED_same_depth(m, l);
        cp_dict_INC_set_BLACK_if_needed(m);
        SLOW_ASSERT(very_good_tree(m));
        augment(aug, m, NULL, CP_DICT_AUG_JOIN);
        return m;
    }

    /* check situation */
    assert(!cp_dict_red(r));
    assert(cp_dict_height2(l) > cp_dict_height2(r));
    assert(cp_dict_height2(l) > 0);
    assert(l != NULL);

    /* unless the root changes, l will be the root. */
    cp_dict_t *root = l;

    /* find black node in l that has the same height as r on right
     * edge of tree l.  Since c may become NULL, we need its parent p. */
    cp_dict_t *p = l;
    cp_dict_t *c = p->edge[!i];
    while (cp_dict_red(c) || (cp_dict_height2(c) > cp_dict_height2(r))) {
        p = c;
        c = p->edge[!i];
    }
    assert(p != NULL);

    /* put m where c is */
    cp_dict_set_child_and_parent(p, !i, m);
    cp_dict_set_child_and_parent(m, i,  c);
    cp_dict_set_child_and_parent(m, !i, r);
    cp_dict_set_RED_same_depth(m, c);
    assert(good_tree(m, true));
    augment(aug, m, NULL, CP_DICT_AUG_JOIN);

    /* rebalance */
    while ((p->parent != NULL) && cp_dict_red(p) && cp_dict_red(p->edge[!i])) {
        augment(aug, p, m, CP_DICT_AUG_NOP);
        m = p;
        p = p->parent;
        assert(!cp_dict_red(p));

        /*    (p)B1       =>   (p)B1       =>     (m)R1
         *   /   \            /   \              /   \
         *  R    (m)R0       R    (m)R1        (p)B1  (r)B1
         *      /   \            /   \        /   \
         *     S    (r)R0       S    (r)B1   R     S
         */
        cp_dict_INC_set_BLACK(m->edge[!i]);
        cp_dict_INC(m);
        rb_rotate(aug, &root, i, p);

        /* move up more */
        p = m;
        if (p->parent == NULL) {
            break;
        }
        p = m->parent;
    }

    /* solve double red at root */
    cp_dict_INC_set_BLACK_if_needed(p);
    SLOW_ASSERT(very_good_tree(root) || (show_tree(root),0));
    augment(aug, p, NULL, CP_DICT_AUG_FINI);

    return root;
}

/**
 * Join two trees.
 *
 * This is the same as cp_dict_join3(), but without adding
 * an inner node.  This is a bit more expensive than
 * cp_dict_join3().
 *
 * This internally uses cp_dict_extract_min() and
 * then cp_dict_join3().
 *
 * See cp_dict_join2() for a function without augmentation callback.
 *
 * Time complexity: O(height(l) + height(r)) = O(log n)
 *
 * Stack complexity: O(1)
 */
CP_WUR
extern cp_dict_t *cp_dict_join2_aug(
    cp_dict_t *l,
    cp_dict_t *r,
    cp_dict_aug_t *aug)
{
    if (l == NULL) {
        return r;
    }
    if (r == NULL) {
        return l;
    }
    cp_dict_t *m = cp_dict_extract_min_aug(&r, aug);
    return cp_dict_join3_aug(l, m, r, aug);
}

/**
 * Split a tree based on a reference value and a comparison
 * function.
 *
 * This is an internal function.  Use the more type-safe cp_dict_split()
 * instead.
 *
 * Elements that compare less will be in `*l`, those greater will
 * be in `*r.  Equal elements will be in `*r` if `back` is 1,
 * or will be in `*l` if `back` is 0.
 *
 * The cmp function will receive the \p idx pointer as the first
 * parameter (just like find_ref).
 *
 * Time complexity:  O(log n)
 *
 * Stack complexity: O(log n)
 */
extern void cp_dict_split_aug_(
    cp_dict_t **l,
    cp_dict_t **r,
    cp_dict_t *n,
    void *idx,
    cp_dict_cmp_t_ cmp,
    void *user,
    bool back,
    cp_dict_aug_t *aug)
{
    /* split empty tree? => two empty trees */
    if (n == NULL) {
        *l = NULL;
        *r = NULL;
        return;
    }

    /* FIXME: this can probably be done non-recursively by
     * running this bottom up:
     * (1) find the leaf based on cmp(idx),
     * (2) walk back up using parent and remerge.
     */

    assert(cp_dict_is_root(n));

    augment(aug, n, NULL, CP_DICT_AUG_SPLIT);

    /* disassemble root */
    cp_dict_t *nl = n->edge[0];
    n->edge[0] = NULL;
    if (nl != NULL) {
        nl->parent = NULL;
    }

    cp_dict_t *nr = n->edge[1];
    n->edge[1] = NULL;
    if (nr != NULL) {
        nr->parent = NULL;
    }
    assert(!cp_dict_is_member(n));

    /* compare */
    unsigned i = cmp(idx, n, user) >= back;
    /*   n OP idx   cmp(idx,n)   back  i   meaning
     *   -----------------------------------------------
     *     <        -1           1     0   n goes right
     *     <        -1           0     0   n goes right
     *     >        +1           1     1   n goes left
     *     >        +1           0     1   n goes left
     *     =        0            1     0   n goes right
     *     =        0            0     1   n goes left
     */

    /* recurse to disassemble the tree where n does not go,
     * then assemble the tree where it does go. */
    cp_dict_t *nm;
    if (i) {
        /* n goes left, so disassemble right tree */
        cp_dict_split_aug_(&nm, r, nr, idx, cmp, user, back, aug);
        /* then reassemble left tree */
        *l = cp_dict_join3_aug(nl, n, nm, aug);
    }
    else {
        /* n goes right, so disassemble left tree */
        cp_dict_split_aug_(l, &nm, nl, idx, cmp, user, back, aug);
        /* then reassemble right tree */
        *r = cp_dict_join3_aug(nm, n, nr, aug);
    }
}

/* ********************************************************************** */
/* macros */
#ifdef CP_MACRO_

/**
 * Find a node in the tree.
 *
 * The node is returned if found, otherwise, this returns NULL.
 *
 * If dup is 0, this finds some equal element.  For -2, it finds
 * the first, for +2, it finds the last.
 *
 * The cmp function will receive the \p idx pointer as the first
 * parameter.
 *
 * Time complexity: O(log n)
 *
 * Stack complexity: O(1)
 */
extern macro val cp_dict_find(
    val *idx, cp_dict_t *root, val cmp, val *user, int dup)
{
    int (*_l_cmp)(
        __typeof__(idx),
        cp_dict_t *,
        __typeof__(user)) = cmp;
    cp_dict_find_ref_(
        NULL,
        (void*)(size_t)idx,
        root,
        (cp_dict_cmp_t_)_l_cmp,
        (void*)(size_t)user,
        dup);
}

/**
 * Find a node in the tree.
 *
 * The node is returned if found and duplicate==0, otherwise, this returns NULL.
 *
 * The key's reference is returned, too, so that this can be used to directly
 * insert in the found location in the tree using cp_dict_insert_ref().
 * If the found node is the root, the reference points is (NULL,1), i.e., marking
 * the found node as the successor to the NULL node.
 *
 * If the tree is empty, the reference will be (NULL,1).
 *
 * The cmp function will receive the \p idx pointer as the first
 * parameter.
 *
 * If duplicate is -1 or +1, this will return NULL for non-equal
 * matches, and will set up ref to point to the insertion position
 * left (duplicate < 0) or right (duplicate > 0) of the actual
 * element.  In this setup, the function will always the smallest or
 * largest equal node, or NULL if none was equal.
 *
 * If duplicate is 0, this will return some entry that compared equal.
 *
 * If duplicate is -2, this will return the left-most ('smallest') equal
 * entry and set up the reference just like -1 was passed.
 *
 * If duplicate is +2, this will return the right-most ('largest') equal
 * entry and set up the reference just like +1 was passed.
 *
 * Time complexity: O(log n) (for top-down find)
 *
 * Stack complexity: O(1)
 */
extern macro val cp_dict_find_ref(
    cp_dict_ref_t *ref, val *idx, cp_dict_t *root, val cmp, val *user, int dup)
{
    int (*_l_cmp)(
        __typeof__(idx),
        cp_dict_t *,
        __typeof__(user)) = cmp;
    cp_dict_find_ref_(
        ref,
        (void*)(size_t)idx,
        root,
        (cp_dict_cmp_t_)_l_cmp,
        (void*)(size_t)user,
        dup);
}

/**
 * Insert a new node, then rebalance.
 *
 * This takes a pointer to the root.  The root may be updated by the operation.
 *
 * This takes a node and a separate key for insertion.  Once inserted into the dictionary,
 * the order will not change, so in some cases, this can be used to insert nodes without
 * storing the key inside the node.  In that case, cp_dict_find() cannot be used, but
 * iteration will still work in the order of insertion.
 *
 * If duplicate is non-0, will insert duplicates to the given side (-1: left, +1: right).
 *
 * \returns an equal node if there was one and duplicate is 0.
 *
 * See cp_dict_insert_by() for a function without augmentation callback.
 *
 * See cp_dict_insert_update_by() for a function with an O(1) min/max update.
 *
 * Time complexity: O(log n) (for top-down find + bottom-up rebalance)
 *
 * Stack complexity: O(1)
 */
extern macro val cp_dict_insert_by_aug(
    cp_dict_t *nnew, val *idx,
    cp_dict_t **root,
    val cmp, val *user,
    int dup,
    cp_dict_aug_t *aug)
{
    int (*_l_cmp)(
        __typeof__(idx),
        cp_dict_t *,
        __typeof__(user)) = cmp;
    cp_dict_insert_by_aug_(
        nnew,
        (void*)(size_t)idx,
        root,
        (cp_dict_cmp_t_)_l_cmp,
        (void*)(size_t)user,
        dup,
        aug);
}

/**
 * Insert a new node, then rebalance.
 *
 * This takes as key the node itself, and otherwise behaves like cp_dict_insert_by().
 *
 * See cp_dict_insert() for a function without augmentation callback.
 *
 * See cp_dict_insert_update() for a function with an O(1) min/max update.
 *
 * Time complexity: O(log n) (for top-down find + bottom-up rebalance)
 *
 * Stack complexity: O(1)
 */
extern macro val cp_dict_insert_aug(
    cp_dict_t *nnew,
    cp_dict_t **root,
    val cmp, val user,
    int dup,
    cp_dict_aug_t *aug)
{
    cp_dict_insert_by_aug(nnew, nnew, root, cmp, user, dup, aug);
}

/**
 * Insert a new node, then rebalance.
 *
 * This takes a pointer to the root.  The root may be updated by the operation.
 *
 * This also takes optional pointers to the minimum and/or maximum of
 * the tree and updates them as well, if necessary.  The overhead if
 * doing this is O(1), so it is faster than running O(log n)
 * cp_dict_min() and/or cp_dict_max() after the operation.
 *
 * This takes a node and a separate key for insertion.  Once inserted
 * into the dictionary, the order will not change, so in some cases,
 * this can be used to insert nodes without storing the key inside the
 * node.  In that case, cp_dict_find() cannot be used, but iteration
 * will still work in the order of insertion.
 *
 * If duplicate is non-0, will insert duplicates to the given side
 * (-1: left, +1: right).
 *
 * \returns an equal node if there was one and duplicate is 0.
 *
 * See cp_dict_insert_update_by() for a function without augmentation callback.
 *
 * See cp_dict_insert_by() for a function without min/max update.
 *
 * Time complexity: O(log n) (for top-down find + bottom-up rebalance)
 *
 * Stack complexity: O(1)
 */
extern macro val cp_dict_insert_update_by_aug(
    cp_dict_t *nnew, val *idx,
    cp_dict_t **root, cp_dict_t **minp, cp_dict_t **maxp,
    val cmp, val *user,
    int dup,
    cp_dict_aug_t *aug)
{
    int (*_l_cmp)(
        __typeof__(idx),
        cp_dict_t *,
        __typeof__(user)) = cmp;
    cp_dict_insert_update_by_aug_(
        nnew,
        (void*)(size_t)idx,
        root, minp, maxp,
        (cp_dict_cmp_t_)_l_cmp,
        (void*)(size_t)user,
        dup,
        aug);
}

/**
 * Insert a new node, then rebalance.
 *
 * This takes as key the node itself, and otherwise behaves like cp_dict_insert_update_by().
 *
 * See cp_dict_insert_update() for a function without augmentation callback.
 *
 * See cp_dict_insert() for a function without a min/max update.
 *
 * Time complexity: O(log n) (for top-down find + bottom-up rebalance)
 *
 * Stack complexity: O(1)
 */
extern macro val cp_dict_insert_update_aug(
    cp_dict_t *nnew,
    cp_dict_t **root, cp_dict_t **minp, cp_dict_t **maxp,
    val cmp, val user,
    int dup,
    cp_dict_aug_t *aug)
{
    cp_dict_insert_update_by_aug(
        nnew, nnew, root, minp, maxp, cmp, user, dup, aug);
}

/**
 * Split a tree based on a reference value and a comparison
 * function.
 *
 * Elements that compare less will be in `*l`, those greater will
 * be in `*r.  Equal elements will be in `*r` if `leq` is 0,
 * or will be in `*l` if `leq` is 1.
 *
 * The cmp function will receive the \p idx pointer as the first
 * parameter (just like find_ref).
 *
 * See cp_dict_split() for a function without augmentation callback.
 *
 * Time complexity:  O(log n)
 *
 * Stack complexity: O(log n)
 */
extern macro void cp_dict_split_aug(
    cp_dict_t **r, cp_dict_t **l, cp_dict_t *root,
    val *idx, val cmp, val *user,
    val leq,
    cp_dict_aug_t *aug)
{
    int (*_l_cmp)(
        __typeof__(idx),
        cp_dict_t *,
        __typeof__(user)) = cmp;
    cp_dict_split_aug_(
        r, l, root,
        (void*)(size_t)idx,
        (cp_dict_cmp_t_)_l_cmp,
        (void*)(size_t)user,
        leq,
        aug);
}

#endif /*0*/
