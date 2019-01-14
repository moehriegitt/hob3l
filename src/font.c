/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <ctype.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/mat.h>
#include <hob3l/font.h>
#include <hob3l/csg2.h>

#define ZWNJ    0x200C
#define ZWJ     0x200D
#define ZWSP    0x200B
#define ZWNBSP  0xFEFF

#define G_OTHER  0
#define G_BASE   1
#define G_EXTEND 2

#define PROF_MAX 0x0fffffff

#define INTERVAL

typedef struct {
    unsigned lo, hi;
} interval_t;

static interval_t uni_def_ign[] = {
#include "unidefign.inc"
};

typedef struct {
    unsigned lo, hi;
    unsigned value;
} interval_plus_t;

static interval_plus_t uni_grapheme[] = {
#include "unigrapheme.inc"
};

/**
 * A code point sequence reader.
 *
 * When combining and handling ligatures, reading the input stream
 * of characters is a bit complex, so we'll encapsulate this into
 * a data structure to make life a bit easiert. */
typedef struct {
    unsigned data[32];
    size_t size;
    bool eot;
    unsigned (*next)(void *user);
    void *user;
    cp_font_t const *font;
} seq_t;

/**
 * A glyph that is rendered by the main algorithm.  Currently,
 * this can do algorithmically base glyph + above + below.
 * Anything else must be done by the font's combination mechanisms.
 * If seq_take fails, base will be 0.
 */
typedef struct {
    union {
        struct {
            unsigned base;
            unsigned above;
            unsigned below;
        };
        unsigned code[3]; /* indexed by CP_FONT_CT_* constant */
    };
    union {
        struct {
            unsigned _unused;
            unsigned above_high;
            unsigned _below_low; /* currently not used */
        };
        unsigned alt_code[3]; /* indexed by CP_FONT_CT_* constant */
    };
} glyph_t;

/**
 * Metric of a glyph in one axis.
 * This returns: the size of the middle section, the width of
 * the left section, the size of the left section.
 */
typedef struct {
    int middle;
    int width[2];
} metric_t;

static int cmp_glyph(
    unsigned const *glyph_id,
    cp_font_glyph_t const *glyph,
    void *user CP_UNUSED)
{
    unsigned a = *glyph_id;
    unsigned b = glyph->id;
    return a < b ? -1 : a > b ? +1 : 0;
}

static int cmp_map1(
    unsigned const *a,
    cp_font_map_t const *b,
    void *user CP_UNUSED)
{
    if (*a != b->first) {
        return *a < b->first ? -1 : +1;
    }
    return 0;
}

static cp_font_map_t *map1_lookup(
    cp_v_font_map_t const *map,
    unsigned first)
{
    size_t i = cp_v_bsearch(&first, map, cmp_map1, NULL);
    return cp_v_nth_ptr0(map, i);
}

static bool map1_lookup1(
    cp_v_font_map_t const *map,
    unsigned *result,
    unsigned first)
{
    cp_font_map_t *m = map1_lookup(map, first);
    if (m == NULL) {
        return false;
    }
    *result = m->result;
    return true;
}

static int cmp_map2(
    unsigned const (*a)[2],
    cp_font_map_t const *b,
    void *user CP_UNUSED)
{
    if ((*a)[0] != b->first) {
        return (*a)[0] < b->first ? -1 : +1;
    }
    if ((*a)[1] != b->second) {
        return (*a)[1] < b->second ? -1 : +1;
    }
    return 0;
}

static cp_font_map_t *map2_lookup(
    cp_v_font_map_t const *map,
    unsigned first,
    unsigned second)
{
    unsigned key[2] = { first, second };
    size_t i = cp_v_bsearch(&key, map, cmp_map2, NULL);
    return cp_v_nth_ptr0(map, i);
}

static bool map2_lookup1(
    cp_v_font_map_t const *map,
    unsigned *result,
    unsigned first,
    unsigned second)
{
    cp_font_map_t *m = map2_lookup(map, first, second);
    if (m == NULL) {
        return false;
    }
    *result = m->result;
    return true;
}

/**
 * This returns whether the second glyph is merged into the first
 * (i.e., is ligated).
 * It does not return whether anything was replaced: this can only
 * be found indirectly by comparing *first before and after the call.
 * This is because this function can also be used to find replacement
 * glyphs based on post-context, in which case only the first glyph
 * changes, but is not merged into the second.
 */
static bool mof_lookup(
    cp_v_font_map_t const *map,
    unsigned disabled_if,
    unsigned *first,
    unsigned second)
{
    cp_font_map_t *m = map2_lookup(map, *first, second);
    if (m == NULL) {
        return false;
    }
    if (((1U << (m->flags & CP_FONT_MOF_TYPE_MASK)) & disabled_if) != 0) {
        return false;
    }
    *first = m->result;
    return (m->flags & CP_FONT_MOF_KEEP_SECOND) == 0;
}

