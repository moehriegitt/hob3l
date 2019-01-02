/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_FONT_TAM_H_
#define CP_FONT_TAM_H_

#include <stdint.h>
#include <hob3lbase/vec_tam.h>
#include <hob3lbase/err_tam.h>

/* typical font weight values */
#define CP_FONT_WEIGHT_ULTRA_THIN    28
#define CP_FONT_WEIGHT_THIN          57
#define CP_FONT_WEIGHT_LIGHT         85
#define CP_FONT_WEIGHT_BOOK          113
#define CP_FONT_WEIGHT_MEDIUM        142
#define CP_FONT_WEIGHT_BOLD          170
#define CP_FONT_WEIGHT_HEAVY         198
#define CP_FONT_WEIGHT_BLACK         227
#define CP_FONT_WEIGHT_ULTRA_BLACK   255

/* typical font stretch values */
#define CP_FONT_STRETCH_CONDENSED    70
#define CP_FONT_STRETCH_REGULAR      100
#define CP_FONT_STRETCH_WIDE         130

/* typical font slope values */
#define CP_FONT_SLOPE_ROMAN          100
#define CP_FONT_SLOPE_OBLIQUE        120

/** marker for special coordinate values; y defines what is special */
#define CP_FONT_X_SPECIAL 0xffff

/** special marker: end of polygon */
#define CP_FONT_Y_END 0

/* Combining types */
/* This does not need to be stored in the comptype table, it is the default
 * for all combining characters not found in that table. */
#define CP_FONT_CT_OTHER 0
/* Above combining */
#define CP_FONT_CT_ABOVE 1
/* Below combining */
#define CP_FONT_CT_BELOW 2

/* Flags for glyph table: */

/** This is a sequence (if not set: it is a polygon rendering) */
#define CP_FONT_GF_SEQUENCE   0x01
#define CP_FONT_GF_RESERVED1_ 0x02
#define CP_FONT_GF_RESERVED2_ 0x04
#define CP_FONT_GF_RESERVED3_ 0x08

/* Flags for combinining table */

#define CP_FONT_MCF_RESERVED0_ 0x01
#define CP_FONT_MCF_RESERVED1_ 0x02
#define CP_FONT_MCF_RESERVED2_ 0x04
#define CP_FONT_MCF_RESERVED3_ 0x08

/* Flags for optional, ligature, joining glyph combination table: */

/**
 * Type of combination.
 */
#define CP_FONT_MOF_TYPE_MASK   0x03
/**
 * The composition is mandatory and cannot be inhibited. */
#define CP_FONT_MOF_MANDATORY   0x00
/**
 * The composition is a ligature and, thus, globally optional based on the
 * font_gc_t::mof_disable bits. */
#define CP_FONT_MOF_LIGATURE    0x01
/**
 * The composition joins two glyphs and is, thus, globally optional based on the
 * font_gc_t::mof_disable bits. */
#define CP_FONT_MOF_JOINING     0x02
/**
 * An optional ligature: if set, they are inhibited unless ZWJ is used. */
#define CP_FONT_MOF_OPTIONAL    0x03

/**
 * Do not ligate.  Replace first glyph and keep second anyway.  This is for
 * post-context replacements.
 */
#define CP_FONT_MOF_KEEP_SECOND 0x04
#define CP_FONT_MOF_RESERVED3_  0x08

/**
 * This is a kerning value stored as signed integer in glyph coordinates
 * in cp_font_map_t::second. Otherwise (i.e., bit is not set), this is
 * a replacement glyph. */
#define CP_FONT_MXF_KERNING    0x01
#define CP_FONT_MXF_RESERVED1_ 0x02
#define CP_FONT_MXF_RESERVED2_ 0x04
#define CP_FONT_MXF_RESERVED3_ 0x08

/* Flags for language specific replacement table */

#define CP_FONT_MLF_RESERVED0_ 0x01
#define CP_FONT_MLF_RESERVED1_ 0x02
#define CP_FONT_MLF_RESERVED2_ 0x04
#define CP_FONT_MLF_RESERVED3_ 0x08

/* Input values for alternative base glyph table in above/below combination context.
 * This is the 'second' value that is passed to the lookup table.
 */

