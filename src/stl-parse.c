/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/dict.h>
#include <hob3lbase/pool.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/mat.h>
#include <hob3l/stl-parse.h>
#include <hob3l/csg3_tam.h>
#include <hob3l/syn_tam.h>
#include <hob3l/vec3-dict.h>
#include "internal.h"

#define CHECK_NORMAL 1

/* Token types 1..127 are reserved for single character syntax tokens. */
/* Token types 128..255 are reserved for future use. */
#define T_EOF       0

#define _T_TOKEN    256
#define T_ERROR     (_T_TOKEN + 1)
#define T_ID        (_T_TOKEN + 2)
#define T_FLOAT     (_T_TOKEN + 4)
#define T_STRING    (_T_TOKEN + 5)

#define _T_KEY      512 /* marker for keywords */
#define K_SOLID     (_T_KEY + 1)
#define K_ENDSOLID  (_T_KEY + 2)
#define K_FACET     (_T_KEY + 3)
#define K_NORMAL    (_T_KEY + 4)
#define K_ENDFACET  (_T_KEY + 5)
#define K_OUTER     (_T_KEY + 6)
#define K_LOOP      (_T_KEY + 7)
#define K_ENDLOOP   (_T_KEY + 8)
#define K_VERTEX    (_T_KEY + 9)

typedef struct {
    cp_pool_t *tmp;
    cp_csg3_poly_t *poly;
    cp_err_t *err;
    cp_syn_input_t *input;

    char lex_cur;
    char *lex_string;
    char *lex_end;

    unsigned tok_type;
    const char *tok_string;
    const char *tok_loc;

    cp_vec3_dict_t point;
} parse_t;

static void lex_next(parse_t *p)
{
    /* EOF? */
    if (p->lex_string >= p->lex_end) {
        p->lex_cur = '\0';
        /* do not push lex_string further */
        return;
    }

    p->lex_string++;
    p->lex_cur = *p->lex_string;
}