static void seq_init(
    seq_t *seq,
    cp_font_t const *font,
    unsigned (*next)(void *user),
    void *user)
{
    seq->size = 0;
    seq->eot = false;
    seq->next = next;
    seq->user = user;
    seq->font = font;
}

static void seq_append(
    seq_t *seq,
    unsigned cp)
{
    cp_font_map_t *m = map1_lookup(&seq->font->decompose, cp);
    if (m != NULL) {
        seq_append(seq, m->result);
        if (m->second != 0) {
            seq_append(seq, m->second);
        }
        return;
    }
    assert(seq->size < cp_countof(seq->data));
    seq->data[seq->size] = cp;
    seq->size++;
}

static unsigned seq_peek(
    seq_t *seq,
    size_t pos)
{
    while (pos >= seq->size) {
        if (seq->eot || (pos >= cp_countof(seq->data))) {
            return 0;
        }
        unsigned n = seq->next(seq->user);
        if (n == 0) {
            seq->eot = true;
            return 0;
        }
        seq_append(seq, n);
    }
    return seq->data[pos];
}

static void seq_poke(
    seq_t *seq,
    size_t pos,
    unsigned value)
{
    assert((pos < seq->size) && "Cannot poke what wasn't peeked");
    seq->data[pos] = value;
}

static void seq_remove(
    seq_t *seq,
    size_t pos,
    size_t count)
{
    if (count == 0) {
        return;
    }
    assert(((pos + count) <= seq->size) && "Cannot remove what wasn't peeked");
    assert(seq->size >= count);
    seq->size -= count;
    memcpy(&seq->data[pos], &seq->data[pos + count], sizeof(seq->data[0]) * (seq->size - pos));
}

static int interval_cmp(void const *_a, void const *_b)
{
    unsigned const *a = _a;
    interval_t const *b = _b;
    if (*a < b->lo) {
        return -1;
    }
    if (*a > b->hi) {
        return +1;
    }
    return 0;
}

static bool cp_default_ignorable(unsigned x)
{
    return bsearch(&x, uni_def_ign,
        cp_countof(uni_def_ign), sizeof(uni_def_ign[0]),
        interval_cmp) != NULL;
}

static unsigned cp_grapheme(unsigned x)
{
    interval_plus_t *p = bsearch(&x, uni_grapheme,
        cp_countof(uni_grapheme), sizeof(uni_grapheme[0]),
        interval_cmp);
    if (p == NULL) {
        return G_BASE;
    }
    return p->value;
}

static void cp_font_draw_path(
    cp_csg2_poly_t *poly,
    cp_font_gc_t *gc,
    size_t i)
{
    cp_csg2_path_t *path = cp_v_push0(&poly->path);
    for(;;) {
        cp_font_xy_t const *c = cp_v_nth_ptr(&gc->font->coord, i);
        if (c->x == CP_FONT_X_SPECIAL) {
            if (c->y == CP_FONT_Y_END) {
                return;
            }
            CP_DIE();
        }
        i++;
        cp_v_push(&path->point_idx, poly->point.size);
        cp_vec2_loc_t *pt = cp_v_push0(&poly->point);
        pt->coord.x = (c->x * gc->scale_x) + gc->state.cur_x;
        pt->coord.y = (c->y * gc->scale_y) - gc->base_y;
    }
}

static void cp_font_draw_poly_or(
    cp_v_obj_p_t *out,
    cp_font_gc_t *gc,
    cp_font_path_t const *path,
    size_t count)
{
    for (cp_size_each(i, count)) {
        cp_csg2_poly_t *poly = cp_csg2_new(*poly, gc->loc);
        cp_v_push(out, cp_csg2_cast(cp_obj_t, poly));
        cp_font_draw_path(poly, gc, path->data[i]);
    }
}

static void cp_font_draw_poly_xor(
    cp_v_obj_p_t *out,
    cp_font_gc_t *gc,
    cp_font_path_t const *path,
    size_t count)
{
    if (count == 0) {
        return;
    }
    cp_csg2_poly_t *poly = cp_csg2_new(*poly, gc->loc);
    cp_v_push(out, cp_csg2_cast(cp_obj_t, poly));
    for (cp_size_each(i, count)) {
        cp_font_draw_path(poly, gc, path->data[i]);
    }
}

static void cp_font_draw_poly(
    cp_v_obj_p_t *out,
    cp_font_gc_t *gc,
    cp_font_path_t const *path,
    size_t count)
{
    gc->state.cur_x -= path->border_x.side[gc->right2left] * gc->scale_x;
    if (gc->font->flags & CP_FONT_FF_XOR) {
        cp_font_draw_poly_xor(out, gc, path, count);
    }
    else {
        cp_font_draw_poly_or(out, gc, path, count);
    }
    gc->state.cur_x += path->border_x.side[!gc->right2left] * gc->scale_x;
}

