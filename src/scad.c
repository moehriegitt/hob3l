/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/vchar.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/panic.h>
#include <hob3l/gc.h>
#include <hob3l/scad.h>
#include <hob3l/syn.h>
#include "internal.h"

typedef struct {
    cp_scad_tree_t *top;
    cp_err_t *err;
    cp_syn_tree_t *syn;
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
    __func_new(CP_FILE, CP_LINE, rp, t, syn, type)

static bool __func_new(
    char const *file,
    int line,
    cp_scad_t **rp,
    ctxt_t *t,
    cp_syn_stmt_item_t *syn,
    cp_scad_type_t type)
{
    static const size_t size[] = {
        [CP_SCAD_UNION]        = sizeof(cp_scad_combine_t),
        [CP_SCAD_DIFFERENCE]   = sizeof(cp_scad_combine_t),
        [CP_SCAD_INTERSECTION] = sizeof(cp_scad_combine_t),
        [CP_SCAD_SPHERE]       = sizeof(cp_scad_sphere_t),
        [CP_SCAD_CUBE]         = sizeof(cp_scad_cube_t),
        [CP_SCAD_CYLINDER]     = sizeof(cp_scad_cylinder_t),
        [CP_SCAD_POLYHEDRON]   = sizeof(cp_scad_polyhedron_t),
        [CP_SCAD_MULTMATRIX]   = sizeof(cp_scad_multmatrix_t),
        [CP_SCAD_TRANSLATE]    = sizeof(cp_scad_xyz_t),
        [CP_SCAD_MIRROR]       = sizeof(cp_scad_xyz_t),
        [CP_SCAD_SCALE]        = sizeof(cp_scad_xyz_t),
        [CP_SCAD_ROTATE]       = sizeof(cp_scad_rotate_t),
        [CP_SCAD_CIRCLE]       = sizeof(cp_scad_circle_t),
        [CP_SCAD_SQUARE]       = sizeof(cp_scad_square_t),
        [CP_SCAD_POLYGON]      = sizeof(cp_scad_polygon_t),
        [CP_SCAD_COLOR]        = sizeof(cp_scad_color_t),
    };
    assert(type < cp_countof(size));
    assert(size[type] != 0);
    cp_scad_t *r = cp_calloc(file, line, 1, size[type]);
    *rp = r;
    r->type = type;
    r->loc = syn->loc;
    r->modifier = syn->modifier;
    if (r->modifier & CP_GC_MOD_ROOT) {
        if (t->top->root != NULL) {
            cp_vchar_printf(&t->err->msg, "Multiple '!' modifiers in tree.\n");
            t->err->loc = syn->loc;
            t->err->loc2 = t->top->root->loc;
            return false;
        }
        t->top->root = r;
    }
    return true;
}

static bool combine_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_combine_t *r = &_r->_combine;
    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static val_t evaluate(
    cp_syn_value_t const *x)
{
    if (x->type != CP_SYN_VALUE_ID) {
        return false;
    }
    char const *id = cp_syn_value_id_const(x)->value;
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
    if (v->type == CP_SYN_VALUE_INT) {
        *r = cp_syn_value_int_const(v)->value;
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
    cp_vchar_printf(&t->err->msg, "Expected a %"_Pz"u-bit unsigned int value.\n", sizeof(*r)*8);
    t->err->loc = v->loc;
    return false;
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
    cp_vchar_printf(&t->err->msg, "Expected a bool value.\n");
    t->err->loc = v->loc;
    return false;
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
    if (v->type == CP_SYN_VALUE_FLOAT) {
        *r = v->_float.value;
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
    cp_vchar_printf(&t->err->msg, "Expected a float or int value.\n");
    t->err->loc = v->loc;
    return false;
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
    cp_vchar_printf(&t->err->msg, "Expected a float or int value within 0..1.\n");
    t->err->loc = v->loc;
    return false;
}

static bool _get_grey(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_grey(r, t, v);
}

#if 0
static bool try_get_str(
    char const **r,
    cp_syn_value_t const *v)
{
    if (v->type == CP_SYN_VALUE_STRING) {
        *r = v->_string.value;
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
    cp_vchar_printf(&t->err->msg, "Expected a string value.\n");
    t->err->loc = v->loc;
    return false;
}

static bool _get_str(
    void *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    return get_str(r, t, v);
}
#endif /*0*/

static bool try_get_size(
    size_t *r,
    cp_syn_value_t const *v)
{
    long long ll;
    if (try_get_longlong(&ll, v)) {
        if (ll < 0) {
            return false;
        }
        if (sizeof(v->_int.value) != sizeof(*r)) {
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
    cp_vchar_printf(&t->err->msg, "Expected a %"_Pz"u-bit unsigned int value.\n", sizeof(*r)*8);
    t->err->loc = v->loc;
    return false;
}

__unused static bool _get_size(
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
    if ((v->type != CP_SYN_VALUE_ARRAY) ||
        (v->_array.value.size != 2))
    {
        cp_vchar_printf(&t->err->msg, "Expected a vector of size 2.\n");
        t->err->loc = v->loc;
        return false;
    }

    for (cp_size_each(y, 2)) {
        if (!get_float(&r->v[y], t, v->_array.value.data[y])) {
            return false;
        }
    }

    return true;
}

__unused
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
    if ((v->type != CP_SYN_VALUE_ARRAY) ||
        (v->_array.value.size != 3))
    {
        cp_vchar_printf(&t->err->msg, "Expected a vector of size 3.\n");
        t->err->loc = v->loc;
        return false;
    }

    for (cp_size_each(y, 3)) {
        if (!get_float(&r->v[y], t, v->_array.value.data[y])) {
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

static bool get_vec4(
    cp_vec4_t *r,
    ctxt_t *t,
    cp_syn_value_t const *v)
{
    if ((v->type != CP_SYN_VALUE_ARRAY) ||
        (v->_array.value.size != 4))
    {
        cp_vchar_printf(&t->err->msg, "Expected a vector of size 4.\n");
        t->err->loc = v->loc;
        return false;
    }

    for (cp_size_each(y, 4)) {
        if (!get_float(&r->v[y], t, v->_array.value.data[y])) {
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

    if ((v->type != CP_SYN_VALUE_ARRAY) ||
        (
            (v->_array.value.size != 3) &&
            (v->_array.value.size != 4)
        ))
    {
        cp_vchar_printf(&t->err->msg, "Expected a 3x3, 4x4, or 3x3+T matrix array.\n");
        t->err->loc = v->loc;
        return false;
    }

    for (cp_size_each(y, v->_array.value.size)) {
        cp_syn_value_t const *l = v->_array.value.data[y];
        if ((l->type == CP_SYN_VALUE_ARRAY) &&
            (l->_array.value.size == 3))
        {
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
        cp_vchar_printf(&t->err->msg, "Not a valid 3x3+T matrix: last row must be [0,0,0,1].\n");
        t->err->loc = v->_array.value.data[3]->loc;
        return false;
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
    ctxt_t *t __unused,
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
#define PARAM_VEC3_OR_FLOAT(n,x,m) PARAM(n, x, _get_vec3_or_float, cp_vec3_t, m)
#define PARAM_MAT3W(n,x,m)         PARAM(n, x, _get_mat3w,  cp_mat3w_t,       m)
#define PARAM_RAW(n,x,m)           PARAM(n, x, _get_raw,    cp_syn_value_t const *, m)

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
                cp_vchar_printf(&t->err->msg, "Expected parameter name.\n");
                t->err->loc = a->value->loc;
                return false;
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
            cp_vchar_printf(&t->err->msg, "Unknown parameter name '%s'.\n", a->key);
            t->err->loc = a->value->loc;
            return false;
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
            cp_vchar_printf(&t->err->msg, "Missing '%s' parameter.\n", pos[o].name);
            t->err->loc = loc;
            return false;
        }
    }
    for (cp_size_each(o, name_cnt)) {
        if (name[o].have_p != NULL) {
            *name[o].have_p = name[o].have;
        }
        else
        if (!name[o].have) {
            cp_vchar_printf(&t->err->msg, "Missing '%s' parameter.\n", name[o].name);
            t->err->loc = loc;
            return false;
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

static bool multmatrix_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_multmatrix_t *r = cp_scad_multmatrix(_r);

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_MAT3W("m", &r->m, NULL),
        ),
        ()))
    {
        return false;
    }

    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool cube_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_cube_t *r = cp_scad_cube(_r);

    r->size.x = r->size.y = r->size.z = 1;
    r->center = false;

    return GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_VEC3_OR_FLOAT("size",   &r->size,   ((bool[]){false})),
            PARAM_BOOL         ("center", &r->center, ((bool[]){false})),
        ),
        ());
}

static bool square_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_square_t *r = cp_scad_square(_r);

    r->size.x = r->size.y = 1;
    r->center = false;

    return GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_VEC2_OR_FLOAT("size",   &r->size,   ((bool[]){false})),
            PARAM_BOOL         ("center", &r->center, ((bool[]){false})),
        ),
        ());
}

static bool sphere_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_sphere_t *r = cp_scad_sphere(_r);

    r->_fn = 0;
    r->_fa = 12;
    r->_fs = 2;
    r->r = 1;

    bool have_r = false;
    double d;
    bool have_d = false;
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_FLOAT ("r",   &r->r, &have_r),
        ),
        (
            PARAM_FLOAT ("d",   &d, &have_d),
            PARAM_FLOAT ("$fa", &r->_fa, ((bool[]){false})),
            PARAM_FLOAT ("$fs", &r->_fs, ((bool[]){false})),
            PARAM_UINT32("$fn", &r->_fn, ((bool[]){false})),
        )))
    {
        return false;
    }

    if (have_r && have_d) {
        cp_vchar_printf(&t->err->msg, "Either 'r' or 'd' parameters expected, but found both.\n");
        t->err->loc = f->loc;
        return false;
    }
    if (have_d) {
        r->r = d / 2;
    }

    return true;
}

