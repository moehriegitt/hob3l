/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* SCAD parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <hob3l/syn.h>
#include <hob3l/syn-msg.h>
#include <hob3lbase/vchar.h>
#include <hob3lbase/alloc.h>
#include "internal.h"

/* Token types 1..127 are reserved for single character syntax tokens. */
/* Token types 128..255 are reserved for future use. */
#define T_EOF       0

#define _T_TOKEN    256
#define T_ERROR     (_T_TOKEN + 1)
#define T_ID        (_T_TOKEN + 2)
#define T_INT       (_T_TOKEN + 3)
#define T_FLOAT     (_T_TOKEN + 4)
#define T_STRING    (_T_TOKEN + 5)
#define T_PATH      (_T_TOKEN + 6)
#define T_LCOM      (_T_TOKEN + 7) /* line comment */
#define T_BCOM      (_T_TOKEN + 8) /* block comment */

#define _T_KEY      512 /* marker for keywords */
#define K_INCLUDE   (_T_KEY + 1) /* 'include' */
#define K_USE       (_T_KEY + 2) /* 'use' */
#define K_MODULE    (_T_KEY + 3) /* 'module' */
#define K_FUNCTION  (_T_KEY + 4) /* 'function' */

typedef struct {
    char cur;
    char *string;
    char *end;
    cp_syn_file_t *file;
} lex_t;

typedef struct {
    unsigned type;
    const char *string;
    const char *loc;
} tok_t;

typedef struct {
    lex_t lex;
    tok_t tok;
} parse_ctxt_t;

typedef struct {
    cp_syn_tree_t *tree;
    cp_err_t *err;
    cp_syn_input_t *input;
    parse_ctxt_t c;
} parse_t;

static void tok_next(parse_t *p);

static inline parse_ctxt_t begin_file(
    parse_t *p,
    cp_syn_file_t *f)
{
    parse_ctxt_t prev = p->c;
    p->c = (parse_ctxt_t){};

    p->c.lex.string = f->content.data;
    p->c.lex.cur = *p->c.lex.string;
    p->c.lex.end = f->content.data + f->content.size;
    p->c.lex.file = f;

    tok_next(p);

    return prev;
}

static inline void end_file(
    parse_t *p,
    parse_ctxt_t *prev)
{
    p->c = *prev;
    *prev = (parse_ctxt_t){};
}

/**
 * Whether c is ASCII white space character.
 */
