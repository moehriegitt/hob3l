/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/pool.h>
#include <hob3lbase/utf8.h>
#include <hob3l/xml-parse.h>

typedef struct {
    cp_pool_t *tmp;
    cp_err_t *err;
    unsigned opt;

    unsigned cur;
    char *loc;
    char *str_next;
    char *str_end;

    char *tok_start;
} parse_t;

typedef struct ctxt ctxt_t;
struct ctxt {
    ctxt_t *prev;

    /** the bottom of the current list we append children to */
    cp_xml_t **bottom;

    /** namespace list */
    cp_xml_t *ns_map;

    /** default namespace */
    char const *ns_default;
};

char const cp_xml_any_[] = "--match anything--";

/* ********************************************************************** */

#define TRY(cmd) \
    ({ \
        __typeof__((cmd)) _res = (cmd); \
        if (!_res) { return 0; } \
        _res; \
    })

#define ERR(p, l, ...) \
    do{ \
        parse_t *p_ = (p); \
        p_->err->loc = l; \
        cp_vchar_printf(&(p_)->err->msg, __VA_ARGS__); \
        return 0; \
    }while(0)

#define PUSH(bottom, elem) \
    do{ \
        __typeof__(&(bottom)) botp_ = &(bottom); \
        __typeof__((elem)) elem_ = (elem); \
        **botp_ = elem_; \
        *botp_ = &elem_->next; \
    }while(0)

/* ********************************************************************** */

#define IND ind*4, ""

/**
 * Return a human readable name for the given enum value
 * of type cp_xml_type_t.
 */
extern char const *cp_xml_type_stringify(
    unsigned x)
{
    switch (x) {
    case CP_XML_DOC:   return "doc";
    case CP_XML_ATTR:  return "attr";
    case CP_XML_ELEM:  return "elem";
    case CP_XML_CDATA: return "cdata";
    }
    return "?";
}

/**
 * Recursive XML dumper (for debugging) */
static void xml_dump_rec(
    FILE *f,
    unsigned ind,
    cp_xml_t *n)
{
    for (; n; n = n->next) {
        fprintf(f, "%*s%s", IND, cp_xml_type_stringify(n->type));
        if (n->ns_prefix || n->data) {
            fprintf(f, " ");
            if (n->ns_prefix) {
                fprintf(f, "%s:", n->ns_prefix);
            }
            if (n->data) {
                fprintf(f, "'%s'", n->data);
            }
        }
        if (n->ns) {
            fprintf(f, " '%s'", n->ns);
        }
        fprintf(f, "\n");
        xml_dump_rec(f, ind+1, n->child);
    }
}

/**
 * XML dumper (for debugging) */
extern void cp_xml_dump(
    FILE *f,
    cp_xml_t *n)
{
    xml_dump_rec(f, 0, n);
}

static bool lex_next(
    parse_t *p)
{
    p->loc = p->str_next;

    cp_utf8_iter_t iter = {
        .data = p->str_next,
        .size = CP_MONUS(p->str_end, p->str_next),
        .error_msg = NULL,
    };
    p->cur = cp_utf8_decode(&iter);
    if (iter.error_msg != NULL) {
        ERR(p, p->loc, "UTF-8 decoding error: %s\n", iter.error_msg);
    }
    p->str_next += iter.data - p->str_next;
    return true;
}

/* ********************************************************************** */

static bool start_file(
    parse_t *p,
    cp_syn_file_t *f)
{
    p->str_next = f->content.data;
    p->str_end = f->content.data + f->content.size;
    TRY(lex_next(p));
    return true;
}

/**
 * Whether a character is XML White space
 */
extern bool cp_xml_is_space(unsigned c)
{
    switch (c) {
    case ' ': case '\t': case '\r': case '\n': case 0x0c:
    case 0x0085: case 0x2028: case 0x2029:
#if 0 /* not XML white space, but text white space */
    case 0x2000: case 0x2001: case 0x2002: case 0x2003: case 0x2004:
    case 0x2005: case 0x2006: case 0x2007: case 0x2008: case 0x2009:
    case 0x200A: case 0x205F: case 0x3000:
#endif
        return true;
    }
    return false;
}

