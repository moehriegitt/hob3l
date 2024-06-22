/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <strings.h>
#include <stdint.h>
#include <hob3l/gc.h>
#include <hob3lbase/stream.h>
#include <hob3lbase/vec.h>

#include "gc-color.inc"

/**
 * Print a string of characters that respresent a modifier in scad syntax.
 */
extern void cp_gc_modifier_put_scad(
    cp_stream_t *s,
    unsigned modifier)
{
    if ((modifier & CP_GC_MOD_HASH) == CP_GC_MOD_HASH) {
        cp_printf(s, "#");
    }
    if ((modifier & CP_GC_MOD_PERCENT) == CP_GC_MOD_PERCENT) {
        cp_printf(s, "%%");
    }
    if ((modifier & CP_GC_MOD_AST) == CP_GC_MOD_AST) {
        cp_printf(s, "*");
    }
    if ((modifier & CP_GC_MOD_EXCLAM) == CP_GC_MOD_EXCLAM) {
        cp_printf(s, "!");
    }
}

/**
 * Translate a colour name into an rgb colour
 *
 * Return whether the translation was successful (true=success, false=failure).
 */
extern bool cp_color_by_name(
    cp_color_rgb_t *rgb,
    char const *name)
{
    size_t len = strlen(name);

    /* by RGB hex triple */
    if (name[0] == '#') {
        if ((len != 4) && (len != 7)) {
            return false;
        }
        unsigned v = 0;
        for (char const *n = name + 1; *n; n++) {
            if ((*n >= '0') && (*n <= '9')) {
                v = (v * 16) + (unsigned)(*n - '0');
            }
            else
            if ((*n >= 'a') && (*n <= 'f')) {
                v = (v * 16) + (unsigned)(*n - 'a') + 10;
            }
            else
            if ((*n >= 'A') && (*n <= 'F')) {
                v = (v * 16) + (unsigned)(*n - 'A') + 10;
            }
            else {
                return false;
            }
        }

        /* one digit triple */
        if (len == 4) {
            rgb->r = ((v >> 8) & 0xf) * 0x11;
            rgb->g = ((v >> 4) & 0xf) * 0x11;
            rgb->b = ((v)      & 0xf) * 0x11;
            return true;
        }

        /* two digit triple */
        rgb->r = (v >> 16) & 0xff;
        rgb->g = (v >> 8)  & 0xff;
        rgb->b = (v)       & 0xff;
        return true;
    }

    /* by name */
    color_value_t const *c = color_find(name, len);
    rgb->r = (c->rgb >> 16) & 0xff;
    rgb->g = (c->rgb >> 8)  & 0xff;
    rgb->b = (c->rgb)       & 0xff;
    return true;
}
