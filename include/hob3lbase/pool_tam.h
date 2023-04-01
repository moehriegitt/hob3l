/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_POOL_TAM_H_
#define CP_POOL_TAM_H_

#include <stddef.h>
#include <hob3lbase/alloc_tam.h>

typedef struct cp_pool_block cp_pool_block_t;

typedef struct {
    cp_pool_block_t *head;
} cp_pool_block_list_t;

typedef struct {
    cp_alloc_t alloc[1];
    size_t block_size;
    cp_pool_block_list_t free;
    cp_pool_block_list_t used;
} cp_pool_t;

#endif /*CP_POOL_H_ */
