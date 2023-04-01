/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/vchar.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/panic.h>
#include <hob3l/gc.h>
#include <hob3l/scad.h>
#include <hob3l/syn.h>
#include <hob3l/syn-msg.h>
#include "internal.h"

#define msg(c, ...) \
        _msg_aux(CP_GENSYM(_c), (c), __VA_ARGS__)

#define _msg_aux(c, _c, ...) \
    ({ \
        ctxt_t *c = _c; \
        cp_syn_msg(c->input, c->err, __VA_ARGS__); \
    })

/**
 * Same as msg with parameter CP_ERR_FAIL, but ensures that this
 * can only evaluate as 'false' so that the caller can be sure that
 * the error path is taken.
 */
#define msg_fail(c, ...) \
    ((void)msg(c, CP_ERR_FAIL, __VA_ARGS__), false)

typedef struct {
    cp_scad_tree_t *top;
    cp_err_t *err;
    cp_syn_input_t *input;
    cp_syn_tree_t *syn;
    cp_scad_opt_t const *opt;
} ctxt_t;

typedef char const *val_t;

static char const value_PI[] = "PI";
static char const value_true[] = "true";
static char const value_false[] = "false";
static char const value_undef[] = "undef";

static bool v_scad_from_v_syn_stmt_item(
    ctxt_t *t,
    cp_v_scad_p_t *result,
    cp_v_syn_stmt_item_p_t *fs);

static bool v_scad_from_v_syn_stmt(
    ctxt_t *t,
    cp_v_scad_p_t *result,
    cp_v_syn_stmt_p_t *fs);

#define func_new(rp, t, syn, type) \
    func_new_(CP_FILE, CP_LINE, rp, t, syn, type)

static bool func_new_(
    char const *file,
    int line,
    cp_scad_t **rp,
    ctxt_t *t,
    cp_syn_stmt_item_t *syn,
    cp_scad_type_t type)
{
    static const size_t size[] = {
        [CP_SCAD_UNION]        = sizeof(cp_scad_union_t),
        [CP_SCAD_DIFFERENCE]   = sizeof(cp_scad_difference_t),
        [CP_SCAD_INTERSECTION] = sizeof(cp_scad_intersection_t),
        [CP_SCAD_SPHERE]       = sizeof(cp_scad_sphere_t),
        [CP_SCAD_CUBE]         = sizeof(cp_scad_cube_t),
        [CP_SCAD_CYLINDER]     = sizeof(cp_scad_cylinder_t),
        [CP_SCAD_POLYHEDRON]   = sizeof(cp_scad_polyhedron_t),
        [CP_SCAD_IMPORT]       = sizeof(cp_scad_import_t),
        [CP_SCAD_SURFACE]      = sizeof(cp_scad_surface_t),
        [CP_SCAD_MULTMATRIX]   = sizeof(cp_scad_multmatrix_t),
        [CP_SCAD_TRANSLATE]    = sizeof(cp_scad_translate_t),
        [CP_SCAD_MIRROR]       = sizeof(cp_scad_mirror_t),
        [CP_SCAD_SCALE]        = sizeof(cp_scad_scale_t),
        [CP_SCAD_ROTATE]       = sizeof(cp_scad_rotate_t),
        [CP_SCAD_CIRCLE]       = sizeof(cp_scad_circle_t),
        [CP_SCAD_SQUARE]       = sizeof(cp_scad_square_t),
        [CP_SCAD_POLYGON]      = sizeof(cp_scad_polygon_t),
        [CP_SCAD_PROJECTION]   = sizeof(cp_scad_projection_t),
        [CP_SCAD_TEXT]         = sizeof(cp_scad_text_t),
        [CP_SCAD_COLOR]        = sizeof(cp_scad_color_t),
        [CP_SCAD_LINEXT]       = sizeof(cp_scad_linext_t),
        [CP_SCAD_ROTEXT]       = sizeof(cp_scad_rotext_t),
        [CP_SCAD_HULL]         = sizeof(cp_scad_hull_t),
    };
    assert(type < cp_countof(size));
    assert(size[type] != 0);
    cp_scad_t *r = cp_calloc_(file, line, &cp_alloc_global, 1, size[type]);
    *rp = r;
    r->type = type;
    r->loc = syn->loc;
    r->modifier = syn->modifier;
    if (r->modifier & CP_GC_MOD_ROOT) {
        if (t->top->root != NULL) {
            return msg(t, CP_ERR_FAIL, syn->loc, t->top->root->loc,
                "Multiple '!' modifiers in tree.");
        }
        t->top->root = r;
    }
    return true;
}

static bool union_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_union_t *r = cp_scad_cast(*r, _r);
    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool hull_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_hull_t *r = cp_scad_cast(*r, _r);
    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool intersection_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_intersection_t *r = cp_scad_cast(*r, _r);
    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool difference_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_difference_t *r = cp_scad_cast(*r, _r);
    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static val_t evaluate(
    cp_syn_value_t const *x)
{
    if (x->type != CP_SYN_VALUE_ID) {
        return NULL;
    }
    char const *id = cp_syn_cast(cp_syn_value_id_t, x)->value;
    switch (id[0]) {
    case 'P':
        if (strequ(id, "PI")) {
            return value_PI;
        }
        break;
    case 't':
        if (strequ(id, "true")) {
            return value_true;
        }
        break;
    case 'f':
        if (strequ(id, "false")) {
            return value_false;
        }
        break;
    case 'u':
        if (strequ(id, "undef")) {
            return value_undef;
        }
        break;
    }
    return NULL;
}

static bool try_get_longlong(
    long long *r,
    cp_syn_value_t const *v)
{
    cp_syn_value_int_t const *a = cp_syn_try_cast(*a, v);
    if (a != NULL) {
        *r = a->value;
        return true;
    }

    val_t w = evaluate(v);
    if (w == value_true) {
        *r = 1;
        return true;
    }
    if (w == value_false) {
        *r = 0;
        return true;
    }

    return false;
}