static int subglyph_kern(
    cp_font_t const *font,
    cp_font_subglyph_t const *sg)
{
    return (int)lrint(
         (sg->kern_sub ? -1 : +1) *
         (sg->kern_em / cp_f(CP_FONT_KERN_EM_MASK)) *
         font->em_x);
}

static void render_glyph_one(
    cp_v_obj_p_t *out,
    cp_font_gc_t *gc,
    size_t glyph_idx)
{
    cp_font_glyph_t const *glyph = cp_v_nth_ptr0(&gc->font->glyph, glyph_idx);
    if (glyph == NULL) {
        return;
    }

    if ((glyph->flags & CP_FONT_GF_SEQUENCE) == 0) {
        cp_font_draw_poly(out, gc,
            (cp_font_path_t const *)cp_v_nth_ptr(&gc->font->path, glyph->first),
            glyph->second);
        return;
    }

    cp_font_subglyph_t const *sg =
        (cp_font_subglyph_t const *)cp_v_nth_ptr(&gc->font->path, glyph->first);
    double sx = gc->right2left ? -gc->scale_x : +gc->scale_x;
    for (cp_size_each(i, glyph->second)) {
         gc->state.cur_x += subglyph_kern(gc->font, sg) * sx;
         render_glyph_one(out, gc, sg->glyph);
         sg++;
    }
}

static bool valid_glyph_idx(cp_font_t const *font, size_t i)
{
    return (i < font->glyph.size);
}

static size_t find_glyph(
    cp_font_gc_t *gc,
    unsigned glyph_id)
{
    if (glyph_id == 0) {
        return CP_SIZE_MAX;
    }
    size_t i = cp_v_bsearch(&glyph_id, &gc->font->glyph, cmp_glyph, NULL);
    if (!valid_glyph_idx(gc->font, i)) {
        i = gc->replacement_idx;
    }
    return i;
}

static cp_font_path_t const *get_path(
    cp_font_t const *font,
    cp_font_glyph_t const *glyph)
{
    assert((glyph->flags & CP_FONT_GF_SEQUENCE) == 0);
    cp_font_path_t const *p = (cp_font_path_t const *)cp_v_nth_ptr(&font->path, glyph->first);
    return p;
}

static void get_metric_x_aux(
    metric_t *m,
    cp_font_t const *font,
    size_t glyph_idx)
{
    cp_font_glyph_t const *glyph = cp_v_nth_ptr0(&font->glyph, glyph_idx);
    if (glyph == NULL) {
        m->middle = m->width[0] = m->width[1] = 0;
        return;
    }

    if ((glyph->flags & CP_FONT_GF_SEQUENCE) == 0) {
        cp_font_path_t const *p = get_path(font, glyph);
        m->middle = 0;
        m->width[0] = font->center_x - p->border_x.side[0];
        m->width[1] = p->border_x.side[1] - font->center_x;
        return;
    }

    assert(glyph->second >= 1);
    cp_font_subglyph_t const *sg =
        (cp_font_subglyph_t const *)cp_v_nth_ptr(&font->path, glyph->first);


    metric_t m1;
    get_metric_x_aux(&m1, font, sg->glyph);
    m1.width[0] -= subglyph_kern(font, sg);

    for (cp_size_each(i, glyph->second, 1)) {
        sg++;

        metric_t m2;
        get_metric_x_aux(&m2, font, sg->glyph);

        m->width[0] = m1.width[0];
        m->width[1] = m2.width[1];
        m->middle = m1.middle + m1.width[1] + m2.width[0] + m2.middle;

        m->middle += subglyph_kern(font, sg);

        m1 = *m;
    }
}

static void get_metric_x(
    metric_t *m,
    cp_font_t const *font,
    size_t glyph_idx)
{
    get_metric_x_aux(m, font, glyph_idx);
    /* recompute to get m->middle = 0 */
    m->width[0] += m->middle / 2;
    m->width[1] += (m->middle + 1) / 2;
    m->middle = 0;
}

static int int_max3(int a, int b, int c)
{
    if (b > a) { a = b; }
    if (c > a) { a = c; }
    return a;
}

static cp_font_glyph_t const *left_glyph(
    cp_font_t const *font,
    cp_font_glyph_t const *g)
{
    if ((g->flags & CP_FONT_GF_SEQUENCE) == 0) {
        return g;
    }

    assert(g->second >= 1);
    cp_font_subglyph_t const *sg =
        (cp_font_subglyph_t const *)cp_v_nth_ptr(&font->path, g->first);

    return left_glyph(font, cp_v_nth_ptr(&font->glyph, sg->glyph));
}

