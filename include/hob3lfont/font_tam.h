/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

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
/** The glyph is tall and occupies space above the x-height so that
 * diacritics above must be placed higher up.  This causes the
 * renderer to look up the high alternative diacritic glyph. */
#define CP_FONT_GF_TALL       0x02
/** The glyph is monospaced and should not be kerned.  Kerning
 * info is still stored, if available, so that kerning could
 * still be done if an override option is set.  This is mainly
 * meant for numbers/figures. */
#define CP_FONT_GF_MONO       0x04
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

#define CP_FONT_PROFILE_COUNT     16
#define CP_FONT_PROFILE(min,max)    (((min) | ((max) << 4)) & 0xff)
#define CP_FONT_PROFILE_GET_MIN(x) ((x) & 0xf)
#define CP_FONT_PROFILE_GET_MAX(x) (((x) >> 4) & 0xf)

typedef struct {
    uint8_t x[CP_FONT_GLYPH_LAYER_COUNT];
} cp_font_prof_t;

typedef struct {
    int x[CP_FONT_GLYPH_LAYER_COUNT];
} cp_font_half_profile_t;

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
     * The layers above cap are used for automatic diacritic placement:
     * if there is anything, the upper case diacritic is used (if available),
     * otherwise, the normal lower case is used.  (I.e., on most lower case
     * letters, two diacritics can be placed above and one below without
     * collision.  Collisions can be avoided by defining more glyphs with
     * combined diacritics, as necessary for, e.g., Vietnamese upper case
     * letters). */
    cp_font_prof_t profile;

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
     * For polygons: index into path heap to a variable
     * sized cp_font_path_t structure.
     * For sequences: index into path heap to a
     * cp_font_subglyph_t entry.
     * Note that sequences, neither glyph replacement nor
     * auto-kerning nor tracking is done.  I.e., a sequence
     * does define a single glyph.
     */
    unsigned first : 20;

    /**
     * For polygons:  data entry count in path heap,
     * For sequences: data entry count in path heap.
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

#define CP_FONT_KERN_EM_BITS (32 - 20 - 1)
#define CP_FONT_KERN_EM_MASK (~((~0U) << CP_FONT_KERN_EM_BITS))

/**
 * Sequence entry */
typedef struct {
    /** Index into glyph table of sub-glyph */
    unsigned glyph : 20;

    /**
     * Kerning to apply before rendering the sub-glyph, in
     * units of 1/2047th of an em.
     */
    unsigned kern_em  : CP_FONT_KERN_EM_BITS;

    /**
     * If set, subtract kern_em, otherwise, add it. */
    unsigned kern_sub : 1;
} cp_font_subglyph_t;

CP_STATIC_ASSERT(sizeof(cp_font_subglyph_t) == sizeof(unsigned));

/**
 * Language codes in OpenType format (specificaly, not
 * ISO-639-(1,2,3) -- see OpenType list).
 */
typedef struct {
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

typedef struct {
    /**
     * Three or four upper case characters (may not be NUL terminated)
     * for the OpenType language tag.  ISO-639-(1,3) 2 letter and 3 letter
     * names may also be used.
     * For languages that have no OpenType tag (at the time of writing,
     * Livonian seemingly had none), it is recommended to use the
     * ISO-639-3 code.
     */
    char id[4];

    /**
     * Index into lang array */
    unsigned lang_idx;
} cp_font_lang_map_t;

typedef CP_VEC_T(cp_font_lang_t) cp_v_font_lang_t;
typedef CP_VEC_T(cp_font_lang_map_t) cp_v_font_lang_map_t;

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
     * Descender depth glyph coordinate.
     * The coordinates are normalised across the font, so this is the
     * same for all glyphs.
     */
    uint16_t dec_y;

    /**
     * Center X glyph coordinate.
     * This is the original 0 coordinate around which glyphs are usually
     * designed.  This may be used for fallback heuristic modifier horizontal
     * placement if there is no pre-composed glyph available: the center of
     * the base glyph and the modifer should be the same position. */
    uint16_t center_x;

    /**
     * Amount of space at each glyph X profile step */
    uint16_t space_x[CP_FONT_PROFILE_COUNT];

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
     * The is sorted by cp_font_map_t::first.
     * cp_font_map_t::second is used only for CP_FONT_CT_ABOVE diacritics: it is
     * the high above glyph used instead if the base glyph has a TALL flag.
     * cp_font_map_t::result is one of the CP_FONT_CT_* constants.
     */
    cp_v_font_map_t comb_type;

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
    cp_v_font_map_t base_repl;

    /**
     * List of language entries.  Indexed by lang_map.
     */
    cp_v_font_lang_t lang;

    /**
     * Language specific glyph mappings.
     * This is sorted lexicographically by cp_font_lang_t::id.
     * This may have multiple entries mapping to the same map.  All unique
     * OpenType names, ISO-639-1 names (2 letter) and ISO-638-3 (3 letter)
     * names should be listed, as the library has currently no logic to resolve
     * language names.  Indexing must be done with only capital latin
     * letters A-Z.
     */
    cp_v_font_lang_map_t lang_map;
} cp_font_t;

typedef CP_VEC_T(cp_font_t*) cp_v_font_p_t;

typedef struct {
    /**
     * Current X position for next glyph.
     * For right2left, this will decrease while printing, for left2right,
     * this will increase.
     */
    double cur_x;

    /** Last simple glyph for finding ligatures */
    unsigned last_simple_cp;

    /** Whether last kerning profile is valid and can be used */
    bool last_prof_valid;

    /** Last glyph kerning profile info */
    cp_font_half_profile_t last_prof;

    /** Last width of glyph */
    int last_width[2];

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

    /**
     * Font nominal size (width/height of an em in the unit of the output
     * coordinate system). */
    double em;

    /** The ratio of scaling in X direction wrt. Y direction. */
    double ratio_x;

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
     * Additional glyph spacing (in final coordinate unit, i.e., in 'pt').
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
     * Spacing, something OpenSCAD defines for unknown purposes: make all
     * glyphs by this ratio larger by filling in space.  I.e., this is
     * tracking relative to the glyph width, which  makes the next look
     * totally awkward for any values other than 0.  E.g., for a value
     * of 1, insert the width of the glyph of space, e.g. for a 'W',
     * insert a large amount of space, for an 'i', insert a small amount
     * of space.
     *
     * This is applied before tracking, i.e., tracking is added on top
     * (but is not multipled by this).
     *
     * This is the ratio of the additional width, i.e., the origin is 0 here,
     * not 1 (the setter function uses an origin of 1 just like OpenSCAD).
     */
    double spacing;

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