static bool try_get_uint32(
    unsigned *r,
    cp_syn_value_t const *v)
{
    long long ll;
    if (try_get_longlong(&ll, v)) {
        if (ll < 0) {
            return false;
        }
        if (ll >= (1LL << (sizeof(*r)*8))) {
            return false;
        }
        *r = (unsigned)ll;
        return true;
    }
    return false;
}

static bool get_uint32(
    unsigned *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    if (try_get_uint32(r, v)) {
        return true;
    }
    return msg(t, CP_ERR_FAIL, v->loc, NULL,
        "Expected a %"CP_Z"u-bit unsigned int value.", sizeof(*r)*8);
}

static bool _get_uint32(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_uint32(r, t, v);
}

static bool try_get_bool(
    bool *r,
    cp_syn_value_t const *v)
{
    unsigned u;
    if (try_get_uint32(&u, v)) {
        *r = !!u;
        return true;
    }
    return false;
}

static bool get_bool(
    bool *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    if (try_get_bool(r, v)) {
        return true;
    }
    return msg(t, CP_ERR_FAIL, v->loc, NULL, "Expected a bool value.");
}

static bool _get_bool(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_bool(r, t, v);
}

static bool try_get_float(
    cp_f_t *r,
    cp_syn_value_t const *v)
{
    cp_syn_value_float_t const *a = cp_syn_try_cast(*a, v);
    if (a != NULL) {
        *r = a->value;
        return true;
    }
    long long ll;
    if (try_get_longlong(&ll, v)) {
        *r = (cp_f_t)ll;
        return true;
    }
    if (evaluate(v) == value_PI) {
        *r = CP_PI;
        return true;
    }
    return false;
}

static bool get_float(
    cp_f_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    if (try_get_float(r, v)) {
        return true;
    }
    return msg(t, CP_ERR_FAIL, v->loc, NULL, "Expected a float or int value.");
}

static bool _get_float(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_float(r, t, v);
}

static bool try_get_grey(
    unsigned char *r,
    cp_syn_value_t const *v)
{
    cp_f_t f;
    if (!try_get_float(&f, v)) {
        return false;
    }
    if (cp_lt(f, 0) || cp_gt(f, 1)) {
        return false;
    }
    unsigned x = lrint(f * 255) & 0xff;
    *r = x & 0xff;
    return true;
}

static bool get_grey(
    unsigned char *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    if (try_get_grey(r, v)) {
        return true;
    }
    return msg(t, CP_ERR_FAIL, v->loc, NULL, "Expected a float or int value within 0..1.");
}

static bool _get_grey(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_grey(r, t, v);
}

static bool try_get_str(
    char const **r,
    cp_syn_value_t const *v)
{
    cp_syn_value_string_t const *a = cp_syn_try_cast(*a, v);
    if (a != NULL) {
        *r = a->value;
        return true;
    }
    return false;
}

static bool get_str(
    char const **r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    if (try_get_str(r, v)) {
        return true;
    }
    return msg(t, CP_ERR_FAIL, v->loc, NULL, "Expected a string value.");
}

static bool _get_str(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_str(r, t, v);
}

static bool try_get_size(
    size_t *r,
    cp_syn_value_t const *v)
{
    long long ll;
    if (try_get_longlong(&ll, v)) {
        if (ll < 0) {
            return false;
        }
        if (sizeof(cp_syn_cast(cp_syn_value_int_t, v)->value) != sizeof(*r)) {
             if (ll >= (long long)CP_MAX_OF(*r)) {
                 return false;
             }
        }
        *r = (size_t)ll;
        return true;
    }
    return false;
}

static bool get_size(
    size_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    if (try_get_size(r, v)) {
        return true;
    }
    return msg_fail(t, v->loc, NULL,
        "Expected a %"CP_Z"u-bit unsigned int value.", sizeof(*r)*8);
}

CP_UNUSED static bool _get_size(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_size(r, t, v);
}

static bool get_vec2(
    cp_vec2_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    if (v->type != CP_SYN_VALUE_ARRAY) {
        return msg_fail(t, v->loc, NULL,"Expected a vector.");
    }

    cp_syn_value_array_t const *a = cp_syn_cast(*a, v);
    if (a->value.size != cp_countof(r->v)) {
        return msg_fail(t, v->loc, NULL,
            "Expected a vector of size %"CP_Z"u.", cp_countof(r->v));
    }

    for (cp_arr_each(y, r->v)) {
        if (!get_float(&r->v[y], t, cp_v_nth(&a->value, y))) {
            return false;
        }
    }

    return true;
}

CP_UNUSED
static bool _get_vec2(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_vec2(r, t, v);
}

static bool get_vec2_or_float(
    cp_vec2_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    double xyz;
    if (try_get_float(&xyz, v)) {
        r->x = xyz;
        r->y = xyz;
        return true;
    }
    return get_vec2(r, t, v);
}

static bool _get_vec2_or_float(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_vec2_or_float(r, t, v);
}

static bool get_vec3(
    cp_vec3_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    if (v->type != CP_SYN_VALUE_ARRAY) {
        return msg_fail(t, v->loc, NULL,"Expected a vector.");
    }

    cp_syn_value_array_t const *a = cp_syn_cast(*a, v);
    if (a->value.size != cp_countof(r->v)) {
        return msg_fail(t, v->loc, NULL,
            "Expected a vector of size %"CP_Z"u.", cp_countof(r->v));
    }

    for (cp_arr_each(y, r->v)) {
        if (!get_float(&r->v[y], t, cp_v_nth(&a->value, y))) {
            return false;
        }
    }

    return true;
}

static bool _get_vec3(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_vec3(r, t, v);
}

static bool get_vec2_3(
    cp_vec3_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    cp_syn_value_array_t const *a = cp_syn_try_cast(*a, v);
    if (a && (a->value.size == 2)) {
        return get_vec2(&r->b, t, v);
    }
    return get_vec3(r, t, v);
}

