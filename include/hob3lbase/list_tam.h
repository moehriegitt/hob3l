/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_LIST_TAM_H_
#define CP_LIST_TAM_H_

#include <hob3lbase/list_fwd.h>

/**
 * Generic List node.
 *
 * Note that the list implementation in list.h works with any
 * struct that has 'next' and 'prev' slots.  Using cp_list_t
 * is not mandatory.  It exists for consistency with dict.h
 * and ring.h.
 *
 * The 'ring' member is for convenience for dynamic
 * direction access -- the implementation here does not
 * use it, but if cp_list_t is used, it can be exploited.
 */
struct cp_list {
    union {
        struct {
            cp_list_t *next;
            cp_list_t *prev;
        };
        cp_list_t *edge[2];
    };
};

#endif /* CP_LIST_H_ */