static cp_font_glyph_t const *right_glyph(
    cp_font_t const *font,
    cp_font_glyph_t const *g)
{
    if ((g->flags & CP_FONT_GF_SEQUENCE) == 0) {
        return g;
    }
    assert(g->second >= 1);
    size_t idx = g->first;
    idx += g->second;
    cp_font_subglyph_t const *sg =
        (cp_font_subglyph_t const *)cp_v_nth_ptr(&font->path, idx - 1);

    return right_glyph(font, cp_v_nth_ptr(&font->glyph, sg->glyph));
}

static void get_prof(
    cp_font_half_profile_t prof[],
    cp_font_t const *font,
    size_t glyph_idx,
    int add0,
    int add1)
{
    assert(add0 >= 0);
    assert(add1 >= 0);
    cp_font_glyph_t const *root = cp_v_nth_ptr0(&font->glyph, glyph_idx);
    if (root == NULL) {
        return;
    }

    cp_font_glyph_t const *g[2];
    g[0] = left_glyph(font, root);
    g[1] = right_glyph(font, root);

    cp_font_path_t const *p[2];
    p[0] = get_path(font, g[0]);
    p[1] = get_path(font, g[1]);

    int w0 = p[0]->border_x.left + p[0]->border_x.right;
    int w1 = p[1]->border_x.left + p[1]->border_x.right;

    for (cp_arr_each(i, prof->x)) {
        int d0 = add0 + font->space_x[CP_FONT_PROFILE_GET_LO(p[0]->profile.x[i])];
        if (d0 > w0) {
            d0 = w0;
        }
        if (prof[0].x[i] > d0) {
            prof[0].x[i] = d0;
        }

        int d1 = add1 + font->space_x[CP_FONT_PROFILE_GET_HI(p[1]->profile.x[i])];
        if (d1 > w1) {
            d1 = w1;
        }
        if (prof[1].x[i] > d1) {
            prof[1].x[i] = d1;
        }
    }
}

static void prof_dist_min(int *m, int u)
{
    if (*m > u) { *m = u; }
}

static int prof_dist(
    cp_font_half_profile_t *a,
    cp_font_half_profile_t *b)
{
    int m = PROF_MAX;
    prof_dist_min(&m, a->x[0] + b->x[0]);
    for (cp_arr_each(i, a->x, 1)) {
        prof_dist_min(&m, a->x[i]   + b->x[i]);
        prof_dist_min(&m, a->x[i]   + b->x[i-1]);
        prof_dist_min(&m, a->x[i-1] + b->x[i]);
    }
    return m;
}

static void render_glyph_comb(
    cp_v_obj_p_t *out,
    cp_font_gc_t *gc,
    glyph_t *g)
{
    cp_font_t const *font = gc->font;

    /* possibly replace base glyph */
    unsigned have = 0;
    if (g->above != 0) {
        have |= CP_FONT_MAS_HAVE_ABOVE;
    }
    if (g->below != 0) {
        have |= CP_FONT_MAS_HAVE_BELOW;
    }
    if (have != 0) {
        (void)map2_lookup1(&font->base_repl, &g->base, g->base, have);
    }

    /* base glyph */
    size_t base_idx  = find_glyph(gc, g->base);

    /* possibly replace above glyph */
    cp_font_glyph_t const *base = cp_v_nth_ptr0(&font->glyph, base_idx);
    if (g->above != 0) {
        if ((base != NULL) && (base->flags & CP_FONT_GF_TALL)) {
            g->above = g->above_high;
        }
    }

    /* look up other glyph indices */
    size_t above_idx = find_glyph(gc, g->above);
    size_t below_idx = find_glyph(gc, g->below);

    /* render the three parts */
    metric_t m_base, m_above, m_below;
    get_metric_x(&m_base,  gc->font, base_idx);
    get_metric_x(&m_above, gc->font, above_idx);
    get_metric_x(&m_below, gc->font, below_idx);

    metric_t m;
    m.width[0] = int_max3(m_base.width[0], m_above.width[0], m_below.width[0]);
    m.width[1] = int_max3(m_base.width[1], m_above.width[1], m_below.width[1]);

    unsigned le = gc->right2left;

    /* auto-kerning */
    cp_font_half_profile_t prof[2];
    for (cp_arr_each(i, prof[0].x)) {
         prof[0].x[i] = prof[1].x[i] = PROF_MAX;
    }
    get_prof(prof, font, base_idx,  m.width[0] - m_base.width[0],  m.width[1] - m_base.width[1]);
    get_prof(prof, font, above_idx, m.width[0] - m_above.width[0], m.width[1] - m_above.width[1]);
    get_prof(prof, font, below_idx, m.width[0] - m_below.width[0], m.width[1] - m_below.width[1]);

    int kern = 0;
    bool this_prof_valid = (base->flags & CP_FONT_GF_MONO) == 0;
    if (gc->state.last_prof_valid && this_prof_valid) {
        kern = prof_dist(&gc->state.last_prof, &prof[le]);
        prof_dist_min(&kern, m.width[0] + m.width[1]);
        prof_dist_min(&kern, gc->state.last_width[0] + gc->state.last_width[1]);
        prof_dist_min(&kern, gc->state.last_width[!le] + m.width[le]);
    }
    assert(kern >= 0);
    gc->state.last_prof_valid = this_prof_valid;
    gc->state.last_prof = prof[!le];
    gc->state.last_width[0] = m.width[0];
    gc->state.last_width[1] = m.width[1];

    /* rendering */
    double sx = le ? -gc->scale_x : +gc->scale_x;
    double cx = gc->state.cur_x - (kern * sx);

    gc->state.cur_x = cx + (m.width[le] - m_base.width[le]) * sx;
    render_glyph_one(out, gc, base_idx);

    gc->state.cur_x = cx + (m.width[le] - m_above.width[le]) * sx;
    render_glyph_one(out, gc, above_idx);

    gc->state.cur_x = cx + (m.width[le] - m_below.width[le]) * sx;
    render_glyph_one(out, gc, below_idx);

    gc->state.cur_x = cx + (m.width[0] + m.width[1]) * sx;
}