static bool _get_vec2_3(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_vec2_3(r, t, v);
}

static bool get_vec3_or_float(
    cp_vec3_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    double xyz;
    if (try_get_float(&xyz, v)) {
        r->x = xyz;
        r->y = xyz;
        r->z = xyz;
        return true;
    }
    return get_vec3(r, t, v);
}

static bool _get_vec3_or_float(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_vec3_or_float(r, t, v);
}

static bool get_vec2_3_float(
    cp_vec3_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    cp_syn_value_array_t const *a = cp_syn_try_cast(*a, v);
    if (a && (a->value.size == 2)) {
        return get_vec2(&r->b, t, v);
    }
    return get_vec3_or_float(r, t, v);
}

static bool _get_vec2_3_float(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_vec2_3_float(r, t, v);
}

static bool get_vec4(
    cp_vec4_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    if (v->type != CP_SYN_VALUE_ARRAY) {
        return msg_fail(t, v->loc, NULL, "Expected a vector.");
    }

    cp_syn_value_array_t const *a = cp_syn_cast(*a, v);
    if (a->value.size != cp_countof(r->v)) {
        return msg_fail(t, v->loc, NULL,
            "Expected a vector of size %"CP_Z"u.", cp_countof(r->v));
    }

    for (cp_arr_each(y, r->v)) {
        if (!get_float(&r->v[y], t, cp_v_nth(&a->value, y))) {
            return false;
        }
    }

    return true;
}

static bool get_mat4(
    cp_mat4_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    cp_mat4_unit(r);

    /* This also accepts 3x3 matrices and 4x3 matrixes and will
     * add missing columns and rows from the unit matrix */

    cp_syn_value_array_t const *a = cp_syn_cast(*a, v);
    if ((v->type != CP_SYN_VALUE_ARRAY) ||
        (
            (a->value.size != 2) &&
            (a->value.size != 3) &&
            (a->value.size != 4)
        ))
    {
        return msg_fail(t, v->loc, NULL,
            "Expected a 2x2, 3x3, 4x4, or 3x3+T matrix array.");
    }

    for (cp_size_each(y, a->value.size)) {
        cp_syn_value_t const *l = cp_v_nth(&a->value, y);
        cp_syn_value_array_t const *la = cp_syn_try_cast(*a, l);
        if ((la != NULL) && (la->value.size == 3)) {
            if (!get_vec3(&r->row[y].b, t, l)) {
                return false;
            }
        }
        else
        if (!get_vec4(&r->row[y], t, l)) {
            return false;
        }
    }

    return true;
}

static bool get_mat3w(
    cp_mat3w_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    cp_mat4_t q;
    if (!get_mat4(&q, t, v)) {
        return false;
    }

    if (!cp_mat3w_from_mat4(r, &q)) {
        return msg_fail(t,
            cp_v_nth(&cp_syn_cast(cp_syn_value_array_t, v)->value, 3)->loc, NULL,
            "Not a valid 3x3+T matrix: last row must be [0,0,0,1].");
    }

    return true;
}

static bool _get_mat3w(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_mat3w(r, t, v);
}

static bool get_raw(
    cp_syn_value_t const **r,
    ctxt_t *t CP_UNUSED,
    cp_syn_value_t const *v)
{
    *r = v;
    return true;
}

static bool _get_raw(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_raw(r, t, v);
}

typedef struct {
    char const *name;
    bool (*get)(void *, ctxt_t *, cp_syn_value_t const *);
    void *data;
    bool *have_p;
    bool have;
} param_t;

#define PARAM(n,x,g,t,m) { .name = n, .get = g, .data = 1?(x):((t*)0), .have_p = m, .have = false }
#define PARAM_BOOL(n,x,m)          PARAM(n, x, _get_bool,   bool,             m)
#define PARAM_UINT32(n,x,m)        PARAM(n, x, _get_uint32, unsigned,         m)
#define PARAM_FLOAT(n,x,m)         PARAM(n, x, _get_float,  cp_f_t,           m)
#define PARAM_GREY(n,x,m)          PARAM(n, x, _get_grey,   unsigned char,    m)
#define PARAM_STR(n,x,m)           PARAM(n, x, _get_str,    char const *,     m)
#define PARAM_VEC2(n,x,m)          PARAM(n, x, _get_vec2,   cp_vec2_t,        m)
#define PARAM_VEC2_OR_FLOAT(n,x,m) PARAM(n, x, _get_vec2_or_float, cp_vec2_t, m)
#define PARAM_VEC3(n,x,m)          PARAM(n, x, _get_vec3,   cp_vec3_t,        m)
#define PARAM_VEC2_3(n,x,m)        PARAM(n, x, _get_vec2_3, cp_vec3_t,        m)
#define PARAM_VEC3_OR_FLOAT(n,x,m) PARAM(n, x, _get_vec3_or_float, cp_vec3_t, m)
#define PARAM_VEC2_3_FLOAT(n,x,m)  PARAM(n, x, _get_vec2_3_float,  cp_vec3_t, m)
#define PARAM_MAT3W(n,x,m)         PARAM(n, x, _get_mat3w,  cp_mat3w_t,       m)
#define PARAM_RAW(n,x,m)           PARAM(n, x, _get_raw,    cp_syn_value_t const *, m)

/**
 * Make a parameter optional by providing a dummy output boolean to
 * store whether the data was given. */
#define OPTIONAL  ((bool[]){false})

/**
 * Make a parameter mandatory by providing no place to store whether
 * the data was given. */
#define MANDATORY NULL

