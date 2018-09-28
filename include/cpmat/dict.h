/* -*- Mode: C -*- */
/**
 * Dictionary.
 *
 * Implemented by binary search trees using the red/black algorithm.
 *
 * This is implemented according to the Original CLR red-black trees.
 *
 * This code is from my rbtree vs. avl vs. llrb vs. jwtree vs. aatree
 * comparison project.
 */

#ifndef __CSG_SET_H
#define __CSG_SET_H

#include <assert.h>
#include <cpmat/def.h>
#include <cpmat/dict_tam.h>

/**
 * For expression to iterate the tree in order.
 */
#define cp_dict_each(elem, tree) \
    cp_dict_t *elem = cp_dict_min(tree); \
    elem != NULL; \
    elem = cp_dict_next(elem)

/**
 * Iterate in reverse order
 */
#define cp_dict_each_rev(elem, tree) \
    cp_dict_t *elem = cp_dict_max(tree); \
    elem != NULL; \
    elem = cp_dict_prev(elem)

/**
 * Comparison function for all basic operations of the binary search tree. */
typedef int (*__cp_dict_cmp_t)(
    void *a,
    void *b,
    void *user);

typedef struct {
    cp_dict_t *parent;
    unsigned child;
} cp_dict_ref_t;

/**
 * Internal, type-unsafe variant of cp_dict_find_ref().
 *
 * Do not use, use cp_dict_find_ref() or cp_dict_find() instead.
 */
extern cp_dict_t *__cp_dict_find_ref(
    cp_dict_ref_t *ref,
    void *idx,
    cp_dict_t *root,
    __cp_dict_cmp_t cmp,
    void *user,
    int duplicate);

/**
 * Find a node in the tree.
 *
 * The node is returned if found, otherwise, this returns NULL.
 *
 * If there are multiple nodes of the same key, \p back decides which
 * one to return.  if back=0, return the first.  If back=1, return the last.
 *
 * The cmp function will receive the \p idx pointer as the second
 * parameter.
 */
#define cp_dict_find(idx, root, cmp, user) \
    ({ \
        __typeof__(*(idx))  *__idx = (idx); \
        __typeof__(*(user)) *__user = (user); \
        int (*__cmp)( \
            __typeof__(__idx), \
            cp_dict_t *, \
            __typeof__(__user)) = (cmp); \
        __cp_dict_find_ref( \
            NULL,\
            (void*)(size_t)__idx, \
            root, \
            (__cp_dict_cmp_t)__cmp, \
            (void*)(size_t)__user, \
            0); \
    })

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
 * If there are multiple nodes of the same key, \p back decides which
 * one to return.  if back=0, return the first.  If back=1, return the last.
 *
 * The cmp function will receive the \p idx pointer as the second
 * parameter.
 *
 * If duplicate is non-0, this will not return the exact entry, but will
 * set up ref to point to the insertion position left (duplicate < 0) or
 * right (duplicate > 0) of the actual element.  In this setup, the function
 * will always return NULL.
 *
 * Runtime: O(log n) (for top-down find)
 */
#define cp_dict_find_ref(ref, idx, root, cmp, user, dup) \
    ({ \
        __typeof__(*(idx))  *__idx = (idx); \
        __typeof__(*(user)) *__user = (user); \
        int (*__cmp)( \
            __typeof__(__idx), \
            cp_dict_t *, \
            __typeof__(__user)) = (cmp); \
        __cp_dict_find_ref( \
            ref,\
            (void*)(size_t)__idx, \
            root, \
            (__cp_dict_cmp_t)__cmp, \
            (void*)(size_t)__user, \
            dup); \
    })

/**
 * Get the root node of a tree from an arbitrary node.
 *
 * This can be used if the root point is not stored for some reason,
 * to find the root for any node in the tree.
 *
 * Runtime: O(log n) (for bottom-up parent search)
 */
extern cp_dict_t *cp_dict_root(
    cp_dict_t *n);

/**
 * Internal, type-unsafe variant of cp_dict_insert_by().
 *
 * Do not use, use cp_dict_insert_by() or cp_dict_insert() instead.
 */
