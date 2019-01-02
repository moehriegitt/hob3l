/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_FONT_H_
#define CP_FONT_H_

#include <hob3l/csg.h>
#include <hob3l/font_tam.h>

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
 * Set a given language.
 *
 * name=NULL resets.  Also, if a language is not found, this resets
 * the name setting, which can be seen by checking gc->lang, which
 * will be NULL if the language is reset, or non-NULL if one was found
 * and selected.
 */
extern void cp_font_gc_set_lang(
    cp_font_gc_t *gc,
    char const *name);

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
 * This algorithm generally ignores default-ignorable codepoints, i.e.,
 * kerning is applied across such characters and tracking is only
 * applied after non-default-ignorable codepoints: T+ZWNJ+o will
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

/**
 * Enable/disable ligatures */
static inline void cp_font_gc_enable_ligature(
    cp_font_gc_t *gc,
    bool enable)
{
    gc->mof_disable |= (1 << CP_FONT_MOF_LIGATURE);
    gc->mof_disable ^= enable ? (1 << CP_FONT_MOF_LIGATURE) : 0;
}

/**
 * Enable/disable joining */
static inline void cp_font_gc_enable_joining(
    cp_font_gc_t *gc,
    bool enable)
{
    gc->mof_disable |= (1 << CP_FONT_MOF_JOINING);
    gc->mof_disable ^= enable ? (1 << CP_FONT_MOF_JOINING) : 0;
}

/**
 * Enable/disable joining */
static inline void cp_font_gc_enable_optional(
    cp_font_gc_t *gc,
    bool enable)
{
    gc->mof_enable |= (1 << CP_FONT_MOF_OPTIONAL);
    gc->mof_enable ^= enable ? 0 : (1 << CP_FONT_MOF_OPTIONAL);
}

#endif /* CP_FONT_H_ */