static bool get_arg(
    ctxt_t *t,
    cp_loc_t loc,
    cp_v_syn_arg_p_t *arg,
    param_t *pos,
    size_t pos_cnt,
    param_t *name,
    size_t name_cnt)
{
    bool need_name = false;
    for (cp_v_each(i, arg)) {
        cp_syn_arg_t *a = arg->data[i];
        assert(a->value != NULL);

        param_t *p = NULL;
        if (a->key == NULL) {
            if ((i >= pos_cnt) || need_name) {
                return msg_fail(t, a->value->loc, NULL, "Expected parameter name.");
            }
            p = &pos[i];
            assert(p != NULL);
        }
        else {
            need_name = true;
            for (cp_size_each(o, pos_cnt)) {
                if (strequ(pos[o].name, a->key)) {
                    p = &pos[o];
                    assert(p != NULL);
                    goto get;
                }
            }
            for (cp_size_each(o, name_cnt)) {
                if (strequ(name[o].name, a->key)) {
                    p = &name[o];
                    assert(p != NULL);
                    goto get;
                }
            }
            return msg(t, t->opt->err_unknown_param, a->value->loc, NULL,
                "Unknown parameter name '%s'.", a->key);
        }

    get:
        assert(p != NULL);
        if (!p->get(p->data, t, a->value)) {
            return false;
        }
        p->have = true;
    }

    for (cp_size_each(o, pos_cnt)) {
        if (pos[o].have_p != NULL) {
            *pos[o].have_p = pos[o].have;
        }
        else
        if (!pos[o].have) {
            return msg_fail(t, loc, NULL,
                "Missing '%s' parameter.", pos[o].name);
        }
    }
    for (cp_size_each(o, name_cnt)) {
        if (name[o].have_p != NULL) {
            *name[o].have_p = name[o].have;
        }
        else
        if (!name[o].have) {
            return msg_fail(t, loc, NULL, "Missing '%s' parameter.", name[o].name);
        }
    }


    return true;
}

#define _UNBOX(...) __VA_ARGS__

#define GET_ARG(tree, loc, arg, pos, name) \
    ({ \
        param_t _pos[] = { _UNBOX pos }; \
        param_t _name[] = { _UNBOX name }; \
        get_arg(tree, loc, arg, _pos, cp_countof(_pos), _name, cp_countof(_name)); \
    })

static bool multmatrix_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_multmatrix_t *r = cp_scad_cast(*r, _r);

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_MAT3W("m", &r->m, MANDATORY),
        ),
        ()))
    {
        return false;
    }

    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool cube_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_cube_t *r = cp_scad_cast(*r, _r);

    r->size.x = r->size.y = r->size.z = 1;
    r->center = false;

    return GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_VEC3_OR_FLOAT("size",   &r->size,   OPTIONAL),
            PARAM_BOOL         ("center", &r->center, OPTIONAL),
        ),
        ());
}

static bool square_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_square_t *r = cp_scad_cast(*r, _r);

    r->size.x = r->size.y = 1;
    r->center = false;

    return GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_VEC2_OR_FLOAT("size",   &r->size,   OPTIONAL),
            PARAM_BOOL         ("center", &r->center, OPTIONAL),
        ),
        ());
}

static bool sphere_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_sphere_t *r = cp_scad_cast(*r, _r);

    r->_fn = 0;
    r->r = 1;

    bool have_r = false;
    double _fa;
    double _fs;
    double d;
    bool have_d = false;
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_FLOAT ("r",   &r->r, &have_r),
        ),
        (
            PARAM_FLOAT ("d",   &d, &have_d),
            PARAM_FLOAT ("$fa", &_fa, OPTIONAL),
            PARAM_FLOAT ("$fs", &_fs, OPTIONAL),
            PARAM_UINT32("$fn", &r->_fn, OPTIONAL),
        )))
    {
        return false;
    }

    if (have_r && have_d) {
        return msg_fail(t, f->loc, NULL,
            "Either 'r' or 'd' parameters expected, but found both.");
    }
    if (have_d) {
        r->r = d / 2;
    }

    return true;
}

static bool circle_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_circle_t *r = cp_scad_cast(*r, _r);

    r->_fn = 0;
    r->r = 1;

    double _fa;
    double _fs;
    bool have_r = false;
    double d;
    bool have_d = false;
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_FLOAT ("r",   &r->r, &have_r),
        ),
        (
            PARAM_FLOAT ("d",   &d, &have_d),
            PARAM_FLOAT ("$fa", &_fa, OPTIONAL),
            PARAM_FLOAT ("$fs", &_fs, OPTIONAL),
            PARAM_UINT32("$fn", &r->_fn, OPTIONAL),
        )))
    {
        return false;
    }

    if (have_r && have_d) {
        return msg_fail(t, f->loc, NULL,
            "Either 'r' or 'd' parameters expected, but found both.");
    }
    if (have_d) {
        r->r = d / 2;
    }

    return true;
}

