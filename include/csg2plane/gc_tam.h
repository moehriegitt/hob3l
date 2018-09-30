/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_GC_TAM_H
#define __CP_GC_TAM_H

#include <stdbool.h>

typedef struct {
    unsigned char r,g,b;
} cp_color_rgb_t;

typedef union {
    struct {
        unsigned char r,g,b,a;
    };
    cp_color_rgb_t rgb;
} cp_color_rgba_t;

enum {
    /** Feature bit: show even if cut or removed. */
    CP_GC_MOD_ALWAYS_SHOW = (1 << 0),

    /** Feature bit: ignore in computations, but show in previews. */
    CP_GC_MOD_IGNORE = (1 << 1),

    /** Feature bit: consider only this sub-tree, nothing else. */
    CP_GC_MOD_ROOT = (1 << 2),

    /* Actual modifiers */

    /** 'debug' modifier */
    CP_GC_MOD_HASH = 0x10 | CP_GC_MOD_ALWAYS_SHOW,

    /** 'background' modifier */
    CP_GC_MOD_PERCENT = 0x20 | CP_GC_MOD_ALWAYS_SHOW | CP_GC_MOD_IGNORE,

    /** 'root' modifier */
    CP_GC_MOD_EXCLAM = 0x40 | CP_GC_MOD_ROOT,

    /** 'disable' modifier */
    CP_GC_MOD_AST = 0x80 | CP_GC_MOD_IGNORE,

    /**
     * For masking only the bits that give which modifiers where given,
     * without the feature bits. */
    CP_GC_MOD_MASK = 0xf0,
};

/**
 * Graphics context.
 *
 * This excludes the transformation matrix on purpose, as this stores
 * only meta data, no coordinate related data.
 */
typedef struct {
    /**
     * Whether \a color has been explicitly set (true) or whether
     * it is still the default (false).
     */
    bool have_color;

    /**
     * The color.
     */
    cp_color_rgba_t color;

    /**
     * Modifier bitmask, see CP_GC_MOD*.
     */
    unsigned modifier;
} cp_gc_t;

#endif /* __CP_GC_TAM_H */
