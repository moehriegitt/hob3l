/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */
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
