/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lop/op-def.h>

extern void cq_fail(
    char const *msg)
{
    fprintf(stderr, "FATAL ERROR: %s\n", msg);
    abort();
}