static bool is_space(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static bool is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

static bool is_alpha(char c)
{
    return
        ((c >= 'a') && (c <= 'z')) ||
        ((c >= 'A') && (c <= 'Z'));
}

static void tok_next_aux(parse_t *p)
{
    /* do not scan beyond an error */
    if (p->tok_type == T_ERROR) {
        return;
    }

    /* skip space */
    while (is_space(p->lex_cur)) {
        lex_next(p);
    }

    /* Note that p->tok_string might point to '\0'.  It is needed for a
     * location pointer nevertheless. */
    p->tok_string = p->lex_string;
    p->tok_loc = p->lex_string;

    /* FLOAT */
    if ((p->lex_cur == '+') ||
        (p->lex_cur == '-') ||
        (p->lex_cur == '.') ||
        is_digit(p->lex_cur))
    {
        p->tok_type = T_FLOAT;
        if (*p->lex_string == '\0') {
            /* E.g. '9.9.9' or '9.9"hallo"' or '9.9foo' will all parse
             * as '9.9' + ERROR, because this syntax does not allow
             * multi char tokens to directly follow each other.  This
             * is exploited by pointing directly into the input file
             * and overwriting the end of string with '\0', so we
             * really cannot parse those.
             */
            cp_vchar_printf(&p->err->msg, "Expected no number here.\n");
            p->tok_type = T_ERROR;
            return;
        }

        if (p->lex_cur == '+') {
            lex_next(p);
            p->tok_string = p->lex_string;
        }
        else
        if (p->lex_cur == '-') {
            lex_next(p);
        }
        while (is_digit(p->lex_cur)) {
            lex_next(p);
        }
        if (p->lex_cur == '.') {
            lex_next(p);
            while (is_digit(p->lex_cur)) {
                lex_next(p);
            }
        }
        if ((p->lex_cur == 'e') || (p->lex_cur == 'E')) {
            lex_next(p);
            if ((p->lex_cur == '-') || (p->lex_cur == '+')) {
                lex_next(p);
            }
            while (is_digit(p->lex_cur)) {
                lex_next(p);
            }
        }
        *p->lex_string = '\0';
        return;
    }

    /* ID */
    if (is_alpha(p->lex_cur)) {
        if (*p->lex_string == '\0') {
            cp_vchar_printf(&p->err->msg, "Expected no identifier here.\n");
            p->tok_type = T_ERROR;
            return;
        }

        p->tok_type = T_ID;
        while (is_alpha(p->lex_cur)) {
            lex_next(p);
        }

        *p->lex_string = '\0';
        return;
    }

    /* by default, read a single character */
    if (!(p->lex_cur == (p->lex_cur & 127))) {
        cp_vchar_printf(&p->err->msg, "8-bit characters are not supported in STL file.\n");
        p->tok_type = T_ERROR;
        return;
    }
    p->tok_type = p->lex_cur & 127;
    lex_next(p);
}

static void tok_next_line(parse_t *p)
{
    /* do not scan beyond an error */
    if (p->tok_type == T_ERROR) {
        return;
    }
    lex_next(p);

    /* Note that p->tok_string might point to '\0'.  It is needed for a
     * location pointer nevertheless. */
    p->tok_string = p->lex_string;
    p->tok_loc = p->lex_string;

    while ((*p->lex_string != '\n') && (*p->lex_string != '\0')) {
        lex_next(p);
    }
    p->tok_type = T_STRING;
}

static unsigned sieve_id(char const *s, size_t len)
{
    if (len < 4) {
        return T_ID;
    }

    switch (s[0]) {
    case 'e':
        switch (s[3]) {
        case 'f':
            if (strequ(s, "endfacet")) { return K_ENDFACET; }
            break;
        case 'l':
            if (strequ(s, "endloop")) { return K_ENDLOOP; }
            break;
        case 's':
            if (strequ(s, "endsolid")) { return K_ENDSOLID; }
            break;
        }
        break;
    case 'f':
        if (strequ(s, "facet")) { return K_FACET; }
        break;
    case 'l':
        if (strequ(s, "loop")) { return K_LOOP; }
        break;
    case 'n':
        if (strequ(s, "normal")) { return K_NORMAL; }
        break;
    case 'o':
        if (strequ(s, "outer")) { return K_OUTER; }
        break;
    case 's':
        if (strequ(s, "solid")) { return K_SOLID; }
        break;
    case 'v':
        if (strequ(s, "vertex")) { return K_VERTEX; }
        break;
    }

    return T_ID;
}

static void tok_next(parse_t *p)
{
    tok_next_aux(p);
    if (p->tok_type == T_ID) {
        p->tok_type = sieve_id(p->tok_string, CP_PTRDIFF(p->lex_string, p->tok_string));
    }
}

static bool _expect(
    parse_t *p,
    unsigned type)
{
    if (p->tok_type == type) {
        tok_next(p);
        return true;
    }
    return false;
}

static const char *get_tok_string(
    parse_t *p)
{
    if (p->tok_type & _T_KEY) {
        return p->tok_string;
    }
    switch (p->tok_type) {
    case T_FLOAT:
    case T_ID:
        return p->tok_string;
    }
    return NULL;
}

static const char *get_tok_description(
    unsigned tok_type)
{
    switch (tok_type) {
    case ' ': case '\t': case '\r': case '\n':
        return "white space";

    case T_STRING:
        return "string";

    case T_EOF:
        return "end of file";
    }
    return NULL;
}

static void err_found(
    parse_t *p)
{
    if ((p->tok_type >= 32) && (p->tok_type <= 127)) {
        cp_vchar_printf(&p->err->msg, ", found '%c'", p->tok_type);
    }

    char const *str = get_tok_string(p);
    if (str != NULL) {
        cp_vchar_printf(&p->err->msg, ", found '%s'", str);
    }

    str = get_tok_description(p->tok_type);
    if (str != NULL) {
        cp_vchar_printf(&p->err->msg, ", found %s", str);
    }

    cp_vchar_printf(&p->err->msg, ".\n");
}

static bool expect_err(
    parse_t *p,
    unsigned type)
{
    if (_expect(p, type)) {
        return true;
    }

    if ((type >= 32) && (type <= 127)) {
        cp_vchar_printf(&p->err->msg, "Expected '%c'", type);
        err_found(p);
    }

    char const *str = get_tok_description(type);
    if (str != NULL) {
        cp_vchar_printf(&p->err->msg, "Expected %s", str);
        err_found(p);
    }

    cp_vchar_printf(&p->err->msg, "Unexpected token");
    err_found(p);
    return false;
}

static bool parse_float(
    parse_t *p,
    cp_f_t *v)
{
    *v = strtod(p->tok_string, NULL);
    return expect_err(p, T_FLOAT);
}

static bool parse_vec3(
    parse_t *p,
    cp_vec3_t *v)
{
    for (cp_size_each(i, 3)) {
        if (!parse_float(p, &v->v[i])) {
            return false;
        }
    }
    return true;
}

static bool parse_vertex(
    parse_t *p,
    cp_vec3_dict_node_t **n)
{
    if (!expect_err(p, K_VERTEX)) {
        return false;
    }
    cp_loc_t loc = p->tok_loc;
    cp_vec3_t v;
    if (!parse_vec3(p, &v)) {
        return false;
    }
    *n = cp_vec3_dict_insert(p->tmp, &p->point, &v, loc);
    return true;
}

static bool parse_facet(
    parse_t *p)
{
    cp_loc_t loc = p->tok_loc;
    cp_vec3_t normal = {0};
    if (!expect_err(p, K_FACET) || !expect_err(p, K_NORMAL) || !parse_vec3(p, &normal) ||
        !expect_err(p, K_OUTER) || !expect_err(p, K_LOOP))
    {
        return false;
    }
    cp_vec3_dict_node_t *v[3];
    cp_loc_t vloc[3];
    for (cp_size_each(i, 3)) {
        vloc[i] = p->tok_loc;
        if (!parse_vertex(p, &v[i])) {
            return false;
        }
    }
    if (!expect_err(p, K_ENDLOOP) || !expect_err(p, K_ENDFACET)) {
        return false;
    }

#if CHECK_NORMAL
    /* FIXME: according to spec, there is no need to check the normal.
     * But here's the code to do it in case we want it later: */

    /* check whether the normal is exactly opposite of what is
     * expected so we can swap the points. */
    cp_vec3_t normal2;
    cp_vec3_right_cross3(&normal2, &v[0]->coord, &v[1]->coord, &v[2]->coord);
    if (cp_vec3_has_len0(&normal2)) {
        /* ignore collapsed triangles */
        return true;
    }
    if ((cp_pt_cmp(normal.x, 0) == cp_pt_cmp(normal2.x, 0)) &&
        (cp_pt_cmp(normal.y, 0) == cp_pt_cmp(normal2.y, 0)) &&
        (cp_pt_cmp(normal.z, 0) == cp_pt_cmp(normal2.z, 0)))
    {
        CP_SWAP(&v[1], &v[2]);
        CP_SWAP(&vloc[1], &vloc[2]);
    }
#endif

    cp_csg3_face_t *face = cp_v_push0(&p->poly->face);
    face->loc = loc;
    cp_v_init0(&face->point, 3);
    for (cp_size_each(i, 3)) {
        cp_vec3_loc_ref_t *q = &cp_v_nth(&face->point, 2 - i);
        q->ref = (void*)v[i]->idx;
        q->loc = vloc[i];
    }

    return true;
}

static bool parse_solid(
    parse_t *p)
{
    if (p->tok_type != K_SOLID) {
        return false;
    }
    tok_next_line(p);
    tok_next(p);
    while (p->tok_type == K_FACET) {
        if (!parse_facet(p)) {
            return false;
        }
    }
    if (!expect_err(p, K_ENDSOLID)) {
        return false;
    }
    tok_next_line(p);
    tok_next(p);

    /* copy points */
    cp_v_init0(&p->poly->point, p->point.size);
    for (cp_dict_each(_n, p->point.root)) {
        cp_vec3_dict_node_t *n = CP_BOX_OF(_n, *n, node);
        cp_vec3_loc_t *q = &cp_v_nth(&p->poly->point, n->idx);
        q->coord = n->coord;
        q->loc = n->loc;
    }
    /* update face point refs */
    for (cp_v_each(i, &p->poly->face)) {
        cp_csg3_face_t *f = &cp_v_nth(&p->poly->face, i);
        for (cp_v_each(j, &f->point)) {
            cp_vec3_loc_ref_t *r = &cp_v_nth(&f->point, j);
            r->ref = cp_v_nth_ptr(&p->poly->point, (size_t)r->ref);
        }
    }

    return true;
}

static void stl_start_file(
    parse_t *p,
    cp_syn_file_t *f)
{
    assert(f->content.size >= 1);
    p->lex_string = f->content.data;
    p->lex_cur = *p->lex_string;
    p->lex_end = f->content.data + f->content.size - 1;
}

static bool parse_text(
    parse_t *p,
    cp_syn_file_t *file)
{
    stl_start_file(p, file);

    /* scan first token */
    tok_next(p);

    bool ok = parse_solid(p);
    if (!ok) {
        /* generic error message */
        if (p->err->loc == NULL) {
            p->err->loc = p->tok_loc;
        }
        if (p->err->msg.size == 0) {
            cp_vchar_printf(&p->err->msg, "STL parse error.\n");
        }
        return false;
    }
    if (p->tok_type != T_EOF) {
        p->err->loc = p->tok_loc;
        cp_vchar_printf(&p->err->msg, "Garbage after 'endsolid'.\n");
        return false;
    }
    return true;
}

/* ********************************************************************* */

/**
 * Parse an STL file into a CSG3 polyhedron.
 */
extern bool cp_stl_parse(
    cp_pool_t *tmp,
    cp_err_t *err,
    cp_syn_input_t *input,
    cp_csg3_poly_t *r,
    cp_syn_file_t *file)
{
    assert(r != NULL);
    assert(file != NULL);

    /* basic init */
    parse_t p = {
        .poly = r,
        .err = err,
        .input = input,
        .tmp = tmp
    };

    /* check for text STL */
    if ((file->content.size >= 5) &&
        (memcmp(file->content.data, "solid", 5) == 0))
    {
        return parse_text(&p, file);
    }

    err->loc = file->content.data;
    cp_vchar_printf(&err->msg, "Unrecognised STL file format.\n");
    return false;
}
