/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * Dictionary.
 *
 * Implemented by binary search trees using the red/black algorithm.
 *
 * This is implemented according to the Original CLR red-black trees.
 *
 * Some extensions are implemented:
 *
 *   * a red root node is allowed
 *   * the black height is stored so it's available in O(1)
 *   * an augmentation callback is provided so auxiliary data can be updated
 *   * join3, join2, and split functions for O(log n) bulk operations were added
 *
 * The initial code for this is from my rbtree vs. avl vs. llrb vs. jwtree
 * vs. aatree comparison project.
 */

#ifndef CSG_SET_H_
#define CSG_SET_H_

#include <assert.h>
#include <hob3lbase/base-def.h>
#include <hob3lbase/dict_tam.h>

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
 * For expression to iterate the tree in order, allowing to remove
 * the current element.  This caches the next element.
 */
#define cp_dict_each_robust(elem, tree) \
    cp_dict_t \
       *elem = cp_dict_min(tree), \
       *next_ = elem ? cp_dict_next(elem) : NULL; \
    elem != NULL; \
    elem = next_, next_ = elem ? cp_dict_next(elem) : NULL

/* BEGIN MACRO * DO NOT EDIT, use 'mkmacro' instead. */

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
#define cp_dict_find(idx,root,cmp,user,dup) \
    cp_dict_find_1_(CP_GENSYM(_l_cmpFR), CP_GENSYM(_dupFR), \
        CP_GENSYM(_idxFR), CP_GENSYM(_rootFR), CP_GENSYM(_userFR), (idx), \
        (root), (cmp), (user), (dup))

