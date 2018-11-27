/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <stdlib.h>
#include <hob3lbase/panic.h>
#include <hob3lbase/def.h>

/**
 * Panic exit w/ message.
 *
 * The file/line info should point to a source line.
 *
 * This should be used instead of an assert() if the reason for the
 * failure is not necessarily a bug in the program, but external
 * influence.  I.e., it should be used if even a perfectly correct
 * program could fail.  Out-of-Memory conditions are a reason or
 * failure to write to a global output file or stream.
 *
 * This should not be used if user input data leads to the failure.
 * In this case, a proper error should be raised, usually with diagnostic
 * output that points to the location of the user input data.  Note again
 * that file/line in this function is not meant to point to user input data,
 * but to a source code location for debugging.
 *
 * For debug builds, the caller should think about hiding the file/line
 * information, i.e., to pass NULL,0.  This function will not show the
 * file/line information in non-debug builds.
 *
 * In debug builds, this uses abort() to ease debugging in case this is
 * a bug, but otherwise, it uses exit(EXIT_FAILURE).
 */
CP_NORETURN
CP_PRINTF(3,4)
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

/**
 * Varargs version of cp_panic()
 *
 * For debug builds, the caller should think about hiding the file/line
 * information, i.e., to pass NULL,0.
 */
CP_NORETURN
CP_VPRINTF(3)
extern void cp_vpanic(
    char const *file CP_UNUSED,
    int line CP_UNUSED,
    char const *f,
    va_list va)
{
#ifndef NDEBUG
    if (file != NULL) {
        fprintf(stderr, "%s:%d: ", file, line);
    }
#endif
    fprintf(stderr, "Fatal Error: ");
    vfprintf(stderr, f, va);
    fprintf(stderr, "\n");
#ifndef NDEBUG
    abort();
#else
    exit(EXIT_FAILURE);
#endif
}
