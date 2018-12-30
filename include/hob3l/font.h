/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_FONT_H_
#define CP_FONT_H_

#include <hob3l/csg.h>
#include <hob3l/font_tam.h>

/**
 * Find a glyph in a font.
 */
extern cp_font_glyph_t const *cp_font_find_glyph(
    cp_font_t const *font,
    unsigned glyph_id);

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
    double ratio_x);

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
    unsigned glyph_id);

/**
 * Prints a string.
 *
 * This is like cp_font_print1, but prints until next() returns a NUL
 * characters.
 *
 * Because this handles a sequence, this does more than cp_font_print1:
 *
 * This handles equivalence and ligature composition of glyphs, including
 * ZWJ, ZWNJ, ZWSP to break/combine glyphs.
 */
extern void cp_font_print(
    cp_v_obj_p_t *out,
    cp_font_gc_t *gc,
    unsigned (*next)(void *user),
    void *user);

/**
 * Read one character from a UTF32 string.
 */
extern unsigned cp_font_read_str_utf32(void *user);

/**
 * Read one character from an ISO-8859-1 (including US-ASCII) string.
 */
extern unsigned cp_font_read_str_latin1(void *user);

/**
 * Like cp_font_print, but from an ISO-8859-1 character string */
static inline void cp_font_print_str_latin1(
    cp_v_obj_p_t *out,
    cp_font_gc_t *gc,
    char const *s)
{
    cp_font_print(out, gc, cp_font_read_str_latin1, &s);
}

/**
 * Like cp_font_print, but from an UTF32 string */
static inline void cp_font_print_str32(
    cp_v_obj_p_t *out,
    cp_font_gc_t *gc,
    unsigned const *s)
{
    cp_font_print(out, gc, cp_font_read_str_utf32, &s);
}

#endif /* CP_FONT_H_ */