/** Going to put something on top. Can be combined with CP_FONT_HAS_HAVE_BELOW. */
#define CP_FONT_MAS_HAVE_ABOVE  0x01
/** Going to put something below.  Can be combined with CP_FONT_MAS_HAVE_ABOVE. */
#define CP_FONT_MAS_HAVE_BELOW  0x02
/** Both above and below */
#define CP_FONT_MAS_HAVE_BOTH   (CP_FONT_MAS_HAVE_ABOVE | CP_FONT_MAS_HAVE_BELOW)

/* Flags for alternative base glyph table in above/below combination context. */

#define CP_FONT_MAF_RESERVED0_ 0x01
#define CP_FONT_MAF_RESERVED1_ 0x02
#define CP_FONT_MAF_RESERVED2_ 0x04
#define CP_FONT_MAF_RESERVED3_ 0x08

/**
 * Overlapping paths are XORed, i.e., an even-odd algorithm must be used.
 * If not set: paths are additive, i.e., a greater-than-zero algorithm must
 * be used.
 * Note that paths in this font format are not constrained in their direction:
 * they may run CW or CCW without changing semantics.
 */
#define CP_FONT_FF_XOR 0x01

/*
 * This font format is limited to glyph IDs up to 0xfffff.
 * I.e., Unicode Plane 16 is not usable, and no characters outside Unicode
 * codepoint range can be defined. */
#define CP_FONT_ID_WIDTH 20
#define CP_FONT_ID_MASK  (~((~0U) << CP_FONT_ID_WIDTH))

#define CP_FONT_FLAG_WIDTH 4
#define CP_FONT_FLAG_MASK  (~((~0U) << CP_FONT_FLAG_WIDTH))

/**
 * For auto-kerning and glyph composition: X with profile.
 *
 * This works by deviding the glyph into discrete layers where
 * available space is stored from the border into the glyph.
 */
#define CP_FONT_GLYPH_LAYER_COUNT 16
#define CP_FONT_GLYPH_ABOVE_XHI   9

/**
 * A coordinate in the glyph coord system.
 *
 * This is normalised to run from 0..0xfffe in both X and Y
 * coordinates.  The font defines where is the Y baseline.  Each glyph
 * defines the left and the right coordinate.
 *
 * The value x=CP_FONT_X_SPECIAL indicates a special values.  y then
 * indicates which special value, see CP_FOMT_Y_*.
 */
typedef struct {
    uint16_t x;
    uint16_t y;
} cp_font_xy_t;

typedef CP_VEC_T(cp_font_xy_t) cp_v_font_xy_t;

typedef struct {
    /**
     * The nominal left/right border of the glyph in glyph coordinates.
     *
     * This is not the minimum/maximum coordinate, but the amount
     * the cursor needs to move when rendering.  (I.e., the
     * definition of the width of the glyph without kerning.)
     */
    union {
        struct {
            uint16_t left;
            uint16_t right;
        };
        uint16_t side[2];
    } border_x;

    /**
     * For each x profile layer, the maximum amount of possible kerning
     * is stored from 0 (no space left) to 14 (a lot of space left).
     * The value 15 is reserved to mean that no part of the glyph
     * is on this level (infinite/maximum kerning possible).  The
     * exact amount of kerning is stored in cp_font_t::space_x map
     * in glyph coordinates.
     *
     * The left space is stored in the high nibble, the right space in the
     * low nibble of each value.
     *
     * Layers of X width profile data:
     *
     *   13,14,15 above cap
     *   12       at cap
     *   9,10,11  between xhi and cap
     *   8        at xhi
     *   4,5,6,7  between base and xhi
     *   3        at base
     *   0,1,2    below base
     *
     * The layers above cap are used for automatic diacritic placement:
     * if there is anything, the upper case diacritic is used (if available),
     * otherwise, the normal lower case is used.  (I.e., on most lower case
     * letters, two diacritics can be placed above and one below without
     * collision.  Collisions can be avoided by defining more glyphs with
     * combined diacritics, as necessary for, e.g., Vietnamese upper case
     * letters). */
    uint8_t profile_x[CP_FONT_GLYPH_LAYER_COUNT];

    /**
     * List of indices in the coordinate heap of first glyph coordinate
     * of a path.  The array has 'cp_font_glyph_t::second' entries.
     * An empty array is possible for empty glyphs e.g. white space. */
    uint32_t data[];
} cp_font_path_t;