extern cp_dict_t *__cp_dict_insert_by(
    cp_dict_t *nnew,
    void *idx,
    cp_dict_t **root,
    __cp_dict_cmp_t,
    void *user,
    int duplicate);

/**
 * Insert a new node, then rebalance.
 *
 * This takes a pointer to the root.  The root may be updated by the operation.
 *
 * This takes a node and a separate key for insersion.  Once inserted into the dictionary,
 * the order will not change, so in some cases, this can be used to insert nodes without
 * storing the key inside the node.  In that case, cp_dict_find() cannot be used, but
 * iteration will still work in the order of insertion.
 *
 * If duplicate is non-0, will insert duplicates to the given side (-1: left, +1: right).
 *
 * \returns an equal node if there was one and duplicate is 0.
 *
 * Runtime: O(log n) (for top-down find + bottom-up rebalance)
 */
#define cp_dict_insert_by(nnew, idx, root, cmp, user, dup) \
    ({ \
        __typeof__(*(idx))  *__idx = (idx); \
        __typeof__(*(user)) *__user = (user); \
        int (*__cmp)( \
            __typeof__(__idx), \
            cp_dict_t *, \
            __typeof__(__user)) = (cmp); \
        __cp_dict_insert_by( \
            nnew, \
            (void*)(size_t)__idx, \
            root, \
            (__cp_dict_cmp_t)__cmp, \
            (void*)(size_t)__user, \
            dup); \
    })

/**
 * Insert a new node, then rebalance.
 *
 * This takes as key the node itself, and otherwise behaves like cp_dict_insert_by().
 *
 * Runtime: O(log n) (for top-down find + bottom-up rebalance)
 */
#define cp_dict_insert(nnew, root, cmp, user, dup) \
    ({ \
        cp_dict_t *__nnew = (nnew); \
        cp_dict_insert_by(__nnew, __nnew, root, cmp, user, dup); \
    })

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
 * Runtime: O(log n) (for bottom-up rebalance)
 */
extern void cp_dict_insert_ref(
    cp_dict_t *nnew,
    cp_dict_ref_t const *ref,
    cp_dict_t **root);

/**
 * Start to iterate.
 * back=0 finds the first element, back=1 finds the last.
 *
 * Runtime: O(log n) (for top-down leaf search)
 * For whole tree iteration, start + n*step have runtime O(n).
 */
extern cp_dict_t *cp_dict_start(
    cp_dict_t *root,
    unsigned back);

/**
 * Iterate a tree: do one step.
 * back=0 does a step forward, back=1 does a step backward.
 *
 * Runtime: O(log n) (for bottom-up/top-down search for next)
 * For whole tree iteration, start + n*step have runtime O(n),
 * i.e., ammortized costs for step are O(1).
 */
extern cp_dict_t *cp_dict_step(
    cp_dict_t *last,
    unsigned back);

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
 * Runtime: O(log n) (for bottom-up rebalance)
 */
extern void cp_dict_remove(
    cp_dict_t *c,
    cp_dict_t **root);


/**
 * Swap two nodes from same or different tree.
 *
 * This can also exchange a node that's in the tree by one that is not.
 *
 * Runtime: O(1).
 */
extern void cp_dict_swap(
    cp_dict_t *a,
    cp_dict_t *b);

/**
 * Swap two nodes, also updating root.
 */
extern void cp_dict_swap_update_root(
    cp_dict_t **r,
    cp_dict_t *a,
    cp_dict_t *b);

/**
 * Swap two nodes, also updating roots of two trees.
 */
extern void cp_dict_swap_update_root2(
    cp_dict_t **r1,
    cp_dict_t **r2,
    cp_dict_t *a,
    cp_dict_t *b);

/* *** static inline functions ****************************************** */

/**
 * Initialise a new node
 *
 * Runtime: O(1)
 */
static inline void cp_dict_init (
    cp_dict_t *node)
{
    CP_ZERO(node);
}

/**
 * Get child 0 or child 1.
 *
 * Runtime: O(1)
 */