static bool circle_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_circle_t *r = cp_scad_circle(_r);

    r->_fn = 0;
    r->_fa = 12;
    r->_fs = 2;
    r->r = 1;

    bool have_r = false;
    double d;
    bool have_d = false;
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_FLOAT ("r",   &r->r, &have_r),
        ),
        (
            PARAM_FLOAT ("d",   &d, &have_d),
            PARAM_FLOAT ("$fa", &r->_fa, ((bool[]){false})),
            PARAM_FLOAT ("$fs", &r->_fs, ((bool[]){false})),
            PARAM_UINT32("$fn", &r->_fn, ((bool[]){false})),
        )))
    {
        return false;
    }

    if (have_r && have_d) {
        cp_vchar_printf(&t->err->msg, "Either 'r' or 'd' parameters expected, but found both.\n");
        t->err->loc = f->loc;
        return false;
    }
    if (have_d) {
        r->r = d / 2;
    }

    return true;
}

static bool polyhedron_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_polyhedron_t *r = cp_scad_polyhedron(_r);

    cp_syn_value_t const *_points = NULL;
    cp_syn_value_t const *_triangles = NULL;
    cp_syn_value_t const *_faces = NULL;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_RAW ("points", &_points, NULL),
            PARAM_RAW ("faces",  &_faces,  ((bool[]){false})),
        ),
        (
            PARAM_RAW ("triangles", &_triangles, ((bool[]){false})),
        )))
    {
        return false;
    }

    if (_triangles != NULL) {
        if (_faces != NULL) {
            cp_vchar_printf(&t->err->msg, "Either 'faces' or 'triangles' expected, but found both.\n");
            t->err->loc = f->loc;
            return false;
        }
        _faces = _triangles;
    }
    if (_faces == NULL) {
        cp_vchar_printf(&t->err->msg, "Either 'faces' or 'triangles' expected, but found none.\n");
        t->err->loc = f->loc;
        return false;
    }

    if (_points->type != CP_SYN_VALUE_ARRAY) {
        cp_vchar_printf(&t->err->msg, "Expected array of points.\n");
        t->err->loc = _points->loc;
        return false;
    }
    cp_syn_value_array_t const *points = &_points->_array;
    cp_v_init0(&r->points, points->value.size);
    for (cp_v_each(i, &points->value)) {
        if (!get_vec3(&r->points.data[i].coord, t, points->value.data[i])) {
            return false;
        }
        r->points.data[i].loc = points->value.data[i]->loc;
    }

    if (_faces->type != CP_SYN_VALUE_ARRAY) {
        cp_vchar_printf(&t->err->msg, "Expected array of faces.\n");
        t->err->loc = _faces->loc;
        return false;
    }
    cp_syn_value_array_t const *faces = &_faces->_array;
    cp_v_init0(&r->faces, faces->value.size);
    for (cp_v_each(i, &faces->value)) {
        cp_syn_value_t const *_face = faces->value.data[i];
        if (_face->type != CP_SYN_VALUE_ARRAY) {
            cp_vchar_printf(&t->err->msg, "Expected array of point indices.\n");
            t->err->loc = _face->loc;
            return false;
        }
        cp_syn_value_array_t const *face = &_face->_array;
        if (face->value.size < 3) {
            cp_vchar_printf(&t->err->msg, "Expected at least 3 point indices, but found only %"_Pz"u.\n",
                face->value.size);
            t->err->loc = face->loc;
            return false;
        }

        r->faces.data[i].loc = _face->loc;
        cp_v_init0(&r->faces.data[i].points, face->value.size);
        for (cp_v_each(j, &face->value)) {
            size_t idx;
            if (!get_size(&idx, t, face->value.data[j])) {
                return false;
            }
            if (!(idx < r->points.size)) {
                cp_vchar_printf(&t->err->msg,
                    "Index out of range; have %"_Pz"u points, but found index %"_Pz"u.\n",
                    r->points.size,
                    idx);
                t->err->loc = face->value.data[j]->loc;
                t->err->loc2 = points->loc;
                return false;
            }

            cp_vec3_loc_ref_t *pr = &r->faces.data[i].points.data[j];
            pr->ref = &r->points.data[idx];
            pr->loc = face->value.data[j]->loc;
        }
    }

    return true;
}