typedef struct {
    /** glyph ID */
    unsigned id : 20;

    /**
     * See CP_FONT_GF_*
     */
    unsigned flags : 4;

    /**
     * For polygons: index into path heap.
     * For decompositions: first decomposition glyph ID.
     * Note that decompositions, joining, and ligatures all
     * disable additional letter spacing locally
     * (see cp_font_gc_t::letter_spacing).
     */
    unsigned first : 20;

    /**
     * For polygons: data entry count in path heap,
     * For decompositions: second decomposition glyph ID.
     *
     * Note that no 1:1 mappings are needed: if one glyph is
     * completely equivalent to another, it can just point to
     * the same path info record.
     */
    unsigned second : 20;
} __attribute__((__aligned__(8))) cp_font_glyph_t;

typedef struct {
    /** glyph ID */
    unsigned first : 20;

    /**
     * See CP_FONT_MF_*
     */
    unsigned flags : 4;

    /**
     * For compositions: second glyph ID,
     * For conditional mappings: a bitmap of CP_FONT_MO_*
     * For language mapping: an index into cp_font::lang_tab
     */
    unsigned second : 20;

    /**
     * Resulting glyph ID,
     */
    unsigned result : 20;
} __attribute__((__aligned__(8))) cp_font_map_t;

typedef CP_VEC_T(cp_font_glyph_t) cp_v_font_glyph_t;
typedef CP_VEC_T(cp_font_map_t) cp_v_font_map_t;

/**
 * Language codes in OpenType format (specificaly, not
 * ISO-639-(1,2,3) -- see OpenType list).
 */
typedef struct {
    /**
     * Three or four upper case characters (always NUL terminated)
     * for the OpenType language tag.
     * For languages that have no OpenType tag (at the time of writing,
     * Livonian seemingly had none), it is recommended to use the
     * ISO-639-3 code.
     */
    char id[8];

    /**
     * This is just like cp_font_t::optional table, only language specific.
     * E.g. in Dutch, [i]+[j] combines into [ij], but normally, it does not.
     */
    cp_v_font_map_t optional;

    /**
     * The map to use for mapping glyphs 1:1.
     * The is sorted by cp_font_map_t::(first,second).
     * cp_font_map_t::second is not used and must be 0.
     * The CP_FONT_MLF_* flags are used.
     */
    cp_v_font_map_t one2one;
} cp_font_lang_t;

typedef CP_VEC_T(cp_font_lang_t) cp_v_font_lang_t;
typedef CP_VEC_T(uint32_t) cp_v_uint32_t;