static bool polyhedron_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_polyhedron_t *r = cp_scad_cast(*r, _r);

    cp_syn_value_t const *_points = NULL;
    cp_syn_value_t const *_triangles = NULL;
    cp_syn_value_t const *_faces = NULL;
    unsigned _convexity;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_RAW ("points", &_points, MANDATORY),
            PARAM_RAW ("faces",  &_faces,  OPTIONAL),
        ),
        (
            PARAM_RAW ("triangles", &_triangles, OPTIONAL),
            PARAM_UINT32("convexity", &_convexity, OPTIONAL),
        )))
    {
        return false;
    }

    if (_triangles != NULL) {
        if (_faces != NULL) {
            return msg_fail(t, f->loc, NULL,
                "Either 'faces' or 'triangles' expected, but found both.");
        }
        _faces = _triangles;
    }
    if (_faces == NULL) {
        return msg_fail(t, f->loc, NULL,
            "Either 'faces' or 'triangles' expected, but found none.");
    }

    if (_points->type != CP_SYN_VALUE_ARRAY) {
        return msg_fail(t, _points->loc, NULL, "Expected array of points.");
    }
    cp_syn_value_array_t const *points = cp_syn_cast(*points, _points);
    cp_v_init0(&r->points, points->value.size);
    for (cp_v_each(i, &points->value)) {
        if (!get_vec3(&cp_v_nth(&r->points, i).coord, t, cp_v_nth(&points->value, i))) {
            return false;
        }
        cp_v_nth(&r->points, i).loc = cp_v_nth(&points->value, i)->loc;
    }

    if (_faces->type != CP_SYN_VALUE_ARRAY) {
        return msg_fail(t, _faces->loc, NULL, "Expected array of faces.");
    }
    cp_syn_value_array_t const *faces = cp_syn_cast(*faces, _faces);
    cp_v_init0(&r->faces, faces->value.size);
    for (cp_v_each(i, &faces->value)) {
        cp_syn_value_t const *_face = cp_v_nth(&faces->value, i);
        if (_face->type != CP_SYN_VALUE_ARRAY) {
            return msg_fail(t, _face->loc, NULL, "Expected array of point indices.");
        }
        cp_syn_value_array_t const *face = cp_syn_cast(*face, _face);
        if (face->value.size < 3) {
            return msg_fail(t, _face->loc, NULL,
                "Expected at least 3 point indices, but found only %"CP_Z"u.",
                face->value.size);
        }

        cp_v_nth(&r->faces, i).loc = _face->loc;
        cp_v_init0(&cp_v_nth(&r->faces, i).points, face->value.size);
        for (cp_v_each(j, &face->value)) {
            size_t idx;
            if (!get_size(&idx, t, cp_v_nth(&face->value, j))) {
                return false;
            }
            if (!(idx < r->points.size)) {
                return msg_fail(t, cp_v_nth(&face->value, j)->loc, points->loc,
                    "Index out of range; have %"CP_Z"u points, but found index %"CP_Z"u.",
                    r->points.size,
                    idx);
            }

            cp_vec3_loc_ref_t *pr = &cp_v_nth(&r->faces, i).points.data[j];
            pr->ref = &cp_v_nth(&r->points, idx);
            pr->loc = cp_v_nth(&face->value, j)->loc;
        }
    }

    return true;
}

static bool polygon_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_polygon_t *r = cp_scad_cast(*r, _r);

    cp_syn_value_t const *_points = NULL;
    cp_syn_value_t const *_paths = NULL;
    unsigned _convexity;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_RAW ("points", &_points, MANDATORY),
            PARAM_RAW ("paths",  &_paths,  OPTIONAL),
            PARAM_UINT32("convexity", &_convexity, OPTIONAL),
        ),
        ()))
    {
        return false;
    }

    if (_points->type != CP_SYN_VALUE_ARRAY) {
        return msg_fail(t, _points->loc, NULL, "Expected an array of points.");
    }
    cp_syn_value_array_t const *points = cp_syn_cast(*points, _points);
    cp_v_init0(&r->points, points->value.size);
    for (cp_v_each(i, &points->value)) {
        if (!get_vec2(&cp_v_nth(&r->points, i).coord, t, cp_v_nth(&points->value, i))) {
            return false;
        }
        cp_v_nth(&r->points, i).loc = cp_v_nth(&points->value, i)->loc;
    }

    if (_paths == NULL || (evaluate(_paths) == value_undef)) {
        cp_v_init0(&r->paths, 1);
        cp_v_init0(&cp_v_nth(&r->paths, 0).points, r->points.size);
        for (cp_v_each(j, &r->points)) {
            cp_vec2_loc_ref_t *pr = &cp_v_nth(&r->paths, 0).points.data[j];
            pr->ref = &r->points.data[j];
            pr->loc = points->value.data[j]->loc;
        }
    }
    else {
        if (_paths->type != CP_SYN_VALUE_ARRAY) {
            return msg_fail(t, _paths->loc, NULL, "Expected an array of paths.");
        }
        cp_syn_value_array_t const *paths = cp_syn_cast(*paths, _paths);
        cp_v_init0(&r->paths, paths->value.size);
        for (cp_v_each(i, &paths->value)) {
            cp_syn_value_t const *_path = cp_v_nth(&paths->value, i);
            if (_path->type != CP_SYN_VALUE_ARRAY) {
                return msg_fail(t, _path->loc, NULL,
                    "Expected an array of point indices.");
            }
            cp_syn_value_array_t const *path = cp_syn_cast(*path, _path);
            if (path->value.size < 3) {
                return msg_fail(t, path->loc, NULL,
                    "Expected at least 3 point indices, but found only %"CP_Z"u.",
                    path->value.size);
            }

            cp_v_nth(&r->paths, i).loc = _path->loc;
            cp_v_init0(&cp_v_nth(&r->paths, i).points, path->value.size);
            for (cp_v_each(j, &path->value)) {
                size_t idx;
                if (!get_size(&idx, t, cp_v_nth(&path->value, j))) {
                    return false;
                }
                if (!(idx < r->points.size)) {
                    return msg_fail(t, cp_v_nth(&path->value, j)->loc, points->loc,
                        "Index out of range; have %"CP_Z"u points, but found index %"CP_Z"u.",
                        r->points.size,
                        idx);
                }

                cp_vec2_loc_ref_t *pr = &cp_v_nth(&r->paths, i).points.data[j];
                pr->ref = &cp_v_nth(&r->points, idx);
                pr->loc = cp_v_nth(&path->value, j)->loc;
            }
        }
    }

    return true;
}