static inline cp_dict_t *cp_dict_child(cp_dict_t *n, unsigned i)
{
    assert(i <= 1);
    return n->edge[i];
}

/**
 * Get the index of a child in its parent node.
 *
 * Runtime: O(1)
 */
static inline unsigned cp_dict_idx(cp_dict_t *parent, cp_dict_t *child)
{
    assert((cp_dict_child(parent,0) == child) ||
           (cp_dict_child(parent,1) == child));
    return cp_dict_child(parent,1) == child;
}

/**
 * Get first element (e.g., the minimum).
 *
 * Runtime: O(log n) (for top-down leaf search)
 * For whole tree iteration, min + n*next have runtime O(n).
 */
static inline cp_dict_t *cp_dict_min(cp_dict_t *root)
{
    return cp_dict_start(root,0);
}

/**
 * Get last element (e.g., the maximum).
 *
 * Runtime: O(log n) (for top-down leaf search)
 * For whole tree iteration, max + n*prev have runtime O(n).
 */
static inline cp_dict_t *cp_dict_max(cp_dict_t *root)
{
    return cp_dict_start(root,1);
}

/**
 * Extract the first or last element.
 *
 * Runtime: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_start(cp_dict_t **root, unsigned i)
{
    cp_dict_t *r = cp_dict_start(*root, i);
    if (r == NULL) {
        return NULL;
    }
    cp_dict_remove(r, root);
    return r;
}

/**
 * Extract the first element (minimum).
 *
 * Runtime: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_min(cp_dict_t **root)
{
    return cp_dict_extract_start(root, 0);
}

/**
 * Extract the last element (maximum).
 *
 * Runtime: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_max(cp_dict_t **root)
{
    return cp_dict_extract_start(root, 1);
}

/**
 * Same as cp_dict_step(), but returns NULL if last == NULL
 * instead of assert-failing.
 */
static inline cp_dict_t *cp_dict_step0(
    cp_dict_t *last,
    unsigned back)
{
    return last == NULL ? NULL : cp_dict_step(last, back);
}

/**
 * Iterate a tree: get the next node.
 *
 * Runtime: O(log n) (for bottom-up/top-down search for next)
 * For whole tree iteration, min + n*next have runtime O(n),
 * i.e., ammortized costs for next are O(1).
 */
static inline cp_dict_t *cp_dict_next(
    cp_dict_t *last)
{
    return cp_dict_step(last,0);
}

/**
 * Same as cp_dict_next(), but returns NULL if last == NULL
 * instead of assert-failing.
 */
static inline cp_dict_t *cp_dict_next0(
    cp_dict_t *last)
{
    return cp_dict_step0(last,0);
}

/**
 * Iterate a tree: get the previsou node.
 *
 * Runtime: O(log n) (for bottom-up/top-down search for prev)
 * For whole tree iteration, max + n*prev have runtime O(n),
 * i.e., ammortized costs for prev are O(1).
 */
static inline cp_dict_t *cp_dict_prev(
    cp_dict_t *last)
{
    return cp_dict_step(last,1);
}

/**
 * Same as cp_dict_prev(), but returns NULL if last == NULL
 * instead of assert-failing.
 */
static inline cp_dict_t *cp_dict_prev0(
    cp_dict_t *last)
{
    return cp_dict_step0(last,1);
}

/**
 * Whether the given node is a root.
 *
 * Runtime: O(1)
 */
static inline bool cp_dict_is_root(
    cp_dict_t *n)
{
    return (n == NULL) || (n->parent == NULL);
}

/**
 * Whether the node is in a dictionary.
 *
 * Note that each node is actually its own 1-element dictionary, so
 * this function returns 'true' only in the non-trivial case whether
 * at least one other element is in the tree.
 */
static inline bool cp_dict_is_member(
    cp_dict_t *n)
{
    return
        (n != NULL) &&
        (
            (n->parent != NULL) ||
            (n->edge[0] != NULL) ||
            (n->edge[1] != NULL)
        );
}

#endif /* __CSG_SET_H */
