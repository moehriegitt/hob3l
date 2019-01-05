/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3l/syn-msg.h>
#include <hob3l/syn.h>

static int cmp_line(
    char const *key, char const *const *elem, char const **_end CP_UNUSED)
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

static bool is_printable(
    char c)
{
    unsigned u = (unsigned char)c;
    return (u == '\n') || (u == '\t') || (u == '\r') ||
           ((u >= 32) && (u <= 126));
}

static bool format_source_line(
    cp_vchar_t *out,
    size_t *pos,
    char const *loc,
    char const *src,
    size_t len)
{
    cp_vchar_push(out, ' ');
    size_t old = out->size;
    bool need_cr = true;
    size_t x = 0;
    for (char const *i = src, *e = i + len; i != e; i++) {
        if (i == loc) {
            *pos = x;
        }
        if (*i == '\t') {
            size_t new_x = (x + 4) & -4U;
            while (x < new_x) {
                cp_vchar_push(out, ' ');
                x++;
            }
        }
        else if (!is_printable(*i)) {
            if (old != out->size) {
                cp_vchar_printf(out, "[...binary...]");
            }
            break;
        }
        else {
            cp_vchar_push(out, *i);
            x++;
        }
        if (*i == '\n') {
            need_cr = false;
        }
    }

    if (old == out->size) {
        out->size = old;
        return false;
    }

    if (need_cr) {
        cp_vchar_push(out, '\n');
    }
    return true;
}

static void cp_syn_get_loc_src_aux(
    cp_vchar_t *pre,
    cp_vchar_t *post,
    cp_syn_input_t *tree,
    cp_loc_t token,
    char const *msg)
{
    cp_syn_loc_t loc;
    if (!cp_syn_get_loc(&loc, tree, token)) {
        return;
    }

    /* format source text */
    size_t pos = CP_SIZE_MAX;
    bool ok = format_source_line(
        post,
        &pos,
        loc.orig + CP_PTRDIFF(token, loc.copy),
        loc.orig,
        CP_PTRDIFF(loc.orig_end, loc.orig));

    cp_vchar_printf(pre, "%s:%"CP_Z"u:", cp_vchar_cstr(&loc.file->filename), loc.line+1);
    if (pos != CP_SIZE_MAX) {
        cp_vchar_printf(pre, "%"CP_Z"u:", pos+1);
    }
    cp_vchar_printf(pre, " %s", msg);

    /* format pointer */
    if ((pos != CP_SIZE_MAX) && ok) {
        cp_vchar_printf(post, " %*s^\n", (int)pos, "");
    }
}

/* ********************************************************************** */

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
    cp_syn_input_t *tree,
    cp_loc_t token)
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
            (token <= f->content.data + f->content.size))
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

/**
 * Additional to cp_syn_get_loc, also get the source line citation
 *
 * This ensures that pre and post can be dereferenced as a C string.
 */
extern void cp_syn_format_loc(
    cp_vchar_t *pre,
    cp_vchar_t *post,
    cp_syn_input_t *tree,
    cp_loc_t token,
    cp_loc_t token2)
{
    cp_vchar_init(pre);
    (void)cp_vchar_cstr(pre);

    cp_vchar_init(post);
    (void)cp_vchar_cstr(post);

    cp_syn_get_loc_src_aux(pre, post, tree, token, "");
    if ((token2 != NULL) && (token2 != token)) {
        cp_vchar_t post2;
        cp_vchar_init(&post2);
        cp_syn_get_loc_src_aux(post, &post2, tree, token2, "Info: See also here.\n");
        cp_vchar_append(post, &post2);
        cp_vchar_fini(&post2);
    }
}
/**
 * Raise an error message with a location.
 *
 * This takes a parameter \p ign to decide whether this will become
 * an error, a warning, or whether the error will be ignored.  In
 * case of an error, this returns true, otherwise false.
 *
 * va_list version of cp_syn_msg().
 */
CP_VPRINTF(6)
extern bool cp_syn_vmsg(
    cp_syn_input_t *syn,
    cp_err_t *e,
    unsigned ign,
    cp_loc_t loc,
    cp_loc_t loc2,
    char const *msg, va_list va)
{
    switch (ign) {
    case CP_ERR_WARN:{
        cp_vchar_t pre, post;
        cp_syn_format_loc(&pre, &post, syn, loc, loc2);
        fprintf(stderr, "%sWarning: ", cp_vchar_cstr(&pre));
        vfprintf(stderr, msg, va);
        fprintf(stderr, " Ignoring.\n%s", cp_vchar_cstr(&post));
        cp_vchar_init(&pre);
        cp_vchar_init(&post);
        return true;}

    case CP_ERR_IGNORE:
        return true;

    default:
    case CP_ERR_FAIL:
        cp_vchar_vprintf(&e->msg, msg, va);
        if (loc != NULL) {
            e->loc = loc;
        }
        if (loc2 != NULL) {
            e->loc2 = loc2;
        }
        return false;
    }
}

/**
 * Raise an error message with a location.
 *
 * This takes a parameter \p ign to decide whether this will become
 * an error, a warning, or whether the error will be ignored.  In
 * case of an error, this returns true, otherwise false.
 *
 * '...' version of cp_syn_vmsg().
 */
CP_PRINTF(6,7)
extern bool cp_syn_msg(
    cp_syn_input_t *syn,
    cp_err_t *e,
    unsigned ign,
    cp_loc_t loc,
    cp_loc_t loc2,
    char const *msg, ...)
{
    va_list va;
    va_start(va, msg);
    bool r = cp_syn_vmsg(syn, e, ign, loc, loc2, msg, va);
    va_end(va);
    return r;
}