static bool mirror_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_mirror_t *r = cp_scad_cast(*r, _r);
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_VEC2_3("v", &r->v, MANDATORY),
        ),
        ()))
    {
        return false;
    }
    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool translate_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_translate_t *r = cp_scad_cast(*r, _r);
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_VEC2_3("v", &r->v, MANDATORY),
        ),
        ()))
    {
        return false;
    }
    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool color_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_color_t *r = cp_scad_cast(*r, _r);
    r->rgba.a = 255;

    cp_syn_value_t const *_c = NULL;

    bool have_alpha = false;
    unsigned char alpha = 0;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_RAW  ("c", &_c, MANDATORY),
            PARAM_GREY ("alpha", &alpha, &have_alpha),
        ),
        ()))
    {
        return false;
    }

    r->valid = true;
    if ((_c == NULL) || (evaluate(_c) == value_undef)) {
        /* if no value is set or if value is undef, ignore request
         * and leave color invalid */
        r->valid = false;
    }
    else
    if (_c->type == CP_SYN_VALUE_ARRAY) {
        cp_syn_value_array_t const *c = cp_syn_cast(*c,_c);
        if (c->value.size < 3) {
            return msg_fail(t, c->loc, NULL,
                "Expected at least 3 colour components, but found %"CP_Z"u.",
                c->value.size);
        }
        size_t mx = 3 + (have_alpha ? 0 : 1);
        if (c->value.size > mx) {
            return msg_fail(t, c->loc, NULL,
                "Expected at most %"CP_Z"u colour components, but found %"CP_Z"u.",
                mx,
                c->value.size);
        }
        for (cp_v_each(i, &c->value)) {
            if (!get_grey(&r->rgba.c[i], t, cp_v_nth(&c->value, i))) {
                return false;
            }
        }
    }
    else
    if (_c->type == CP_SYN_VALUE_STRING) {
        cp_syn_value_string_t const *c = cp_syn_cast(*c, _c);
        if (!cp_color_by_name(&r->rgba.rgb, c->value)) {
            return msg_fail(t, c->loc, NULL, "Unknown colour '%s'.", c->value);
        }
    }
    else {
        return msg_fail(t, _c->loc, NULL,
            "Expected an array or string for color definition.");
    }

    if (have_alpha) {
        r->rgba.a = alpha;
    }

    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool scale_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_scale_t *r = cp_scad_cast(*r, _r);
    r->v.z = 1;
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_VEC2_3_FLOAT("v", &r->v, MANDATORY),
        ),
        ()))
    {
        return false;
    }
    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool rotate_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_rotate_t *r = cp_scad_cast(*r, _r);

    r->a = 0;
    CP_ZERO(&r->n);
    r->n.z = 1;

    cp_syn_value_t const *a = NULL;

    bool have_v;
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_RAW ("a", &a,    MANDATORY),
            PARAM_VEC3("v", &r->n, &have_v),
        ),
        ()))
    {
        return false;
    }

    assert(a != NULL);
    if (a->type == CP_SYN_VALUE_ARRAY) {
        if (have_v) {
            return msg_fail(t, f->loc, NULL,
                "Either 'a' or 'v' is expected to be a vector, but found both.");
        }
        if (!get_vec3(&r->n, t, a)) {
            return false;
        }
        r->around_n = false;
    }
    else {
        if (!get_float(&r->a, t, a)) {
            return false;
        }
        r->around_n = true;
    }

    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool linext_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_linext_t *r = cp_scad_cast(*r, _r);

    r->slices = 1;
    r->scale.x = 1;
    r->scale.y = 1;
    double _fa;
    double _fs;
    unsigned _convexity;

    if (!GET_ARG(t, f->loc, &f->arg,
        (),
        (
            PARAM_FLOAT ("height",       &r->height,  MANDATORY),
            PARAM_BOOL  ("center",       &r->center,  OPTIONAL),
            PARAM_UINT32("slices",       &r->slices,  OPTIONAL),
            PARAM_FLOAT ("twist",        &r->twist,   OPTIONAL),
            PARAM_VEC2_OR_FLOAT("scale", &r->scale,   OPTIONAL),
            PARAM_UINT32("convexity",    &_convexity, OPTIONAL),
            PARAM_FLOAT ("$fa",          &_fa,        OPTIONAL),
            PARAM_FLOAT ("$fs",          &_fs,        OPTIONAL),
            PARAM_UINT32("$fn",          &r->_fn,     OPTIONAL),
        )))
    {
        return false;
    }

    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool rotext_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_rotext_t *r = cp_scad_cast(*r, _r);
    r->angle = 360;

    double _fa;        
    double _fs;
    unsigned _convexity;

    if (!GET_ARG(t, f->loc, &f->arg,
        (),
        (
            PARAM_FLOAT ("angle",        &r->angle,   OPTIONAL),
            PARAM_UINT32("convexity",    &_convexity, OPTIONAL),
            PARAM_FLOAT ("$fa",          &_fa,        OPTIONAL),
            PARAM_FLOAT ("$fs",          &_fs,        OPTIONAL),
            PARAM_UINT32("$fn",          &r->_fn,     OPTIONAL),
        )))
    {
        return false;
    }

    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool cylinder_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_q)
{
    cp_scad_cylinder_t *q = cp_scad_cast(*q, _q);

    q->_fn = 0;
    q->h = 1;
    q->r1 = 1;
    q->r2 = 1;
    q->center = false;

    double _fa;
    double _fs;
    double r;
    bool have_r = false;
    bool have_r1 = false;
    bool have_r2 = false;

    double d, d1, d2;
    bool have_d  = false;
    bool have_d1 = false;
    bool have_d2 = false;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_FLOAT ("h",      &q->h, OPTIONAL),
            PARAM_FLOAT ("r1",     &q->r1, &have_r1),
            PARAM_FLOAT ("r2",     &q->r2, &have_r2),
            PARAM_BOOL  ("center", &q->center, OPTIONAL),
        ),
        (
            PARAM_FLOAT ("d",   &d,  &have_d),
            PARAM_FLOAT ("d1",  &d1, &have_d1),
            PARAM_FLOAT ("d2",  &d2, &have_d2),
            PARAM_FLOAT ("r",   &r,  &have_r),
            PARAM_FLOAT ("$fa", &_fa, OPTIONAL),
            PARAM_FLOAT ("$fs", &_fs, OPTIONAL),
            PARAM_UINT32("$fn", &q->_fn, OPTIONAL),
        )))
    {
        return false;
    }

    if (have_d && (have_d1 || have_d2)) {
        return msg_fail(t, f->loc, NULL,
            "Either 'd' or 'd1'/'d2' parameters expected, but found both.");
    }
    if (have_d) {
        d1 = d2 = d;
        have_d1 = have_d2 = true;
    }

    if (have_r && (have_r1 || have_r2)) {
        return msg_fail(t, f->loc, NULL,
            "Either 'r' or 'r1'/'r2' parameters expected, but found both.");
    }
    if (have_r) {
        q->r1 = q->r2 = r;
        have_r1 = have_r2 = true;
    }

    if (have_r && have_d) {
        return msg_fail(t, f->loc, NULL,
            "Either 'r' or 'd' parameters expected, but found both.");
    }
    if (have_r1 && have_d1) {
        return msg_fail(t, f->loc, NULL,
            "Either 'r1' or 'd1' parameters expected, but found both.");
    }
    if (have_r2 && have_d2) {
        return msg_fail(t, f->loc, NULL,
            "Either 'r2' or 'd2' parameters expected, but found both.");
    }

    if (have_d1) {
        q->r1 = d1 / 2;
    }
    if (have_d2) {
        q->r2 = d2 / 2;
    }

    return true;
}

