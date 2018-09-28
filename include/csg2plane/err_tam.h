/* -*- Mode: C -*- */

#ifndef __CP_ERR_TAM_H
#define __CP_ERR_TAM_H

#include <cpmat/vchar.h>

typedef char const *cp_loc_t;

/**
 * Error location
 */
typedef struct {
    /**
     * Human readable error message.
     */
    cp_vchar_t msg;

    /**
     * The error location.
     */
    cp_loc_t loc;

    /**
     * Secondary error location (may be NULL). */
    cp_loc_t loc2;
} cp_err_t;

#endif /* __CP_ERR_TAM_H */