extern bool syn_is_space(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

/**
 * Whether c is an ASCII digit character.
 */
extern bool syn_is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

/**
 * Whether c is an ASCII alphabetic character.
 */
extern bool syn_is_alpha(char c)
{
    return
        ((c >= 'a') && (c <= 'z')) ||
        ((c >= 'A') && (c <= 'Z'));
}

static bool have_err_msg(parse_t *p)
{
    return p->err->msg.size > 0;
}

static void lex_next(parse_t *p)
{
    /* EOF? */
    if (p->c.lex.string >= p->c.lex.end) {
        p->c.lex.cur = '\0';
        /* do not push c.lex.string further */
        return;
    }

    p->c.lex.string++;
    p->c.lex.cur = *p->c.lex.string;
}

static void tok_next_aux2(parse_t *p)
{
    /* do not scan beyond an error */
    if (p->c.tok.type == T_ERROR) {
        return;
    }

    /* skip space */
    while (syn_is_space(p->c.lex.cur)) {
        lex_next(p);
    }

    /* Note that p->c.tok.string might point to '\0'.  It is needed for a
     * location pointer nevertheless. */
    p->c.tok.string = p->c.lex.string;
    p->c.tok.loc = p->c.lex.string;

    /* INT and FLOAT */
    if ((p->c.lex.cur == '+') ||
        (p->c.lex.cur == '-') ||
        (p->c.lex.cur == '.') ||
        syn_is_digit(p->c.lex.cur))
    {
        if (*p->c.lex.string == '\0') {
            /* E.g. '9.9.9' or '9.9"hallo"' or '9.9foo' will all parse
             * as '9.9' + ERROR, because this syntax does not allow
             * multi char tokens to directly follow each other.  This
             * is exploited by pointing directly into the input file
             * and overwriting the end of string with '\0', so we
             * really cannot parse those.
             */
            if (!have_err_msg(p)) {
                cp_vchar_printf(&p->err->msg, "Expected no number here.\n");
            }
            p->c.tok.type = T_ERROR;
            return;
        }

        p->c.tok.type = T_INT;
        if (p->c.lex.cur == '+') {
            lex_next(p);
            p->c.tok.string = p->c.lex.string;
        }
        else
        if (p->c.lex.cur == '-') {
            lex_next(p);
        }
        while (syn_is_digit(p->c.lex.cur)) {
            lex_next(p);
        }
        if (p->c.lex.cur == '.') {
            p->c.tok.type = T_FLOAT;
            lex_next(p);
            while (syn_is_digit(p->c.lex.cur)) {
                lex_next(p);
            }
        }
        if ((p->c.lex.cur == 'e') || (p->c.lex.cur == 'E')) {
            p->c.tok.type = T_FLOAT;
            lex_next(p);
            if ((p->c.lex.cur == '-') || (p->c.lex.cur == '+')) {
                lex_next(p);
            }
            while (syn_is_digit(p->c.lex.cur)) {
                lex_next(p);
            }
        }
        *p->c.lex.string = '\0';
        return;
    }

    /* ID */
    if ((p->c.lex.cur == '$') || (p->c.lex.cur == '_') || syn_is_alpha(p->c.lex.cur)) {
        if (*p->c.lex.string == '\0') {
            if (!have_err_msg(p)) {
                cp_vchar_printf(&p->err->msg, "Expected no identifier here.\n");
            }
            p->c.tok.type = T_ERROR;
            return;
        }

        p->c.tok.type = T_ID;
        if (p->c.lex.cur == '$') {
            lex_next(p);
        }
        while (
            syn_is_alpha(p->c.lex.cur) ||
            syn_is_digit(p->c.lex.cur) ||
            (p->c.lex.cur == '_'))
        {
            lex_next(p);
        }

        *p->c.lex.string = '\0';
        return;
    }

    /* STRING */
    if (p->c.lex.cur == '"') {
        lex_next(p);
        p->c.tok.type = T_STRING;
        p->c.tok.string = p->c.lex.string;
        while (*p->c.lex.string != '"') {
            if (*p->c.lex.string == '\\') {
                lex_next(p);
                if (*p->c.lex.string & ~0x7f) {
                    if (!have_err_msg(p)) {
                        cp_vchar_printf(&p->err->msg,
                            "8-bit characters are not supported after '\\'.\n");
                    }
                    p->c.tok.loc = p->c.lex.string;
                    p->c.tok.type = T_ERROR;
                    return;
                }
            }
            if (*p->c.lex.string == '\0') {
                if (!have_err_msg(p)) {
                    cp_vchar_printf(&p->err->msg, "End of file inside string.\n");
                }
                p->c.tok.type = T_ERROR;
                return;
            }
            lex_next(p);
        }
        *p->c.lex.string = '\0';
        lex_next(p);
        return;
    }

    /* NOTE: comments are not NUL terminated because the tokens
     * are thrown away anyway, and it could disrupt parsing by
     * overwriting the first character of an ID token.
     */

    /* line comment */
    if ((p->c.lex.cur == '/') && (p->c.lex.string[1] == '/')) {
        p->c.tok.type = T_LCOM;
        while ((p->c.lex.cur != '\n') && (p->c.lex.cur != '\0')) {
            lex_next(p);
        }
        /* do not eat '\n', it will be consumed as white space anyway. */
        return;
    }

    /* block comment */
    if ((p->c.lex.cur == '/') && (p->c.lex.string[1] == '*')) {
        p->c.tok.type = T_BCOM;
        lex_next(p);
        lex_next(p);
        char prev = 0;
        while ((prev != '*') || (p->c.lex.cur != '/')) {
            if (p->c.lex.cur == '\0') {
                if (!have_err_msg(p)) {
                    cp_vchar_printf(&p->err->msg, "File ends inside comment.\n");
                }
                p->c.tok.type = T_ERROR;
                break;
            }
            prev = p->c.lex.cur;
            lex_next(p);
        }
        /* each final '/' (also works at EOF) */
        lex_next(p);
        return;
    }

    /* by default, read a single character */
    if (!(p->c.lex.cur == (p->c.lex.cur & 127))) {
        if (!have_err_msg(p)) {
            cp_vchar_printf(&p->err->msg,
                "8-bit characters are not supported as symbols.\n");
        }
        p->c.tok.type = T_ERROR;
        return;
    }
    p->c.tok.type = p->c.lex.cur & 127;
    lex_next(p);
}

static bool is_comment(unsigned tok_type)
{
    return (tok_type == T_LCOM) || (tok_type == T_BCOM);
}

static void tok_next(parse_t *p)
{
    do {
        tok_next_aux2(p);
    } while (is_comment(p->c.tok.type));
}

/** make a <....> path string token if the next token is currently '<' */
static void tok_path(parse_t *p)
{
    /* do not scan beyond an error */
    if (p->c.tok.type != '<') {
        return;
    }

    /* PATH */
    p->c.tok.type = T_PATH;
    p->c.tok.string = p->c.lex.string;
    while (*p->c.lex.string != '>') {
        if (*p->c.lex.string == '\0') {
            if (!have_err_msg(p)) {
                cp_vchar_printf(&p->err->msg, "End of file inside path.\n");
            }
            p->c.tok.type = T_ERROR;
            return;
        }
        lex_next(p);
    }
    *p->c.lex.string = '\0';
    lex_next(p);
}

static unsigned sieve_id(char const *s)
{
    switch (s[0]) {
    case 'i':
        if (strequ(s, "include")) { return K_INCLUDE; }
        break;
    case 'u':
        if (strequ(s, "use")) { return K_USE; }
        break;
    case 'm':
        if (strequ(s, "module")) { return K_MODULE; }
        break;
    case 'f':
        if (strequ(s, "function")) { return K_FUNCTION; }
        break;
    }
    return T_ID;
}

static void sieve(parse_t *p)
{
    if (p->c.tok.type != T_ID) {
        return;
    }
    p->c.tok.type = sieve_id(p->c.tok.string);
}

static bool expect(
    parse_t *p,
    unsigned type)
{
    if (p->c.tok.type == type) {
        tok_next(p);
        return true;
    }
    return false;
}

static const char *get_tok_string(
    parse_t *p)
{
    if (p->c.tok.type & _T_KEY) {
        return p->c.tok.string;
    }
    switch (p->c.tok.type) {
    case T_INT:
    case T_FLOAT:
    case T_ID:
        return p->c.tok.string;
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

    case T_PATH:
        return "path";

    case T_EOF:
        return "end of file";

    case T_LCOM:
        return "comment";

    case T_BCOM:
        return "comment";
    }
    return NULL;
}

static void err_found(
    parse_t *p)
{
    if ((p->c.tok.type >= 32) && (p->c.tok.type <= 127)) {
        cp_vchar_printf(&p->err->msg, ", found '%c'", p->c.tok.type);
    }

    char const *str = get_tok_string(p);
    if (str != NULL) {
        cp_vchar_printf(&p->err->msg, ", found '%s'", str);
    }

    str = get_tok_description(p->c.tok.type);
    if (str != NULL) {
        cp_vchar_printf(&p->err->msg, ", found %s", str);
    }

    cp_vchar_printf(&p->err->msg, ".\n");
}

static bool expect_err(
    parse_t *p,
    unsigned type)
{
    if (expect(p, type)) {
        return true;
    }

    if (have_err_msg(p)) {
        return false;
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

static bool parse_stmt_list(
    parse_t *p,
    cp_v_syn_stmt_p_t *r);

static bool parse_stmt_item_list(
    parse_t *p,
    cp_v_syn_stmt_item_p_t *r);

static bool parse_item_push_stmt(
    parse_t *p,
    cp_v_syn_stmt_p_t *r);

static bool parse_item_push_stmt_item(
    parse_t *p,
    cp_v_syn_stmt_item_p_t *r);

static bool looking_at_value(
    parse_t *p);

static bool parse_value(
    parse_t *p,
    cp_syn_value_t **r);

static bool parse_id(
    parse_t *p,
    char const **string)
{
    *string = p->c.tok.string;
    return expect_err(p, T_ID);
}

static bool parse_int(
    parse_t *p,
    cp_syn_value_int_t *r)
{
    assert(r->type == CP_SYN_VALUE_INT);
    r->value = strtoll(p->c.tok.string, NULL, 10);
    return expect_err(p, T_INT);
}

static bool parse_float(
    parse_t *p,
    cp_syn_value_float_t *r)
{
    assert(r->type == CP_SYN_VALUE_FLOAT);
    r->value = strtod(p->c.tok.string, NULL);
    return expect_err(p, T_FLOAT);
}

static bool parse_string(
    parse_t *p,
    cp_syn_value_string_t *r)
{
    assert(r->type == CP_SYN_VALUE_STRING);
    r->value = p->c.tok.string;
    return expect_err(p, T_STRING);
}

static cp_syn_value_t *value_id_new(cp_loc_t loc)
{
    cp_syn_value_id_t *x = cp_syn_new(*x, loc);
    x->value = loc;
    return cp_syn_cast(cp_syn_value_t,x);
}

static bool parse_new_id(
    parse_t *p,
    cp_syn_value_t **rp)
{
    *rp = value_id_new(p->c.tok.string);
    return expect_err(p, T_ID);
}

static bool parse_new_int(
    parse_t *p,
    cp_syn_value_t **rp)
{
    cp_syn_value_int_t *v = cp_syn_new(*v, p->c.tok.loc);
    *rp = cp_syn_cast(**rp, v);
    return parse_int(p, v);
}

static bool parse_new_float(
    parse_t *p,
    cp_syn_value_t **rp)
{
    cp_syn_value_float_t *v = cp_syn_new(*v, p->c.tok.loc);
    *rp = cp_syn_cast(**rp, v);
    return parse_float(p, v);
}

static bool parse_new_string(
    parse_t *p,
    cp_syn_value_t **rp)
{
    cp_syn_value_string_t *v = cp_syn_new(*v, p->c.tok.loc);
    *rp = cp_syn_cast(**rp, v);
    return parse_string(p, v);
}

/**
 * Either a range ([a:b] or [a:b:c]) or an array ([], [a], [a,b], [a,b,c,...])
 *
 * To distinguish which one it is, we unfortunately need a bit of look-ahead.
 */
static bool parse_new_range_or_array(
    parse_t *p,
    cp_syn_value_t **rp)
{
    cp_loc_t loc = p->c.tok.loc;
    if (!expect_err(p, '[')) {
        return false;
    }

    if (expect(p, ']')) {
        /* empty array */
        cp_syn_value_array_t *v = cp_syn_new(*v, loc);
        *rp = cp_syn_cast(**rp, v);
        return true;
    }

    cp_syn_value_t *start = NULL;
    if (!parse_value(p, &start)) {
        return false;
    }

    if (expect(p, ':')) {
        /* range! */
        cp_syn_value_range_t *v = cp_syn_new(*v, loc);
        *rp = cp_syn_cast(**rp, v);
        v->start = start;

        if (!parse_value(p, &v->end)) {
            return false;
        }

        if (expect(p, ':')) {
            v->inc = v->end;
            if (!parse_value(p, &v->end)) {
                return false;
            }
        }
    }
    else {
        /* array! */
        cp_syn_value_array_t *v = cp_syn_new(*v, loc);
        *rp = cp_syn_cast(**rp, v);
        cp_v_syn_value_p_t *a = &v->value;
        cp_v_push(a, start);

        cp_syn_value_t *elem;
        while (expect(p, ',') &&
               looking_at_value(p))
        {
            if (!parse_value(p, &elem)) {
                return false;
            }
            cp_v_push(a, elem);
        }
    }

    return expect_err(p, ']');
}

static bool looking_at_value(
    parse_t *p)
{
    switch (p->c.tok.type) {
    case T_INT:
    case T_FLOAT:
    case T_STRING:
    case T_ID:
    case '[':
        return true;

    default:
        return false;
    }
}

static bool parse_value(
    parse_t *p,
    cp_syn_value_t **r)
{
    switch (p->c.tok.type) {
    case T_INT:
        return parse_new_int(p, r);

    case T_FLOAT:
        return parse_new_float(p, r);

    case T_STRING:
        return parse_new_string(p, r);

    case T_ID:
        return parse_new_id(p, r);

    case '[':
        return parse_new_range_or_array(p, r);

    default:
        if (!have_err_msg(p)) {
            cp_vchar_printf(&p->err->msg, "Expected int, float, or identifier");
            err_found(p);
        }
        return false;
    }
}

static bool looking_at_arg(
    parse_t *p)
{
    return (p->c.tok.type == T_ID) || looking_at_value(p);
}

static bool parse_arg(
    parse_t *p,
    cp_syn_arg_t *r)
{
    if (p->c.tok.type == T_ID) {
        char const *t1;
        bool ok CP_UNUSED = parse_id(p, &t1);
        assert(ok);
        if (!expect(p, '=')) {
            r->value = value_id_new(t1);
            return true;
        }
        r->key = t1;

    }
    return parse_value(p, &r->value);
}

static bool parse_push_arg(
    parse_t *p,
    cp_v_syn_arg_p_t *r)
{
    cp_syn_arg_t *f = CP_NEW(*f);
    cp_v_push(r, f);
    return parse_arg(p, f);
}

static bool parse_args(
    parse_t *p,
    cp_v_syn_arg_p_t *r)
{
    for (;;) {
        if (!looking_at_arg(p)) {
            return true;
        }
        if (!parse_push_arg(p, r)) {
            return false;
        }
        if (p->c.tok.type == ')') {
            return true;
        }
        if (!expect_err(p, ',')) {
            return false;
        }
    }
}

static bool looking_at_modifier(
    parse_t *p)
{
    return
        (p->c.tok.type == '*') ||
        (p->c.tok.type == '%') ||
        (p->c.tok.type == '!') ||
        (p->c.tok.type == '#');
}

static bool looking_at_stmt_item(
    parse_t *p)
{
    sieve(p);
    return
        (p->c.tok.type == T_ID) ||
        (p->c.tok.type == K_INCLUDE) || /* this pulls in multiple items */
        (p->c.tok.type == ';') ||
        (p->c.tok.type == '{') ||
        looking_at_modifier(p);
}

static bool looking_at_stmt(
    parse_t *p)
{
    if (looking_at_stmt_item(p)) {
        return true;
    }
    if (p->c.tok.type & _T_KEY) { /* allow any keyword (top-level) */
        return true;
    }
    return false;
}

static bool parse_modifier(
    parse_t *p,
    unsigned *mod)
{
    for (;;) {
        switch(p->c.tok.type) {
        case '!': *mod |= CP_GC_MOD_EXCLAM;  break;
        case '*': *mod |= CP_GC_MOD_AST;     break;
        case '%': *mod |= CP_GC_MOD_PERCENT; break;
        case '#': *mod |= CP_GC_MOD_HASH;    break;
        default:
            return true;
        }
        tok_next(p);
    }
}

static bool parse_stmt_item(
    parse_t *p,
    cp_syn_stmt_item_t *r)
{
    if (p->c.tok.type == '{') {
        r->functor = "{";
        r->loc = p->c.tok.loc;
    }
    else {
        bool ok =
            parse_modifier(p, &r->modifier) &&
            parse_id(p, &r->functor) &&
            expect_err(p, '(') &&
            parse_args(p, &r->arg) &&
            expect_err(p, ')');
        if (!ok) {
            return false;
        }
        r->loc = r->functor;
    }

    switch (p->c.tok.type) {
    case '{':
        /* body in { ... } */
        return
            expect(p, '{') &&
            parse_stmt_item_list(p, &r->body) &&
            expect_err(p, '}');

    default:
        return parse_item_push_stmt_item(p, &r->body);
    }
}

static bool parse_stmt_use(
    parse_t *p,
    cp_syn_stmt_use_t *r)
{
    if (!expect(p, K_USE)) {
        return false;
    }
    tok_path(p);

    r->path = p->c.tok.string;
    if (!expect(p, T_PATH)) {
        return false;
    }

    return true;
}

static bool parse_include_push_stmt_item(
    parse_t *p,
    cp_v_syn_stmt_item_p_t *r,
    char const *fn)
{
    cp_syn_file_t *file = CP_NEW(*file);
    if (!cp_syn_read(file, p->err, p->input, p->c.tok.loc, fn, NULL)) {
        assert(p->err->msg.size > 0);
        return false;
    }
    expect(p, T_PATH);

    parse_ctxt_t prev = begin_file(p, file);

    if (!parse_stmt_item_list(p, r)) {
        return false;
    }

    if (p->c.tok.type != T_EOF) {
        cp_vchar_printf(&p->err->msg, "Expected end of include file'");
        err_found(p);
        return false;
    }

    end_file(p, &prev);
    return true;
}

static bool parse_item_push_stmt_item(
    parse_t *p,
    cp_v_syn_stmt_item_p_t *r)
{
    if (expect(p, ';')) {
        return true;
    }
    sieve(p);
    if (expect(p, K_INCLUDE)) {
        tok_path(p);
        return parse_include_push_stmt_item(p, r, p->c.tok.string);
    }
    cp_syn_stmt_item_t *f = cp_syn_new(*f, p->c.tok.string);
    cp_v_push(r, f);
    return parse_stmt_item(p, f);
}

static bool parse_item_push_stmt(
    parse_t *p,
    cp_v_syn_stmt_p_t *r)
{
    /* cast the vector: we'll only push items, but into a more generic vector */
    return parse_item_push_stmt_item(p, (cp_v_syn_stmt_item_p_t*)r);
}

static bool parse_stmt_item_list(
    parse_t *p,
    cp_v_syn_stmt_item_p_t *r)
{
    for (;;) {
        if (!looking_at_stmt_item(p)) {
            return true;
        }
        if (!parse_item_push_stmt_item(p, r)) {
            return false;
        }
    }
}

static bool parse_stmt_list(
    parse_t *p,
    cp_v_syn_stmt_p_t *r)
{
    for (;;) {
        if (!looking_at_stmt(p)) {
            return true;
        }
        switch (p->c.tok.type) {
        case K_USE:{
            cp_syn_stmt_use_t *f = cp_syn_new(*f, p->c.tok.string);
            cp_v_push(r, cp_syn_cast(cp_syn_stmt_t, f));
            if (!parse_stmt_use(p, f)) {
                return false;
            }
            break;}
        default:
            if (p->c.tok.type & _T_KEY) {
                return true;
            }
            if (!parse_item_push_stmt(p, r)) {
                return false;
            }
            break;
        }
    }
}

static bool is_path_sep(
    char c)
{
#if defined(__WIN32) || defined(__WIN64)
    if (c == '\\') {
        return true;
    }
#endif
    if (c == '/') {
        return true;
    }
    return false;
}

static bool is_absolute(
    char const *fn)
{
#if defined(__WIN32) || defined(__WIN64)
    if (syn_is_alpha(*fn) && (fn[1] == ':')) {
        return true;
    }
#endif
    if (is_path_sep(*fn)) {
        return true;
    }
    return false;
}

static bool read_file(
    cp_err_t *err,
    cp_syn_input_t *input,
    cp_syn_file_t *f,
    cp_loc_t include_loc,
    char const *filename,
    FILE *file)
{
    cp_vchar_t dir = {0};

    if ((include_loc != NULL) && !is_absolute(filename)) {
        cp_syn_loc_t loc;
        if (!cp_syn_get_loc(&loc, input, include_loc)) {
            cp_vchar_printf(&err->msg,
                "Internal error: unable to retrieve location of file input location if '%s'.",
                filename);
            err->loc = include_loc;
            return false;
        }

        cp_vchar_append(&dir, &loc.file->filename);
        while ((dir.size > 0) && !is_path_sep(cp_v_last(&dir))) {
            dir.size--;
        }
        dir.data[dir.size] = 0; /* terminate again */
    }

    cp_vchar_printf(&f->filename, "%s%s", cp_vchar_cstr(&dir), filename);
    if (file == NULL) {
        (void)cp_vchar_cstr(&f->filename);
        file = fopen(f->filename.data, "rb");
        if (file == NULL) {
            cp_vchar_printf(&err->msg,
                "Unable to open '%s' for reading: %s\n",
                f->filename.data, strerror(errno));
            err->loc = include_loc;
            return false;
        }
    }
    f->file = file;
    f->include_loc = include_loc;

    cp_vchar_fini(&dir);

    /* read file */
    for(;;) {
        char buff[4096];
        size_t cnt = fread(buff, 1, sizeof(buff), f->file);
        assert(cnt <= sizeof(buff));
        if (cnt == 0) {
            if (feof(f->file)) {
                break;
            }
            cp_vchar_printf(&err->msg, "File read error: %s.\n",
                strerror(ferror(f->file)));
            err->loc = include_loc;
            return false;
        }
        cp_vchar_append_arr(&f->content, buff, cnt);
    }
    size_t z = f->content.size;
    cp_vchar_push(&f->content, '\0');
    f->content.size = z;

    /* make a copy */
    cp_vchar_append(&f->content_orig, &f->content);

    /* init scanner */
    char const *start = f->content.data;
    char const *end = f->content.data + z;

    /* cut into lines for lookup */
    cp_v_push(&f->line, start);
    for (char const *i = start; i < end; i++) {
        if (*i == '\n') {
            cp_v_push(&f->line, i+1);
        }
    }
    if (cp_v_last(&f->line) != end) {
        cp_v_push(&f->line, end);
    }

    return true;
}

/* ********************************************************************** */

/**
 * Read a file into memory and store it in the input file table.
 *
 * include_loc is the location of the file name in a parent file.  This
 * is stored in f, and used to resolve relative file names.
 *
 * If file is NULL, this function tries to open the file in binary mode
 * (all parsers work with binary mode), otherwise, the file is taken as is.
 */
extern bool cp_syn_read(
    cp_syn_file_t *f,
    cp_err_t *err,
    cp_syn_input_t *input,
    cp_loc_t include_loc,
    char const *filename,
    FILE *file)
{
    if (!read_file(err, input, f, include_loc, filename, file)) {
        return false;
    }
    cp_v_push(&input->file, f);
    return true;
}

/**
 * Parse a file into a SCAD syntax tree.
 */
extern bool cp_syn_parse(
    cp_err_t *err,
    cp_syn_input_t *input,
    cp_syn_tree_t *r,
    cp_syn_file_t *file)
{
    assert(r != NULL);
    assert(file != NULL);

    /* basic init */
    parse_t p[1];
    CP_ZERO(p);
    p->tree = r;
    p->err = err;
    p->input = input;

    (void)begin_file(p, file);

    bool ok = parse_stmt_list(p, &r->toplevel);
    if (!ok) {
        /* generic error message */
        if (err->loc == NULL) {
            err->loc = p->c.tok.loc;
        }
        if (!have_err_msg(p)) {
            cp_vchar_printf(&err->msg, "SCAD parse error.\n");
        }
        return false;
    }
    if (p->c.tok.type != T_EOF) {
        err->loc = p->c.tok.loc;
        cp_vchar_printf(&err->msg,
            "Parsing ended before end of file: operator or object functor expected");
        err_found(p);
        return false;
    }
    return true;
}
