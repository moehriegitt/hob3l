/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <stdlib.h>
#include <hob3lbase/panic.h>

/**
 * Panic exit w/ message.
 *
 * The file/line info should point to a source line.
 *
 * This should be used instead of an assert() if the reason for the
 * failure is not necessarily a bug in the program, but external
 * influence.  I.e., it should be used if even a perfectly correct
 * program could fail.
 *
 * This should not be used if user input data leads to the failure.
 * In this case, a proper error should be raised, usually with diagnostic
 * output that points to the location of the user input data.  Note again
 * that file/line in this function is not meant to point to user input data,
 * but to a source code location for debugging.
 *
 * For debug builds, the caller should think about hiding the file/line
 * information, i.e., to pass NULL,0.
 */
__attribute__((noreturn))
__attribute__((format(printf,3,4)))
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
 * Varargs version of __cp_panic()
 *
 * For debug builds, the caller should think about hiding the file/line
 * information, i.e., to pass NULL,0.
 */
__attribute__((noreturn))
__attribute__((format(printf,3,0)))
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