static bool is_simple(glyph_t const *c)
{
    assert(c->base != 0);
    return (c->above == 0) && (c->below == 0);
}

/**
 * Render a single glyph plus above glyph plus below glyph
 *
 * gc->state.glyph_cnt is incremented exactly each time tracking
 * is inserted.
 */
static void cp_font_render_glyph(
    cp_v_obj_p_t *out,
    cp_font_gc_t *gc,
    glyph_t *g)
{
    assert(g->base != 0);

    /* language specific base glyph replacement */
    if (gc->lang != NULL) {
        (void)map1_lookup1(&gc->lang->one2one, &g->base, g->base);
    }

    /* kerning and contextual forms of base glyph */
    cp_font_map_t const *m = map2_lookup(&gc->font->context, g->base, gc->state.last_simple_cp);
    if (m != NULL) {
        if (m->flags & CP_FONT_MXF_KERNING) {
            /* only applied for simple glyph, because we don't know whether kerning
             * will work with combining characters */
            if (is_simple(g)) {
                unsigned bit_cnt = (sizeof(int) * 8) - CP_FONT_ID_WIDTH;
                int kern = ((int)(m->result << bit_cnt)) >> bit_cnt; /* sign-extend */
                gc->state.cur_x += kern * gc->scale_x;
            }
        }
        else {
            g->base = m->result;
        }
    }

    /* remember old position to infer spacing amount */
    double old_x = gc->state.cur_x;

    /* render */
    render_glyph_comb(out, gc, g);

    /* update state */
    gc->state.last_simple_cp = 0;
    if (is_simple(g)) {
        gc->state.last_simple_cp = g->base;
    }
    gc->state.cur_x += (gc->state.cur_x - old_x) * gc->spacing;
    gc->state.cur_x += (gc->right2left ? -1 : +1) * gc->tracking;
    gc->state.glyph_cnt++;
}

/**
 * This does all the combining that is possible with the current font and
 * returns the base, the above, and the below combining characters.  The
 * sequence contains what will be printed next.  Combining characters
 * that cannot be combined into the sequence will be kept to be reiterated
 * as a separate glyph later.
 *
 * This returns true iff the sequence was non-empty and a glyph was removed
 * and can be rendered.
 *
 * This does not do language specific stuff and no ligatures, joining, optional
 * replacement, because those apply to two glyphs extracted using this function.
 *
 * E.g., in case the font has a glyph [breve + acute] and [u + horn], then this
 * should handle correctly a sequence like:
 *
 *    u, breve, ZWNJ, dot_below, acute, diaeresis, ZWJ, grave, horn, ZWJ, X, ...
 *    1  5      3,5   5          8      6          3,7  7      8     3,4
 *
 * The numbers below the sequence indicate in which step in the algorithm removes
 * the character from the sequence, or which one keeps it explicitly.
 *
 * The sequence of combining characters may be in any order (provided same type
 * combining chars (breve+acute) are kept in order), and with any intervening
 * ZWJ and ZWNJ characters (which are tries to be combined, but are removed
 * inside the sequence if they fail to combine but a subsequent combination
 * succeeds).  This should then return:
 *
 *    g->base  = [u + horn]
 *    g->above = [breve + acute]
 *    g->below = [dot_below]
 *
 * And seq should start:
 *
 *    diaeresis, grave, ZWJ, X, ...
 */
