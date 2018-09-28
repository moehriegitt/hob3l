/* -*- Mode: C -*- */
/*
 * Original CLR implementation.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <cpmat/dict.h>

static cp_dict_t old_root = { NULL, { NULL, NULL }, false };

static inline cp_dict_t *cp_dict_parent(
    cp_dict_t *n)
{
    return n->parent;
}

static inline bool cp_dict_red(
    cp_dict_t *e)
{
    return (e != NULL) && e->red;
}

static inline void cp_dict_set_child(
    cp_dict_t *r,
    unsigned i,
    cp_dict_t *n)
{
    assert(i < cp_countof(r->edge));
    r->edge[i] = n;
}

static inline void cp_dict_collapse_edge(
    cp_dict_t *r,
    unsigned i,
    cp_dict_t *e)
{
    assert(i < cp_countof(r->edge));
    if ((e != NULL) && cp_dict_red(r->edge[i])) {
        e->red = true;
    }
    r->edge[i] = e;
}


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
}

static void _balance_insert(
    cp_dict_t **root,
    cp_dict_t *x)
{
    while ((x != *root) && cp_dict_red(x->parent)) {
        unsigned side = cp_dict_idx(x->parent->parent, x->parent);
        cp_dict_t *y = x->parent->parent->edge[!side];
        if (cp_dict_red(y)) {
            x->parent->red = false;
            y->red = false;
            x->parent->parent->red = true;

            x = x->parent->parent;
        }
        else {
            if (x == x->parent->edge[!side]) {
                x = x->parent;
                rb_rotate(root, side, x);
            }
            x->parent->red = false;
            x->parent->parent->red = true;
            rb_rotate(root, !side, x->parent->parent);
        }
    }
}

extern cp_dict_t *__cp_dict_find_ref(
    cp_dict_ref_t *ref,
    void *idx,
    cp_dict_t *n,
    __cp_dict_cmp_t cmp,
    void *user,
    int duplicate)
{
    unsigned i = 1;
    cp_dict_t *p = n;
    while (n != NULL) {
        int d = cmp(idx, n, user);
        if (d == 0) {
            d = duplicate;
        }
        if (d == 0) {
            break;
        }
        p = n;
        i = d > 0;
        n = cp_dict_child(n, i);
    }

    assert(i <= 1);
    if (ref != NULL) {
        ref->parent = p;
        ref->child = i;
    }

    return n;
}

extern void cp_dict_insert_ref(
    cp_dict_t *node,
    cp_dict_ref_t const *ref,
    cp_dict_t **root)
{
    assert(node != NULL);
    assert(ref != NULL);
    assert(root != NULL);
    assert(!cp_dict_is_member(node));
    assert(!node->red);

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
        *root = node;
        assert(!node->red);
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
    node->red = true;

    /* Rebalance */
    cp_dict_t *r = *root;
    _balance_insert(&r, node);
    r->red = false;
    *root = r;
    assert(!r->red);
}

extern cp_dict_t *__cp_dict_insert_by(
    cp_dict_t *node,
    void *key,
    cp_dict_t **root,
    __cp_dict_cmp_t cmp,
    void *user,
    int duplicate)
{
    assert(root);
    assert(node);
    assert(node->parent == NULL);
    assert(cp_dict_child(node, 0) == NULL);
    assert(cp_dict_child(node, 1) == NULL);

    /* find */
    cp_dict_ref_t ref;
    cp_dict_t *n = __cp_dict_find_ref(&ref, key, *root, cmp, user, duplicate);
    if (n != NULL) {
        /* found exact entry => no duplicates are wanted */
        return n;
    }

    /* insert */
    cp_dict_insert_ref(node, &ref, root);
    return NULL;
}

static inline unsigned char idx(
    cp_dict_t *parent,
    cp_dict_t *child)
{
    assert((cp_dict_child(parent,0) == child) || (cp_dict_child(parent,1) == child));
    return cp_dict_child(parent,1) == child;
}

