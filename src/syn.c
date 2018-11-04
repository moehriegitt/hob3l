/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/* SCAD parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <hob3l/syn.h>
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
    cp_syn_tree_t *tree;

    char lex_cur;
    char *lex_string;
    char *lex_end;

    unsigned tok_type;
    const char *tok_string;
    const char *tok_loc;
} parse_t;

static bool have_err_msg(parse_t *p)
{
    return p->tree->err.msg.size > 0;
}

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

static void tok_next_aux2(parse_t *p)
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

    /* INT and FLOAT */
    if ((p->lex_cur == '+') ||
        (p->lex_cur == '-') ||
        (p->lex_cur == '.') ||
        is_digit(p->lex_cur))
    {
        if (*p->lex_string == '\0') {
            /* E.g. '9.9.9' or '9.9"hallo"' or '9.9foo' will all parse
             * as '9.9' + ERROR, because this syntax does not allow
             * multi char tokens to directly follow each other.  This
             * is exploited by pointing directly into the input file
             * and overwriting the end of string with '\0', so we
             * really cannot parse those.
             */
            if (!have_err_msg(p)) {
                cp_vchar_printf(&p->tree->err.msg, "Expected no number here.\n");
            }
            p->tok_type = T_ERROR;
            return;
        }

        p->tok_type = T_INT;
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
            p->tok_type = T_FLOAT;
            lex_next(p);
            while (is_digit(p->lex_cur)) {
                lex_next(p);
            }
        }
        if ((p->lex_cur == 'e') || (p->lex_cur == 'E')) {
            p->tok_type = T_FLOAT;
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
    if ((p->lex_cur == '$') || (p->lex_cur == '_') || is_alpha(p->lex_cur)) {
        if (*p->lex_string == '\0') {
            if (!have_err_msg(p)) {
                cp_vchar_printf(&p->tree->err.msg, "Expected no identifier here.\n");
            }
            p->tok_type = T_ERROR;
            return;
        }

        p->tok_type = T_ID;
        if (p->lex_cur == '$') {
            lex_next(p);
        }
        while (
            is_alpha(p->lex_cur) ||
            is_digit(p->lex_cur) ||
            (p->lex_cur == '_'))
        {
            lex_next(p);
        }

        *p->lex_string = '\0';
        return;
    }

    /* STRING */
    if (p->lex_cur == '"') {
        lex_next(p);
        p->tok_type = T_STRING;
        p->tok_string = p->lex_string;
        while (*p->lex_string != '"') {
            if (*p->lex_string == '\\') {
                lex_next(p);
                if (*p->lex_string & ~0x7f) {
                    if (!have_err_msg(p)) {
                        cp_vchar_printf(&p->tree->err.msg,
                            "8-bit characters are not supported after '\\'.\n");
                    }
                    p->tok_loc = p->lex_string;
                    p->tok_type = T_ERROR;
                    return;
                }
            }
            if (*p->lex_string == '\0') {
                if (!have_err_msg(p)) {
                    cp_vchar_printf(&p->tree->err.msg, "End of file inside string.\n");
                }
                p->tok_type = T_ERROR;
                return;
            }
            lex_next(p);
        }
        *p->lex_string = '\0';
        lex_next(p);
        return;
    }

    /* NOTE: comments are not NUL terminated because the tokens
     * are thrown away anyway, and it could disrupt parsing by
     * overwriting the first character of an ID token.
     */

    /* line comment */
    if ((p->lex_cur == '/') && (p->lex_string[1] == '/')) {
        p->tok_type = T_LCOM;
        while ((p->lex_cur != '\n') && (p->lex_cur != '\0')) {
            lex_next(p);
        }
        /* do not eat '\n', it will be consumed as white space anyway. */
        return;
    }

    /* block comment */
    if ((p->lex_cur == '/') && (p->lex_string[1] == '*')) {
        p->tok_type = T_BCOM;
        lex_next(p);
        lex_next(p);
        char prev = 0;
        while ((prev != '*') || (p->lex_cur != '/')) {
            if (p->lex_cur == '\0') {
                if (!have_err_msg(p)) {
                    cp_vchar_printf(&p->tree->err.msg, "File ends inside comment.\n");
                }
                p->tok_type = T_ERROR;
                break;
            }
            prev = p->lex_cur;
            lex_next(p);
        }
        /* each final '/' (also works at EOF) */
        lex_next(p);
        return;
    }

    /* by default, read a single character */
    if (!(p->lex_cur == (p->lex_cur & 127))) {
        if (!have_err_msg(p)) {
            cp_vchar_printf(&p->tree->err.msg,
                "8-bit characters are not supported as symbols.\n");
        }
        p->tok_type = T_ERROR;
        return;
    }
    p->tok_type = p->lex_cur & 127;
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
    } while (is_comment(p->tok_type));
}