#define cp_dict_find_1_(_l_cmp,dup,idx,root,user,_idx,_root,cmp,_user,_dup) \
    ({ \
        __typeof__(*_idx) *idx = _idx; \
        cp_dict_t * root = _root; \
        __typeof__(*_user) *user = _user; \
        int dup = _dup; \
        int (*_l_cmp)( \
            __typeof__(idx), \
            cp_dict_t *, \
            __typeof__(user)) = cmp; \
        cp_dict_find_ref_( \
            NULL, \
            (void*)(size_t)idx, \
            root, \
            (cp_dict_cmp_t_)_l_cmp, \
            (void*)(size_t)user, \
            dup); \
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
#define cp_dict_find_ref(ref,idx,root,cmp,user,dup) \
    cp_dict_find_ref_1_(CP_GENSYM(_l_cmpWQ), CP_GENSYM(_dupWQ), \
        CP_GENSYM(_idxWQ), CP_GENSYM(_refWQ), CP_GENSYM(_rootWQ), \
        CP_GENSYM(_userWQ), (ref), (idx), (root), (cmp), (user), (dup))

#define cp_dict_find_ref_1_(_l_cmp,dup,idx,ref,root,user,_ref,_idx,_root,cmp,_user,_dup) \
    ({ \
        cp_dict_ref_t * ref = _ref; \
        __typeof__(*_idx) *idx = _idx; \
        cp_dict_t * root = _root; \
        __typeof__(*_user) *user = _user; \
        int dup = _dup; \
        int (*_l_cmp)( \
            __typeof__(idx), \
            cp_dict_t *, \
            __typeof__(user)) = cmp; \
        cp_dict_find_ref_( \
            ref, \
            (void*)(size_t)idx, \
            root, \
            (cp_dict_cmp_t_)_l_cmp, \
            (void*)(size_t)user, \
            dup); \
    })

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
#define cp_dict_insert_by_aug(nnew,idx,root,cmp,user,dup,aug) \
    cp_dict_insert_by_aug_1_(CP_GENSYM(_l_cmpFT), CP_GENSYM(_augFT), \
        CP_GENSYM(_dupFT), CP_GENSYM(_idxFT), CP_GENSYM(_nnewFT), \
        CP_GENSYM(_rootFT), CP_GENSYM(_userFT), (nnew), (idx), (root), \
        (cmp), (user), (dup), (aug))

#define cp_dict_insert_by_aug_1_(_l_cmp,aug,dup,idx,nnew,root,user,_nnew,_idx,_root,cmp,_user,_dup,_aug) \
    ({ \
        cp_dict_t * nnew = _nnew; \
        __typeof__(*_idx) *idx = _idx; \
        cp_dict_t ** root = _root; \
        __typeof__(*_user) *user = _user; \
        int dup = _dup; \
        cp_dict_aug_t * aug = _aug; \
        int (*_l_cmp)( \
            __typeof__(idx), \
            cp_dict_t *, \
            __typeof__(user)) = cmp; \
        cp_dict_insert_by_aug_( \
            nnew, \
            (void*)(size_t)idx, \
            root, \
            (cp_dict_cmp_t_)_l_cmp, \
            (void*)(size_t)user, \
            dup, \
            aug); \
    })

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
#define cp_dict_insert_aug(nnew,root,cmp,user,dup,aug) \
    cp_dict_insert_aug_1_(CP_GENSYM(_augZO), CP_GENSYM(_dupZO), \
        CP_GENSYM(_nnewZO), CP_GENSYM(_rootZO), (nnew), (root), (cmp), \
        (user), (dup), (aug))

#define cp_dict_insert_aug_1_(aug,dup,nnew,root,_nnew,_root,cmp,user,_dup,_aug) \
    ({ \
        cp_dict_t * nnew = _nnew; \
        cp_dict_t ** root = _root; \
        int dup = _dup; \
        cp_dict_aug_t * aug = _aug; \
        cp_dict_insert_by_aug(nnew, nnew, root, cmp, user, dup, aug); \
    })

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
#define cp_dict_insert_update_by_aug(nnew,idx,root,minp,maxp,cmp,user,dup,aug) \
    cp_dict_insert_update_by_aug_1_(CP_GENSYM(_l_cmpUT), CP_GENSYM(_augUT), \
        CP_GENSYM(_dupUT), CP_GENSYM(_idxUT), CP_GENSYM(_maxpUT), \
        CP_GENSYM(_minpUT), CP_GENSYM(_nnewUT), CP_GENSYM(_rootUT), \
        CP_GENSYM(_userUT), (nnew), (idx), (root), (minp), (maxp), (cmp), \
        (user), (dup), (aug))

#define cp_dict_insert_update_by_aug_1_(_l_cmp,aug,dup,idx,maxp,minp,nnew,root,user,_nnew,_idx,_root,_minp,_maxp,cmp,_user,_dup,_aug) \
    ({ \
        cp_dict_t * nnew = _nnew; \
        __typeof__(*_idx) *idx = _idx; \
        cp_dict_t ** root = _root; \
        cp_dict_t ** minp = _minp; \
        cp_dict_t ** maxp = _maxp; \
        __typeof__(*_user) *user = _user; \
        int dup = _dup; \
        cp_dict_aug_t * aug = _aug; \
        int (*_l_cmp)( \
            __typeof__(idx), \
            cp_dict_t *, \
            __typeof__(user)) = cmp; \
        cp_dict_insert_update_by_aug_( \
            nnew, \
            (void*)(size_t)idx, \
            root, minp, maxp, \
            (cp_dict_cmp_t_)_l_cmp, \
            (void*)(size_t)user, \
            dup, \
            aug); \
    })

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
#define cp_dict_insert_update_aug(nnew,root,minp,maxp,cmp,user,dup,aug) \
    cp_dict_insert_update_aug_1_(CP_GENSYM(_augBV), CP_GENSYM(_dupBV), \
        CP_GENSYM(_maxpBV), CP_GENSYM(_minpBV), CP_GENSYM(_nnewBV), \
        CP_GENSYM(_rootBV), (nnew), (root), (minp), (maxp), (cmp), (user), \
        (dup), (aug))

#define cp_dict_insert_update_aug_1_(aug,dup,maxp,minp,nnew,root,_nnew,_root,_minp,_maxp,cmp,user,_dup,_aug) \
    ({ \
        cp_dict_t * nnew = _nnew; \
        cp_dict_t ** root = _root; \
        cp_dict_t ** minp = _minp; \
        cp_dict_t ** maxp = _maxp; \
        int dup = _dup; \
        cp_dict_aug_t * aug = _aug; \
        cp_dict_insert_update_by_aug( \
            nnew, nnew, root, minp, maxp, cmp, user, dup, aug); \
    })

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
#define cp_dict_split_aug(r,l,root,idx,cmp,user,leq,aug) \
    cp_dict_split_aug_1_(CP_GENSYM(_l_cmpIG), CP_GENSYM(_augIG), \
        CP_GENSYM(_idxIG), CP_GENSYM(_lIG), CP_GENSYM(_rIG), \
        CP_GENSYM(_rootIG), CP_GENSYM(_userIG), (r), (l), (root), (idx), \
        (cmp), (user), (leq), (aug))

#define cp_dict_split_aug_1_(_l_cmp,aug,idx,l,r,root,user,_r,_l,_root,_idx,cmp,_user,leq,_aug) \
    do{ \
        cp_dict_t ** r = _r; \
        cp_dict_t ** l = _l; \
        cp_dict_t * root = _root; \
        __typeof__(*_idx) *idx = _idx; \
        __typeof__(*_user) *user = _user; \
        cp_dict_aug_t * aug = _aug; \
        int (*_l_cmp)( \
            __typeof__(idx), \
            cp_dict_t *, \
            __typeof__(user)) = cmp; \
        cp_dict_split_aug_( \
            r, l, root, \
            (void*)(size_t)idx, \
            (cp_dict_cmp_t_)_l_cmp, \
            (void*)(size_t)user, \
            leq, \
            aug); \
    }while(0)

/* END MACRO */

/**
 * Same as cp_dict_split_aug() without an augmentation callback.
 */
#define cp_dict_split(r,l,root,idx,cmp,user,leq) \
    cp_dict_split_aug(r,l,root,idx,cmp,user,leq,NULL)

/**
 * Same as cp_dict_insert_by_aug() without an augmentation callback.
 */
#define cp_dict_insert_by(nnew,idx,root,cmp,user,dup) \
    cp_dict_insert_by_aug(nnew,idx,root,cmp,user,dup,NULL)

/**
 * Same as cp_dict_insert_aug() without an augmentation callback.
 *
 * The general naming of the insert family is as follows:
 *
 *     cp_dict_insert(_update)?(_by|_ref)?(_aug)?
 *
 * The "_update" versions update minimum and/or maximum in O(1).
 *
 * The "_by" versions take an additional reference argument for finding
 * the insertion position, while without this, the new element is used
 * instead.
 *
 * The "_ref" versions use a reference to a child to determine the
 * insertion position, as returned by cp_dict_find_ref().
 *
 * The "_aug" versions take an additional argument to an augmentation
 * callback that can be used to update auxiliary data in the tree while
 * the tree is being re-balanced.
 */
#define cp_dict_insert(nnew,root,cmp,user,dup) \
    cp_dict_insert_aug(nnew,root,cmp,user,dup,NULL)

/**
 * Same as cp_dict_insert_update_by_aug() without an augmentation callback.
 */
#define cp_dict_insert_update_by(nnew,idx,root,minp,maxp,cmp,user,dup) \
    cp_dict_insert_update_by_aug(nnew,idx,root,minp,maxp,cmp,user,dup,NULL)

/**
 * Same as cp_dict_insert_update_aug() without an augmentation callback.
 */
#define cp_dict_insert_update(nnew,root,minp,maxp,cmp,user,dup) \
    cp_dict_insert_update_aug(nnew,root,minp,maxp,cmp,user,dup,NULL)

/**
 * Comparison function for all basic operations of the binary search tree. */
typedef int (*cp_dict_cmp_t_)(
    void *a,
    void *b,
    void *user);

typedef enum {
    /**
     * counter-clockwise rotation (i.e., to the left).  `main` is the
     * new parent that was the right child, `aux` is old parent that
     * is now the left child.
     * This is invoked after the action.
     * This is a balancing operation.
     */
    CP_DICT_AUG_LEFT  = 0,

    /**
     * clockwise rotation (i.e., to the right).  `main` is the
     * new parent that was the left child, `aux` is old parent that
     * is now the right child
     * This is invoked after the action.
     * This is a balancing operation.
     */
    CP_DICT_AUG_RIGHT = 1,

    /**
     * Step up.  `main` is the new parent, `aux` is the child from where
     * the algorithm stepped up.  This does not mark a change, it is just
     * an information that the algorithm moved up, in case bottom-to-top
     * updates need to be done by the augmentation callback, e.g., after
     * a leaf is added.
     * This is only invoked if there is no rotation, but we're moving up
     * without any action.
     * This is a balancing operation (albeit without any change).
     */
    CP_DICT_AUG_NOP,

    /**
     * Step up two steps.  This is like invoking CP_DICT_AUG_NOP(`main`, `aux`)
     * followed by CP_DICT_AUG_NOP(`main->parent`, `main`).  This exists to
     * eliminate one callback function call for a frequent operation in the
     * RB tree balancing algorithm.
     * This is a balancing operation (albeit without any change).
     */
    CP_DICT_AUG_NOP2,

    /** End of balance.  `main` is the node where the recursion stopped
     * balancing, `aux` is the child node of `main` from where the
     * algorithm moved up, or NULL if the child that triggered this
     * was deleted.
     * There was no change to the tree when this is involved, but
     * the algorith stops because the tree is fully balanced.  This
     * callback is an opportunity to finish the augmentation, if
     * necessary, by moving up to the root.
     * This may not be invoked if there already was another augmentation
     * callback for the new root node.
     * This is a balancing operation (albeit without any change).
     */
    CP_DICT_AUG_FINI,

    /**
     * Addition of a leaf node.  `main` is that node, `aux` is NULL.
     * This is invoked after the action.
     * This is a modification of the tree, balancing will follow,
     * starting at the parent of the inserted node.
     */
    CP_DICT_AUG_ADD,

    /**
     * Swap of two nodes and remove the second one.  `main` is node
     * to be removed, an ancester node of the one that is swapped,
     * and `aux` is the replacement that will be swapped and then
     * cut off at the the bottom of the tree.
     * This is invoked before the swap and cut is done.
     * A CP_DICT_AUG_CUT_LEAF notification will follow after the
     * swap is done.
     * This is a modification of the tree, balancing will follow,
     * starting at the parent of the removed node.
     */
    CP_DICT_AUG_CUT_SWAP,

    /**
     * Removal of a leaf node.  `main` is the former parent of the
     * removed node, and `aux` is the removed node.
     * This is invoked after the change is performed.
     * Despite the name, this is called for cutting off 'half-leaves',
     * too, i.e., nodes that halve one child.  That one child is them
     * attached to the parent instead of the node that is cut off.
     * This is a modification of the tree, balancing will follow,
     * starting at the parent of the removed node.
     */
    CP_DICT_AUG_CUT_LEAF,

    /**
     * Join of two nodes and an element.  `main` is the root of the
     * joining.  `aux` is NULL.
     * This is invoked after the action.
     * This is a modification of the tree, balancing will follow,
     * starting at the parent of the joined node.
     */
    CP_DICT_AUG_JOIN,

    /**
     * Split of a node.  `main` is the node that is going to be split
     * into three parts: the singular node, the left tree, the
     * right tree.
     * This is invoked before the change is performed, so that
     * the augmentation function can examine the children.
     * This is a modification of the tree, balancing will follow.
     */
    CP_DICT_AUG_SPLIT,
} cp_dict_aug_type_t;

/**
 * Augmentation callback: called after each structural change of
 * of the data structure to enable updating auxiliary data.
 *
 * `main` and `aux` are two nodes involved in the augmentation
 * (sometimes `aux` is NULL), and `type` defines what happened.
 */
typedef struct cp_dict_aug cp_dict_aug_t;

struct cp_dict_aug {
    void (*event)(
        cp_dict_aug_t *aug,
        cp_dict_t *main,
        cp_dict_t *aux,
        cp_dict_aug_type_t type);
};

/**
 * Reference where in a tree an element was found.
 * This is used also for inserting in a given position, and
 * the description mainly focusses on that view, i.e., where
 * a new node would go in a tree.
 */
typedef struct {
    /**
     * The parent of the node where to insert.
     *
     * If this is NULL, then the tree is empty and the insertion
     * position is the new root.
     *
     * If this is NULL and the cp_dict_insert_ref() function finds
     * a non-empty tree, then it will insert at the minimum node if
     * child is 1, and at the maximum node if child is 0.
     */
    cp_dict_t *parent;

    /**
     * Which child to replace, left (0) or right (1). */
    unsigned child;

    /**
     * Statistics about the path that was taken to find the insertion
     * position.
     *
     * With this informatino, is to possible to update a cached
     * mininun and/or maximum in O(1).
     *
     * Bit 0 is 1 if cp_dict_find_ref() did a step down via a left
     * edge.  Bit 1 is 1 if cp_dict_find_ref() did a step down via a
     * right edge.  And bit 2 is 1 if an equal element was found.  If
     * this is 0, then the tree is empty and the root will be
     * replaced.
     *
     * This means that if (path & 5) is 0, then the new element will
     * be the maximum.  If (path & 6) is 0, then the new element will
     * be the minimum.  Both (at the root) or none (at an inner node)
     * of these cases may happen.
     */
    unsigned path;
} cp_dict_ref_t;

extern char const *cp_dict_str_aug_type(
    cp_dict_aug_type_t t);

/**
 * Return the black height of the given node
 *
 * This is internal information and it exposes the implementation
 * (red-black trees).  But it may be interesting for debugging.
 * Do not use this productively in algorithms, but only in
 * debugging, benchmarking, testing, etc.
 */
extern size_t cp_dict_black_height(
    cp_dict_t *n);

/**
 * Return whether the node is RED
 *
 * This is internal information and it exposes the implementation
 * (red-black trees).  But it may be interesting for debugging.
 * Do not use this productively in algorithms, but only in
 * debugging, benchmarking, testing, etc.
 */
extern size_t cp_dict_is_red(
    cp_dict_t *n);

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
    unsigned dir);

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
    cp_dict_t *n);

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
    unsigned dir);

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
    int duplicate);

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
    cp_dict_aug_t *aug);

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
    cp_dict_aug_t *aug);

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
    cp_dict_t *b);

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
    cp_dict_t *b);

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
    cp_dict_t *b);

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
    cp_dict_aug_t *aug);

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
    cp_dict_aug_t *aug);

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
    cp_dict_aug_t *aug);

