/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */
/*
 * Original CLR red-black trees.
 *
 * This is from my rbtree vs. avl vs. llrb vs. jwtree vs. aatree project.
 */

#ifndef CP_DICT_TAM_H_
#define CP_DICT_TAM_H_

#include <stddef.h>

typedef struct cp_set {
    struct cp_set *parent;
    struct cp_set *edge[2];
    size_t stat;
} cp_dict_t;

#endif /* CP_DICT_TAM_H_ */