static bool maybe_tag_begin(unsigned c)
{
    /* cannot begin with "-", ".", or a numeric digit. */
    switch (c) {
    case '-':
    case '.':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return false;
    }
    return true;
}

static bool is_tag_char(unsigned c)
{
    if (cp_xml_is_space(c)) {
        return false;
    }
    if (c > 0x7f) {
        return true;
    }
    switch (c) {
    case '!':  case '"': case '#':  case '$': case '%':  case '&': case '\'': case '(':
    case ')':  case '*': case '+':  case ',': case '/':  case ';': case '<':  case '=':
    case '>':  case '?': case '@':  case '[': case '\\': case ']': case '^':  case '`':
    case '{':  case '|': case '}':  case '~': case ':':
        return false;
    }
    return true;
}

static char *parse_tag(
    parse_t *p)
{
    char *tok = p->loc;

    if (maybe_tag_begin(p->cur)) {
        while (is_tag_char(p->cur)) {
            TRY(lex_next(p));
        }
    }
    if (p->tok_start == p->loc) {
        ERR(p, p->tok_start, "Expected a tag name.\n");
    }

    /* NUL terminate */
    *p->loc = 0;

    return tok;
}

static bool skip_space(
    parse_t *p)
{
    while (cp_xml_is_space(p->cur)) {
        TRY(lex_next(p));
    }
    return true;
}

static bool expect(
    parse_t *p,
    char const *s)
{
    cp_loc_t loc = p->loc;
    char const *s_orig = s;
    for (; *s; s++) {
        if (p->cur != (unsigned char)*s) {
            ERR(p, loc, "Expected '%s'", s_orig);
        }
        TRY(lex_next(p));
    }
    return true;
}

static unsigned digit_val(unsigned c)
{
    if ((c >= '0') && (c <= '9')) { return c - '0'; }
    if ((c >= 'a') && (c <= 'z')) { return c - 'a' + 10; }
    if ((c >= 'A') && (c <= 'Z')) { return c - 'A' + 10; }
    return 99;
}

static bool read_num(unsigned *np, parse_t *p, unsigned base)
{
    unsigned n = 0;
    for (;;) {
        unsigned val = digit_val(p->cur);
        if (val >= base) {
            break;
        }
        n = (n * base) + val;
        TRY(lex_next(p));
    }
    *np = n;
    return true;
}

static char *parse_text(
    parse_t *p,
    unsigned delim)
{
    char *cdata = p->loc;
    char *dst = cdata;
    for (;;) {
        /* read & decode */
        unsigned cur = p->cur;
        if ((cur == delim) || (cur == '<') || (cur == 0)) {
            break;
        }
        cp_loc_t loc = p->loc;
        if (cur == '&') {
            TRY(lex_next(p));
            switch (p->cur) {
            case 'q': TRY(expect(p, "quot")); cur = '"';  break;
            case 'l': TRY(expect(p, "lt"));   cur = '<';  break;
            case 'g': TRY(expect(p, "gt"));   cur = '>';  break;

            case 'a':
                TRY(lex_next(p));
                if (p->cur == 'm') {
                    TRY(expect(p, "mp")); cur = '&';
                }
                else {
                    TRY(expect(p, "pos")); cur = '\'';
                }
                break;

            case '#':
                TRY(lex_next(p));
                if ((p->cur == 'x') || (p->cur == 'X')) {
                    TRY(lex_next(p));
                    TRY(read_num(&cur, p, 16));
                }
                else {
                    TRY(read_num(&cur, p, 10));
                }
                break;

            default:
                ERR(p, loc, "Unrecognised XML entity.");
            }
            if (p->cur != ';') {
                ERR(p, p->loc, "Expected ';'");
            }
        }

        /* store char */
        if (!cp_utf8_ok(cur)) {
            ERR(p, loc, "UTF-8 encoding error.");
        }
        size_t len = cp_utf8_encode(dst, CP_MONUS(p->str_next, dst), cur);
        assert(len > 0);
        dst += len;

        /* advance input */
        TRY(lex_next(p));
    }

    /* terminate */
    *dst = 0;
    return cdata;
}

static bool parse_cdata(
    cp_xml_t **rp,
    parse_t *p,
    unsigned delim)
{
    char const *loc = p->loc;
    char *text = TRY(parse_text(p, delim));
    cp_xml_t *r = CP_POOL_NEW(p->tmp, *r);
    *rp = r;
    r->type = CP_XML_CDATA;
    r->loc = loc;
    r->data = text;
    return true;
}