/* *** static inline functions ****************************************** */

/**
 * Same as cp_dict_insert_ref_aug() without an augmentation callback.
 * function.
 */
static inline void cp_dict_insert_ref(
    cp_dict_t *node,
    cp_dict_ref_t const *ref,
    cp_dict_t **root)
{
    cp_dict_insert_ref_aug(node, ref, root, NULL);
}

/**
 * Same as cp_dict_remove_aug() without an augmentation callback.
 * function.
 */
static inline void cp_dict_remove(
    cp_dict_t *c,
    cp_dict_t **root)
{
    cp_dict_remove_aug(c, root, NULL);
}

/**
 * Same as cp_dict_join3_aug() without an augmentation callback.
 */
CP_WUR
static inline cp_dict_t *cp_dict_join3(
    cp_dict_t *l,
    cp_dict_t *m,
    cp_dict_t *r)
{
    return cp_dict_join3_aug(l, m, r, NULL);
}

/**
 * Same as cp_dict_join2_aug() without an augmentation callback.
 */
CP_WUR
static inline cp_dict_t *cp_dict_join2(
    cp_dict_t *l,
    cp_dict_t *r)
{
    return cp_dict_join2_aug(l, r, NULL);
}

/**
 * Initialise a new node
 *
 * Time complexity: O(1)
 */