static bool polygon_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_polygon_t *r = cp_scad_polygon(_r);

    cp_syn_value_t const *_points = NULL;
    cp_syn_value_t const *_paths = NULL;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_RAW ("points", &_points, NULL),
            PARAM_RAW ("paths",  &_paths,  ((bool[]){false})),
        ),
        ()))
    {
        return false;
    }

    if (_points->type != CP_SYN_VALUE_ARRAY) {
        cp_vchar_printf(&t->err->msg, "Expected array of points.\n");
        t->err->loc = _points->loc;
        return false;
    }
    cp_syn_value_array_t const *points = &_points->_array;
    cp_v_init0(&r->points, points->value.size);
    for (cp_v_each(i, &points->value)) {
        if (!get_vec2(&r->points.data[i].coord, t, points->value.data[i])) {
            return false;
        }
        r->points.data[i].loc = points->value.data[i]->loc;
    }

    if (_paths == NULL) {
        cp_v_init0(&r->paths, 1);
        cp_v_init0(&r->paths.data[0].points, r->points.size);
        for (cp_v_each(j, &r->points)) {
            cp_vec2_loc_ref_t *pr = &r->paths.data[0].points.data[j];
            pr->ref = &r->points.data[j];
            pr->loc = points->value.data[j]->loc;
        }
    }
    else {
        if (_paths->type != CP_SYN_VALUE_ARRAY) {
            cp_vchar_printf(&t->err->msg, "Expected array of paths.\n");
            t->err->loc = _paths->loc;
            return false;
        }
        cp_syn_value_array_t const *paths = &_paths->_array;
        cp_v_init0(&r->paths, paths->value.size);
        for (cp_v_each(i, &paths->value)) {
            cp_syn_value_t const *_path = paths->value.data[i];
            if (_path->type != CP_SYN_VALUE_ARRAY) {
                cp_vchar_printf(&t->err->msg, "Expected array of point indices.\n");
                t->err->loc = _path->loc;
                return false;
            }
            cp_syn_value_array_t const *path = &_path->_array;
            if (path->value.size < 3) {
                cp_vchar_printf(&t->err->msg, "Expected at least 3 point indices, but found only %"_Pz"u.\n",
                    path->value.size);
                t->err->loc = path->loc;
                return false;
            }

            r->paths.data[i].loc = _path->loc;
            cp_v_init0(&r->paths.data[i].points, path->value.size);
            for (cp_v_each(j, &path->value)) {
                size_t idx;
                if (!get_size(&idx, t, path->value.data[j])) {
                    return false;
                }
                if (!(idx < r->points.size)) {
                    cp_vchar_printf(&t->err->msg,
                        "Index out of range; have %"_Pz"u points, but found index %"_Pz"u.\n",
                        r->points.size,
                        idx);
                    t->err->loc = path->value.data[j]->loc;
                    t->err->loc2 = points->loc;
                    return false;
                }

                cp_vec2_loc_ref_t *pr = &r->paths.data[i].points.data[j];
                pr->ref = &r->points.data[idx];
                pr->loc = path->value.data[j]->loc;
            }
        }
    }

    return true;
}