static void seq_take(
    glyph_t *g,
    cp_font_t const *font,
    seq_t *seq)
{
    /* base character */
    g->base = seq_peek(seq, 0);
    g->above = 0;
    g->below = 0;
    if (g->base == 0) {
        return;
    }
    seq_remove(seq, 0, 1); /*1*/

    /* only continue if the character we read is a base character */
    if (cp_grapheme(g->base) != G_BASE) {
        return;
    }

    /* try to find above, below, and combine stuff */
    size_t n = 0;
    size_t i = 0;
    bool complete[3] = {0}; /* indexed by CP_FONT_CT_* */
    for (;;) {
        cp_font_map_t const *comb;
        unsigned comb_type;
        unsigned next = seq_peek(seq, i);

        /* end of text */
        if (next == 0) {
            return;
        }

        /* Try to combine ZWJ/ZWNJ with base glyph.  If it works, remove,
         * otherwise, skip and read on. */
        if ((next == ZWJ) || (next == ZWNJ)) {
            if (map2_lookup1(&font->compose, &g->base, g->base, next)) {
                /* if ZWJ/ZWNJ combines with base, then remove the sequence and continue */
                goto next_comb;
            }
            /* skip ZWJ/ZWNJ for now and search more combining chars behind it */
            i++;
            continue;
        }

        /* If we find anything but a combining character, we stop */
        if (cp_grapheme(next) != G_EXTEND) {
            return;
        }

        /* Get combining type: if we fail here, we're done. */
        comb_type = 0;
        comb = map1_lookup(&font->comb_type, next);
        if (comb != NULL) {
            comb_type = comb->result;
        }
        assert(comb_type < cp_countof(complete));

        /* this type of combining character's combining is done => keep and continue */
        if (complete[comb_type]) {
            goto keep_this;
        }

        if (comb_type == 0) {
            /* try to combine with base character */
            if (map2_lookup1(&font->compose, &g->base, g->base, next)) {
                goto next_comb;
            }
        }
        else {
            /* is it the first one we want to keep? */
            if (g->code[comb_type] == 0) {
                /* try to combine with base char */
                if (map2_lookup1(&font->compose, &g->base, g->base, next)) {
                    goto next_comb;
                }

                /* otherwise, store */
                g->code[comb_type] = next;
                g->alt_code[comb_type] = comb->second;
                goto next_comb;
            }

            /* try to combine with previous modifier of same type */
            if (map2_lookup1(&font->compose, &g->code[comb_type], g->code[comb_type], next))
            {
                /* look up alternative glyph */
                g->alt_code[comb_type] = g->code[comb_type];
                comb = map1_lookup(&font->comb_type, g->code[comb_type]);
                if (comb != NULL) {
                    g->alt_code[comb_type] = comb->second;
                }
                goto next_comb;
            }
        }

        /* no more combining for this type, as this would mess up the order */
        complete[comb_type] = true;

    keep_this:
        seq_poke(seq, n, next);
        n++;

    next_comb:
        /* consume all combining characters from n..i and go on to next char */
        seq_remove(seq, n, i - n + 1);
        i = n;
    }
}

static void clear_kerning(cp_font_state_t *state)
{
    state->last_simple_cp = 0;
    state->last_prof_valid = false;
    state->last_width[0] = 0;
    state->last_width[1] = 0;
    CP_ZERO(&state->last_prof);
}

/* ********************************************************************** */

/**
 * Set the font in the gc.
 *
 * This resets the font, em, ratio_x, scale_x/y, base_y,
 * replacement, tracking, spacing, and kerning state.
 *
 * Font scaling is set up so that 1em scales as \p size.
 * If the output devices uses a pt scale, then size=12
 * generates a 12pt font.
 *
 * \p ratio_x can be used to set a different scaling in
 * x direction.
 *
 * Vertical alignment will be set to baseline alignment.
 * This can be changed to top or bottom alignment by
 * reassigning gc->base_y to e.g. font->top_y * gc->scale_y.
 * for top-alignment (and accordingly, font->bottom_y can be
 * used.  This is then the font-global vertical alignment.
 * For actual glyph size alignment, gc->base_y should be set to
 * 0 before rendering, then the min/max of the glyph coordinates
 * should be computed and considered in the final vertical
 * positioning of the rendered text.
 */
extern void cp_font_gc_set_font(
    cp_font_gc_t *gc,
    cp_font_t const *font,
    double pt_size,
    double ratio_x)
{
    if (gc->font == font) {
        return;
    }
    gc->font = font;
    gc->em = pt_size;
    gc->ratio_x = ratio_x;
    gc->spacing = 0;
    gc->tracking = 0;
    gc->scale_x = (pt_size / font->em_x) * ratio_x;
    gc->scale_y = pt_size / font->em_y;
    gc->base_y = font->base_y * gc->scale_y;
    gc->replacement_idx = CP_SIZE_MAX; /* read by find_glyph => invalidate */
    gc->replacement_idx = find_glyph(gc, 0xFFFD);

    /* do not try to do contextual replacement or kerning across different fonts */
    clear_kerning(&gc->state);
}