static bool string_unquote(
    ctxt_t *t,
    cp_vchar_t *v,
    char const *s)
{
    (void)cp_vchar_cstr(v);
    for (char const *i = s, *e = s + strlen(s); i != e; i++) {
        if (*i == '\\') {
            i++;
            switch (*i) {
            case '\\': cp_vchar_push(v, '\\'); break;
            case '\"': cp_vchar_push(v, '\"'); break;
            case 't':  cp_vchar_push(v, '\t'); break;
            case 'n':  cp_vchar_push(v, '\n'); break;
            case 'r':  cp_vchar_push(v, '\r'); break;
            default:
                return msg_fail(t, i, NULL, "Unsupported string escape character.");
            }
        }
        else {
            cp_vchar_push(v, *i);
        }
    }
    return true;
}

static bool import_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_q)
{
    cp_scad_import_t *q = cp_scad_cast(*q, _q);

    char const *_layer;
    unsigned _convexity;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_STR   ("file",      &q->file_tok, MANDATORY),
            PARAM_STR   ("layer",     &_layer,      OPTIONAL),
            PARAM_UINT32("convexity", &_convexity,  OPTIONAL),
        ),
        ()))
    {
        return false;
    }

    if (!string_unquote(t, &q->file, q->file_tok)) {
        return false;
    }

    return true;
}

static bool surface_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_q)
{
    cp_scad_surface_t *q = cp_scad_cast(*q, _q);

    unsigned _convexity;
    bool _invert;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_STR   ("file",      &q->file_tok, MANDATORY),
            PARAM_BOOL  ("center",    &q->center,   OPTIONAL),
        ),
        (
            PARAM_BOOL  ("invert",    &_invert,     OPTIONAL),
            PARAM_UINT32("convexity", &_convexity,  OPTIONAL),
        )))
    {
        return false;
    }

    if (!string_unquote(t, &q->file, q->file_tok)) {
        return false;
    }

    return true;
}

static bool text_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_q)
{
    cp_scad_text_t *q = cp_scad_cast(*q, _q);

    q->font = "Nozzl3 Sans";
    q->halign = "left";
    q->valign = "baseline";
    q->script = "latin";
    q->language = "en";
    q->direction = "ltr";
    q->size = 10.0;
    q->spacing = 1.0;
    q->tracking = 0.0;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_STR   ("text",      &q->text,      MANDATORY),
        ),
        (
            PARAM_FLOAT ("size",      &q->size,      OPTIONAL),
            PARAM_STR   ("font",      &q->font,      OPTIONAL),
            PARAM_STR   ("halign",    &q->halign,    OPTIONAL),
            PARAM_STR   ("valign",    &q->valign,    OPTIONAL),
            PARAM_STR   ("direction", &q->direction, OPTIONAL),
            PARAM_STR   ("language",  &q->language,  OPTIONAL),
            PARAM_STR   ("script",    &q->script,    OPTIONAL),
            PARAM_FLOAT ("spacing",   &q->spacing,   OPTIONAL),
            PARAM_FLOAT ("tracking",  &q->tracking,  OPTIONAL),
        )))
    {
        return false;
    }

    return true;
}

static bool projection_from_item(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_q)
{
    cp_scad_projection_t *q = cp_scad_cast(*q, _q);
    unsigned _convexity;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_BOOL("cut", &q->cut, OPTIONAL),
        ),
        (
            PARAM_UINT32("convexity", &_convexity,  OPTIONAL),
        )))
    {
        return false;
    }

    return v_scad_from_v_syn_stmt_item(t, &q->child, &f->body);
}

typedef struct {
    char const *id;
    cp_scad_type_t type;
    bool (*from)(
        ctxt_t *t,
        cp_syn_stmt_item_t *f,
        cp_scad_t *r);
    char const *const *arg_pos;
    char const *const *arg_name;
} cmd_t;

static int cmp_name_cmd(void const *_a, void const *_b, void *user CP_UNUSED)
{
    char const *a = _a;
    cmd_t const *b = _b;
    return strcmp(a, b->id);
}