/** make a <....> path string token if the next token is currently '<' */
static void tok_path(parse_t *p)
{
    /* do not scan beyond an error */
    if (p->tok_type != '<') {
        return;
    }

    /* PATH */
    p->tok_type = T_PATH;
    p->tok_string = p->lex_string;
    while (*p->lex_string != '>') {
        if (*p->lex_string == '\0') {
            if (!have_err_msg(p)) {
                cp_vchar_printf(&p->tree->err.msg, "End of file inside path.\n");
            }
            p->tok_type = T_ERROR;
            return;
        }
        lex_next(p);
    }
    *p->lex_string = '\0';
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
    if (p->tok_type != T_ID) {
        return;
    }
    p->tok_type = sieve_id(p->tok_string);
}

static bool expect(
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
    case T_INT:
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
    if ((p->tok_type >= 32) && (p->tok_type <= 127)) {
        cp_vchar_printf(&p->tree->err.msg, ", found '%c'", p->tok_type);
    }

    char const *str = get_tok_string(p);
    if (str != NULL) {
        cp_vchar_printf(&p->tree->err.msg, ", found '%s'", str);
    }

    str = get_tok_description(p->tok_type);
    if (str != NULL) {
        cp_vchar_printf(&p->tree->err.msg, ", found %s", str);
    }

    cp_vchar_printf(&p->tree->err.msg, ".\n");
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
        cp_vchar_printf(&p->tree->err.msg, "Expected '%c'", type);
        err_found(p);
    }

    char const *str = get_tok_description(type);
    if (str != NULL) {
        cp_vchar_printf(&p->tree->err.msg, "Expected %s", str);
        err_found(p);
    }

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
    *string = p->tok_string;
    return expect_err(p, T_ID);
}

static bool parse_int(
    parse_t *p,
    cp_syn_value_int_t *r)
{
    assert(r->type == CP_SYN_VALUE_INT);
    r->value = strtoll(p->tok_string, NULL, 10);
    return expect_err(p, T_INT);
}

static bool parse_float(
    parse_t *p,
    cp_syn_value_float_t *r)
{
    assert(r->type == CP_SYN_VALUE_FLOAT);
    r->value = strtod(p->tok_string, NULL);
    return expect_err(p, T_FLOAT);
}

static bool parse_string(
    parse_t *p,
    cp_syn_value_string_t *r)
{
    assert(r->type == CP_SYN_VALUE_STRING);
    r->value = p->tok_string;
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
    *rp = value_id_new(p->tok_string);
    return expect_err(p, T_ID);
}

static bool parse_new_int(
    parse_t *p,
    cp_syn_value_t **rp)
{
    cp_syn_value_int_t *v = cp_syn_new(*v, p->tok_loc);
    *rp = cp_syn_cast(**rp, v);
    return parse_int(p, v);
}

static bool parse_new_float(
    parse_t *p,
    cp_syn_value_t **rp)
{
    cp_syn_value_float_t *v = cp_syn_new(*v, p->tok_loc);
    *rp = cp_syn_cast(**rp, v);
    return parse_float(p, v);
}