static bool xyz_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_xyz_t *r = &_r->_xyz;
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_VEC3("v", &r->v, NULL),
        ),
        ()))
    {
        return false;
    }
    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool color_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_color_t *r = cp_scad_color(_r);
    r->rgba.a = 255;

    cp_syn_value_t const *_c = NULL;

    bool have_alpha = false;
    unsigned char alpha = 0;

    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_RAW  ("c", &_c, NULL),
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
        cp_syn_value_array_t const *c = cp_syn_value_array_const(_c);
        if (c->value.size < 3) {
            cp_vchar_printf(&t->err->msg,
                "Expected at least 3 colour components, but found %"_Pz"u.\n",
                c->value.size);
            t->err->loc = c->loc;
            return false;
        }
        size_t mx = 3 + (have_alpha ? 0 : 1);
        if (c->value.size > mx) {
            cp_vchar_printf(&t->err->msg,
                "Expected at most %"_Pz"u colour components, but found %"_Pz"u.\n",
                mx,
                c->value.size);
            t->err->loc = c->loc;
            return false;
        }
        for (cp_v_each(i, &c->value)) {
            if (!get_grey(&r->rgba.c[i], t, cp_v_nth(&c->value, i))) {
                return false;
            }
        }
    }
    else
    if (_c->type == CP_SYN_VALUE_STRING) {
        cp_syn_value_string_t const *c = cp_syn_value_string_const(_c);
        if (!cp_color_by_name(&r->rgba.rgb, c->value)) {
            cp_vchar_printf(&t->err->msg, "Unknown colour '%s'.\n", c->value);
            t->err->loc = c->loc;
            return false;
        }
    }
    else {
        cp_vchar_printf(&t->err->msg, "Expected array or string for color definition.\n");
        t->err->loc = _c->loc;
        return false;
    }

    if (have_alpha) {
        r->rgba.a = alpha;
    }

    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool scale_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_scale_t *r = cp_scad_scale(_r);
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_VEC3_OR_FLOAT("v", &r->v, NULL),
        ),
        ()))
    {
        return false;
    }
    return v_scad_from_v_syn_stmt_item(t, &r->child, &f->body);
}