static inline void cp_dict_init (
    cp_dict_t *node)
{
    CP_ZERO(node);
}

/**
 * Get child 0 or child 1.
 *
 * Time complexity: O(1)
 */
static inline cp_dict_t *cp_dict_child(cp_dict_t *n, unsigned i)
{
    assert(i <= 1);
    return n->edge[i];
}

/**
 * Get the index of a child in its parent node.
 *
 * Time complexity: O(1)
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
 * Time complexity: O(log n) (for top-down leaf search)
 * For whole tree iteration, min + n*next have runtime O(n).
 */
static inline cp_dict_t *cp_dict_min(cp_dict_t *root)
{
    return cp_dict_start(root,0);
}

/**
 * Get last element (e.g., the maximum).
 *
 * Time complexity: O(log n) (for top-down leaf search)
 * For whole tree iteration, max + n*prev have runtime O(n).
 */
static inline cp_dict_t *cp_dict_max(cp_dict_t *root)
{
    return cp_dict_start(root,1);
}

/**
 * Extract the first or last (one of the extrema) element.
 *
 * This is just cp_dict_start() followed by cp_dict_remove_aug().
 *
 * Note that updating the extremem is O(1), because cp_dict_step() is
 * guaranteed O(1) when invoked on an extremum, because, e.g., the
 * next of the minimum is either its father, its sibling, or its
 * nephew.  There is cp_dict_extract_update_start_aug() to implement
 * this case conveniently.
 *
 * See cp_dict_extract_start() for a function without augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_start_aug(
    cp_dict_t **root,
    unsigned i,
    cp_dict_aug_t *aug)
{
    cp_dict_t *r = cp_dict_start(*root, i);
    if (r == NULL) {
        return NULL;
    }
    cp_dict_remove_aug(r, root, aug);
    return r;
}

/**
 * Extract the first or last element.
 *
 * See cp_dict_extract_update_start() for a slightly faster function that
 * reads and updates a cached extremum.
 *
 * See cp_dict_extract_start_aug() for a function with augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_start(cp_dict_t **root, unsigned i)
{
    return cp_dict_extract_start_aug(root, i, NULL);
}

/**
 * Extract the first or last element (one of the extrema) when it is cached.
 *
 * This updates the cached extremum.
 *
 * This is faster then running cp_dict_extract_start_aug() followed by
 * cp_dict_start().
 *
 * See cp_dict_extract_update_start() for a function without augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_update_start_aug(
    cp_dict_t **root,
    cp_dict_t **extremum,
    unsigned i,
    cp_dict_aug_t *aug)
{
    cp_dict_t *r = *extremum;
    if (r == NULL) {
        return NULL;
    }
    assert(cp_dict_step(r, !i) == NULL);
    *extremum = cp_dict_step(r, i);
    cp_dict_remove_aug(r, root, aug);
    return r;
}

/**
 * Extract the first or last element (one of the extrema) when it is cached.
 *
 * This updates the cached extremum.
 *
 * This is faster then running cp_dict_extract_start_aug() followed by
 * cp_dict_start().
 *
 * See cp_dict_extract_update_start_aug() for a function with augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_update_start(
    cp_dict_t **root,
    cp_dict_t **extremum,
    unsigned i)
{
    return cp_dict_extract_update_start_aug(root, extremum, i, NULL);
}

/**
 * Extract the first element (minimum).
 *
 * See cp_dict_extract_min_update_aug() for a slightly faster function that
 * reads and updates a cached extremum.
 *
 * See cp_dict_extract_min() for a function without augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_min_aug(cp_dict_t **root, cp_dict_aug_t *aug)
{
    return cp_dict_extract_start_aug(root, 0, aug);
}

/**
 * Extract the first element (minimum).
 *
 * See cp_dict_extract_min_update() for a slightly faster function that
 * reads and updates a cached extremum.
 *
 * See cp_dict_extract_min_aug() for a function with augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_min(cp_dict_t **root)
{
    return cp_dict_extract_start(root, 0);
}

/**
 * Extract the last element (maximum).
 *
 * See cp_dict_extract_max_update_aug() for a slightly faster function that
 * reads and updates a cached extremum.
 *
 * See cp_dict_extract_max() for a function without augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_max_aug(cp_dict_t **root, cp_dict_aug_t *aug)
{
    return cp_dict_extract_start_aug(root, 1, aug);
}

/**
 * Extract the last element (maximum).
 *
 * See cp_dict_extract_max_update() for a slightly faster function that
 * reads and updates a cached extremum.
 *
 * See cp_dict_extract_max_aug() for a function with augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_max(cp_dict_t **root)
{
    return cp_dict_extract_start(root, 1);
}

/**
 * Extract the first element (minimum) when it is cached.
 *
 * See cp_dict_extract_update_min() for a function without augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_update_min_aug(
    cp_dict_t **root,
    cp_dict_t **minp,
    cp_dict_aug_t *aug)
{
    return cp_dict_extract_update_start_aug(root, minp, 0, aug);
}

/**
 * Extract the first element (minimum) when it is cached.
 *
 * See cp_dict_extract_update_min_aug() for a function with augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_update_min(
    cp_dict_t **root,
    cp_dict_t **minp)
{
    return cp_dict_extract_update_start(root, minp, 0);
}

/**
 * Extract the last element (maximum) when it is cached.
 *
 * See cp_dict_extract_update_max() for a function without augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_update_max_aug(
    cp_dict_t **root,
    cp_dict_t **maxp,
    cp_dict_aug_t *aug)
{
    return cp_dict_extract_update_start_aug(root, maxp, 1, aug);
}

/**
 * Extract the last element (maximum) when it is cached.
 *
 * See cp_dict_extract_update_max_aug() for a function with augmentation callback.
 *
 * Time complexity: O(log n).
 */
