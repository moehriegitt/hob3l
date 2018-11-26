/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_ERR_TAM_H_
#define CP_ERR_TAM_H_

#include <hob3lbase/vchar.h>

/**
 * Option: on error, fail */
#define CP_ERR_FAIL 0

/**
 * Option: on error, warn, but continue */
#define CP_ERR_WARN 1

/**
 * Option: ignore the error */
#define CP_ERR_IGNORE  2

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

#endif /* CP_ERR_TAM_H_ */
