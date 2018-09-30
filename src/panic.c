/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <stdlib.h>
#include <cpmat/panic.h>

extern void cp_vpanic(
    char const *file,
    int line,
    char const *f,
    va_list va)
{
    if (file != NULL) {
        fprintf(stderr, "%s:%d: ", file, line);
    }
    fprintf(stderr, "Fatal Error: ");
    vfprintf(stderr, f, va);
    fprintf(stderr, "\n");
    abort();
}

extern void cp_panic(
    char const *file,
    int line,
    char const *f,
    ...)
{
    va_list va;
    va_start(va, f);
    cp_vpanic(file, line, f, va);
    va_end(va);
}