static bool rotate_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_r)
{
    cp_scad_rotate_t *r = cp_scad_rotate(_r);

    r->a = 0;
    CP_ZERO(&r->n);
    r->n.z = 1;

    cp_syn_value_t const *a = NULL;

    bool have_v;
    if (!GET_ARG(t, f->loc, &f->arg,
        (
            PARAM_RAW ("a", &a,    NULL),
            PARAM_VEC3("v", &r->n, &have_v),
        ),
        ()))
    {
        return false;
    }

    assert(a != NULL);
    if (a->type == CP_SYN_VALUE_ARRAY) {
        if (have_v) {
            cp_vchar_printf(&t->err->msg,
                "Either 'a' or 'v' is expected to be a vector, but found both.\n");
            t->err->loc = f->loc;
            return false;
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

static bool cylinder_from_func(
    ctxt_t *t,
    cp_syn_stmt_item_t *f,
    cp_scad_t *_q)
{
    cp_scad_cylinder_t *q = cp_scad_cylinder(_q);

    q->_fn = 0;
    q->_fa = 12;
    q->_fs = 2;
    q->h = 1;
    q->r1 = 1;
    q->r2 = 1;
    q->center = false;

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
            PARAM_FLOAT ("h",      &q->h, ((bool[]){false})),
            PARAM_FLOAT ("r1",     &q->r1, &have_r1),
            PARAM_FLOAT ("r2",     &q->r2, &have_r2),
            PARAM_BOOL  ("center", &q->center, ((bool[]){false})),
        ),
        (
            PARAM_FLOAT ("d",   &d,  &have_d),
            PARAM_FLOAT ("d1",  &d1, &have_d1),
            PARAM_FLOAT ("d2",  &d2, &have_d2),
            PARAM_FLOAT ("r",   &r,  &have_r),
            PARAM_FLOAT ("$fa", &q->_fa, ((bool[]){false})),
            PARAM_FLOAT ("$fs", &q->_fs, ((bool[]){false})),
            PARAM_UINT32("$fn", &q->_fn, ((bool[]){false})),
        )))
    {
        return false;
    }

    if (have_d && (have_d1 || have_d2)) {
        cp_vchar_printf(&t->err->msg, "Either 'd' or 'd1'/'d2' parameters expected, but found both.\n");
        t->err->loc = f->loc;
        return false;
    }
    if (have_d) {
        d1 = d2 = d;
        have_d1 = have_d2 = true;
    }

    if (have_r && (have_r1 || have_r2)) {
        cp_vchar_printf(&t->err->msg, "Either 'r' or 'r1'/'r2' parameters expected, but found both.\n");
        t->err->loc = f->loc;
        return false;
    }
    if (have_r) {
        q->r1 = q->r2 = r;
        have_r1 = have_r2 = true;
    }

    if (have_r && have_d) {
        cp_vchar_printf(&t->err->msg, "Either 'r' or 'd' parameters expected, but found both.\n");
        t->err->loc = f->loc;
        return false;
    }
    if (have_r1 && have_d1) {
        cp_vchar_printf(&t->err->msg, "Either 'r1' or 'd1' parameters expected, but found both.\n");
        t->err->loc = f->loc;
        return false;
    }
    if (have_r2 && have_d2) {
        cp_vchar_printf(&t->err->msg, "Either 'r2' or 'd2' parameters expected, but found both.\n");
        t->err->loc = f->loc;
        return false;
    }

    if (have_d1) {
        q->r1 = d1 / 2;
    }
    if (have_d2) {
        q->r2 = d2 / 2;
    }

    return true;
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

static int cmp_name_cmd(void const *_a, void const *_b, void *user __unused)
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
           .from = circle_from_func
        },
        {
           .id = "color",
           .type = CP_SCAD_COLOR,
           .from = color_from_func
        },
        {
           .id = "cube",
           .type = CP_SCAD_CUBE,
           .from = cube_from_func
        },
        {
           .id = "cylinder",
           .type = CP_SCAD_CYLINDER,
           .from = cylinder_from_func
        },
        {
           .id = "difference",
           .type = CP_SCAD_DIFFERENCE,
           .from = combine_from_func
        },
        {
           .id = "group",
           .type = CP_SCAD_UNION,
           .from = combine_from_func
        },
        {
           .id = "intersection",
           .type = CP_SCAD_INTERSECTION,
           .from = combine_from_func
        },
        {
           .id = "linear_extrude",
           .type = 0,
           .from = NULL,
        },
        {
           .id = "mirror",
           .type = CP_SCAD_MIRROR,
           .from = xyz_from_func
        },
        {
           .id = "multmatrix",
           .type = CP_SCAD_MULTMATRIX,
           .from = multmatrix_from_func
        },
        {
           .id = "polygon",
           .type = CP_SCAD_POLYGON,
           .from = polygon_from_func
        },
        {
           .id = "polyhedron",
           .type = CP_SCAD_POLYHEDRON,
           .from = polyhedron_from_func
        },
        {
           .id = "rotate",
           .type = CP_SCAD_ROTATE,
           .from = rotate_from_func
        },
        {
           .id = "scale",
           .type = CP_SCAD_SCALE,
           .from = scale_from_func
        },
        {
           .id = "sphere",
           .type = CP_SCAD_SPHERE,
           .from = sphere_from_func
        },
        {
           .id = "square",
           .type = CP_SCAD_SQUARE,
           .from = square_from_func
        },
        {
           .id = "text",
           .type = 0,
           .from = NULL,
        },
        {
           .id = "translate",
           .type = CP_SCAD_TRANSLATE,
           .from = xyz_from_func
        },
        {
           .id = "union",
           .type = CP_SCAD_UNION,
           .from = combine_from_func
        },
        {
           .id = "{",
           .type = CP_SCAD_UNION,
           .from = combine_from_func
        },
    };

    size_t idx = cp_bsearch(
        f->functor, cmds, cp_countof(cmds), sizeof(cmds[0]), cmp_name_cmd, NULL);
    if (idx >= cp_countof(cmds)) {
        t->err->loc = f->loc;
        cp_vchar_printf(&t->err->msg, "Unrecognised operator/object: '%s'.\n", f->functor);
        return false;
    }

    cmd_t const *c = &cmds[idx];
    if (c->type == 0) {
        cp_syn_loc_t loc;
        if (cp_syn_get_loc(&loc, t->syn, f->loc)) {
            fprintf(stderr, "%s:%"_Pz"u: ", loc.file->filename.data, loc.line+1);
        }
        fprintf(stderr, "Warning: Ignoring unsupported '%s'.\n", f->functor);
        return true;
    }

    cp_scad_t *r;
    if (!func_new(&r, t, f, c->type)) {
        return false;
    }
    cp_v_push(result, r);
    return c->from(t, f, r);
}

static bool v_scad_from_syn_stmt_use(
    ctxt_t *t __unused,
    cp_v_scad_p_t *result __unused,
    cp_syn_stmt_use_t *f __unused)
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
       return v_scad_from_syn_stmt_item(t, result, cp_syn_stmt_item(f));
    case CP_SYN_STMT_USE:
       return v_scad_from_syn_stmt_use(t, result, cp_syn_stmt_use(f));
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

/**
 * Same as cp_scad_from_syn_stmt_item, applied to each element
 * of the 'func' vector.
 *
 * On success, returns true.
 * In case of error, returns false and fills in tree->err_loc and tree->err_msg.
 */
extern bool cp_scad_from_syn_tree(
    cp_scad_tree_t *result,
    cp_syn_tree_t *syn)
{
    ctxt_t t = {
        .syn = syn,
        .top = result,
        .err = &syn->err,
    };
    return v_scad_from_v_syn_stmt(&t, &result->toplevel, &syn->toplevel);
}
