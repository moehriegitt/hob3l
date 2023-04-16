/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include "hob3lmat-test.h"
#include "math-test.h"

int main(void)
{
    TEST_RUN(cp_math_test());

    fprintf(stderr, "TEST:OK\n");
    return 0;
}
