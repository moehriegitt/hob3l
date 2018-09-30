/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <csg2plane/gc.h>
#include <cpmat/stream.h>

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