static inline cp_dict_t *cp_dict_extract_update_max(
    cp_dict_t **root,
    cp_dict_t **maxp)
{
    return cp_dict_extract_update_start(root, maxp, 1);
}

/**
 * Same as cp_dict_step(), but returns NULL if last == NULL
 * instead of assert-failing.
 */
static inline cp_dict_t *cp_dict_step0(
    cp_dict_t *last,
    unsigned dir)
{
    return last == NULL ? NULL : cp_dict_step(last, dir);
}

/**
 * Iterate a tree: get the next node.
 *
 * Time complexity:  O(log n), amortized in iteration: O(1),
 * on minimum: O(1).
 *
 * Stack complexity: O(1)
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
 * Time complexity:  O(log n), amortized in iteration: O(1),
 * on maximum: O(1).
 *
 * Stack complexity: O(1)
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
 * Time complexity: O(1)
 */
static inline bool cp_dict_is_root(
    cp_dict_t *n)
{
    return (n == NULL) || (n->parent == NULL);
}

/**
 * Whether the node is a singleton tree.
 */
static inline bool cp_dict_is_singleton(
    cp_dict_t *n)
{
    return (n != NULL) && (n->parent == NULL) && (n->edge[0] == NULL) && (n->edge[1] == NULL);
}

/**
 * Whether the node is in a dictionary.
 *
 * Note that each node is actually its own 1-element dictionary, so
 * this function returns 'true' only in the non-trivial case whether
 * at least one other element is in the tree.
 *
 * For a full check, there is cp_dict_is_contains(), which is
 * precise, but slower.  There is also cp_dict_may_contain(), which
 * is also O(1), but has fewer false negatives than this.
 *
 * Time complexity: O(1)
 */
