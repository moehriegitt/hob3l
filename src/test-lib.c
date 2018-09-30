/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <stdlib.h>
#include "test.h"

extern void __cp_test_fail(
    char const *file,
    int line,
    char const *msg1,
    char const *msg2)
{
    fprintf(stderr, "%s:%d: Error: %s %s\n", file, line, msg1, msg2);
    fprintf(stderr, "TEST FAILED.\n");
    exit(1);
}