typedef struct {
    /** Full font name */
    char const *name;

    /** Language family name */
    char const *family_name;

    char const *weight_name;
    char const *slope_name;
    char const *stretch_name;
    char const *size_name;

    /**
     * Width of 1em in glyph coordinate points.
     *
     * This is for scaling the font to a given pt size.
     * This is not the mininum/maximum coordinate the glyph data contains,
     * which is not stored, because coordinates are normalised to be +-0x7fff.
     */
    uint16_t em_x;

    /**
     * Height of 1em in glyph coordinate points.
     */
    uint16_t em_y;

    /**
     * Height above base line of font in glyph coordinate points.
     * This together with bottom_y is the line advance.
     * This is not the mininum/maximum coordinate the glyph data contains,
     * which is not stored, because coordinates are normalised to be +-0x7fff.
     */
    uint16_t top_y;

    /**
     * Depth below base line of font in glyph coordinate points.
     * This together with top_y is the line advance.
     * This is not the mininum/maximum coordinate the glyph data contains,
     * which is not stored, because coordinates are normalised to be +-0x7fff.
     */
    uint16_t bottom_y;

    /**
     * Baseline glyph coordinate.
     * The coordinates are normalised across the font, so this is the
     * same for all glyphs.
     */
    uint16_t base_y;

    /**
     * Capital height glyph coordinate.
     * The coordinates are normalised across the font, so this is the
     * same for all glyphs.
     */
    uint16_t cap_y;

    /**
     * Small x height glyph coordinate.
     * The coordinates are normalised across the font, so this is the
     * same for all glyphs.
     */
    uint16_t xhi_y;

    /**
     * Center X glyph coordinate.
     * This is the original 0 coordinate around which glyphs are usually
     * designed.  This may be used for fallback heuristic modifier horizontal
     * placement if there is no pre-composed glyph available: the center of
     * the base glyph and the modifer should be the same position. */
    uint16_t center_x;

    /**
     * Amount of space at each glyph X profile layer */
    uint16_t space_x[CP_FONT_GLYPH_LAYER_COUNT];

    /**
     * Font flags, see CP_FONT_FF_* */
    uint16_t flags;

    /** Weight in 0..255 (see CP_FONT_WEIGHT_* for names) */
    uint8_t weight;

    /** Slope in percent (see CP_FONT_SLANT_* for names):  */
    uint8_t slope;

    /** Stretch in percent of Book (see CP_FONT_WIDTH_* for names) */
    uint8_t stretch;

    /** Lower end of optimum size range, in points */
    uint8_t min_size;

    /** Upper end of optimum size range, in points */
    uint8_t max_size;

    /**
     * Glyph map.
     * This also stores unconditional (compatibility) decompositions. */
    cp_v_font_glyph_t glyph;

    /** Path heap. */
    cp_v_uint32_t path;

    /** Coordinate heap */
    cp_v_font_xy_t coord;

    /**
     * Unconditional canonical decomposition
     * This is applied when reading the input stream to render composes
     * glyphs that the font has not special glyphs for.  This is typically
     * not the complete set of decompositions that Unicode defines, but
     * exactly what the font needs.
     * cp_font_map_t::(first, second, result) are all glyph IDs.  This is
     * indexed by first.  The decomposition is result,second (in that order).
     */
    cp_v_font_map_t decompose;

    /**
     * Unconditional combination; cp_font_map_t::second is a glyph ID.
     * The is sorted be cp_font_map_t::(first,second).
     * The CP_FONT_MCF_* flags are used.
     * This is not entirely unconditional: if the CP_FONT_MCF_LIGATURE
     * bit is set, then this is only done if ligatures are switched on.
     * Likewise, if CP_FONT_MCF_JOINING is switched on, this is only done
     * if joining is switched on (the distinction is that 'fi' is a
     * LIGATURE mapping while Arabic script joining is a JOIN mapping,
     * and the two can be controlled globally with separate settings).
     *
     * ZWNJ may always be used to inhibit any combinations.
     *
     * ZWJ by default switches ligature mappings back on, but special
     * mappings with ZWJ can also be put in this map, e.g. to implement
     * the Arabic behaviour where LAM+ALEF compose into something
     * different than LAM+ZWJ+ALEF.
     */
    cp_v_font_map_t compose;

    /** Mandatory, optional, ligature, and joining table for alternative
     * glyphs after combining sequences are processed.
     * cp_font_map_t::flags are the CP_FONT_MOF_* flags.
     * cp_font_map_t::first, second, and result are glyph IDs.
     * The is sorted be cp_font_map_t::(first, second).
     */
    cp_v_font_map_t optional;

    /**
     * Combining class mask.  The Unicode combining class is only normative
     * for canonical orderinng, but we need to know the class as it is actually
     * used in the font.  This engine handles above and below combining
     * characters algorithmically in simple (but helpful) cases, so these two
     * classes are distinguished.
     * The is sorted be cp_font_map_t::first.
     * cp_font_map_t::second is not used.
     * cp_font_map_t::result is one of the CP_FONT_CT_* constants.
     */
    cp_v_font_map_t combtype;

    /**
     * Kerning table and replacement glyph table, applied after rendering
     * the first glyph to the next glyph.
     * The is sorted by cp_font_map_t::(first,second).
     * cp_font_map_t::first is a glyph ID of the preceding glyph (the last
     * one that was printed), cp_font_map_t::second is the next glyph to be
     * printed.
     * The CP_FONT_MXF_* flags are used.
     * cp_font_map_t::result is either a kerning value (in glyph coordinates,
     * signed), or a replacement glyph ID, depending on the
     * CP_FONT_MXF_KERNING bit.
     */
    cp_v_font_map_t context;

    /**
     * Alternative base glyph replacement table.
     * Used when combining characters are put above and/or below a base glyph
     * to look up an alternative base glyph (e.g. to map 'I' to 'DOTLESS I'
     * when something is put on top).
     * The is sorted by cp_font_map_t::(first,second).
     * The CP_FONT_MAF_* flags are used.
     * The CP_FONT_MAS_* values are used for cp_font_map_t::second.
     * cp_font_map_t::first is the glyph ID of the base glyph.
     *
     * Note: All characters with the Soft_Dotted property that are
     * supported be the font need to have the undotted base character in this
     * table for the renderer to work.
     */
    cp_v_font_map_t baserepl;

    /**
     * Language specific glyph mappings.
     * This is sorted lexicographically by cp_font_lang_t::id.
     */
    cp_v_font_lang_t lang;
} cp_font_t;