static inline bool cp_dict_is_member(
    cp_dict_t const *n)
{
    return
        (n != NULL) &&
        (
            (n->parent != NULL) ||
            (n->edge[0] != NULL) ||
            (n->edge[1] != NULL)
        );
}

/**
 * Whether the node is in the given dictionary.
 *
 * This is a little better than cp_dict_is_member() wrt. false negatives:
 * if n is a single member of a tree, then this function checks whether
 * it is actually the root, in which case, this returns true, too.
 *
 * For a full check, there is cp_dict_is_contains(), which is
 * precise, but slower.
 *
 * Time complexity: O(1)
 */
static inline bool cp_dict_may_contain(
    cp_dict_t const *haystack,
    cp_dict_t const *needle)
{
    return
        (needle != NULL) && (haystack != NULL) &&
        ((needle == haystack) || cp_dict_is_member(needle));
}

/**
 * Whether the node is a member of the given dictionary.
 *
 * Time complexity: O(log n)
 */
static inline bool cp_dict_contains(
    cp_dict_t const *haystack,
    cp_dict_t const *needle)
{
    cp_dict_t *root = cp_dict_root((cp_dict_t *)(size_t)needle);
    return (needle != NULL) && (haystack != NULL) && (root == haystack);
}

/**
 * Internal, type-unsafe variant of cp_dict_insert_update_by().
 *
 * Do not use, use cp_dict_insert_update_by() or cp_dict_insert_update() instead.
 */
