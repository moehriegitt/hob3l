/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_PANIC_H
#define __CP_PANIC_H

#include <stdarg.h>

/**
 * Panic if this code is reached.
 */
#define CP_DIE(...) \
    cp_panic(CP_FILE, CP_LINE, "This code should not be reached: " __VA_ARGS__)

/**
 * Not yet implemented. */
#define CP_NYI(...) \
    cp_panic(CP_FILE, CP_LINE, "Not implemented: " __VA_ARGS__)

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
__attribute__((noreturn))
__attribute__((format(printf,3,4)))
extern void cp_panic(
    char const *file,
    int line,
    char const *f,
    ...);

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
    va_list va);

#endif /* __CP_PANIC_H */