static bool parse_new_string(
    parse_t *p,
    cp_syn_value_t **rp)
{
    cp_syn_value_string_t *v = cp_syn_new(*v, p->tok_loc);
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
    cp_loc_t loc = p->tok_loc;
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
    switch (p->tok_type) {
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
    switch (p->tok_type) {
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
            cp_vchar_printf(&p->tree->err.msg, "Expected int, float, or identifier");
            err_found(p);
        }
        return false;
    }
}

static bool looking_at_arg(
    parse_t *p)
{
    return (p->tok_type == T_ID) || looking_at_value(p);
}

static bool parse_arg(
    parse_t *p,
    cp_syn_arg_t *r)
{
    if (p->tok_type == T_ID) {
        char const *t1;
        bool ok __unused = parse_id(p, &t1);
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
        if (p->tok_type == ')') {
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
        (p->tok_type == '*') ||
        (p->tok_type == '%') ||
        (p->tok_type == '!') ||
        (p->tok_type == '#');
}

static bool looking_at_stmt_item(
    parse_t *p)
{
    sieve(p);
    return
        (p->tok_type == T_ID) ||
        (p->tok_type == ';') ||
        (p->tok_type == '{') ||
        looking_at_modifier(p);
}

static bool looking_at_stmt(
    parse_t *p)
{
    if (looking_at_stmt_item(p)) {
        return true;
    }
    if (p->tok_type & _T_KEY) {
        return true;
    }
    return false;
}

static bool parse_modifier(
    parse_t *p,
    unsigned *mod)
{
    for (;;) {
        switch(p->tok_type) {
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
    if (p->tok_type == '{') {
        r->functor = "{";
        r->loc = p->tok_loc;
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

    switch (p->tok_type) {
    case ';':
        /* short way out: terminated by ';' */
        return expect(p, ';');

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

    r->path = p->tok_string;
    if (!expect(p, T_PATH)) {
        return false;
    }

    return true;
}

static bool parse_item_push_stmt(
    parse_t *p,
    cp_v_syn_stmt_p_t *r)
{
    if (expect(p, ';')) {
        return true;
    }
    cp_syn_stmt_item_t *f = cp_syn_new(*f, p->tok_string);
    cp_v_push(r, cp_syn_cast(cp_syn_stmt_t, f));
    return parse_stmt_item(p, f);
}

static bool parse_item_push_stmt_item(
    parse_t *p,
    cp_v_syn_stmt_item_p_t *r)
{
    if (expect(p, ';')) {
        return true;
    }
    cp_syn_stmt_item_t *f = cp_syn_new(*f, p->tok_string);
    cp_v_push(r, f);
    return parse_stmt_item(p, f);
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
        switch (p->tok_type) {
        case K_USE:{
            cp_syn_stmt_use_t *f = cp_syn_new(*f, p->tok_string);
            cp_v_push(r, cp_syn_cast(cp_syn_stmt_t, f));
            if (!parse_stmt_use(p, f)) {
                return false;
            }
            break;}
        default:
            if (p->tok_type & _T_KEY) {
                return true;
            }
            if (!parse_item_push_stmt(p, r)) {
                return false;
            }
            break;
        }
    }
}

static bool cp_scad_read_file(
    parse_t *p,
    cp_syn_tree_t *r,
    cp_syn_file_t *f,
    char const *filename,
    FILE *file)
{
    cp_vchar_printf(&f->filename, "%s", filename);
    f->file = file;

    /* read file */
    for(;;) {
        char buff[4096];
        size_t cnt = fread(buff, 1, sizeof(buff), f->file);
        assert(cnt <= sizeof(buff));
        if (cnt == 0) {
            if (feof(f->file)) {
                break;
            }
            cp_vchar_printf(&r->err.msg, "File read error: %s.\n",
                strerror(ferror(f->file)));
            return false;
        }
        cp_vchar_append_arr(&f->content, buff, cnt);
    }
    size_t z = f->content.size;
    cp_vchar_push(&f->content, '\0');

    /* make a copy */
    cp_vchar_append(&f->content_orig, &f->content);

    /* init scanner */
    p->lex_string = f->content.data;
    p->lex_cur = *p->lex_string;
    p->lex_end = f->content.data + z;

    /* cut into lines for lookup */
    cp_v_push(&f->line, p->lex_string);
    for (char const *i = p->lex_string; i < p->lex_end; i++) {
        if (*i == '\n') {
            cp_v_push(&f->line, i+1);
        }
    }
    if (cp_v_last(&f->line) != p->lex_end) {
        cp_v_push(&f->line, p->lex_end);
    }

    return true;
}

static int cmp_line(
    char const *key, char const *const *elem, char const **_end __unused)
{
    if (key < elem[0]) {
        return -1;
    }
    if (key == elem[0]) {
        return 0;
    }
    assert(&elem[1] < _end);
    /* We know that we can access the next array element,
     * because if we point to the last entry, we know that
     * key == elem[0] as the last line is empty. */
    if (key > elem[1]) {
        return +1;
    }
    return 0;
}

/* ********************************************************************** */

/**
 * Parse a file into a SCAD syntax tree.
 */
extern bool cp_syn_parse(
    cp_syn_tree_t *r,
    char const *filename,
    FILE *file)
{
    assert(r != NULL);
    assert(file != NULL);
    CP_ZERO(r);

    /* basic init */
    parse_t p[1];
    CP_ZERO(p);
    p->tree = r;

    cp_syn_file_t *f = CP_NEW(*f);
    cp_v_push(&r->file, f);
    if (!cp_scad_read_file(p, r, f, filename, file)) {
        return false;
    }

    /* scan first token */
    tok_next(p);

    bool ok = parse_stmt_list(p, &r->toplevel);
    if (!ok) {
        /* generic error message */
        if (r->err.loc == NULL) {
            r->err.loc = p->tok_loc;
        }
        if (!have_err_msg(p)) {
            cp_vchar_printf(&r->err.msg, "Parse error.\n");
        }
        return false;
    }
    if (p->tok_type != T_EOF) {
        r->err.loc = p->tok_loc;
        cp_vchar_printf(&r->err.msg, "Operator or object functor expected.\n");
        return false;
    }
    return true;
}

/**
 * Return a file location for a pointer to a token or any
 * other pointer into the file contents.
 *
 * This returns file and line number, but not posititon on the line,
 * because which position it is depends on tab width, so this is left
 * to the caller.
 *
 * To make it possible to count the position of the line, the original
 * contents of the line and an end-pointer of that line can be used:
 * loc->file->contents_orig can be indexed with loc-line for that.
 *
 * Note that lines are not NUL terminated, but the pointer at index
 * loc->line+1 (start of next line) defines the end of the line.
 *
 * For convenience, the cp_syn_loc_t already contains pointers to
 * orig line, orig line end, copied line (with parser inserted NULs),
 * copied line end.
 *
 * Returns whether the location was found.
 */
extern bool cp_syn_get_loc(
    cp_syn_loc_t *loc,
    cp_syn_tree_t *tree,
    char const *token)
{
    CP_ZERO(loc);
    loc->loc = token;

    /* FIXME:
     * Possibly sort files by pointer (idx 0 must be
     * toplevel, however).  We expect only a handful
     * of file currently (usually one), so we currently
     * don't bother.
     */
    for (cp_v_each(i, &tree->file)) {
        cp_syn_file_t *f = cp_v_nth(&tree->file, i);
        if ((token >= f->content.data) &&
            (token < f->content.data + f->content.size))
        {
            loc->file = f;
            loc->line = cp_v_bsearch(
                token, &f->line, cmp_line, f->line.data + f->line.size);
            assert(loc->line != CP_SIZE_MAX);
            assert(loc->line < f->line.size);

            /* compute line ranges for convenience */
            loc->copy = cp_v_nth(&f->line, loc->line);
            loc->copy_end = loc->copy;
            if (loc->line + 1 < f->line.size) {
                loc->copy_end = cp_v_nth(&f->line, loc->line+1);
            }

            loc->orig =
                f->content_orig.data +
                CP_PTRDIFF(loc->copy, f->content.data);
            loc->orig_end =
                loc->orig + CP_PTRDIFF(loc->copy_end, loc->copy);

            return true;
        }
    }
    return false;
}