static int cmp_lang_name(
    char const *name,
    cp_font_lang_map_t const *lang,
    void *user CP_UNUSED)
{
    size_t n = sizeof(lang->id);
    for (size_t i = 0; i < n; i++) {
        unsigned a = (unsigned char)toupper((unsigned char)name[i]);
        unsigned b = (unsigned char)toupper((unsigned char)lang->id[i]);
        if (a != b) {
            return a < b ? -1 : +1;
        }
        if (a == 0) {
            return 0;
        }
    }
    return name[n] == 0 ? 0 : +1;
}

/**
 * Set a given language.
 *
 * name=NULL resets.  Also, if a language is not found, this resets
 * the name setting, which can be seen by checking gc->lang, which
 * will be NULL if the language is reset, or non-NULL if one was found
 * and selected.
 */
extern void cp_font_gc_set_lang(
    cp_font_gc_t *gc,
    char const *name)
{
    gc->lang = NULL;
    if (name == NULL) {
        return;
    }
    size_t i = cp_v_bsearch(name, &gc->font->lang_map, cmp_lang_name, NULL);
    cp_font_lang_map_t const *m = cp_v_nth_ptr0(&gc->font->lang_map, i);
    if (m != NULL) {
        gc->lang = cp_v_nth_ptr0(&gc->font->lang, m->lang_idx);
    }
}

/**
 * Renders a string into a set of polygons.
 *
 * The rendered polygons will be added to \p out.
 *
 * This appends to out.  No other operation is done on \p out, i.e.,
 * no deeper structures are modified.
 *
 * This updates gc->state.
 *
 * In the following, text in [] lists what is planned, but not yet
 * implemented.
 *
 * [This handles kerning, which is applied before rendering a glyph
 * based on gc->state.last_cp.  This includes handling zero-width space
 * to inhibit kerning.]
 *
 * [This handles right2left glyph replacement (e.g. to swap parentheses).]
 *
 * This handles canonical, ligature, joining, and optional composition
 * of glyphs, including ZWJ, ZWNJ, ZWSP to break/combine glyphs.
 *
 * This algorithm generally ignores default-ignorable code points, i.e.,
 * kerning is applied across such characters and tracking is only
 * applied after non-default-ignorable code points: T+ZWNJ+o will
 * kern T+o normally and will insert tracking only once.
 *
 * However, to be able to separate kerning pairs, ZWSP (U+200B) and
 * ZWNBSP (U+FEFF) inhibit kerning and contextual glyph selection.
 * Still, tracking is not applied multiple times, i.e., T+ZWSP+o will
 * not kern T+o, but tracking will still only be inserted once.
 *
 * gc->state.glyph_cnt is incremented exactly each time tracking
 * is inserted.
 *
 * [This handles feature specific glyph replacement.]
 *
 * This handles language specific glyph replacement.
 *
 * This handles language specific ligature composition.
 *
 * This does not handle text direction changes -- that must be done
 * by a higher software layer.  If the right2left flag is switched while
 * printing, glyphs will overlap.  Different direction text chunks must
 * be printed separately and then joined later.
 *
 * This does not handle line breaks.  Any line break characters will
 * simply be looked up in the font and probably cause replacement
 * characters to be printed.  A higher software layer must handle this.
 *
 * This does not handle dynamic white space, nor mark white space, i.e., for
 * text that is both left and right flush, a higher software layers must
 * print word-wise and adjust the words the white space.
 *
 * This is guaranteed to only append to \p out, so higher layers can try
 * to print something to see whether it becomes too wide, e.g. for
 * automatic line breaking, and then revert to a previous state
 * (by reverting gc->state).
 *
 * To reset the gc for a newline, gc->state can be zeroed.
 *
 * Combining characters are not combined across calls to this function,
 * i.e., if the string starts with combining characters, they are
 * rendered as spacing characters.
 *
 * If a glyph is not available, this will render a replacement
 * character (gc->replacement) if one is available, otherwise, it will
 * render nothing.
 *
 * The algorithm used may create polygons with duplicate points, which,
 * strictly speaking, is against the rules of the CSG2 submodule.  But
 * since the csg2-bool algorithm handles this without a problem, it is
 * tolerated here.  But this means that the resulting polygons should
 * always be passed through the CSG2 bool algorithm before continuing
 * with anything else.
 *
 * At a lower level, this handles compatibility decomposition, i.e,
 * this may render multiple subglyphs if the given glyph ID
 * decomposes.  However, such decompositions are completely internal
 * and count as a single glyph, e.g. wrt. tracking.
 *
 * At a lower level, this handles the simple internal fallback
 * low-level heuristic rendering of combining glyphs.  This mechanism
 * is not the Unicode canonical composition, which is handled by upper
 * layers (e.g. cp_font_print()).  Instead, this low-level combining
 * glyph rendering allows max. two combining glyphs: one above, one
 * below.  More complex situations need to be handled by the font,
 * e.g. by either providing a composes, or by composing multiple
 * combining glyphs into a single one that has both diacritics
 * (e.g. diaeresis above + macron above).  This function handles the
 * placement of diacritics above on tall characters by trying to find
 * a replacement glyph for the diacritic for tall characters, i.e., no
 * magic, but again, a look-up step in the font.
 */
