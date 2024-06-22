/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

/*
 * Uunfortunately, this code has a lot of casts and relies completely on
 * the correctness of the dynamic type checking.  But it is concise and
 * allows simplificytion of many recursion algorithms in the tool.
 */

#include <hob3lbase/panic.h>
#include <hob3l/csg2_tam.h>
#include <hob3l/csg3_tam.h>

typedef struct cp_iter_map cp_iter_map_t;

typedef bool (*cp_iter_cb_t)(cp_iter_map_t const *, void *, cp_obj_t *);

typedef bool (*cp_iter_v_cb_t)(cp_iter_map_t const *, void *, cp_v_obj_p_t *);

struct cp_iter_map {
    cp_iter_cb_t circle;
    cp_iter_cb_t unknown;

    bool (*add) (cp_iter_map_t const *, void *, unsigned **, size_t);
    bool (*sub) (cp_iter_map_t const *, void *, unsigned **, size_t);
    bool (*cut) (cp_iter_map_t const *, void *, unsigned **, size_t);
};

extern bool cp_iter(
    cp_iter_map_t const *map,
    void *user,
    unsigned *o);

extern bool cp_iter(
    cp_iter_map_t const *m,
    void *u,
    unsigned *o)
{
    assert(o != NULL);
    switch (*o) {
    case CP_CSG2_ADD:
    case CP_CSG2_CIRCLE:
        if (m->circle) { return m->circle(
        break;
    }
    if (cb == NULL) {
        cb = (cb_t)m->unknown;
    }
    if (cb != NULL) {
        return cb(m, u, o);
    }
    CP_NYI("type=0x%x", *o);
    return false;
}
