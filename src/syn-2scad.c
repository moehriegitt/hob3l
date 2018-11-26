/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* SCAD parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <hob3lbase/vchar.h>
#include <hob3lbase/stream.h>
#include <hob3lbase/panic.h>
#include <hob3l/syn.h>
#include "internal.h"

static void cp_syn_stmt_put_scad(
    cp_stream_t *s,
    int d,
    cp_syn_stmt_t *fs);

static void cp_v_syn_stmt_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_syn_stmt_p_t *fs)
{
    for (cp_v_each(i, fs)) {
        cp_syn_stmt_put_scad(s, d, fs->data[i]);
    }
}

static void cp_syn_value_put_scad(
    cp_stream_t *s,
    int d CP_UNUSED,
    cp_syn_value_t *f)
{
    switch (f->type) {
    case CP_SYN_VALUE_ID:
        cp_printf(s, "%s", cp_syn_cast(cp_syn_value_id_t, f)->value);
        return;

    case CP_SYN_VALUE_INT:
        cp_printf(s, "%"CP_LL"d", cp_syn_cast(cp_syn_value_int_t, f)->value);
        return;

    case CP_SYN_VALUE_FLOAT:
        cp_printf(s, FF, cp_syn_cast(cp_syn_value_float_t, f)->value);
        return;

    case CP_SYN_VALUE_STRING:
        cp_printf(s, "\"%s\"", cp_syn_cast(cp_syn_value_string_t, f)->value);
        return;

    case CP_SYN_VALUE_RANGE:{
        cp_syn_value_range_t *g = cp_syn_cast(*g, f);
        cp_printf(s, "[");
        cp_syn_value_put_scad(s, d, g->start);
        cp_printf(s, ":");
        if (g->inc) {
            cp_syn_value_put_scad(s, d, g->inc);
            cp_printf(s, ":");
        }
        cp_syn_value_put_scad(s, d, g->end);
        cp_printf(s, "]");
        return;}

    case CP_SYN_VALUE_ARRAY: {
        cp_syn_value_array_t *g = cp_syn_cast(*g, f);
        cp_printf(s, "[");
        for (cp_v_each(i, &g->value)) {
            if (i != 0) {
                cp_printf(s, ",");
            }
            cp_syn_value_put_scad(s, d, cp_v_nth(&g->value,i));
        }
        cp_printf(s, "]");
        return;}
    }

    assert(0 && "Unrecognised syntax tree object type");
}

static void cp_syn_stmt_item_put_scad(
    cp_stream_t *s,
    int d,
    cp_syn_stmt_item_t *f)
{
    cp_printf(s, "%*s", d, "");
    if (f->functor != NULL) {
        cp_printf(s, "%s(", f->functor);
        for (cp_v_each(i, &f->arg)) {
            cp_syn_arg_t const *a = f->arg.data[i];
            if (i != 0) {
                cp_printf(s, ",");
            }
            if (a->key) {
                cp_printf(s, "%s=", a->key);
            }
            cp_syn_value_put_scad(s, d + IND, a->value);
        }
        cp_printf(s, ")");
    }
    if (f->body.size == 0) {
        cp_printf(s, ";\n");
        return;
    }
    cp_printf(s, " {\n");
    for (cp_v_each(i, &f->body)) {
        cp_syn_stmt_item_put_scad(s, d+IND, f->body.data[i]);
    }
    cp_printf(s, "%*s}\n", d, "");
}

static void cp_syn_stmt_use_put_scad(
    cp_stream_t *s,
    int d,
    cp_syn_stmt_use_t *f)
{
    cp_printf(s, "%*suse <%s>\n", d, "", f->path);
}

static void cp_syn_stmt_put_scad(
    cp_stream_t *s,
    int d,
    cp_syn_stmt_t *f)
{
    switch (f->type) {
    case CP_SYN_STMT_ITEM:
        cp_syn_stmt_item_put_scad(s, d, cp_syn_cast(cp_syn_stmt_item_t, f));
        return;
    case CP_SYN_STMT_USE:
        cp_syn_stmt_use_put_scad(s, d, cp_syn_cast(cp_syn_stmt_use_t, f));
        return;
    default:
        CP_NYI("type=0x%x", f->type);
    }
}

/**
 * Dump in SCAD format */
extern void cp_syn_tree_put_scad(
    cp_stream_t *s,
    cp_syn_tree_t *result)
{
    cp_v_syn_stmt_put_scad(s, 0, &result->toplevel);
}