static inline cp_dict_t *cp_dict_insert_update_by_aug_(
    cp_dict_t *node,
    void *key,
    cp_dict_t **root,
    cp_dict_t **minp,
    cp_dict_t **maxp,
    cp_dict_cmp_t_ cmp,
    void *user,
    int dup,
    cp_dict_aug_t *aug)
{
    assert(root);
    assert(node);
    assert(node->parent == NULL);
    assert(cp_dict_child(node, 0) == NULL);
    assert(cp_dict_child(node, 1) == NULL);

    /* find */
    cp_dict_ref_t ref;
    cp_dict_t *n = cp_dict_find_ref_(&ref, key, *root, cmp, user, dup);
    if (n != NULL) {
        /* found exact entry => no duplicates are wanted */
        return n;
    }

    /* update */
    if ((maxp != NULL) && ((ref.path & 5) == 0)) {
        *maxp = node;
    }
    if ((minp != NULL) && ((ref.path & 6) == 0)) {
        *minp = node;
    }

    /* insert */
    cp_dict_insert_ref_aug(node, &ref, root, aug);
    return NULL;
}

/**
 * Internal, type-unsafe variant of cp_dict_insert_by().
 *
 * Do not use, use cp_dict_insert_by() or cp_dict_insert() instead.
 */
