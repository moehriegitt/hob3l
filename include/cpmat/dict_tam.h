/* -*- Mode: C -*- */
/*
 * Original CLR red-black trees.
 *
 * This is from my rbtree vs. avl vs. llrb vs. jwtree vs. aatree project.
 */

#ifndef __CSG_SET_TAM_H
#define __CSG_SET_TAM_H

#include <stddef.h>

typedef struct cp_set {
    struct cp_set *parent;
    struct cp_set *edge[2];
    size_t red;
} cp_dict_t;

#endif /* __CSG_SET_TAM_H */
