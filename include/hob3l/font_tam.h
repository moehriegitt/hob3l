/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_FONT_TAM_H_
#define CP_FONT_TAM_H_

#include <stdint.h>
#include <hob3lbase/vec_tam.h>

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

/** This is a decomposition (if not set: it is a polygon rendering) */
#define CP_FONT_GF_DECOMPOSE  0x01
#define CP_FONT_GF_RESERVED1_ 0x02
#define CP_FONT_GF_RESERVED2_ 0x04
#define CP_FONT_GF_RESERVED3_ 0x08

#define CP_FONT_MF_RESERVED0_ 0x01
#define CP_FONT_MF_RESERVED1_ 0x02
#define CP_FONT_MF_RESERVED2_ 0x04
#define CP_FONT_MF_RESERVED3_ 0x08

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
#define CP_FONT_MASK_CODEPOINT 0xfffff
#define CP_FONT_MASK_PATH_IDX  0xfffff
#define CP_FONT_MASK_LANG_IDX  0xfffff

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
typedef CP_VEC_T(char const *) cp_v_char_const_p_t;
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
     * Center X glyph coordinate.
     * This is the original 0 coordinate around which glyphs are usually
     * designed.  This may be used for fallback heuristic modifier horizontal
     * placement if there is no pre-composed glyph available: the center of
     * the base glyph and the modifer should be the same position. */
    uint16_t center_x;

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
     * A map of language tags */
    cp_v_char_const_p_t lang_tab;

    /**
     * Unconditional composition; cp_font_compose_t::second is a glyph ID. */
    cp_v_font_map_t compose;

    /**
     * Ligature composition; cp_font_compose_t::second is a glyph ID. */
    cp_v_font_map_t liga_compose;

    /**
     * Language specific mapping; cp_font_compose_t::second is a language index into lang_tab. */
    cp_v_font_map_t lang_map;
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

    /** Replacement glyph.  NULL if none is available. */
    cp_font_glyph_t const *replacement;

    /** Language */
    char const *lang;

    /** Text direction is right to lefT? */
    bool right2left;

    /** Print state, updated by printing routines. */
    cp_font_state_t state;

    /*
     * Vertical and horizontal alignment must be done after rendering.
     * For vertical alignment, a line-breaking algorithm is needed as
     * this only prints single lines.
     * For horizontal alignment, the 'cur_x' can be used to determine
     * the width of the total text printed.
     */
} cp_font_gc_t;

#endif /* CP_FONT_TAM_H_ */