static bool parse_scoped_name(
    char const **nspp,
    char **namep,
    parse_t *p)
{
    *namep = TRY(parse_tag(p));
    *nspp = NULL;
    if (p->cur == ':') {
        TRY(lex_next(p));
        *nspp = *namep;
        *namep = TRY(parse_tag(p));
    }
    return true;
}

static bool parse_attr(
    cp_xml_t **rp,
    parse_t *p)
{
    *rp = NULL;

    TRY(skip_space(p));

    /* stop at anything that looks like the end of an attribute list */
    switch (p->cur) {
    case 0: case '/': case '>': case '?':
        return true;
    }

    /* attribute prefix and name: */
    cp_loc_t loc = p->loc;
    char const *nsp = NULL;
    char *name = NULL;
    TRY(parse_scoped_name(&nsp, &name, p));

    /* set output */
    cp_xml_t *r = CP_POOL_NEW(p->tmp, *r);
    *rp = r;
    r->type = CP_XML_ATTR;
    r->loc = loc;
    r->data = name;
    r->ns_prefix = nsp;

    /* if no = follows, then we assume x="x" */
    if (p->cur != '=') {
        return true;
    }
    TRY(lex_next(p));

    /* expect " or ' */
    unsigned delim = p->cur;
    if ((delim != '"') && (delim != '\'')) {
        ERR(p, p->loc, "Expected \" or ' (double or single quotation mark).\n");
    }
    TRY(lex_next(p));

    /* parse the attribute value */
    TRY(parse_cdata(&r->child, p, delim));

    /* parse end quotation mark */
    if (p->cur != delim) {
        ERR(p, p->loc, "Expected %c (end quotation mark).\n", delim);
    }
    TRY(lex_next(p));

    return true;
}

static bool parse_attr_list(
    parse_t *p,
    ctxt_t *c)
{
    for (;;) {
        cp_xml_t *attr = NULL;
        TRY(parse_attr(&attr, p));
        if (attr == NULL) {
            return true;
        }
        assert(attr->next == NULL);

        if ((attr->ns_prefix != NULL) && strequ(attr->ns_prefix, "xmlns")) {
            attr->next = c->ns_map;
            c->ns_map = attr;
        }
        else
        if ((attr->ns_prefix == NULL) && strequ(attr->data, "xmlns")) {
            c->ns_default = attr->child->data;
        }
        else {
            PUSH(c->bottom, attr);
        }
    }
}

static bool skip_comment(
    parse_t *p)
{
    for (;;) {
        if ((p->cur == '-') && (*p->str_next == '-')) {
            TRY(expect(p, "-->"));
            return true;
        }
        TRY(lex_next(p));
    }
}

static bool parse_elem(
    cp_xml_t **rp,
    parse_t *p,
    ctxt_t *c);

/**
 * This does not handle </ tokens: they cause errors here.
 */
static bool parse_content(
    cp_xml_t **rp,
    parse_t *p,
    ctxt_t *c)
{
    *rp = NULL;

again:
    /* EOF is OK, leaves *rp==NULL */
    if (p->cur == 0) {
        return true;
    }

    if (p->cur == '<') {
        TRY(lex_next(p));
        /* comment */
        if  (p->cur == '!') {
            TRY(expect(p, "!--"));
            TRY(skip_comment(p));
            goto again;
        }

        /* element */
        return parse_elem(rp, p, c);
    }

    /* cdata */
    return parse_cdata(rp, p, '<');
}

/**
 * Parse a list of content.
 */
static bool parse_content_list(
    parse_t *p,
    ctxt_t *c,
    char const *ns_prefix,
    char const *tag)
{
    for (;;) {
        /* end tag */
        if ((tag != NULL) && (p->cur == '<') && (*p->str_next == '/')) {
            TRY(lex_next(p) && lex_next(p));
            if (ns_prefix != NULL) {
                TRY(expect(p, ns_prefix) && expect(p, ":"));
            }
            TRY(expect(p, tag) && skip_space(p) && expect(p, ">"));
            return true;
        }

        cp_xml_t *cont = NULL;
        TRY(parse_content(&cont, p, c));
        if (cont == NULL) {
            if (tag != NULL) {
                ERR(p, p->loc, "Unexpected end of file, expected end tag");
            }
            return true;
        }
        PUSH(c->bottom, cont);
    }
}

