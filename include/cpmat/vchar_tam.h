/* -*- Mode: C -*- */

#ifndef __CP_VCHAR_TAM_H
#define __CP_VCHAR_TAM_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <cpmat/def.h>

typedef struct {
    size_t alloc;
    size_t size;
    /** Either NULL or a 0 terminated string */
    char *data;
} cp_vchar_t;

#endif /* __CP_VCHAR_H */
