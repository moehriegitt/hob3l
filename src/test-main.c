/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include "test.h"
#include "math-test.h"
#include "dict-test.h"
#include "list-test.h"
#include "ring-test.h"

int main(void)
{
    TEST_RUN(cp_math_test());
    TEST_RUN(cp_dict_test());
    TEST_RUN(cp_list_test());
    TEST_RUN(cp_ring_test());

    fprintf(stderr, "TEST:OK\n");
    return 0;
}