static bool find_ns(
    char const **nsp,
    parse_t *p,
    ctxt_t *c,
    char const *ns_prefix,
    bool is_elem)
{
    if (ns_prefix == NULL) {
        *nsp = is_elem ? c->ns_default : NULL;
        return true;
    }

    for (cp_xml_t *nm = c->ns_map; nm != NULL; nm = nm->next) {
        if (strequ(nm->data, ns_prefix)) {
            *nsp = nm->child->data;
            return true;
        }
    }

    ERR(p, ns_prefix, "Namespace prefix '%s' not found.\n", ns_prefix);
    return true;
}

static bool resolve_ns(
    parse_t *p,
    ctxt_t *c,
    cp_xml_t *r)
{
    TRY(find_ns(&r->ns, p, c, r->ns_prefix, true));
    for (cp_xml_t *k = r->child; k != NULL; k = k->next) {
        assert(k->type == CP_XML_ATTR);
        TRY(find_ns(&k->ns, p, c, k->ns_prefix, false));
    }
    return true;
}

/**
 * Parses an element, with the '<' already consumed. */
static bool parse_elem(
    cp_xml_t **rp,
    parse_t *p,
    ctxt_t *co)
{
    char const *loc = p->loc;
    char const *nsp = NULL;
    char *tag = NULL;
    TRY(parse_scoped_name(&nsp, &tag, p));

    cp_xml_t *r = CP_POOL_NEW(p->tmp, *r);
    *rp = r;
    r->type = CP_XML_ELEM;
    r->loc = loc;
    r->ns_prefix = nsp;
    r->data = tag;

    ctxt_t *c = &(ctxt_t){
        .prev = co,
        .bottom = &r->child,
        .ns_default = co->ns_default,
        .ns_map = co->ns_map,
    };
    TRY(parse_attr_list(p, c));
    TRY(resolve_ns(p, c, r));
    if (p->cur == '/') {
        return expect(p, "/>");
    }
    TRY(expect(p, ">"));
    TRY(parse_content_list(p, c, nsp, tag));
    if (p->opt & CP_XML_OPT_CHOMP) {
        cp_xml_chomp(r);
    }
    return true;
}

static bool parse_doc(
    parse_t *p,
    cp_xml_t *doc)
{
    if (p->cur == CP_UNICODE_BOM) {
        TRY(lex_next(p));
    }

    ctxt_t *c = &(ctxt_t){
        .bottom = &doc->child,
    };

    /* parse <?xml ...; also allow this to be missing. */
    if ((p->cur == '<') && (*p->str_next == '?')) {
        TRY(lex_next(p) && lex_next(p));
        char const *tag = TRY(parse_tag(p));
        if (!strequ(tag, "xml")) {
            ERR(p, tag, "Expected 'xml'.\n");
        }

        TRY(parse_attr_list(p, c));
        TRY(expect(p, "?>"));
    }
    TRY(skip_space(p));

    /* parse <!DOCTYPE ...>; allow and ignore this */
    if ((p->cur == '<') && strpref(p->str_next, "!DOCTYPE")) {
        TRY(expect(p, "<!DOCTYPE"));
        while (p->cur != '>') {
            if (p->cur == 0) {
                ERR(p, p->loc, "Premature end of file inside <!DOCTYPE...");
            }
            TRY(lex_next(p));
        }
        TRY(lex_next(p));
        TRY(skip_space(p));
    }

    /* parse like element body */
    cp_xml_t **bodyp = c->bottom;
    TRY(parse_content_list(p, c, NULL, NULL));
    cp_xml_chomp(doc);

    /* check structure */
    cp_xml_t *body = *bodyp;
    if (body == NULL) {
        ERR(p, p->loc, "Expected top-level element.");
    }
    if (body->type != CP_XML_ELEM) {
        ERR(p, body->loc, "Expected top-level element, found %s", cp_xml_type_stringify(body->type));
    }
    if (body->next != NULL) {
        ERR(p, body->next->loc, "Expected single top-level element.");
    }

    return true;
}

