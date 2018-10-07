/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* SCAD parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <cpmat/vchar.h>
#include <cpmat/stream.h>
#include <csg2plane/syn.h>
#include "internal.h"

static void cp_syn_func_put_scad(
    cp_stream_t *s,
    int d,
    cp_syn_func_t *fs);

static void cp_v_syn_func_put_scad(
    cp_stream_t *s,
    int d,
    cp_v_syn_func_p_t *fs)
{
    for (cp_v_each(i, fs)) {
        cp_syn_func_put_scad(s, d, fs->data[i]);
    }
}

static void cp_syn_value_put_scad(
    cp_stream_t *s,
    int d __unused,
    cp_syn_value_t *f)
{
    switch (f->type) {
    case CP_SYN_VALUE_ID:
        cp_printf(s, "%s", f->_id.value);
        return;

    case CP_SYN_VALUE_INT:
        cp_printf(s, "%"_Pll"d", f->_int.value);
        return;

    case CP_SYN_VALUE_FLOAT:
        cp_printf(s, FF, f->_float.value);
        return;

    case CP_SYN_VALUE_STRING:
        cp_printf(s, "\"%s\"", f->_id.value);
        return;

    case CP_SYN_VALUE_RANGE:
        cp_printf(s, "[");
        cp_syn_value_put_scad(s, d, f->_range.start);
        cp_printf(s, ":");
        if (f->_range.inc) {
            cp_syn_value_put_scad(s, d, f->_range.inc);
            cp_printf(s, ":");
        }
        cp_syn_value_put_scad(s, d, f->_range.end);
        cp_printf(s, "]");
        return;

    case CP_SYN_VALUE_ARRAY:
        cp_printf(s, "[");
        for (cp_v_each(i, &f->_array.value)) {
            if (i != 0) {
                cp_printf(s, ",");
            }
            cp_syn_value_put_scad(s, d, f->_array.value.data[i]);
        }
        cp_printf(s, "]");
        return;
    }

    assert(0 && "Unrecognised syntax tree object type");
}

static void cp_syn_func_put_scad(
    cp_stream_t *s,
    int d,
    cp_syn_func_t *f)
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
        cp_syn_func_put_scad(s, d+IND, f->body.data[i]);
    }
    cp_printf(s, "%*s}\n", d, "");
}

extern void cp_syn_tree_put_scad(
    cp_stream_t *s,
    cp_syn_tree_t *result)
{
    cp_v_syn_func_put_scad(s, 0, &result->toplevel);
}