static bool v_scad_from_syn_stmt_item(
    ctxt_t *t,
    cp_v_scad_p_t *result,
    cp_syn_stmt_item_t *f)
{
    static cmd_t const cmds[] = {
        {
           .id = "circle",
           .type = CP_SCAD_CIRCLE,
           .from = circle_from_item
        },
        {
           .id = "color",
           .type = CP_SCAD_COLOR,
           .from = color_from_item
        },
        {
           .id = "cube",
           .type = CP_SCAD_CUBE,
           .from = cube_from_item
        },
        {
           .id = "cylinder",
           .type = CP_SCAD_CYLINDER,
           .from = cylinder_from_item
        },
        {
           .id = "difference",
           .type = CP_SCAD_DIFFERENCE,
           .from = difference_from_item
        },
        {
           .id = "group",
           .type = CP_SCAD_UNION,
           .from = union_from_item
        },
        {
           .id = "hull",
           .type = CP_SCAD_HULL,
           .from = hull_from_item
        },
        {
           .id = "import",
           .type = CP_SCAD_IMPORT,
           .from = import_from_item
        },
        {
           .id = "import_stl",
           .type = CP_SCAD_IMPORT,
           .from = import_from_item
        },
        {
           .id = "intersection",
           .type = CP_SCAD_INTERSECTION,
           .from = intersection_from_item
        },
        {
           .id = "linear_extrude",
           .type = CP_SCAD_LINEXT,
           .from = linext_from_item,
        },
        {
           .id = "mirror",
           .type = CP_SCAD_MIRROR,
           .from = mirror_from_item
        },
        {
           .id = "multmatrix",
           .type = CP_SCAD_MULTMATRIX,
           .from = multmatrix_from_item
        },
        {
           .id = "polygon",
           .type = CP_SCAD_POLYGON,
           .from = polygon_from_item
        },
        {
           .id = "polyhedron",
           .type = CP_SCAD_POLYHEDRON,
           .from = polyhedron_from_item
        },
        {
           .id = "projection",
           .type = CP_SCAD_PROJECTION,
           .from = projection_from_item
        },
        {
           .id = "render",
           .type = CP_SCAD_UNION,
           .from = union_from_item
        },
        {
           .id = "rotate",
           .type = CP_SCAD_ROTATE,
           .from = rotate_from_item
        },
        {
           .id = "rotate_extrude",
           .type = CP_SCAD_ROTEXT,
           .from = rotext_from_item,
        },
        {
           .id = "scale",
           .type = CP_SCAD_SCALE,
           .from = scale_from_item
        },
        {
           .id = "sphere",
           .type = CP_SCAD_SPHERE,
           .from = sphere_from_item
        },
        {
           .id = "square",
           .type = CP_SCAD_SQUARE,
           .from = square_from_item
        },
        {
           .id = "surface",
           .type = CP_SCAD_SURFACE,
           .from = surface_from_item
        },
        {
           .id = "text",
           .type = CP_SCAD_TEXT,
           .from = text_from_item,
        },
        {
           .id = "translate",
           .type = CP_SCAD_TRANSLATE,
           .from = translate_from_item
        },
        {
           .id = "union",
           .type = CP_SCAD_UNION,
           .from = union_from_item
        },
        {
           /* FIXME: This has difference scoping rules, use different type. */
           .id = "{",
           .type = CP_SCAD_UNION,
           .from = union_from_item
        },
    };

    size_t idx = cp_bsearch(
        f->functor, cmds, cp_countof(cmds), sizeof(cmds[0]), cmp_name_cmd, NULL);
    if (idx >= cp_countof(cmds)) {
        return msg(t, t->opt->err_unknown_functor, f->loc, NULL,
            "Unknown functor/operator/object: '%s'.", f->functor);
    }

    cmd_t const *c = &cmds[idx];
    if (c->type == 0) {
        return msg(t, t->opt->err_unsupported_functor, f->loc, NULL,
            "Unsupported functor '%s'.", f->functor);
    }

    cp_scad_t *r;
    if (!func_new(&r, t, f, c->type)) {
        return false;
    }
    cp_v_push(result, r);

    assert(c->from != NULL);
    return c->from(t, f, r);
}

static bool v_scad_from_syn_stmt_use(
    ctxt_t *t CP_UNUSED,
    cp_v_scad_p_t *result CP_UNUSED,
    cp_syn_stmt_use_t *f CP_UNUSED)
{
    CP_NYI("use <...>");
}

static bool v_scad_from_syn_stmt(
    ctxt_t *t,
    cp_v_scad_p_t *result,
    cp_syn_stmt_t *f)
{
    switch (f->type) {
    case CP_SYN_STMT_ITEM:
       return v_scad_from_syn_stmt_item(t, result, cp_syn_cast(cp_syn_stmt_item_t, f));
    case CP_SYN_STMT_USE:
       return v_scad_from_syn_stmt_use(t, result, cp_syn_cast(cp_syn_stmt_use_t, f));
    default:
       CP_NYI("type=0x%x", f->type);
    }
}

static bool v_scad_from_v_syn_stmt_item(
    ctxt_t *t,
    cp_v_scad_p_t *result,
    cp_v_syn_stmt_item_p_t *fs)
{
    for (cp_v_each(i, fs)) {
        if (!v_scad_from_syn_stmt_item(t, result, fs->data[i])) {
            return false;
        }
    }
    return true;
}

static bool v_scad_from_v_syn_stmt(
    ctxt_t *t,
    cp_v_scad_p_t *result,
    cp_v_syn_stmt_p_t *fs)
{
    for (cp_v_each(i, fs)) {
        if (!v_scad_from_syn_stmt(t, result, fs->data[i])) {
            return false;
        }
    }
    return true;
}

/* ********************************************************************** */

/**
 * Same as cp_scad_from_syn_stmt_item, applied to each element
 * of the 'func' vector.
 *
 * On success, returns true.
 * In case of error, returns false and fills in tree->err_loc and tree->err_msg.
 */
extern bool cp_scad_from_syn_tree(
    cp_scad_tree_t *result,
    cp_syn_input_t *input,
    cp_err_t *err,
    cp_syn_tree_t *syn)
{
    ctxt_t t = {
        .syn = syn,
        .top = result,
        .err = err,
        .input = input,
        .opt = result->opt,
    };
    return v_scad_from_v_syn_stmt(&t, &result->toplevel, &syn->toplevel);
}