static void charpp_chomp(
    char **sp)
{
    char *s = *sp;

    /* chomp start */
    for (;;) {
        unsigned u = cp_utf8_take(&s);
        if (u == 0) {
            break;
        }
        if (!cp_xml_is_space(u)) {
            break;
        }

        /* update start of string */
        *sp = s;
    }

    /* chomp end */
    char *e = NULL;
    for (;;) {
        char *o = s;
        unsigned u = cp_utf8_take(&s);
        if (u == 0) {
            break;
        }
        if (!cp_xml_is_space(u)) {
            e = NULL;
        }
        else
        if (e == NULL) {
            e = o;
        }
    }
    if (e != NULL) {
        *e = 0;
    }
}

/**
 * Chomp all white space from CDATA of children, then remove all
 * empty CDATA.
 */
extern void cp_xml_chomp(
    cp_xml_t *r)
{
    assert(r != NULL);
    for (cp_xml_t **cp = &r->child; *cp != NULL; ) {
        cp_xml_t *c = *cp;

        if (c->type == CP_XML_CDATA) {
            charpp_chomp(&c->data);
            c->loc = c->data;
            if (*c->data == 0) {
                *cp = c->next;
                c->next = NULL;
                /* CP_POOL_FREE(c); */
                continue;
            }
        }
        cp = &((*cp)->next);
    }
}

/**
 * Parse an XML file into an XML syntax tree.
 */
extern bool cp_xml_parse(
    cp_xml_t **rp,
    cp_pool_t *tmp,
    cp_err_t *err,
    cp_syn_file_t *file,
    unsigned opt)
{
    assert(rp != NULL);
    assert(file != NULL);

    cp_xml_t *r = CP_POOL_NEW(tmp, *r);
    r->type = CP_XML_DOC;
    *rp = r;

    /* basic init */
    parse_t p = {
        .err = err,
        .tmp = tmp,
        .opt = opt,
    };
    TRY(start_file(&p, file));
    r->loc = p.loc;

    TRY(parse_doc(&p, r));

    return true;
}

/**
 * Find a node in a list based on different criteria.
 * This can be used to search a node's ->child list or
 * to continue search on a node with it's ->next list.
 */
extern cp_xml_t *cp_xml_find(
    /** The start of the list to search */
    cp_xml_t *n,

    /** The node type to match, or 0 to match any */
    cp_xml_type_t type,

    /** The data to match (using `strequ0()`), or CP_XML_ANY to match any */
    char const *data,

    /** The namespace to match (using `==`), or CP_XML_ANY to match any */
    char const *ns)
{
    for (; n != NULL; n = n->next) {
        if (((type == 0) || (n->type == type)) &&
            ((ns   == CP_XML_ANY) || (n->ns == ns)) &&
            ((data == CP_XML_ANY) || strequ0(n->data, data)))
        {
            return n;
        }
    }
    return NULL;
}

/**
 * Recursively set the given `new_ns` to each node that has `ns == old_ns`
 * or `strequ(ns, new_ns)`.
 *
 * For non-element nodes, if `old_ns == NULL`, then the namespace is not reset.
 *
 * If `old_ns` is NULL, this sets the default name space for elements.
 *
 * This can be used to make a given namespace token-identical (to `new_ns`).
 */
extern void cp_xml_set_ns(
    cp_xml_t *x,
    char const *old_ns,
    char const *new_ns)
{
    assert(new_ns != NULL);
    for (; x != NULL; x = x->next) {
        if (x->ns == NULL) {
            if ((x->type == CP_XML_ELEM) && (old_ns == NULL)) {
                /* set default namespace */
                x->ns = new_ns;
            }
        }
        else
        if ((x->ns == old_ns) || strequ(x->ns, new_ns)) {
            /* make token-identical */
            x->ns = new_ns;
        }
        cp_xml_set_ns(x->child, old_ns, new_ns);
    }
}

/**
 * Skip the attribute nodes (which are first in a child list) and return
 * a pointer to the first non-attr pointer (or to NULL).
 */
extern cp_xml_t **cp_xml_skip_attr_p(
    cp_xml_t **xp)
{
    assert(xp != NULL);
    for (; *xp && ((*xp)->type == CP_XML_ATTR); xp = &(*xp)->next);
    return xp;
}
