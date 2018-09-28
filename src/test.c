/* -*- Mode: C -*- */

#include <stdio.h>
#include "test.h"

int main(void)
{
    TEST_RUN(cp_dict_test());
    TEST_RUN(cp_list_test());
    TEST_RUN(cp_ring_test());

    fprintf(stderr, "TEST:OK\n");
    return 0;
}