static inline cp_dict_t *cp_dict_insert_by_aug_(
    cp_dict_t *node,
    void *key,
    cp_dict_t **root,
    cp_dict_cmp_t_ cmp,
    void *user,
    int dup,
    cp_dict_aug_t *aug)
{
    return cp_dict_insert_update_by_aug_(
        node, key, root, NULL, NULL, cmp, user, dup, aug);
}

/**
 * Insert a new node, then rebalance.
 *
 * This takes a pointer to the root.  The root may be updated by the operation.
 *
 * The insert position is determined by "pos": "node" is inserted before if "dir" is 0,
 * or after if "dir" is 1.
 *
 * See cp_dict_insert_at() for a function without augmentation callback.
 *
 * See cp_dict_insert_update_at() for a function with an O(1) min/max update.
 *
 * Time complexity: O(log n) (for bottom-up rebalance)
 *
 * Stack complexity: O(1)
 */
static inline void cp_dict_insert_update_at_aug(
    cp_dict_t *node,
    cp_dict_t *pos,
    unsigned dir,
    cp_dict_t **root,
    cp_dict_t **minp,
    cp_dict_t **maxp,
    cp_dict_aug_t *aug)
{
    assert(root);
    assert(node);
    assert(node->parent == NULL);
    assert(cp_dict_child(node, 0) == NULL);
    assert(cp_dict_child(node, 1) == NULL);

    cp_dict_ref_t ref = { .parent = pos, .child = dir };

    /* update */
    if ((maxp != NULL) && ((*maxp == NULL) || ((*maxp == pos) && (dir == 1)))) {
        *maxp = node;
    }
    if ((minp != NULL) && ((*minp == NULL) || ((*minp == pos) && (dir == 0)))) {
        *minp = node;
    }

    /* insert */
    cp_dict_insert_ref_aug(node, &ref, root, aug);
}

/**
 * Insert before or after a given node and update min and max.
 *
 * Like cp_dict_insert_update_at_aug(), without the augmentation callback.
 *
 * Time complexity: O(log n) (for bottom-up rebalance)
 *
 * Stack complexity: O(1)
 */
static inline void cp_dict_insert_update_at(
    cp_dict_t *node,
    cp_dict_t *pos,
    unsigned dir,
    cp_dict_t **root,
    cp_dict_t **minp,
    cp_dict_t **maxp)
{
    cp_dict_insert_update_at_aug(node, pos, dir, root, minp, maxp, NULL);
}

/**
 * Insert before or after a given node.
 *
 * Like cp_dict_insert_update_at(), without updating min/max.
 *
 * Time complexity: O(log n) (for bottom-up rebalance)
 *
 * Stack complexity: O(1)
 */
static inline void cp_dict_insert_at(
    cp_dict_t *node,
    cp_dict_t *pos,
    unsigned dir,
    cp_dict_t **root)
{
    cp_dict_insert_update_at_aug(node, pos, dir, root, NULL, NULL, NULL);
}

/**
 * Insert before or after a given node.
 *
 * Like cp_dict_insert_update_at_aug(), without updating min/max.
 *
 * Time complexity: O(log n) (for bottom-up rebalance)
 *
 * Stack complexity: O(1)
 */
static inline void cp_dict_insert_at_aug(
    cp_dict_t *node,
    cp_dict_t *pos,
    unsigned dir,
    cp_dict_t **root,
    cp_dict_aug_t *aug)
{
    cp_dict_insert_update_at_aug(node, pos, dir, root, NULL, NULL, aug);
}

#endif /* CSG_SET_H_ */