static void swap_remove_u(
    cp_dict_t **p,
    int *ip,
    unsigned *m,
    cp_dict_t *c,
    cp_dict_t *d)
{
    /* Left child must be NULL here, since the stand-in node is a
     * successor of a 2-node. */
    assert(d->edge[0] == NULL);

    /* The stand-in node for removal may have a right child. */
    cp_dict_t *e = cp_dict_child(d,1);

    /* start by assuming c is not the parent of d.
     * f is d's father, e is the only edge of d. */
    cp_dict_t *f = d->parent;

    /* return color of removed node */
    *m = cp_dict_red(d);

    /* copy over all parent, children, colours */
    *d = *c;

    /* i == 1 iff c is the parent of d (fig: (b))
     * In this case, e's new parent (usually f) is actually d */
    unsigned char i = (f == c);
    *ip = i;
    if (i) {
        f = d;
    }

    /* Cut off the node below f, it is always NULL.  Keep the colour
     * (could produce a red NULL edge).
     * Note the usage of i: it is the edge index.  This is due to how
     * cp_dict_next() moves down: if it moves only one step, it moves right,
     * otherwise left.  And if it moves only one step, c is parent of d.
     * For case (2), this is redundant.
     */
    cp_dict_collapse_edge(f, i, e);
    if (e != NULL) {
        e->parent = f;
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
    *p = f;
}

static void remove_u(
    cp_dict_t **p,
    int *ip,
    unsigned *m,
    cp_dict_t *c)
{
    /* fig: (D) */
    assert(c != NULL);
    /* a 2-node has to be swapped with its next node */
    if ((c->edge[0] != NULL) && (c->edge[1] != NULL)) {
        return swap_remove_u(p, ip, m, c, cp_dict_next(c));
    }

    *m = cp_dict_red(c);

    /* fig: (a) */
    /* get non-null child and parent */
    cp_dict_t *b = c->edge[!c->edge[0]];
    *p = c->parent;
    if (b != NULL) {
        b->parent = *p;
    }

    /* possibly we're done */
    if (*p == NULL) {
        *p = b;
        assert(*ip < 0);
        return;
    }

    /* skip node c */
    assert(c->parent == *p);
    unsigned char i = idx(*p, c);
    *ip = i;
    cp_dict_collapse_edge(*p, i, b);
}

static void balance_remove(
    cp_dict_t **root,
    cp_dict_t *p,
    unsigned i,
    cp_dict_t *x)
{
    while (!cp_dict_red(x)) {
        cp_dict_t *w = p->edge[!i];
        if (cp_dict_red(w)) {
            w->red = false;
            p->red = true;
            rb_rotate(root, i, p);
            w = p->edge[!i];
        }

        if (w != NULL) {
            if (!cp_dict_red(w->edge[!i]) && !cp_dict_red(w->edge[i])) {
                w->red = true;
            }
            else {
                if (!cp_dict_red(w->edge[!i])) {
                    w->edge[i]->red = false;
                    w->red = true;
                    rb_rotate(root, !i, w);
                    w = p->edge[!i];
                }
                w->red = p->red;
                p->red = false;
                w->edge[!i]->red = false;
                rb_rotate(root, i, p);
                return;
            }
        }

        x = p;
        p = p->parent;
        if (p == NULL) {
            break;
        }

        i = idx(p, x);
    }
    x->red = 0;
}

extern void cp_dict_remove(
    cp_dict_t *c,
    cp_dict_t **root)
{
    assert(c != NULL);

    /* if we remove the root, we remember a child pointer for
     * resetting root if necessary */
    cp_dict_t *z = NULL;
    if (c->parent == NULL) {
        z = c->edge[0] ?: c->edge[1];
    }

    cp_dict_t *p;
    unsigned red;
    int i = -1;
    remove_u(&p, &i, &red, c);
    cp_dict_init(c);
    if (i < 0) {
        if (root != NULL) {
            *root = p;
        }
        if (p != NULL) {
            assert(p->parent == NULL);
            p->red = false;
        }
        return;
    }

    cp_dict_t *r = &old_root;
    if (!red) {
        balance_remove(&r, p, i & 1, p->edge[i]);
    }

    if (root != NULL) {
        if (r != &old_root) {
            assert(r->parent == NULL);
            assert(!r->red);
            *root = r;
        }
        else
        if (z != NULL) {
            r = cp_dict_root(z);
            assert(r->parent == NULL);
            assert(!r->red);
            *root = r;
        }
    }
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

    /* handle if one is the child of the other */
    if (a->parent == a) {
        a->parent = b;
    }
    if (b->parent == b) {
        b->parent = a;
    }

    /* update relative's pointers */
    swap_update_child(a, b);
    swap_update_child(b, a);

    swap_update_parent(a, 0);
    swap_update_parent(a, 1);
    swap_update_parent(b, 0);
    swap_update_parent(b, 1);
}

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

/*EOF*/