typedef CP_VEC_T(cp_font_t*) cp_v_font_p_t;

typedef struct {
    /**
     * Current X position for next glyph.
     * For right2left, this will decrease while printing, for left2right,
     * this will increase.
     */
    double cur_x;

    /** Last glyph relevant for kerning */
    unsigned last_cp;

    /**
     * Glyph count: the actual number of glyphs rendered, i.e., the
     * number of indivisible (wrt. tracking) entities rendered.
     * This does not count default-ignorable characters.
     */
    size_t glyph_cnt;
} cp_font_state_t;

/**
 * The graphics context for rendering.
 *
 * Vertical rendering is currently not implemented.
 */
typedef struct {
    /** Location to be used for rendering polygons */
    cp_loc_t loc;

    /** Font to use */
    cp_font_t const *font;

    /** Font scaling */
    double scale_x, scale_y;

    /** Base line in scaled coordinates */
    double base_y;

    /**
     * Replacement glyph idx (in cp_font_t::glyph).  large value out of range of
     * array if none is available. */
    size_t replacement_idx;

    /** Language specific map (NULL if disabled) */
    cp_font_lang_t const *lang;

    /** Text direction is right to left? */
    bool right2left;

    /**
     * Additional glyph spacing (in final coordinate unit).
     * This is applied once after drawing a glyph.  If is not applied
     * inside combinations, ligatures, joining pairs, or after default
     * ignorable codepoints, and if this is non-0, this does not affect
     * whether ligatures etc. are searched or how kerning is applied.
     *
     * This is also not applied between glyphs that are internally decomposed
     * to render a glyph, i.e. a font may render a 'IJ' ligature by decomposing
     * it into two glyphs, I and J, but no additional tracking is added
     * between I and J in that case if the input stream has a IJ ligature or
     * if an IJ ligature is automatically combined.
     *
     * This is applied additional to kerning.
     *
     * This can be set by software for both-side flush rendering by dividing
     * the free space after dummy rendering by (state.glyph_cnt - 1) and then
     * rendering again.
     *
     * For stretching white space instead of glyph space for flushing, a higher
     * level rendering algorithm is needed.
     *
     * This is not applied at default-ignorable characters.
     */
    double tracking;

    /**
     * Inhibit combinations by default if any of thse CP_FONT_MCF_* bits
     * are set.
     * This contains 1 << _MCF_ bits values, i.e., a bitmask of possible
     * combination types.
     */
    unsigned mof_disable;

    /**
     * Switch on these MCF by default.
     * This contains 1 << _MCF_ bits values, i.e., a bitmask of possible
     * combination types.
     */
    unsigned mof_enable;

    /** Print state, updated by printing routines. */
    cp_font_state_t state;

    /*
     * Vertical and horizontal alignment must be done after rendering.
     *
     * For vertical alignment, a line-breaking algorithm is needed as
     * this only prints single lines.
     *
     * For horizontal alignment, 'state.cur_x' can be used to determine
     * the width of the total text printed.
     */
} cp_font_gc_t;

#endif /* CP_FONT_TAM_H_ */
