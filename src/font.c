/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lbase/alloc.h>
#include <hob3l/font.h>
#include <hob3l/csg2.h>

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

static int cmp_glyph(
    unsigned const *glyph_id,
    cp_font_glyph_t const *glyph,
    void *user CP_UNUSED)
{
    unsigned a = *glyph_id;
    unsigned b = glyph->id;
    return a < b ? -1 : a > b ? +1 : 0;
}

/* ********************************************************************** */

/**
 * Find a glyph in a font.
 */
extern cp_font_glyph_t const *cp_font_find_glyph(
    cp_font_t const *font,
    unsigned glyph_id)
{
    size_t i = cp_v_bsearch(&glyph_id, &font->glyph, cmp_glyph, NULL);
    if (i >= font->glyph.size) {
        return NULL;
    }
    return &cp_v_nth(&font->glyph, i);
}

/**
 * Set the font in the gc.
 *
 * This resets the font, scale_x/y, base_y, and replacement.
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
    gc->font = font;
    gc->scale_x = (pt_size / font->em_x) * ratio_x;
    gc->scale_y = pt_size / font->em_y;
    gc->base_y = font->base_y * gc->scale_y;
    gc->replacement = cp_font_find_glyph(font, 0xFFFD);
}

/**
 * Render a single glyph.
 *
 * The rendered polygons will be added to \p out.
 *
 * This appends to out.  No other operation is done on \p out, i.e.,
 * no deeper structures are modified.
 *
 * This updates gc->state (i.e., cur_x and last_cp).
 *
 * Text in [] lists what is planned, but not yet implemented.
 *
 * [This handles kerning, which is applied before rendering a glyph
 * based on gc->state.last_cp.  This includes handling zero-width space
 * to inhibit kerning.]
 *
 * [This handles right2left glyph replacement (e.g. to swap parentheses).]
 *
 * [This handles language based glyph replacement.]
 *
 * [This handles feature based glyph replacement.]
 *
 * This handles compatibility decomposition, i.e, this may render multiple
 * glyphs if the given glyph ID decomposes.
 *
 * This does not handle glyph composition of multiple glyphs into a single
 * one, but this must be done by higher layers of printing.
 *
 * This does not handle ligature composition, nor zero-width non-joiner
 * and stuff like that, but this must be done at a higher software layer.
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
 * Since glyph composition is applied by higher layers, while decomposition
 * is applied by this function, no combining characters will compose
 * with decomposed characters.  E.g. [Lj] + [^] will not render
 * [L][j with circumflex], because the [Lj] ligature is decomposed only after
 * trying to compose with [^].  This only works properly if the font has
 * a dedicated composition for [Lj] + [^] (which is unlikely).
 *
 * Any zero width character that is passed to this function inhibits
 * kerning, even if it is not supposed to (like zero-width non-joiner).
 * This function does not expect to see those control characters,
 * because they belong to the glyph composition layer that should not
 * pass them down.
 *
 * This is guaranteed to only append to \p out, so higher layers can try
 * to print something to see whether it becomes too wide, e.g. for
 * automatic line breaking, and then revert to a previous state
 * (by reverting gc->state).
 *
 * This only appends objects of type cp_csg2_poly_t to \p out.
 *
 * To reset the gc for a newline, gc->state need to be zeroed.
 *
 * If the glyph is not available, this will render a replacement
 * character (gc->replacement) if one is available, otherwise, it will
 * render nothing.
 *
 * The algorithm used may create polygons with duplicate points, which,
 * strictly speaking, is against the rules of the CSG2 submodule.  But
 * since the csg2-bool algorithm handles this without a problem, it is
 * tolerated here.  But this means that the resulting polygons should
 * always be passed through the CSG2 bool algorithm before continuing
 * with anything else.
 */
extern void cp_font_print1(
   cp_v_obj_p_t *out,
   cp_font_gc_t *gc,
   unsigned glyph_id)
{
   cp_font_glyph_t const *glyph = cp_font_find_glyph(gc->font, glyph_id);
   if (glyph == NULL) {
       glyph = gc->replacement;
       if (glyph == NULL) {
           return;
       }
   }
   assert(glyph != NULL);

   if (glyph->flags & CP_FONT_GF_DECOMPOSE) {
       cp_font_print1(out, gc, glyph->first);
       cp_font_print1(out, gc, glyph->second);
       return;
   }

   cp_font_draw_poly(out, gc,
       (cp_font_path_t const *)cp_v_nth_ptr(&gc->font->path, glyph->first),
       glyph->second);
   gc->state.last_cp = glyph_id;
}