extern void cp_font_print(
    cp_v_obj_p_t *out,
    cp_font_gc_t *gc,
    unsigned (*next)(void *user),
    void *user)
{
    unsigned gc_mof_disable =
        gc->mof_disable |
        ((~gc->mof_enable) & (1 << CP_FONT_MOF_OPTIONAL));

    seq_t seq;
    seq_init(&seq, gc->font, next, user);

    glyph_t g2;
    seq_take(&g2, gc->font, &seq);
    while (g2.base != 0) {
        glyph_t g1 = g2;
        g2.base = 0;

        /* Try to apply optional, joining, ligature combinations.
         * Do this only if the glyphs have no combining characters.
         */
        seq_take(&g2, gc->font, &seq);

        if (is_simple(&g1)) {
            unsigned mof_disable = gc_mof_disable;
            while ((g2.base != 0) && is_simple(&g2)) {
                /* try to ligate/join based on language */
                if ((gc->lang != NULL) &&
                    mof_lookup(&gc->lang->optional, mof_disable, &g1.base, g2.base))
                {
                    seq_take(&g2, gc->font, &seq);
                    continue;
                }

                /* try to ligate/join */
                if (mof_lookup(&gc->font->optional, mof_disable, &g1.base, g2.base)) {
                    seq_take(&g2, gc->font, &seq);
                    continue;
                }

                /* stop ligation unless ZWJ is found */
                if (g2.base != ZWJ) {
                    break;
                }

                /* find next non-ZWJ character */
                do {
                    seq_take(&g2, gc->font, &seq);
                } while (g2.base == ZWJ);

                /* combine only simple glyphs */
                if ((g2.base == 0) || !is_simple(&g2)) {
                    break;
                }

                /* try to ligate ZWJ with new glyph */
                if (gc->lang != NULL) {
                    (void)map2_lookup1(&gc->lang->optional, &g2.base, ZWJ, g2.base);
                }
                (void)map2_lookup1(&gc->font->optional, &g2.base, ZWJ, g2.base);

                /* disable any glyph combination prohibitions and try to ligate again */
                mof_disable = 0;
            }
        }

        /* ignore= */
        if (cp_default_ignorable(g1.base)) {
            if ((g1.base == ZWSP) || (g1.base == ZWNBSP)) {
                /* inhibit kerning and alternative glyph selection */
                clear_kerning(&gc->state);
            }
            continue;
        }

        /* render (finally) */
        cp_font_render_glyph(out, gc, &g1);
    }
}

/**
 * Read one character from a UTF32 string.
 */
extern unsigned cp_font_read_str_utf32(void *user)
{
    unsigned const **s = user;
    unsigned r = **s;
    (*s)++;
    return r;
}

/**
 * Read one character from an ISO-8859-1 (including US-ASCII) string.
 */
extern unsigned cp_font_read_str_latin1(void *user)
{
    unsigned char const **s = user;
    unsigned r = **s;
    (*s)++;
    return r;
}

/**
 * Enable/disable ligatures */
extern void cp_font_gc_enable_ligature(
    cp_font_gc_t *gc,
    bool enable)
{
    gc->mof_disable |= (1 << CP_FONT_MOF_LIGATURE);
    gc->mof_disable ^= enable ? (1 << CP_FONT_MOF_LIGATURE) : 0;
}

/**
 * Enable/disable joining */
extern void cp_font_gc_enable_joining(
    cp_font_gc_t *gc,
    bool enable)
{
    gc->mof_disable |= (1 << CP_FONT_MOF_JOINING);
    gc->mof_disable ^= enable ? (1 << CP_FONT_MOF_JOINING) : 0;
}

/**
 * Enable/disable joining */
extern void cp_font_gc_enable_optional(
    cp_font_gc_t *gc,
    bool enable)
{
    gc->mof_enable |= (1 << CP_FONT_MOF_OPTIONAL);
    gc->mof_enable ^= enable ? 0 : (1 << CP_FONT_MOF_OPTIONAL);
}

/**
 * Set amount of tracking.
 * Tracking is set in 'pt' (i.e., the output unit).
 */
extern void cp_font_gc_set_tracking(
    cp_font_gc_t *gc,
    double amount)
{
    gc->tracking = amount;
}

/**
 * Set amount of spacing compatibly with OpenSCAD. */
extern void cp_font_gc_set_spacing(
    cp_font_gc_t *gc,
    double amount)
{
    gc->spacing = amount - 1;
    if (cp_eq(gc->spacing, 0)) {
        gc->spacing = 0;
    }
}
