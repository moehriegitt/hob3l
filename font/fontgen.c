/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * This generates the standard font for hob3l's 'text' command.
 */

/* TODO:
 *   - have a max. amount of kerning per char (mainly for spaces, dashes, and
 *     brackets). Currently, the glyph width is the only max for kerning.
 *   - have a max relative amount of kerning so that . + ' + - do not kern
 *     to the same left border, but each char should add a linewide of step
 *     in x direction.  This max should be per font.
 *   - check how kerning can be done in l'
 *     -> usually, diaritics should have less padding, but l' is a ligature ATM,
 *     and the profile cannot currently have different edges.  Also, ligatures
 *     are not kerned well externally.  Hmm.  Maybe reformulate this with a
 *     (new type of) diacritic?
 *   - fine-tune character width (accents, j, l, J, k, etc.)
 *   - fix over- and undercut: only those letters that are round should go up
 *     or down to (-2,X,0,0) for X=+6,+3,-3, but those that are parallel even
 *     for a bit should be (-3,X,0,0), and those that touch rectangularly
 *     should be (0,X,0,0) and those in 45 degree need to be checked.
 *   - 0 is more pointed as a subscript than in normal size => should not be
 *     the case.  Probably due to min_dist, which may need to be adjusted based
 *     on magnification.
 *   - check that upper case letters have lower case equivs and vice versa
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <hob3lbase/def.h>
#include <hob3lbase/panic.h>
#include <hob3lbase/vchar.h>
#include <hob3lbase/vec.h>
#include <hob3lbase/mat.h>
#include <hob3lbase/obj.h>
#include <hob3lbase/alloc.h>
#include <hob3lbase/pool.h>
#include <hob3l/csg2.h>
#include <hob3l/font.h>

#include "uniname.h"

#define FLATTEN_POLY 0

#define FAMILY_NAME "Nozzl3 Sans"
#define DEFAULT_STYLE "Book"

#define INTV_SIZE(_a,_b) \
    ({ \
        __typeof__(_a) a = (_a); \
        __typeof__(_b) b = (_b); \
        assert(a <= b); \
        assert(sizeof(a) <= sizeof(size_t)); \
        assert(sizeof(b) <= sizeof(size_t)); \
        (size_t)(b - a + 1); \
    })

typedef CP_ARR_T(unsigned) cp_a_unsigned_t;
typedef CP_VEC_T(unsigned) cp_v_unsigned_t;

typedef CP_ARR_T(unsigned const *) cp_a_unsigned_const_p_t;

typedef CP_ARR_T(int const)    cp_a_int_const_t;
typedef CP_ARR_T(double const) cp_a_double_const_t;

typedef signed char font_coord_t;

typedef struct font font_t;
typedef struct font_glyph font_glyph_t;

typedef struct {
    struct {
        font_coord_t x;
        font_coord_t y;
    };
    font_coord_t v[2];
} font_vec2_t;

typedef struct {
    font_vec2_t lo;
    font_vec2_t hi;
} font_box_t;

typedef struct {
    double lo;
    double hi;
} minmax_t;

typedef enum {
    FONT_DRAW_MERGE = 1,
    FONT_DRAW_SEQUENCE,
    FONT_DRAW_STROKE,
    FONT_DRAW_XFORM,
    FONT_DRAW_REF,
    FONT_DRAW_WIDTH,
    FONT_DRAW_LPAD,
    FONT_DRAW_RPAD,
} font_draw_type_t;

#define font_draw_typeof(type) \
    _Generic(type, \
        font_draw_t:           CP_ABSTRACT, \
        font_draw_ref_t:       FONT_DRAW_REF, \
        font_draw_width_t:     FONT_DRAW_WIDTH, \
        font_draw_lpad_t:      FONT_DRAW_LPAD, \
        font_draw_rpad_t:      FONT_DRAW_RPAD, \
        font_draw_xform_t:     FONT_DRAW_XFORM, \
        font_draw_merge_t:     FONT_DRAW_MERGE, \
        font_draw_sequence_t:  FONT_DRAW_SEQUENCE, \
        font_draw_stroke_t:    FONT_DRAW_STROKE)

#define font_draw_new(r)        cp_new_type_(font_draw_typeof, r)
#define font_draw_cast(t,x)     cp_cast_(font_draw_typeof, t, x)
#define font_draw_try_cast(t,x) cp_try_cast_(font_draw_typeof, t, x)

#define FONT_DRAW_OBJ_ CP_OBJ_TYPE_(font_draw_type_t)

typedef enum {
    FONT_VERTEX_POINTED = 0,
    FONT_VERTEX_BEGIN,
    FONT_VERTEX_END,
    FONT_VERTEX_IN,  /* like BEGIN, but without horizontal stroke correction */
    FONT_VERTEX_OUT, /* like END,   but without horizontal stroke correction */
    FONT_VERTEX_MIRROR,
    FONT_VERTEX_ROUND,
    FONT_VERTEX_LARGE,
    FONT_VERTEX_HUGE,
    FONT_VERTEX_GIANT,
    FONT_VERTEX_SMALL,
    FONT_VERTEX_CHAMFER,
    FONT_VERTEX_ANGLED,
    FONT_VERTEX_TIGHT,
    FONT_VERTEX_DENT,
    FONT_VERTEX_NEW,
} font_vertex_type_t;

typedef enum {
    FONT_STRAIGHT,
    FONT_BOTTOM_LEFT,
    FONT_BOTTOM_RIGHT,
    FONT_TOP_LEFT,
    FONT_TOP_RIGHT,
} font_corner_type_t;

#define FONT_CORNER_MAX FONT_TOP_RIGHT

/**
 * This encodes the coordinate for the vertices of the strokes.
 *
 * primary and secondary are indices into the coord_x/y array.  The
 * index is offset by box.lo.x.  The coordinates from the coord_x
 * array are scaled by scale_x.
 *
 * interpol defines the lerp between primary and secondary for the
 * base coordinate.  Interpol runs from 0 (at primary) to 60
 * (at secondary.  It is also a signed value that theoretically,
 * values between -128 and +127 can be used for points outside
 * primary..secondary.  60 was chosen because it has a lot of
 * integer divisors: 1,2,3,4,5,6,10,12,15,20,30,60.
 *
 * sub is the index into the sub_x/y arrays, with negative numbers
 * indexing from the end of that array, i.e., for sub<0, the
 * index is cp_countof(sub_*) + i.  The sub_* values are taken to be
 * relative to line_width, and the sign is swapped if primary
 * is negative.  This correction of the position is done
 * via another array instead of directly via the percentage of the
 * line width so that in a family of fonts, corrections can all be
 * relative to the line width of the normal weight font.
 *
 * In total, the coordinate described by this record is (for * = x,y):
 *
 *    (lerp(coord_*[primary   - box.lo.*],
 *          coord_*[secondary - box.lo.*],
 *          interpol/60.) * scale_*)
 *     + (line_width *
 *           (primary < 0 ? -1 : +1) *
 *           sign(sub)*sub_*[abs(sub)])
 *
 * scale_y has a fixed value of 1.
 *
 * Additionally, values of 'large_dist + min_dist' can be added in
 * 1/60 units by specifying 'round'.
 *
 * Further, coordinates can be offset by a multiple of the dot size
 * of the font using the 'dot_rel' 1/60 multiplier.
 *
 * Further, lengths defined by distances can be added using 'len':
 * the length (coord_x(to) - coord_x(from)) * frac/60.

 * CURRENTLY NOT USED:
 * Further, lengths from the other coordinate (y for x and x for y) can
 * be used using olen: the length (coord_y(to) - coord_y(from)) * frac/60.
 * can be added.
 */
typedef struct {
    int8_t sub;
    int8_t primary;
    int8_t secondary;
    int16_t interpol;
    int16_t dot_rel;
    struct {
        int8_t  from;
        int8_t  to;
        int16_t frac;
    } len;
    /* currently not used: position by other dimension's length */
    struct {
        int8_t  from;
        int8_t  to;
        int16_t frac;
    } olen;
    unsigned _line;
} font_def_coord_t;

typedef struct {
    unsigned code_point;
    char const *name;
} unicode_t;

typedef struct {
    font_vertex_type_t type;
    font_def_coord_t x;
    font_def_coord_t y;
} font_def_vertex_t;

typedef CP_ARR_T(font_def_vertex_t const) font_a_def_vertex_const_t;

typedef union font_draw font_draw_t;

typedef CP_ARR_T(font_draw_t const * const) font_a_draw_const_p_const_t;

typedef CP_ARR_T(unicode_t const) font_a_unicode_const_t;

typedef struct {
    FONT_DRAW_OBJ_
    font_a_def_vertex_const_t vertex;
} font_draw_stroke_t;

typedef struct {
    FONT_DRAW_OBJ_
    font_a_draw_const_p_const_t child;
} font_draw_merge_t;

typedef struct {
    unicode_t unicode;
    double kern;
} font_subglyph_t;

typedef CP_ARR_T(font_subglyph_t const *const) font_a_subglyph_const_p_const_t;

typedef struct {
    FONT_DRAW_OBJ_
    font_a_subglyph_const_p_const_t seq;
} font_draw_sequence_t;

/**
 * To map coordinates.
 */
typedef struct {
    /**
     * Negates the X def coordinates before mapping through coord_x.
     */
    bool swap_x;

    /**
     * Transform final coordinates
     *
     * This translation is not applied to .min_coord/.max_coord settings.
     *
     * The Y coordinate base is at base_y, the X coordinate at X=0 (pre-coord_x-lookup).
     */
    cp_mat2w_t xform;

    /**
     * Transform before applying line-width adjustments
     *
     * Note that xy are mapped separately, so no slanting or rotations
     * or any transformation using both coordinates is possible. Basically,
     * just xlat, scale are possible here.
     *
     * Although scaling is possible, mirroring is not possible ATM,
     * because the line_width-relative positioning's sign is based on
     * the non-xformed coordinate (and also, for 0, sign-swapping
     * would not work anyway).
     *
     * This translation is not applied to .min_coord/.max_coord settings.
     *
     * The Y coordinate base is at base_y, the X coordinate at X=0 (pre-coord_x-lookup).
     */
    cp_mat2w_t pre_xform;

    bool line_width_defined;
    double line_width;
} font_gc_t;

typedef struct font_draw_xform font_draw_xform_t;

typedef void (*font_xform_t)(font_t const *, font_gc_t *, font_draw_xform_t const *);

struct font_draw_xform {
    FONT_DRAW_OBJ_
    font_xform_t xform;
    font_draw_t const *child;
    /** parameters for transformation filter */
    double a, b;
};

typedef struct {
    FONT_DRAW_OBJ_
    unicode_t unicode;
} font_draw_ref_t;

typedef struct {
    FONT_DRAW_OBJ_
    unicode_t unicode;
} font_draw_width_t;

typedef struct {
    FONT_DRAW_OBJ_
    unicode_t unicode;
} font_draw_lpad_t;

typedef struct {
    FONT_DRAW_OBJ_
    unicode_t unicode;
} font_draw_rpad_t;

typedef struct {
    cp_a_vec2_t point;
} font_draw_path_t;

typedef CP_VEC_T(font_draw_path_t) font_v_draw_path_t;

typedef struct {
    cp_vec2_minmax_t box;
    font_v_draw_path_t path;
} font_draw_poly_t;

union font_draw {
    FONT_DRAW_OBJ_
};

typedef enum {
    /** mandatorily ligate a+b into the glyph where this is found (language specific or not) */
    MAP_TYPE_MANDATORY,
    /** compose ligate a+b into the glyph where this is found (language specific or not) */
    MAP_TYPE_LIGATURE,
    /** join a+b into the glyph where this is found (language specific or not) */
    MAP_TYPE_JOINING,
    /** if the preceeding glyph is a, apply this amount of kerning (globally) */
    MAP_TYPE_OPTIONAL,

    /** canonically compose a+b into the glyph where this is found (language specific or not) */
    MAP_TYPE_CANON,
    /** like LIGATURE or JOINING, but is off by default an needs to be enabled using ZWJ */
    MAP_TYPE_KERNING,
    /** if the preceeding glyph is a, replace current glyph by b (globally) */
    MAP_TYPE_CONTEXT,
    /** replace this glyph by a (language specific) alternative glyph */
    MAP_TYPE_REPLACE,

    /** replace this glyph if it is combined with something above and/or below */
    MAP_TYPE_BASE_REPLACE,

    /** flag to mark to keep second glyph */
    MAP_FLAG_KEEP = 0x100,

    /** like MANDATORY,but replace only the first glyph (mandatory), keep the second */
    MAP_TYPE_MANDATORY_KEEP = MAP_TYPE_MANDATORY | MAP_FLAG_KEEP,
    /** like LIGATURE,but replace only the first glyph (mandatory), keep the second */
    MAP_TYPE_LIGATURE_KEEP  = MAP_TYPE_LIGATURE | MAP_FLAG_KEEP,
    /** like JOINING,but replace only the first glyph (mandatory), keep the second */
    MAP_TYPE_JOINING_KEEP = MAP_TYPE_JOINING | MAP_FLAG_KEEP,
    /** like OPTIONAL,but replace only the first glyph (mandatory), keep the second */
    MAP_TYPE_OPTIONAL_KEEP = MAP_TYPE_OPTIONAL | MAP_FLAG_KEEP,
} font_def_map_type_t;

typedef struct {
    font_def_map_type_t type;
    font_glyph_t const *glyph; /* used internally -- not specified in glyph data */
    unicode_t a;
    unicode_t b;
    unsigned value;
    double amount;
    char const *lang;
} font_def_map_t;

typedef CP_ARR_T(font_def_map_t) font_a_def_map_t;
typedef CP_VEC_T(font_def_map_t) font_v_def_map_t;

typedef struct {
    unicode_t unicode;

    /** an above diacritic, and this is the glyph of the high variant. */
    unicode_t high_above;

    /** a below diacritic (CP_FONT_CT_BELOW) */
    bool is_below;

    /** whether this is a monospaced glyph (switches off kerning by default) */
    bool mono;

    /** final width scaling factor */
    double width_mul;

    /** recenter glyph at this coordinate after setting width manually */
    font_def_coord_t const *center_coord;

    /** min glyph coord set manually (otherwise: set by draw bounding box) */
    font_def_coord_t const *min_coord;

    /** max glyph coord set manually (otherwise: set by draw bounding box) */
    font_def_coord_t const *max_coord;

    /** like min_coord, but indexes coord_y */
    font_def_coord_t const *min_coord_from_y;

    /** like max_coord, but indexes coord_y */
    font_def_coord_t const *max_coord_from_y;

    /** absolute setting for left padding (default: use font default + lpad_add) */
    double lpad_abs;

    /** absolute setting for right padding (default: use font default + lpad_add) */
    double rpad_abs;

    /** increase left padding */
    double lpad_add;

    /** increase right padding */
    double rpad_add;

    /** index in line_width array for strokes, see LS_*
     * This is the intermediate value between the two integer positions.
     */
    double line_step;

    /** Composition data */
    font_a_def_map_t map;

    /** draw tree (may be NULL (for white space)) */
    font_draw_t const *draw;
} font_def_glyph_t;

typedef CP_ARR_T(font_def_glyph_t const) font_a_def_glyph_const_t;

typedef struct {
    font_vertex_type_t type;
    cp_vec2_t coord;
    double line_width;
    double radius_mul;
} font_vertex_t;

typedef CP_VEC_T(font_vertex_t) font_v_vertex_t;

struct font_glyph {
    unicode_t unicode;

    /**
     * Unslanted bounding box of graphics.
     * For empty glyph: 0 width on base_y.
     *
     * This is basically equal to draw->box, but can be overridden using
     * min_coord/max_coord.
     */
    cp_vec2_minmax_t box;

    /**
     * Unslanted rendering box: usually expanded box, but occasionally may
     * be smaller than box if the glyph overlaps w/ neighbouring glyphs.
     * Currently, only dim.min.x and dim.min.y is used.  Y is fixed as base_y.
     */
    cp_vec2_minmax_t dim;

    /**
     * Minmax per coll_box layer */
    minmax_t coll_box[CP_FONT_GLYPH_LAYER_COUNT];

    double lpad;
    double rpad;

    font_draw_poly_t *draw;

    font_def_glyph_t const *def;
    cp_csg2_poly_t *final_poly;
    cp_font_glyph_t *final;

    font_glyph_t *width_of;
    font_glyph_t const *lpad_of;
    font_glyph_t const *rpad_of;
    font_glyph_t const *line_step_of;

    font_t *font;
    /** for diagnosis: which coord_x was used? */
    bool *used_x;
    /** for diagnosis: which coord_y was used? */
    bool *used_y;
};

typedef CP_VEC_T(font_glyph_t) font_v_glyph_t;

typedef struct {
    char const *family_name;
    char const *weight_name;
    char const *slope_name;
    char const *stretch_name;
    char const *size_name;

    /** Weight in 0..255 (see CP_FONT_WEIGHT_* for names) */
    uint8_t weight;

    /** Slope in percent (see CP_FONT_SLOPE_* for names):  */
    uint8_t slope;

    /** Stretch in percent of Regular (see CP_FONT_STRETCH_* for names) */
    uint8_t stretch;

    /** Size in pt for optimal usage, in point size */
    uint8_t min_size, max_size;

    font_box_t box;
    font_coord_t cap_y;
    font_coord_t xhi_y;
    font_coord_t base_y;
    font_coord_t dec_y;
    font_coord_t top_y;
    font_coord_t bottom_y;
    double line_width[5];
    double slant;
    double radius[4];
    double angle[2];
    double min_dist;
    cp_a_double_const_t const coord_x;
    cp_a_double_const_t const coord_y;
    /** which y lines to highlight */
    cp_a_int_const_t const highlight_y;

    /** size (=length) of a dot */
    double dot_size;
    double sub_x[10];
    double sub_y[10];
    double scale_x;
    double round_tension;
    double lpad_default;
    double rpad_default;
    unsigned round_step_cnt;
    font_vertex_type_t corner_type[FONT_CORNER_MAX+1];
    font_a_def_glyph_const_t glyph;
} font_def_t;

struct font {
    char const *family_name;
    cp_vchar_t style_name;
    cp_vchar_t name;
    cp_vchar_t filename;
    cp_vec2_minmax_t box_max;
    double coll_box_y[CP_FONT_GLYPH_LAYER_COUNT + 1];
    double cap_y;
    double xhi_y;
    double base_y;
    double dec_y;
    double top_y;
    double bottom_y;
    double slant;
    double em; /* actual em size of this font (for scaling into nominal size) */
    double kern_max;
    font_v_glyph_t glyph;
    font_def_t const *def;
    cp_font_t *final;
    cp_dict_t *coord_dict;
};

typedef CP_VEC_T(font_t *) font_v_font_p_t;

typedef struct {
    cp_vec2_t left;
    cp_vec2_t right;
} font_stroke_end_t;

typedef struct {
    font_stroke_end_t src;
    font_stroke_end_t dst;
} font_stroke_line_t;

/* ********************************************************************** */
/* final glyph format */

/*
 * OpenType Features:
 * smcp: small to smallcaps
 * c2sc: capitals to smallcaps
 * afrc: alternative fractions
 * calt: contextual alternatives
 * pnum: proportional figures (possibly different width)
 * tnum: tabular figures (uniform width)
 * onum: oldstlye figures (medieaval)
 * lnum: lining figures (uniform height)
 * ...
 */

/* ********************************************************************** */

/* Font Data Font 1 */
#define B FONT_VERTEX_BEGIN
#define E FONT_VERTEX_END
#define I FONT_VERTEX_IN
#define O FONT_VERTEX_OUT
#define M FONT_VERTEX_MIRROR
#define R FONT_VERTEX_ROUND
#define L FONT_VERTEX_LARGE
#define H FONT_VERTEX_HUGE
#define G FONT_VERTEX_GIANT
#define S FONT_VERTEX_SMALL
#define P FONT_VERTEX_POINTED
#define C FONT_VERTEX_CHAMFER
#define A FONT_VERTEX_ANGLED
#define T FONT_VERTEX_TIGHT
#define N FONT_VERTEX_NEW
#define D FONT_VERTEX_DENT

#define LS_DEFAULT 0
#define LS_UPPER   1
#define LS_DIGIT   1
#define LS_PUNCT   1
#define LS_THIN    2
#define LS_THINNER 3
#define LS_LOWER   -0.0 /* usually just uses the default, but this is for overriding REF() */

#define PAD_FRACTION 1.0
#define PAD_SCRIPT   1.5
#define PAD_DIA      1.5
#define PAD_DEFAULT  3

#define VEC(x) {{ .data = (x), .size = cp_countof(x) }}

#define DRAW(type_t, ...) \
    ((font_draw_t const *)(type_t const []){{ \
        font_draw_typeof(*((type_t*)NULL)), \
        __VA_ARGS__ \
    }})

#define STROKE(...) \
    DRAW(font_draw_stroke_t, VEC(((font_def_vertex_t const []){ __VA_ARGS__ })))

#define REF(unicode) \
    DRAW(font_draw_ref_t, unicode)

#define WIDTH(unicode) \
    DRAW(font_draw_width_t, unicode)

#define LPAD(unicode) \
    DRAW(font_draw_lpad_t, unicode)

#define RPAD(unicode) \
    DRAW(font_draw_rpad_t, unicode)

#define XFORM(xform, draw) \
    DRAW(font_draw_xform_t, xform, draw, .a=0)

#define XFORM1(xform, A, draw) \
    DRAW(font_draw_xform_t, xform, draw, .a=A)

#define XFORM2(xform, A, B, draw) \
    DRAW(font_draw_xform_t, xform, draw, .a=A, .b=B)

#define MERGE(...) \
    DRAW(font_draw_merge_t, VEC(((font_draw_t const *const[]){ __VA_ARGS__ })))

#define SEQ(...) \
    DRAW(font_draw_sequence_t, VEC(((font_subglyph_t const *const[]){ __VA_ARGS__ })))

#define SUBGLYPH(A,B) ((font_subglyph_t const[]){{ .unicode=B, .kern=(A) }})

#define SUBGLYPH_(A,...) ((font_subglyph_t const[]){{ .unicode=__VA_ARGS__, .kern=(A) }})

#define SAME(...) SEQ(SUBGLYPH_(0,__VA_ARGS__))

#define UNICODE(X,Y) { .code_point = X, .name = Y }

#define COORD(...) ((font_def_coord_t const []){{ __VA_ARGS__, ._line = __LINE__ }})

#define COORD_(...) { __VA_ARGS__ }

#define MAP(...) VEC(((font_def_map_t[]){ __VA_ARGS__ }))

#define CANON(A,B) { .type=MAP_TYPE_CANON,  .a=A, .b=B }

#define MAND(A,B)  { .type=MAP_TYPE_MANDATORY, .a=A, .b=B }
#define JOIN(A,B)  { .type=MAP_TYPE_JOINING,   .a=A, .b=B }
#define LIGA(A,B)  { .type=MAP_TYPE_LIGATURE,  .a=A, .b=B }
#define OPT(A,B)   { .type=MAP_TYPE_OPTIONAL,  .a=A, .b=B }

#define MAND_KEEP(A,B)  { .type=MAP_TYPE_MANDATORY_KEEP, .a=A, .b=B }
#define JOIN_KEEP(A,B)  { .type=MAP_TYPE_JOINING_KEEP,   .a=A, .b=B }
#define LIGA_KEEP(A,B)  { .type=MAP_TYPE_LIGATURE_KEEP,  .a=A, .b=B }
#define OPT_KEEP(A,B)   { .type=MAP_TYPE_OPTIONAL_KEEP,  .a=A, .b=B }

#define WITH_ABOVE(A) { .type=MAP_TYPE_BASE_REPLACE, .a=A, .value=CP_FONT_MAS_HAVE_ABOVE }
#define WITH_BELOW(A) { .type=MAP_TYPE_BASE_REPLACE, .a=A, .value=CP_FONT_MAS_HAVE_BELOW }
#define WITH_BOTH(A)  { .type=MAP_TYPE_BASE_REPLACE, .a=A, .value=CP_FONT_MAS_HAVE_BOTH  }

#define CONTEXT(A,B)  { .type=MAP_TYPE_CONTEXT,  .a=A, .b=B }
#define KERN(A,B)     { .type=MAP_TYPE_KERNING,  .a=A, .amount=B }

#define LANG_ONLY_MAND(L,A,B)  { .type=MAP_TYPE_MANDATORY, .a=A, .b=B, .lang=L }
#define LANG_ONLY_JOIN(L,A,B)  { .type=MAP_TYPE_JOINING,   .a=A, .b=B, .lang=L }
#define LANG_ONLY_LIGA(L,A,B)  { .type=MAP_TYPE_LIGATURE,  .a=A, .b=B, .lang=L }
#define LANG_ONLY_OPT(L,A,B)   { .type=MAP_TYPE_OPTIONAL,  .a=A, .b=B, .lang=L }

#define LANG_REPLACE(L,A)      { .type=MAP_TYPE_REPLACE,   .a=A,       .lang=L }

/* By default, all lang specific ligatures/joinings/pairs will get an optional
 * entry in the global table.
 * We cannot invoke macros from LANG_ONLY_* etc, because the parameters must not
 * be macro expanded because the , within {} would cause errors...  Weird C
 * syntax...
 */
#define LANG_MAND(L,A,B) \
    { .type=MAP_TYPE_MANDATORY, .a=A, .b=B, .lang=L }, \
    { .type=MAP_TYPE_OPTIONAL,  .a=A, .b=B }

#define LANG_JOIN(L,A,B) \
    { .type=MAP_TYPE_JOINING,  .a=A, .b=B, .lang=L }, \
    { .type=MAP_TYPE_OPTIONAL, .a=A, .b=B }

#define LANG_LIGA(L,A,B) \
    { .type=MAP_TYPE_LIGATURE, .a=A, .b=B, .lang=L }, \
    { .type=MAP_TYPE_OPTIONAL, .a=A, .b=B }

#define Q_2_(P,X,Y) { .type = (P), .x = COORD_ X, .y = COORD_ Y }
#define Q_1_(P,X,Y) Q_2_(P,X,Y)
#define Q(P,X,Y)    Q_1_(P,X,Y)

#define REF_DOT_ABOVE          XFORM(ls_lower, REF(U_COMBINING_DOT_ABOVE))
#define REF_CAPITAL_DOT_ABOVE  XFORM(ls_lower, REF(UX_COMBINING_CAPITAL_DOT_ABOVE))

#define REF_DIAERESIS          XFORM(ls_lower, REF(U_COMBINING_DIAERESIS))
#define REF_CAPITAL_DIAERESIS  XFORM(ls_lower, REF(UX_COMBINING_CAPITAL_DIAERESIS))

/* tone contour Y coordinates */
#define Y_CONTOUR_EXTRA_HIGH (+3,-3,+6,60)
#define Y_CONTOUR_HIGH       (+1,-3,+6,45)
#define Y_CONTOUR_MID        ( 0,-3,+6,30)
#define Y_CONTOUR_LOW        (-1,-3,+6,15)
#define Y_CONTOUR_EXTRA_LOW  (-3,-3,+6, 0)

#define RATIO_EM 0.7

#define lang_MAH  "MAH"
#define lang_NLD  "NLD"
#define lang_LIV  "LIV" /* ISO-639-3 code, no OpenType code found */

static void swap_x         (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void slant1         (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void superscript_lc (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void superscript    (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void subscript      (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void enclosed       (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void smallcap       (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void ls_lower       (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void ls_thin        (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void ls_thinner     (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void xlat           (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void xlat_relx      (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void xlat_rely      (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void invert_uc      (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void invert_lc      (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void reverse_lc     (font_t const *, font_gc_t *, font_draw_xform_t const *);
static void turn_lc        (font_t const *, font_gc_t *, font_draw_xform_t const *);

/* ********************************************************************** */

static font_def_glyph_t f1_a_glyph[] = {
    /* white space */
    {
        .unicode = U_ZERO_WIDTH_SPACE,
        .width_mul = -0.0,
        .min_coord = COORD(0,0,0,0),
        .max_coord = COORD(0,0,0,0),
        .draw = NULL,
    },
    {
         /* FIXME: once we do have ideographic characters, use the right width
          * Currently, this is like a capital W.
          */
        .unicode = U_IDEOGRAPHIC_SPACE,
        .line_step = LS_UPPER,
        .min_coord = COORD(+3,-9, 0, 0),
        .max_coord = COORD(+3,+9, 0, 0),
        .draw = NULL,
    },
    {
        .unicode = U_EM_SPACE,
        .center_coord = COORD(0,0,0,0),
        .min_coord_from_y = COORD(0, -3, 0, 0),
        .max_coord_from_y = COORD(0, +6, 0, 0),
        .lpad_abs = -0.0,
        .rpad_abs = -0.0,
        .width_mul = 1/RATIO_EM,
        .draw = NULL,
    },
    {
        .unicode = U_FIGURE_SPACE,
        .mono = true,
        .draw = WIDTH(U_DIGIT_ZERO),
    },
    {
        .unicode = U_PUNCTUATION_SPACE,
        .lpad_abs = -0.0,
        .rpad_abs = -0.0,
        .draw = WIDTH(U_FULL_STOP),
    },
    {
        .unicode = U_MIDDLE_DOT,
        .line_step = LS_PUNCT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+1,0,0, -30)),
            Q(E, ( 0, 0, 0, 0), ( 0,+1,0,0, +30)),
        ),
    },
    {
        .unicode = U_EN_SPACE,
        .width_mul = 1./2.,
        .draw = WIDTH(U_EM_SPACE),
    },
    {
        .unicode = U_THREE_PER_EM_SPACE,
        .width_mul = 1./3.,
        .draw = WIDTH(U_EM_SPACE),
    },
    {
        .unicode = U_FOUR_PER_EM_SPACE,
        .width_mul = 1./4.,
        .draw = WIDTH(U_EM_SPACE),
    },
    {
        .unicode = U_THIN_SPACE,
        .width_mul = 1./5.,
        .draw = WIDTH(U_EM_SPACE),
    },
    {
        .unicode = U_SIX_PER_EM_SPACE,
        .width_mul = 1./6.,
        .draw = WIDTH(U_EM_SPACE),
    },
    {
        .unicode = U_HAIR_SPACE,
        .width_mul = 1./10.,
        .draw = WIDTH(U_EM_SPACE),
    },
    {
        .unicode = U_MEDIUM_MATHEMATICAL_SPACE,
        .width_mul = 4./18.,
        .draw = WIDTH(U_EM_SPACE),
    },
    {
        .unicode = U_SPACE,
        .draw = WIDTH(U_THREE_PER_EM_SPACE),
    },
    {
        .unicode = U_NO_BREAK_SPACE,
        .draw = WIDTH(U_SPACE),
    },
    {
        .unicode = U_NARROW_NO_BREAK_SPACE,
        .draw = WIDTH(U_THIN_SPACE),
    },
    {
        .unicode = U_EM_QUAD,
        .draw = WIDTH(U_EM_SPACE),
    },
    {
        .unicode = U_EN_QUAD,
        .draw = WIDTH(U_EN_SPACE),
    },
    {
        .unicode = U_ZERO_WIDTH_NON_JOINER,
        .draw = WIDTH(U_ZERO_WIDTH_SPACE),
    },
    {
        .unicode = U_ZERO_WIDTH_JOINER,
        .draw = WIDTH(U_ZERO_WIDTH_SPACE),
    },
    {
        .unicode = U_WORD_JOINER,
        .draw = WIDTH(U_ZERO_WIDTH_SPACE),
    },
    {
        .unicode = U_ZERO_WIDTH_NO_BREAK_SPACE,
        .draw = WIDTH(U_ZERO_WIDTH_SPACE),
    },
    {
        .unicode = U_SOFT_HYPHEN,
        .draw = SEQ(SUBGLYPH(0,U_HYPHEN_MINUS)),
    },
    {
        .unicode = U_FIGURE_DASH,
        .mono = true,
        .draw = MERGE(
          WIDTH(U_DIGIT_ZERO),
          STROKE(
            Q(B, (+3,-6,0,0), ( 0,-3,+3,30)),
            Q(E, (+3,+6,0,0), ( 0,-3,+3,30))
          ),
        ),
    },
    {
        .unicode = U_HORIZONTAL_BAR,
        .draw = MERGE(
          WIDTH(U_EM_SPACE),
          STROKE(
            Q(B, (0,0,0,0,.olen={-3,+6,+(int)((30/RATIO_EM)+0.999)}), ( 0,-3,+3,30)),
            Q(E, (0,0,0,0,.olen={-3,+6,-(int)((30/RATIO_EM)+0.999)}), ( 0,-3,+3,30))
          ),
        ),
    },
    {
        .unicode = U_EM_DASH,
        .draw = MERGE(
          WIDTH(U_EM_SPACE),
          STROKE(
            Q(B, (0,0,0,0,-40,.olen={-3,+6,+(int)((30/RATIO_EM)+0.999)}), ( 0,-3,+3,30)),
            Q(E, (0,0,0,0,+40,.olen={-3,+6,-(int)((30/RATIO_EM)+0.999)}), ( 0,-3,+3,30))
          ),
        ),
    },
    {
        .unicode = U_EN_DASH,
        .draw = MERGE(
          WIDTH(U_EN_SPACE),
          STROKE(
            Q(B, (0,0,0,0,-40,.olen={-3,+6,+(int)((15/RATIO_EM)+0.999)}), ( 0,-3,+3,30)),
            Q(E, (0,0,0,0,+40,.olen={-3,+6,-(int)((15/RATIO_EM)+0.999)}), ( 0,-3,+3,30))
          ),
        ),
    },

    /* special characters */
    {
        .unicode = U_OPEN_BOX,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,-2, 0, 0)),
            Q(P, ( 0,-5, 0, 0), ( 0,-4, 0, 0)),
            Q(P, ( 0,+5, 0, 0), ( 0,-4, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,-2, 0, 0)),
        ),
    },
    {
        .unicode = U_REPLACEMENT_CHARACTER,
        .draw = STROKE(
            Q(L, ( 0,-12,+12,30), (+3,+7, 0, 0)),
            Q(L, (+3,-12,  0, 0), ( 0,+7,-5,30)),
            Q(L, ( 0,-12,+12,30), (+3,-5, 0, 0)),
            Q(L, (+3,+12,  0, 0), ( 0,+7,-5,30)),
            Q(N, ( 0,  0,  0, 0), ( 0, 0, 0, 0)),
            Q(B, ( 0,  0,  0, 0), ( 0,-5,+7,18)),
            Q(E, ( 0,  0,  0, 0), ( 0,-5,+7,18, +60)),
            Q(I, ( 0,-12,+12,20), ( 0,-5,+7,38)),
            Q(S, ( 0,-12,+12,30), ( 0,-5,+7,48)),
            Q(S, ( 0,-12,+12,40), ( 0,-5,+7,38)),
            Q(P, ( 0,-12,+12,30), ( 0,-5,+7,28)),
            Q(E, ( 0,-12,+12,30), ( 0,-5,+7,23)),
        ),
    },

    /* punctuation */
    {
        .unicode = U_FULL_STOP,
        .line_step = LS_PUNCT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0, -60)),
        ),
    },
    {
        .unicode = U_COMMA,
        .line_step = LS_PUNCT,
        // .min_coord = COORD(-6,0,0,0),
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0, -60)),
            Q(P, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0,-3, 0, 0), ( 0,-5, 0, 0)),
        ),
    },
    {
        .unicode = U_SEMICOLON,
        .line_step = LS_PUNCT,
        // .min_coord = COORD(-6,0,0,0),
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0, -60)),
            Q(P, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0,-3, 0, 0), ( 0,-5, 0, 0)),
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+2, 0, 0, -60)),
        ),
    },
    {
        .unicode = U_COLON,
        .line_step = LS_PUNCT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0, -60)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+2, 0, 0, -60)),
        ),
    },
    {
        .unicode = U_EXCLAMATION_MARK,
        .line_step = LS_PUNCT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0, -60)),
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0, -135)),
            Q(E, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_INVERTED_EXCLAMATION_MARK,
        .line_step = LS_PUNCT,
        .draw = XFORM(invert_lc, REF(U_EXCLAMATION_MARK)),
    },
    {
        .unicode = U_QUESTION_MARK,
        .line_step = LS_PUNCT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0, -60)),
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0, -135)),
            Q(L, ( 0, 0, 0, 0), ( 0,-3, 0, 0, -190)),
            Q(S, ( 0,+5, 0, 0), ( 0,+2,+3,30)),
            Q(L, ( 0,+5, 0, 0), (-2,+6, 0, 0)),
            Q(S, (+1,-2, 0, 0), (-2,+6, 0, 0)),
            Q(E, (+1,-5, 0, 0), ( 0,+5, 0, 0)),
        ),
    },
    {
        .unicode = U_INVERTED_QUESTION_MARK,
        .line_step = LS_PUNCT,
        .draw = XFORM(turn_lc, REF(U_QUESTION_MARK)),
    },
    {
        .unicode = U_SOLIDUS,
        .min_coord = COORD(0,-4,0,0),
        .max_coord = COORD(0,+4,0,0),
        .draw = STROKE(
            Q(B, ( 0,+4, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0,-4, 0, 0), ( 0,-4, 0, 0)),
        ),
    },
    {
        .unicode = U_REVERSE_SOLIDUS,
        .min_coord = COORD(0,-4,0,0),
        .max_coord = COORD(0,+4,0,0),
        .draw = XFORM(swap_x, REF(U_SOLIDUS)),
    },
    {
        .unicode = U_VERTICAL_LINE,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), (+1,+6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), (+1,-5, 0, 0)),
        ),
    },
    {
        .unicode = U_BROKEN_BAR,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+6,-4,30, +45)),
            Q(B, ( 0, 0, 0, 0), ( 0,+6,-4,30, -45)),
            Q(E, ( 0, 0, 0, 0), ( 0,-4, 0, 0)),
        ),
    },
    {
        .unicode = U_HYPHEN_MINUS,
        .draw = STROKE(
            Q(B, ( 0,+5, 0, 0), ( 0,-3,+3,30)),
            Q(E, ( 0,-5, 0, 0), ( 0,-3,+3,30)),
        ),
    },
    {
        .unicode = U_HYPHEN,
        .draw = SAME(U_HYPHEN_MINUS),
    },
    {
        .unicode = U_NON_BREAKING_HYPHEN,
        .draw = SAME(U_HYPHEN_MINUS),
    },
    {
        /* also called 'SPACING UNDERBAR', but is has no space in most fonts */
        .unicode = U_LOW_LINE,
        .min_coord = COORD(0,-8,0,0),
        .max_coord = COORD(0,+8,0,0),
        .lpad_abs = -0.0,
        .rpad_abs = -0.0,
        .draw = STROKE(
            Q(B, (+2,-8, 0, 0), (+2,-4, 0, 0)),
            Q(E, (+2,+8, 0, 0), (+2,-4, 0, 0)),
        ),
    },
    {
        .unicode = U_QUOTATION_MARK,
        .draw = STROKE(
            Q(B, (+3,-2, 0, 0), ( 0,+6, 0, 0)),
            Q(E, (+3,-2, 0, 0), ( 0,+3, 0, 0)),
            Q(B, (+3,+2, 0, 0), ( 0,+6, 0, 0)),
            Q(E, (+3,+2, 0, 0), ( 0,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_APOSTROPHE,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_SINGLE_LOW_9_QUOTATION_MARK,
        .draw = SAME(U_COMMA),
    },
    {
        .unicode = U_RIGHT_SINGLE_QUOTATION_MARK,
        .line_step = LS_PUNCT,
        //.min_coord = COORD(-6,0,0,0),
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0,+6, 0, 0, -60)),
            Q(E, ( 0,-3, 0, 0), ( 0,+6, 0, 0, -60, .len = { -3, -5, 60 })),
        ),
    },
    {
        .unicode = U_SINGLE_HIGH_REVERSED_9_QUOTATION_MARK,
        .line_step = LS_PUNCT,
        //.max_coord = COORD(+6,0,0,0),
        .draw = XFORM(swap_x, REF(U_RIGHT_SINGLE_QUOTATION_MARK)),
    },
    {
        .unicode = U_LEFT_SINGLE_QUOTATION_MARK,
        .line_step = LS_PUNCT,
        //.max_coord = COORD(+6,0,0,0),
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0, -60, .len = { -3, -5, 60 })),
            Q(P, ( 0, 0, 0, 0), ( 0,+4, 0, 0)),
            Q(E, ( 0,+3, 0, 0), ( 0,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_TURNED_COMMA,
        .line_step = LS_PUNCT,
        //.max_coord = COORD(+6,0,0,0),
        .draw = SAME(U_LEFT_SINGLE_QUOTATION_MARK),
    },
    {
        .unicode = U_MODIFIER_LETTER_APOSTROPHE,
        .line_step = LS_PUNCT,
        //.max_coord = COORD(+6,0,0,0),
        .draw = SAME(U_RIGHT_SINGLE_QUOTATION_MARK),
    },
    {
        .unicode = U_MODIFIER_LETTER_REVERSED_COMMA,
        .line_step = LS_PUNCT,
        //.max_coord = COORD(+6,0,0,0),
        .draw = SAME(U_SINGLE_HIGH_REVERSED_9_QUOTATION_MARK),
    },
    {
        .unicode = U_DOUBLE_LOW_9_QUOTATION_MARK,
        .line_step = LS_PUNCT,
        .draw = STROKE(
            Q(B, (+3,-2,0,0),                 (0,-3,0,0, -60)),
            Q(P, (+3,-2,0,0),                 (0,-3,0,0)),
            Q(E, (+3,-2,0,0, .len={0,-3,60}), (0,-5,0,0)),

            Q(B, (+3,+2,0,0),                 (0,-3,0,0, -60)),
            Q(P, (+3,+2,0,0),                 (0,-3,0,0)),
            Q(E, (+3,+2,0,0, .len={0,-3,60}), (0,-5,0,0)),
        ),
    },
    {
        .unicode = U_RIGHT_DOUBLE_QUOTATION_MARK,
        .line_step = LS_PUNCT,
        .draw = STROKE(
            Q(B, (+3,-2,0,0), (0,+6,0,0)),
            Q(P, (+3,-2,0,0), (0,+6,0,0,-60)),
            Q(E, (+3,-2,0,0, .len={0,-3,60}), (0,+6,0,0,-60, .len={-3,-5,60})),

            Q(B, (+3,+2,0,0), (0,+6,0,0)),
            Q(P, (+3,+2,0,0), (0,+6,0,0,-60)),
            Q(E, (+3,+2,0,0, .len={0,-3,60}), (0,+6,0,0,-60, .len={-3,-5,60})),
        ),
    },
    {
        .unicode = U_DOUBLE_HIGH_REVERSED_9_QUOTATION_MARK,
        .line_step = LS_PUNCT,
        .draw = XFORM(swap_x, REF(U_RIGHT_DOUBLE_QUOTATION_MARK)),
    },
    {
        .unicode = U_LEFT_DOUBLE_QUOTATION_MARK,
        .line_step = LS_PUNCT,
        .draw = STROKE(
            Q(B, (+3,-2,0,0), (0,+6,0,0,-60, .len={-3,-5,60})),
            Q(P, (+3,-2,0,0), (0,+4,0,0)),
            Q(E, (+3,-2,0,0, .len={0,+3,60}), (0,+6,0,0)),

            Q(B, (+3,+2,0,0), (0,+6,0,0,-60, .len={-3,-5,60})),
            Q(P, (+3,+2,0,0), (0,+4,0,0)),
            Q(E, (+3,+2,0,0, .len={0,+3,60}), (0,+6,0,0)),
        ),
    },
    {
        .unicode = U_SINGLE_LEFT_POINTING_ANGLE_QUOTATION_MARK,
        .line_step = LS_THIN,
        //.max_coord = COORD(0,-1,0,0),
        //.min_coord = COORD(0,-6,0,0),
        .draw = STROKE(
            Q(I, ( 0, 0, 0, 0), ( 0,-3,-2,40)),
            Q(D, ( 0,-1, 0, 0), ( 0, 0, 0, 0)),
            Q(O, ( 0, 0, 0, 0), ( 0,+3,+2,40)),
        ),
    },
    {
        .unicode = U_SINGLE_RIGHT_POINTING_ANGLE_QUOTATION_MARK,
        .line_step = LS_THIN,
        //.min_coord = COORD(0,+1,0,0),
        //.max_coord = COORD(0,+6,0,0),
        .draw = XFORM(swap_x, REF(U_SINGLE_LEFT_POINTING_ANGLE_QUOTATION_MARK)),
    },
    {
        .unicode = U_LEFT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK,
        //.min_coord = COORD(0,-9,0,0),
        //.max_coord = COORD(0,+3,0,0),
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, (+2,-3, 0, 0), ( 0,-3,-2,40)),
            Q(D, ( 0,-8, 0, 0), ( 0, 0, 0, 0)),
            Q(O, (+2,-3, 0, 0), ( 0,+3,+2,40)),
            Q(I, (+2,+3, 0, 0), ( 0,-3,-2,40)),
            Q(D, ( 0,-8, 0, 0), ( 0, 0, 0, 0)),
            Q(O, (+2,+3, 0, 0), ( 0,+3,+2,40)),
        ),
    },
    {
        .unicode = U_RIGHT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK,
        //.min_coord = COORD(0,-3,0,0),
        //.max_coord = COORD(0,+9,0,0),
        .line_step = LS_THIN,
        .draw = XFORM(swap_x, REF(U_LEFT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK)),
    },
    {
        .unicode = U_MASCULINE_ORDINAL_INDICATOR,
        .draw = MERGE(
            XFORM(superscript_lc, REF(U_LATIN_SMALL_LETTER_O)),
            STROKE(
              Q(B, ( 0,-5, 0, 0), ( 0, 0, 0, 0)),
              Q(E, ( 0,+5, 0, 0), ( 0, 0, 0, 0)),
            ),
        ),
    },
    {
        .unicode = U_FEMININE_ORDINAL_INDICATOR,
        .draw = MERGE(
            XFORM(superscript_lc, REF(U_LATIN_SMALL_LETTER_A)),
            STROKE(
              Q(B, ( 0,-5, 0, 0), ( 0, 0, 0, 0)),
              Q(E, ( 0,+5, 0, 0), ( 0, 0, 0, 0)),
            ),
        ),
    },

    /* number or currency related signs */
    {
        .unicode = U_NUMBER_SIGN,
        .draw = XFORM(slant1, STROKE(
            Q(B, ( 0,+4, 0, 0), ( 0,+3, 0, 0, .len = { -3, -1, 60 })),
            Q(E, ( 0,+4, 0, 0), ( 0,-3, 0, 0)),
            Q(B, ( 0,-4, 0, 0), ( 0,+3, 0, 0, .len = { -3, -1, 60 })),
            Q(E, ( 0,-4, 0, 0), ( 0,-3, 0, 0)),
            Q(B, ( 0,-9, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0,+9, 0, 0), (-2,+3, 0, 0)),
            Q(B, ( 0,-9, 0, 0), (-2,-1, 0, 0)),
            Q(E, ( 0,+9, 0, 0), (-2,-1, 0, 0)),
        )),
    },
    {
        .unicode = U_DOLLAR_SIGN,
        .draw = STROKE(
            Q(I, (+1,+6, 0, 0), ( 0,+3, 0, 0)),
            Q(R, ( 0,+3, 0, 0), (-2,+5, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (-2,+5, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (-1,-3,+5,30)),
            Q(L, ( 0,+6, 0, 0), (+1,-3,+5,30)),
            Q(L, ( 0,+6, 0, 0), (-2,-3, 0, 0)),
            Q(R, ( 0,-3, 0, 0), (-2,-3, 0, 0)),
            Q(O, (+1,-6, 0, 0), ( 0,-2, 0, 0)),
            Q(B, ( 0, 0, 0, 0), ( 0,+5, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+5, 0, 0, .len = { -3, -5, -60 })),
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-5, 0, 0)),
        ),
    },
    {
        .unicode = U_EURO_SIGN,
        .max_coord = COORD(+1,+7,0,0),
        .min_coord = COORD( 0,-9,0,0),
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(I, (+1, +7, 0, 0), ( 0,+5, 0, 0)),
            Q(R, (+1, +3, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0, -7, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0, -7, 0, 0), (-2,-3, 0, 0)),
            Q(S, (+1, +3, 0, 0), (-2,-3, 0, 0)),
            Q(O, (+1, +7, 0, 0), ( 0,-2, 0, 0)),
            Q(B, ( 0,-10, 0, 0), (-1,-3,+6,35)),
            Q(E, ( 0, +5, 0, 0), (-1,-3,+6,35)),
            Q(B, ( 0,-10, 0, 0), (+1,-3,+6,25)),
            Q(E, ( 0, +3, 0, 0), (+1,-3,+6,25)),
        ),
    },
    {
        .unicode = U_TRADE_MARK_SIGN,
        .draw = SEQ(
            SUBGLYPH(0, U_MODIFIER_LETTER_CAPITAL_T),
            SUBGLYPH(0, U_MODIFIER_LETTER_CAPITAL_M)
        ),
    },
    {
        .unicode = U_AMPERSAND,
        .max_coord = COORD(0,+9,0,0),
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(I, (+1, +4, 0, 0), ( 0,+5, 0, 0)),
            Q(S, (+1, +1, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0, -6, 0, 0), (-2,+6, 0, 0)),
            Q(T, ( 0, -6, 0, 0), ( 0,-3,+6,30)),
            Q(M, ( 0, +5, 0, 0), ( 0,-3,+6,30)),
            Q(T, ( 0, -6, 0, 0), ( 0,-3,+6,30)),
            Q(L, ( 0, -6, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0, +5, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0, +5, 0, 0), ( 0,-3,+6,30)),
            Q(E, ( 0,+10, 0, 0), ( 0,-3,+6,30)),
        ),
    },
    {
        .unicode = U_COMMERCIAL_AT,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(B, ( 0, -4, 0, 0), ( 0,+2, 0, 0)),
            Q(L, ( 0, +4, 0, 0), ( 0,+2, 0, 0)),
            Q(P, ( 0, +4, 0, 0), ( 0,-2, 0, 0)),
            Q(R, ( 0, -4, 0, 0), ( 0,-2, 0, 0)),
            Q(R, ( 0, -4, 0, 0), ( 0, 0, 0, 0)),
            Q(E, ( 0, +4, 0, 0), ( 0, 0, 0, 0)),

            Q(B, ( 0, +4, 0, 0), ( 0,-2, 0, 0)),
            Q(S, ( 0,+10, 0, 0), ( 0,-2, 0, 0)),
            Q(P, ( 0,+10, 0, 0), ( 0, 0, 0, 0)),
            Q(G, ( 0,+10, 0, 0), ( 0,+5, 0, 0)),
            Q(G, ( 0,-10, 0, 0), ( 0,+5, 0, 0)),
            Q(G, ( 0,-10, 0, 0), ( 0,-4,-5,30)),
            Q(E, ( 0, +1, 0, 0), ( 0,-4,-5,30)),
        ),
    },
    {
        .unicode = U_YEN_SIGN,
        .line_step = LS_THIN,
        .draw = MERGE(
          STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(A, ( 0,-7, 0, 0), (+2,+2, 0, 0)),
            Q(A, ( 0,+7, 0, 0), (+2,+2, 0, 0)),
            Q(E, ( 0,+7, 0, 0), ( 0,+6, 0, 0)),
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
          ),
          XFORM(ls_thinner,
            STROKE(
              Q(B, (+3,-7, 0, 0), (+1,-1, 0, 0)),
              Q(E, (+3,+7, 0, 0), (+1,-1, 0, 0)),

              Q(B, (+3,-7, 0, 0), (+1,+1, 0,30)),
              Q(E, (+3,+7, 0, 0), (+1,+1, 0,30)),
            )
          ),
        )
    },
    {
        .unicode = U_CENT_SIGN,
        .draw = MERGE(
          REF(U_LATIN_SMALL_LETTER_C),
          XFORM(ls_thinner,
            STROKE(
              Q(B, ( 0,+1, 0, 0), ( 0,-5, 0, 0)),
              Q(E, ( 0,+1, 0, 0), ( 0, 0, 0, 0, .len={-5,0,+60})),
            )
          )
        ),
    },
    {
        .unicode = U_POUND_SIGN,
        .min_coord = COORD(0,-7,0,0),
        .draw = STROKE(
            Q(B, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
            Q(P, (-2,-8, 0, 0), (-2,-3, 0, 0)),
            Q(P, (-2,-8, 0, 0), ( 0,-2, 0, 0)),
            Q(P, ( 0,-4, 0, 0), ( 0, 0, 0, 0)),
            Q(H, ( 0,-4, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,+1, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,+3,+4,30), (-2,+6, 0, 0)),
            Q(O, ( 0,+5, 0, 0), ( 0,+5, 0, 0)),

            Q(B, ( 0,-8, 0, 0), ( 0,+1,+2,20)),
            Q(E, ( 0,+3, 0, 0), ( 0,+1,+2,20)),
        ),
    },
    {
        .unicode = U_CURRENCY_SIGN,
        .line_step = LS_THIN,
        .min_coord = COORD(0,-7,0,0),
        .max_coord = COORD(0,+7,0,0),
        .draw = STROKE(
            Q(L, ( 0,-7,+7, 0), ( 0,+4,-1,30)),
            Q(P, ( 0,-7,+7,15), ( 0,+4,-1,45)),
            Q(L, ( 0,-7,+7,30), ( 0,+4,-1,60)),
            Q(P, ( 0,-7,+7,45), ( 0,+4,-1,45)),
            Q(L, ( 0,-7,+7,60), ( 0,+4,-1,30)),
            Q(P, ( 0,-7,+7,45), ( 0,+4,-1,15)),
            Q(L, ( 0,-7,+7,30), ( 0,+4,-1, 0)),
            Q(P, ( 0,-7,+7,15), ( 0,+4,-1,15)),

            Q(N, (0), (0)),

            Q(I, ( 0,-7,+7,15), ( 0,+4,-1,45)),
            Q(O, ( 0,-7,+7,-5), ( 0,+4,-1,65)),

            Q(I, ( 0,-7,+7,15), ( 0,+4,-1,15)),
            Q(O, ( 0,-7,+7,-5), ( 0,+4,-1,-5)),

            Q(I, ( 0,-7,+7,45), ( 0,+4,-1,45)),
            Q(O, ( 0,-7,+7,65), ( 0,+4,-1,65)),

            Q(I, ( 0,-7,+7,45), ( 0,+4,-1,15)),
            Q(O, ( 0,-7,+7,65), ( 0,+4,-1,-5)),
        ),
    },
    {
        .unicode = U_MICRO_SIGN,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,+3, 0, 0)),
            Q(E, ( 0,-5, 0, 0), ( 0,-6, 0, 0)),
            Q(B, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_GREEK_CAPITAL_LETTER_OMEGA, // = OHM SIGN
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, (+3,-7, 0, 0), (-2,-3, 0, 0)),
            Q(P, (+2,-2, 0, 0), (-2,-3, 0, 0)),
            Q(P, (+2,-2, 0, 0), ( 0,-1, 0, 0)),
            Q(S, ( 0,-7, 0, 0), ( 0, 0, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(S, ( 0,+7, 0, 0), ( 0, 0, 0, 0)),
            Q(P, (+2,+2, 0, 0), ( 0,-1, 0, 0)),
            Q(P, (+2,+2, 0, 0), (-2,-3, 0, 0)),
            Q(E, (+3,+7, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_PILCROW_SIGN,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(H, ( 0,-7, 0, 0), ( 0,+2, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-4, 0, 0)),

            Q(B, ( 0,+7, 0, 0), ( 0,-4, 0, 0)),
            Q(P, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), (-2,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_DEGREE_SIGN,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(H, ( 0,-4, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,+4, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,+4, 0, 0), (+2,+2, 0, 0)),
            Q(H, ( 0,-4, 0, 0), (+2,+2, 0, 0)),
        ),
    },
    {
        .unicode = U_COPYRIGHT_SIGN,
        .draw = MERGE(
            XFORM(enclosed,
                REF(U_LATIN_CAPITAL_LETTER_C)
            ),
            REF(U_COMBINING_ENCLOSING_CIRCLE)
        ),
    },
    {
        .unicode = U_REGISTERED_SIGN,
        .draw = MERGE(
            XFORM(enclosed,
                REF(U_LATIN_CAPITAL_LETTER_R)
            ),
            REF(U_COMBINING_ENCLOSING_CIRCLE)
        ),
    },
    {
        .unicode = U_SECTION_SIGN,
        .draw = STROKE(
            Q(I, (+1,+6, 0, 0), ( 0,+5, 0, 0)),
            Q(R, ( 0,+3, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (-1,-6,+6,40)),
            Q(E, ( 0, 0, 0, 0), ( 0,-6,+6,40)),

            Q(N,(0),(0)),

            Q(I, (+1,-6, 0, 0), ( 0,-5, 0, 0)),
            Q(R, ( 0,-3, 0, 0), (-2,-6, 0, 0)),
            Q(L, ( 0,+6, 0, 0), (-2,-6, 0, 0)),
            Q(L, ( 0,+6, 0, 0), (-1,+6,-6,40)),
            Q(E, ( 0, 0, 0, 0), ( 0,+6,-6,40)),

            Q(N,(0),(0)),

            Q(P, ( 0, 0, 0, 0), ( 0,+6,-6,40)),
            Q(L, ( 0,-7, 0, 0), ( 0,+6,-6,40)),
            Q(L, ( 0,-7, 0, 0), ( 0,+6,-6,20)),
            Q(P, ( 0, 0, 0, 0), ( 0,+6,-6,20)),
            Q(L, ( 0,+7, 0, 0), ( 0,+6,-6,20)),
            Q(L, ( 0,+7, 0, 0), ( 0,+6,-6,40)),
        ),
    },

    /* fractions and other small numbers */
    {
        .unicode = U_FRACTION_SLASH,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(B, ( 0,+6, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0,-6, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = UX_NARROW_FRACTION_SLASH,
        .min_coord = COORD(-2,0,0,0),
        .max_coord = COORD(+2,0,0,0),
        .lpad_abs = PAD_FRACTION,
        .rpad_abs = PAD_FRACTION,
        .line_step = LS_THIN,
        .draw = REF(U_FRACTION_SLASH),
    },
    {
        .unicode = UX_FRACTION_ZERO,
        .draw = SEQ(
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH),
            SUBGLYPH(0, U_SUBSCRIPT_ZERO)
        ),
    },
    {
        .unicode = UX_FRACTION_ONE,
        .draw = SEQ(
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH),
            SUBGLYPH(0, U_SUBSCRIPT_ONE)
        ),
    },
    {
        .unicode = UX_FRACTION_TWO,
        .draw = SEQ(
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH),
            SUBGLYPH(0, U_SUBSCRIPT_TWO)
        ),
    },
    {
        .unicode = UX_FRACTION_THREE,
        .draw = SEQ(
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH),
            SUBGLYPH(0, U_SUBSCRIPT_THREE)
        ),
    },
    {
        .unicode = UX_FRACTION_FOUR,
        .draw = SEQ(
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH),
            SUBGLYPH(0, U_SUBSCRIPT_FOUR)
        ),
    },
    {
        .unicode = UX_FRACTION_FIVE,
        .draw = SEQ(
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH),
            SUBGLYPH(0, U_SUBSCRIPT_FIVE)
        ),
    },
    {
        .unicode = UX_FRACTION_SIX,
        .draw = SEQ(
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH),
            SUBGLYPH(0, U_SUBSCRIPT_SIX)
        ),
    },
    {
        .unicode = UX_FRACTION_SEVEN,
        .draw = SEQ(
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH),
            SUBGLYPH(0, U_SUBSCRIPT_SEVEN)
        ),
    },
    {
        .unicode = UX_FRACTION_EIGHT,
        .draw = SEQ(
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH),
            SUBGLYPH(0, U_SUBSCRIPT_EIGHT)
        ),
    },
    {
        .unicode = UX_FRACTION_NINE,
        .draw = SEQ(
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH),
            SUBGLYPH(0, U_SUBSCRIPT_NINE)
        ),
    },
    {
        .unicode = U_PERCENT_SIGN,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ZERO),
            SUBGLYPH(0, UX_FRACTION_ZERO)
        ),
    },
    {
        .unicode = U_PER_MILLE_SIGN,
        .draw = SEQ(
            SUBGLYPH(0, U_PERCENT_SIGN),
            SUBGLYPH(0, U_SUBSCRIPT_ZERO)
        ),
    },
    {
        .unicode = U_PER_TEN_THOUSAND_SIGN,
        .draw = SEQ(
            SUBGLYPH(0, U_PER_MILLE_SIGN),
            SUBGLYPH(0, U_SUBSCRIPT_ZERO)
        ),
    },
    {
        .unicode = U_FRACTION_NUMERATOR_ONE,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ONE),
            SUBGLYPH(0, UX_NARROW_FRACTION_SLASH)
        ),
    },
    {
        .unicode = UX_VULGAR_FRACTION_ONE_WHOLE,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ONE),
            SUBGLYPH(0, UX_FRACTION_ONE)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_ONE_HALF,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ONE),
            SUBGLYPH(0, UX_FRACTION_TWO)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_ONE_THIRD,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ONE),
            SUBGLYPH(0, UX_FRACTION_THREE)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_ZERO_THIRDS,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ZERO),
            SUBGLYPH(0, UX_FRACTION_THREE)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_TWO_THIRDS,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_TWO),
            SUBGLYPH(0, UX_FRACTION_THREE)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_ONE_QUARTER,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ONE),
            SUBGLYPH(0, UX_FRACTION_FOUR)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_THREE_QUARTERS,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_THREE),
            SUBGLYPH(0, UX_FRACTION_FOUR)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_ONE_FIFTH,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ONE),
            SUBGLYPH(0, UX_FRACTION_FIVE)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_TWO_FIFTHS,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_TWO),
            SUBGLYPH(0, UX_FRACTION_FIVE)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_THREE_FIFTHS,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_THREE),
            SUBGLYPH(0, UX_FRACTION_FIVE)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_FOUR_FIFTHS,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_FOUR),
            SUBGLYPH(0, UX_FRACTION_FIVE)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_ONE_SIXTH,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ONE),
            SUBGLYPH(0, UX_FRACTION_SIX)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_FIVE_SIXTHS,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_FIVE),
            SUBGLYPH(0, UX_FRACTION_SIX)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_ONE_SEVENTH,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ONE),
            SUBGLYPH(0, UX_FRACTION_SEVEN)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_ONE_NINTH,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ONE),
            SUBGLYPH(0, UX_FRACTION_NINE)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_ONE_TENTH,
        .draw = SEQ(
            SUBGLYPH(0, UX_VULGAR_FRACTION_ONE_WHOLE),
            SUBGLYPH(0, U_SUBSCRIPT_ZERO)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_ONE_EIGHTH,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_ONE),
            SUBGLYPH(0, UX_FRACTION_EIGHT)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_THREE_EIGHTHS,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_THREE),
            SUBGLYPH(0, UX_FRACTION_EIGHT)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_FIVE_EIGHTHS,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_FIVE),
            SUBGLYPH(0, UX_FRACTION_EIGHT)
        ),
    },
    {
        .unicode = U_VULGAR_FRACTION_SEVEN_EIGHTHS,
        .draw = SEQ(
            SUBGLYPH(0, U_SUPERSCRIPT_SEVEN),
            SUBGLYPH(0, UX_FRACTION_EIGHT)
        ),
    },

    /* superscript */
    {
        .unicode = U_SUPERSCRIPT_ZERO,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_DIGIT_ZERO)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_ONE,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            WIDTH(U_SUPERSCRIPT_ZERO),
            XFORM(superscript, REF(U_DIGIT_ONE)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_TWO,
        .draw = MERGE(
            WIDTH(U_SUPERSCRIPT_ZERO),
            XFORM(superscript, REF(U_DIGIT_TWO)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_THREE,
        .draw = MERGE(
            WIDTH(U_SUPERSCRIPT_ZERO),
            XFORM(superscript, REF(U_DIGIT_THREE)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_FOUR,
        .draw = MERGE(
            WIDTH(U_SUPERSCRIPT_ZERO),
            XFORM(superscript, REF(U_DIGIT_FOUR)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_FIVE,
        .draw = MERGE(
            WIDTH(U_SUPERSCRIPT_ZERO),
            XFORM(superscript, REF(U_DIGIT_FIVE)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_SIX,
        .draw = MERGE(
            WIDTH(U_SUPERSCRIPT_ZERO),
            XFORM(superscript, REF(U_DIGIT_SIX)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_SEVEN,
        .draw = MERGE(
            WIDTH(U_SUPERSCRIPT_ZERO),
            XFORM(superscript, REF(U_DIGIT_SEVEN)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_EIGHT,
        .draw = MERGE(
            WIDTH(U_SUPERSCRIPT_ZERO),
            XFORM(superscript, REF(U_DIGIT_EIGHT)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_NINE,
        .draw = MERGE(
            WIDTH(U_SUPERSCRIPT_ZERO),
            XFORM(superscript, REF(U_DIGIT_NINE)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_PLUS_SIGN,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_PLUS_SIGN)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_MINUS,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_MINUS_SIGN)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_EQUALS_SIGN,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_EQUALS_SIGN)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_LEFT_PARENTHESIS,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LEFT_PARENTHESIS)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_RIGHT_PARENTHESIS,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_RIGHT_PARENTHESIS)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_LATIN_SMALL_LETTER_I,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_SMALL_LETTER_I)),
        ),
    },
    {
        .unicode = U_SUPERSCRIPT_LATIN_SMALL_LETTER_N,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_SMALL_LETTER_N)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_A,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_A)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_AE,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_AE)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_B,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_B)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_D,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_D)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_E,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_E)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_G,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_G)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_H,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_H)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_I,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_I)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_J,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_J)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_K,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_K)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_L,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_L)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_M,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_M)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_N,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_N)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_O,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_O)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_P,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_P)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_R,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_R)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_T,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_T)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_U,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_U)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_W,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_W)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_CAPITAL_V,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_CAPITAL_LETTER_V)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_SMALL_H,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_SMALL_LETTER_H)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_SMALL_J,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_SMALL_LETTER_J)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_SMALL_W,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(superscript, REF(U_LATIN_SMALL_LETTER_W)),
        ),
    },
    /* 02B3;MODIFIER LETTER SMALL R */
    /* 02B8;MODIFIER LETTER SMALL Y */
    /* 02E1;MODIFIER LETTER SMALL L */
    /* 02E2;MODIFIER LETTER SMALL S */
    /* 02E3;MODIFIER LETTER SMALL X */
    /* 1D43;MODIFIER LETTER SMALL A */
    /* 1D47;MODIFIER LETTER SMALL B */
    /* 1D48;MODIFIER LETTER SMALL D */
    /* 1D49;MODIFIER LETTER SMALL E */
    /* 1D4D;MODIFIER LETTER SMALL G */
    /* 1D4F;MODIFIER LETTER SMALL K */
    /* 1D50;MODIFIER LETTER SMALL M */
    /* 1D52;MODIFIER LETTER SMALL O */
    /* 1D56;MODIFIER LETTER SMALL P */
    /* 1D57;MODIFIER LETTER SMALL T */
    /* 1D58;MODIFIER LETTER SMALL U */
    /* 1D5B;MODIFIER LETTER SMALL V */
    /* 1D9C;MODIFIER LETTER SMALL C */
    /* 1DA0;MODIFIER LETTER SMALL F */
    /* 1DBB;MODIFIER LETTER SMALL Z */
    /* 1D46;MODIFIER LETTER SMALL TURNED AE */
    /* 02B4;MODIFIER LETTER SMALL TURNED R */
    /* 02B6;MODIFIER LETTER SMALL CAPITAL INVERTED R  */
    /* 1D44;MODIFIER LETTER SMALL TURNED A */
    /* 1D4A;MODIFIER LETTER SMALL SCHWA    */
    /* 1D4B;MODIFIER LETTER SMALL OPEN E   */
    /* 1D4C;MODIFIER LETTER SMALL TURNED OPEN E */
    /* 1D51;MODIFIER LETTER SMALL ENG      */
    /* 1D53;MODIFIER LETTER SMALL OPEN O   */
    /* 1D5A;MODIFIER LETTER SMALL TURNED M */
    /* 1D9E;MODIFIER LETTER SMALL ETH      */
    /* 1D9F;MODIFIER LETTER SMALL REVERSED OPEN E */
    /* 1DA2;MODIFIER LETTER SMALL SCRIPT G */
    /* 1DA3;MODIFIER LETTER SMALL TURNED H */
    /* 1DBA;MODIFIER LETTER SMALL TURNED V */
    /* 1DAB;MODIFIER LETTER SMALL CAPITAL L */
    /* 1DB0;MODIFIER LETTER SMALL CAPITAL N */
    /* A7F9;MODIFIER LETTER SMALL LIGATURE OE */

    /* missing */
    /* 1D3D;MODIFIER LETTER CAPITAL OU  */
    /* 1D2F;MODIFIER LETTER CAPITAL BARRED B      */
    /* 1D32;MODIFIER LETTER CAPITAL REVERSED E    */
    /* 1D3B;MODIFIER LETTER CAPITAL REVERSED N    */

    /* missing: many more 'MODIFIER LETTER SMALL' */
    /* 02E0;MODIFIER LETTER SMALL GAMMA                  */
    /* 02E4;MODIFIER LETTER SMALL REVERSED GLOTTAL STOP  */
    /* 1D45;MODIFIER LETTER SMALL ALPHA                  */
    /* 1D54;MODIFIER LETTER SMALL TOP HALF O             */
    /* 1D55;MODIFIER LETTER SMALL BOTTOM HALF O          */
    /* 1D59;MODIFIER LETTER SMALL SIDEWAYS U             */
    /* 1D5C;MODIFIER LETTER SMALL AIN                    */
    /* 1D5D;MODIFIER LETTER SMALL BETA                   */
    /* 1D5E;MODIFIER LETTER SMALL GREEK GAMMA            */
    /* 1D5F;MODIFIER LETTER SMALL DELTA                  */
    /* 1D60;MODIFIER LETTER SMALL GREEK PHI              */
    /* 1D61;MODIFIER LETTER SMALL CHI                    */
    /* 1D9B;MODIFIER LETTER SMALL TURNED ALPHA           */
    /* 1DA5;MODIFIER LETTER SMALL IOTA                   */
    /* 1DB1;MODIFIER LETTER SMALL BARRED O               */
    /* 1DB2;MODIFIER LETTER SMALL PHI                    */
    /* 1DB4;MODIFIER LETTER SMALL ESH                    */
    /* 1DB6;MODIFIER LETTER SMALL U BAR                  */
    /* 1DB7;MODIFIER LETTER SMALL UPSILON                */
    /* 1DA6;MODIFIER LETTER SMALL CAPITAL I              */
    /* 1DB8;MODIFIER LETTER SMALL CAPITAL U              */
    /* 1DBE;MODIFIER LETTER SMALL EZH                    */
    /* 1DBF;MODIFIER LETTER SMALL THETA                  */
    /* AB5C;MODIFIER LETTER SMALL HENG                   */

    /* subscript */
    {
        .unicode = U_SUBSCRIPT_ZERO,
        .draw = MERGE(
            WIDTH(U_SUPERSCRIPT_ZERO),
            XFORM(subscript, REF(U_DIGIT_ZERO)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_ONE,
        .draw = MERGE(
            WIDTH(U_SUBSCRIPT_ZERO),
            XFORM(subscript, REF(U_DIGIT_ONE)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_TWO,
        .draw = MERGE(
            WIDTH(U_SUBSCRIPT_ZERO),
            XFORM(subscript, REF(U_DIGIT_TWO)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_THREE,
        .draw = MERGE(
            WIDTH(U_SUBSCRIPT_ZERO),
            XFORM(subscript, REF(U_DIGIT_THREE)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_FOUR,
        .draw = MERGE(
            WIDTH(U_SUBSCRIPT_ZERO),
            XFORM(subscript, REF(U_DIGIT_FOUR)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_FIVE,
        .draw = MERGE(
            WIDTH(U_SUBSCRIPT_ZERO),
            XFORM(subscript, REF(U_DIGIT_FIVE)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_SIX,
        .draw = MERGE(
            WIDTH(U_SUBSCRIPT_ZERO),
            XFORM(subscript, REF(U_DIGIT_SIX)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_SEVEN,
        .draw = MERGE(
            WIDTH(U_SUBSCRIPT_ZERO),
            XFORM(subscript, REF(U_DIGIT_SEVEN)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_EIGHT,
        .draw = MERGE(
            WIDTH(U_SUBSCRIPT_ZERO),
            XFORM(subscript, REF(U_DIGIT_EIGHT)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_NINE,
        .draw = MERGE(
            WIDTH(U_SUBSCRIPT_ZERO),
            XFORM(subscript, REF(U_DIGIT_NINE)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_PLUS_SIGN,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(subscript, REF(U_PLUS_SIGN)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_MINUS,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(subscript, REF(U_MINUS_SIGN)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_EQUALS_SIGN,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(subscript, REF(U_EQUALS_SIGN)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_LEFT_PARENTHESIS,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(subscript, REF(U_LEFT_PARENTHESIS)),
        ),
    },
    {
        .unicode = U_SUBSCRIPT_RIGHT_PARENTHESIS,
        .lpad_abs = PAD_SCRIPT,
        .rpad_abs = PAD_SCRIPT,
        .draw = MERGE(
            XFORM(subscript, REF(U_RIGHT_PARENTHESIS)),
        ),
    },

    /* parens and brackets */
    {
        .unicode = U_LEFT_PARENTHESIS,
        .max_coord = COORD(0,+3,0,0),
        .draw = STROKE(
            Q(I, ( 0,+3, 0, 0), ( 0,+6, 0, 0)),
            Q(L, ( 0,-3, 0, 0), ( 0,+3, 0, 0)),
            Q(L, ( 0,-3, 0, 0), ( 0,-2, 0, 0)),
            Q(O, ( 0,+3, 0, 0), ( 0,-5, 0, 0)),
        ),
    },
    {
        .unicode = U_RIGHT_PARENTHESIS,
        .min_coord = COORD(0,-3,0,0),
        .draw = XFORM(swap_x, REF(U_LEFT_PARENTHESIS)),
    },
    {
        .unicode = U_LEFT_SQUARE_BRACKET,
        .draw = STROKE(
            Q(B, ( 0,+3, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-3, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-3, 0, 0), (-2,-5, 0, 0)),
            Q(E, ( 0,+3, 0, 0), (-2,-5, 0, 0)),
        ),
    },
    {
        .unicode = U_RIGHT_SQUARE_BRACKET,
        .draw = XFORM(swap_x, REF(U_LEFT_SQUARE_BRACKET)),
    },
    {
        .unicode = U_LEFT_CURLY_BRACKET,
        .draw = STROKE(
            Q(B, ( 0,+6, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-1, 0, 0), (-2,+6, 0, 0)),
            Q(T, ( 0,+1, 0, 0), ( 0,-5,+6,30)),
            Q(M, ( 0,-5,-5,30), ( 0,-5,+6,30)),
            Q(T, ( 0,+1, 0, 0), ( 0,-5,+6,30)),
            Q(L, ( 0,-1, 0, 0), (-2,-5, 0, 0)),
            Q(E, ( 0,+6, 0, 0), (-2,-5, 0, 0)),
        ),
    },
    {
        .unicode = U_RIGHT_CURLY_BRACKET,
        .draw = XFORM(swap_x, REF(U_LEFT_CURLY_BRACKET)),
    },

    /* math operators */
    {
        .unicode = U_PLUS_SIGN,
        .draw = STROKE(
            Q(B, ( 0,+8, 0, 0), ( 0,-3,+4,30)),
            Q(E, ( 0,-8, 0, 0), ( 0,-3,+4,30)),
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+4, 0, 0)),
        ),
    },
    {
        .unicode = U_MINUS_SIGN,
        .draw = STROKE(
            Q(B, ( 0,+8, 0, 0), ( 0,-3,+4,30)),
            Q(E, ( 0,-8, 0, 0), ( 0,-3,+4,30)),
        ),
    },
    {
        .unicode = U_NOT_SIGN,
        .draw = MERGE(
          WIDTH(U_MINUS_SIGN),
          STROKE(
            Q(B, ( 0,-8, 0, 0), ( 0,+1, 0, 0)),
            Q(P, (-2,+8, 0, 0), ( 0,+1, 0, 0)),
            Q(E, (-2,+8, 0, 0), ( 0,-2, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_DIVISION_SIGN,
        .draw = STROKE(
            Q(B, ( 0,+8, 0, 0), ( 0,-3,+4,30)),
            Q(E, ( 0,-8, 0, 0), ( 0,-3,+4,30)),

            Q(B, ( 0, 0, 0, 0), (-3,-3,+4,30, -60)),
            Q(E, ( 0, 0, 0, 0), (-3,-3,+4,30, -120)),

            Q(B, ( 0, 0, 0, 0), (+3,-3,+4,30, +60)),
            Q(E, ( 0, 0, 0, 0), (+3,-3,+4,30, +120)),
        ),
    },
    {
        .unicode = U_MULTIPLICATION_SIGN,
        .draw = MERGE(
          WIDTH(U_MINUS_SIGN),
          STROKE(
            Q(I, (-2,+8, 0, 0), (-2,+4, 0, 0)),
            Q(O, (-2,-8, 0, 0), (-2,-3, 0, 0)),
            Q(I, (-2,+8, 0, 0), (-2,-3, 0, 0)),
            Q(O, (-2,-8, 0, 0), (-2,+4, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_EQUALS_SIGN,
        .draw = STROKE(
            Q(B, ( 0,+8, 0, 0), ( 0,+2,+2, 0)),
            Q(E, ( 0,-8, 0, 0), ( 0,+2,+2, 0)),
            Q(B, ( 0,+8, 0, 0), ( 0,-1,-1, 0)),
            Q(E, ( 0,-8, 0, 0), ( 0,-1,-1, 0)),
        ),
    },
    {
        .unicode = U_PLUS_MINUS_SIGN,
        .draw = STROKE(
            Q(B, ( 0,+8, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,-8, 0, 0), (-2,-3, 0, 0)),
            Q(B, ( 0,+8, 0, 0), ( 0,-1,+4,30)),
            Q(E, ( 0,-8, 0, 0), ( 0,-1,+4,30)),
            Q(B, ( 0, 0, 0, 0), ( 0,+4, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-1, 0, 0)),
        ),
    },
    {
        .unicode = U_LESS_THAN_SIGN,
        .min_coord = COORD(0,-8,0,0),
        .max_coord = COORD(0,+8,0,0),
        .draw = STROKE(
            Q(I, ( 0,+8, 0, 0), ( 0,-3, 0, 0)),
            Q(P, ( 0,-8, 0, 0), ( 0,-3,+4,30)),
            Q(O, ( 0,+8, 0, 0), ( 0,+4, 0, 0)),
        ),
    },
    {
        .unicode = U_GREATER_THAN_SIGN,
        .min_coord = COORD(0,-8,0,0),
        .max_coord = COORD(0,+8,0,0),
        .draw = XFORM(swap_x, REF(U_LESS_THAN_SIGN)),
    },
    {
        .unicode = U_ASTERISK,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-1, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+5, 0, 0)),

            Q(I, (+3,-5, 0, 0), ( 0,-1,+5,50)),
            Q(P, (-6, 0, 0, 0), ( 0,-1,+5,30)),
            Q(P, (+6, 0, 0, 0), ( 0,-1,+5,30)),
            Q(O, (+3,+5, 0, 0), ( 0,-1,+5,50)),

            Q(I, (+3,-5, 0, 0), ( 0,-1,+5,10)),
            Q(P, (-6, 0, 0, 0), ( 0,-1,+5,30)),
            Q(P, (+6, 0, 0, 0), ( 0,-1,+5,30)),
            Q(O, (+3,+5, 0, 0), ( 0,-1,+5,10)),
        ),
    },
    {
        .unicode = U_TILDE,
        .draw = STROKE(
            Q(I, ( 0,+8, 0, 0), ( 0,-3,+4,37)),
            Q(L, ( 0,+8,-8,20), ( 0,-3,+4,15)),
            Q(L, ( 0,-8,+8,20), ( 0,-3,+4,45)),
            Q(O, ( 0,-8, 0, 0), ( 0,-3,+4,23)),
        ),
    },

    /* digits */
    {
        .unicode = U_DIGIT_ZERO,
        .line_step = LS_DIGIT,
        .mono = true,
        .draw = STROKE(
            Q(L, ( 0,+6, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+6, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_DIGIT_ONE,
        .line_step = LS_DIGIT,
        .mono = true,
        .draw = MERGE(
          WIDTH(U_DIGIT_ZERO),
          STROKE(
            Q(I, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-3, 0, 0), (-2,+6, 0, 0)),
            Q(O, ( 0,-7, 0, 0), ( 0,+5,+6,30)),
            Q(B, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_DIGIT_TWO,
        .line_step = LS_DIGIT,
        .mono = true,
        .draw = MERGE(
          WIDTH(U_DIGIT_ZERO),
          STROKE(
            Q(I, (+1,-6, 0, 0), ( 0,+5, 0, 0)),
            Q(S, (+1,-3, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,+6, 0, 0), (-2,+6, 0, 0)),
            Q(S, ( 0,+6, 0, 0), ( 0,+3,+2,30)),
            Q(S, ( 0,-6, 0, 0), ( 0,-1, 0, 0)),
            Q(P, ( 0,-6, 0, 0), (-2,-3, 0, 0)),
            Q(E, (+2,+6, 0, 0), (-2,-3, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_DIGIT_THREE,
        .line_step = LS_DIGIT,
        .mono = true,
        .draw = MERGE(
          WIDTH(U_DIGIT_ZERO),
          STROKE(
            Q(I, (+1,-6, 0, 0), ( 0,+5, 0, 0)),
            Q(S, (+1,-3, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,+6, 0, 0), (-2,+6, 0, 0)),
            Q(T, ( 0,+6, 0, 0), ( 0,-3,+6,30)),
            Q(M, ( 0,-2, 0, 0), ( 0,-3,+6,30)),
            Q(T, ( 0,+6, 0, 0), ( 0,-3,+6,30)),
            Q(L, ( 0,+6, 0, 0), (-2,-3, 0, 0)),
            Q(S, (+1,-3, 0, 0), (-2,-3, 0, 0)),
            Q(O, (+1,-6, 0, 0), ( 0,-2, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_DIGIT_FOUR,
        .line_step = LS_DIGIT,
        .mono = true,
        .draw = MERGE(
          WIDTH(U_DIGIT_ZERO),
          STROKE(
            Q(I, ( 0,+6, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0,+6, 0, 0), ( 0,+3, 0, 0)),
            Q(I, ( 0,+6, 0, 0), ( 0, 0, 0, 0)),
            Q(P, ( 0,-6, 0, 0), ( 0, 0, 0, 0)),
            Q(S, ( 0,-6, 0, 0), ( 0,+2, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_DIGIT_FIVE,
        .line_step = LS_DIGIT,
        .mono = true,
        .draw = MERGE(
          WIDTH(U_DIGIT_ZERO),
          STROKE(
            Q(I, (+2,+6, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-5, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-6, 0, 0), ( 0,+2, 0, 0)),
            Q(L, ( 0,+6, 0, 0), ( 0,+2, 0, 0)),
            Q(L, ( 0,+6, 0, 0), (-2,-3, 0, 0)),
            Q(S, (+1,-3, 0, 0), (-2,-3, 0, 0)),
            Q(O, (+1,-6, 0, 0), ( 0,-2, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_DIGIT_SIX,
        .line_step = LS_DIGIT,
        .mono = true,
        .draw = MERGE(
          WIDTH(U_DIGIT_ZERO),
          STROKE(
            Q(I, ( 0,+6, 0, 0), ( 0,+5, 0, 0)),
            Q(S, ( 0,+3, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+6, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+6, 0, 0), (-1,-3,+6,30)),
            Q(E, ( 0,-6, 0, 0), (-1,-3,+6,30)),
          )
        ),
    },
    {
        .unicode = U_DIGIT_SEVEN,
        .line_step = LS_DIGIT,
        .mono = true,
        .draw = MERGE(
          WIDTH(U_DIGIT_ZERO),
          STROKE(
            Q(I, (+2,-6, 0, 0), (-2,+6, 0, 0)),
            Q(P, (+2,+6, 0, 0), (-2,+6, 0, 0)),
            Q(P, (+2,+6, 0, 0), (-4,+5, 0, 0)),
            Q(L, ( 0,-2,+6,25), ( 0,+5,-2,30)),
            Q(P, ( 0,-1, 0, 0), ( 0,-2,-3,30)),
            Q(E, ( 0,-1, 0, 0), ( 0,-3, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_DIGIT_EIGHT,
        .line_step = LS_DIGIT,
        .mono = true,
        .draw = MERGE(
          WIDTH(U_DIGIT_ZERO),
          STROKE(
            Q(L, (-1,+6, 0, 0), (-2,+6, 0, 0)),
            Q(T, (-1,+6, 0, 0), ( 0,-3,+6,30)),
            Q(M, ( 0,-1, 0, 0), ( 0,-3,+6,30)),
            Q(T, ( 0,+6, 0, 0), ( 0,-3,+6,30)),
            Q(L, ( 0,+6, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (-2,-3, 0, 0)),
            Q(T, ( 0,-6, 0, 0), ( 0,-3,+6,30)),
            Q(M, ( 0,+1, 0, 0), ( 0,-3,+6,30)),
            Q(T, (-1,-6, 0, 0), ( 0,-3,+6,30)),
            Q(L, (-1,-6, 0, 0), (-2,+6, 0, 0)),

          )
        ),
    },
    {
        .unicode = U_DIGIT_NINE,
        .line_step = LS_DIGIT,
        .mono = true,
        .draw = MERGE(
          WIDTH(U_DIGIT_ZERO),
          STROKE(
            Q(I, ( 0,-6, 0, 0), ( 0,-2, 0, 0)),
            Q(S, ( 0,-3, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+6, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+6, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-6, 0, 0), (+1,-3,+6,30)),
            Q(E, ( 0,+6, 0, 0), (+1,-3,+6,30)),
          )
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_REVERSED_OPEN_E,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(I, (+1,-7, 0, 0), ( 0,+5, 0, 0)),
            Q(S, (+1,-3, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(T, ( 0,+7, 0, 0), ( 0,-3,+6,30)),
            Q(M, ( 0,-2, 0, 0), ( 0,-3,+6,30)),
            Q(T, ( 0,+7, 0, 0), ( 0,-3,+6,30)),
            Q(H, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
            Q(S, (+1,-3, 0, 0), (-2,-3, 0, 0)),
            Q(O, (+1,-7, 0, 0), ( 0,-2, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_OPEN_E,
        .line_step = LS_UPPER,
        .draw = XFORM(swap_x, REF(U_LATIN_CAPITAL_LETTER_REVERSED_OPEN_E)),
    },

    {
        .unicode = U_LATIN_CAPITAL_LETTER_EZH,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, (+1,-7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(A, ( 0,+7, 0, 0), ( 0,-3,+6,35)),
            Q(M, ( 0,-2, 0, 0), ( 0,-3,+6,35)),
            Q(A, ( 0,+7, 0, 0), ( 0,-3,+6,35)),
            Q(H, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
            Q(S, (+1,-3, 0, 0), (-2,-3, 0, 0)),
            Q(O, (+1,-7, 0, 0), ( 0,-2, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_EZH,
        .draw = STROKE(
            Q(B, (+1,-5, 0, 0), (-2,+3, 0, 0)),
            Q(P, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(A, ( 0,+5, 0, 0), ( 0,-6,+3,35)),
            Q(M, ( 0,-2, 0, 0), ( 0,-6,+3,35)),
            Q(A, ( 0,+5, 0, 0), ( 0,-6,+3,35)),
            Q(H, ( 0,+5, 0, 0), (-2,-6, 0, 0)),
            Q(S, (+1,-3, 0, 0), (-2,-6, 0, 0)),
            Q(O, (+1,-5, 0, 0), ( 0,-5, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_YOGH,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(I, (+1,-7, 0, 0), ( 0,+5, 0, 0)),
            Q(S, (+1,-3, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,+7, 0, 0), ( 0,-3,+6,42)),
            Q(O, ( 0,-1, 0, 0), ( 0,-3,+6,30)),

            Q(I, ( 0,+3, 0, 0), ( 0,-3,+6,35)),
            Q(T, ( 0,+7, 0, 0), ( 0,-3,+6,35)),
            Q(H, ( 0,+7, 0, 0), ( 0,-2, 0, 0)),
            Q(O, (+1,-7, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_YOGH,
        .draw = STROKE(
            Q(I, (+1,-5, 0, 0), ( 0,+2, 0, 0)),
            Q(S, (+1,-3, 0, 0), (-2,+3, 0, 0)),
            Q(H, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(P, ( 0,+5, 0, 0), ( 0,-6,+3,42)),
            Q(O, ( 0,-1, 0, 0), ( 0,-6,+3,30)),

            Q(I, ( 0,+2, 0, 0), ( 0,-6,+3,35)),
            Q(T, ( 0,+5, 0, 0), ( 0,-6,+3,35)),
            Q(H, ( 0,+5, 0, 0), ( 0,-5, 0, 0)),
            Q(O, (+1,-5, 0, 0), (-2,-6, 0, 0)),
        ),
    },

    /* latin capital letters */
    {
        .unicode = U_LATIN_CAPITAL_LETTER_A,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),
            Q(C, ( 0,-7, 0, 0), (-1,+6, 0, 0)),
            Q(P, ( 0,-7,+7,30), (-1,+6, 0, 0)),
            Q(C, ( 0,+7, 0, 0), (-1,+6, 0, 0)),
            Q(E, ( 0,+7, 0, 0), ( 0,-3, 0, 0)),
            Q(B, ( 0,-7, 0, 0), ( 0, 0,+3,10)),
            Q(E, ( 0,+7, 0, 0), ( 0, 0,+3,10)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_B,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(P, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(L, (-2,+7, 0, 0), (-2,+6, 0, 0)),
            Q(T, (-2,+7, 0, 0), ( 0,+2, 0, 0)),
            Q(M, ( 0,-7, 0, 0), ( 0,+2, 0, 0)),
            Q(T, ( 0,+7, 0, 0), ( 0,+2, 0, 0)),
            Q(L, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_C,
        .max_coord = COORD(+1,+7,0,0),
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(I, (+1,+7, 0, 0), ( 0,+5, 0, 0)),
            Q(R, (+1,+3, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(S, (+1,+3, 0, 0), (-2,-3, 0, 0)),
            Q(O, (+1,+7, 0, 0), ( 0,-2, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_D,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(H, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_E,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+7, 0, 0), (-2,-3, 0, 0)),

            Q(B, ( 0,-7, 0, 0), ( 0,+2, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,+2, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_F,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),

            Q(B, ( 0,-7, 0, 0), ( 0,+2, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,+2, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_G,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(I, (+1,+7, 0, 0), ( 0,+5, 0, 0)),
            Q(R, (+1,+3, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,+3, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,+7, 0, 0), ( 0,-3,-2,40)),
            Q(P, ( 0,+7, 0, 0), ( 0,-3,+6,15)),
            Q(P, ( 0,+7, 0, 0), (-3,+2, 0, 0)),
            Q(E, ( 0, 0, 0, 0), (-3,+2, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_H,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(B, ( 0,-7, 0, 0), ( 0,+2, 0, 0)),
            Q(E, ( 0,+7, 0, 0), ( 0,+2, 0, 0)),
            Q(B, ( 0,+7, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0,+7, 0, 0), ( 0,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_I,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_J,
        //.min_coord = COORD(0,-3,0,0),
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
            Q(H, ( 0, 0, 0, 0), (-2,-6, 0, 0)),
            Q(E, ( 0,-7, 0, 0), (-2,-6, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_K,
        .max_coord = COORD(+2,+5,0,0),
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),

            Q(B, ( 0,-7, 0, 0), ( 0,-3,+6,30)),
            Q(P, (-2,-3, 0, 0), ( 0,-3,+6,30)),
            Q(E, ( 0,+4, 0, 0), ( 0,+6, 0, 0)),

            Q(B, ( 0,-7, 0, 0), ( 0,-3,+6,30)),
            Q(P, (-2,-3, 0, 0), ( 0,-3,+6,30)),
            Q(E, ( 0,+5, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_L,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
        ),
    },
#if 1
    {
        /* capital small m like for n */
        .unicode = U_LATIN_CAPITAL_LETTER_M,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, (-1,+9, 0, 0), ( 0,-3, 0, 0)),
            Q(H, (-1,+9, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(B, (-1,-9, 0, 0), ( 0,-3, 0, 0)),
            Q(P, (-1,-9, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
#elif 0
    {
        /* dent, no further vertical line in the middle */
        .unicode = U_LATIN_CAPITAL_LETTER_M,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, (+2,-8, 0, 0), ( 0,-3, 0, 0)),
            Q(P, (+2,-8, 0, 0), (-2,+6, 0, 0)),
            Q(P, (-5,-8, 0, 0), (-2,+6, 0, 0)),
            Q(D, ( 0,-8,+8,30), (-3,+6, 0, 0)),
            Q(P, (-5,+8, 0, 0), (-2,+6, 0, 0)),
            Q(P, (+2,+8, 0, 0), (-2,+6, 0, 0)),
            Q(E, (+2,+8, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
#else
    {
        /* chamfer + vertical line in the middle */
        .unicode = U_LATIN_CAPITAL_LETTER_M,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-9, 0, 0), ( 0,-3, 0, 0)),
            Q(P, ( 0,-9, 0, 0), (-2,+6, 0, 0)),
            Q(P, (-6,-9, 0, 0), (-2,+6, 0, 0)),
            Q(C, ( 0,-9,+9,30), (-3,+6, 0, 0)),
            Q(M, ( 0,-9,+9,30), ( 0,+1, 0, 0)),
            Q(C, ( 0,-9,+9,30), (-3,+6, 0, 0)),
            Q(P, (-6,+9, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,+9, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0,+9, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
#endif
#if 0
    {
        /* standard backslashy middle: difficult, very pointed */
        .unicode = U_LATIN_CAPITAL_LETTER_N,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, (+3,+7, 0, 0), ( 0,+6, 0, 0)),
            Q(P, (+3,+7, 0, 0), (-2,-3, 0, 0)),
            Q(P, (-4,+7, 0, 0), (-2,-3, 0, 0)),
            Q(P, (-4,-7, 0, 0), (-2,+6, 0, 0)),
            Q(P, (+3,-7, 0, 0), (-2,+6, 0, 0)),
            Q(E, (+3,-7, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
#else
    {
        /* alternative: capital small n */
        .unicode = U_LATIN_CAPITAL_LETTER_N,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,+7, 0, 0), ( 0,-3, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_ENG,
        .line_step = LS_UPPER,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_N),
            XFORM2(xlat_relx, 0,+7, REF(UX_CAPITAL_LEFT_HOOK_BELOW_IN)),
        ),
    },
#endif
    {
        .unicode = U_LATIN_CAPITAL_LETTER_O,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(H, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_Q,
        .line_step = LS_UPPER,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_O),
            STROKE(
                Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
                Q(L, ( 0, 0, 0, 0), ( 0,-5, 0, 0)),
                Q(E, ( 0,+5, 0, 0), ( 0,-5, 0, 0)),
         )),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_P,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0, 1, 0, 0)),
            Q(H, ( 0,+7, 0, 0), ( 0, 1, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_R,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,+7, 0, 0), ( 0,-3, 0, 0)),
            Q(T, ( 0,+7, 0, 0), (+1,-3,+6,30)),
            Q(M, ( 0,-7, 0, 0), (+1,-3,+6,30)),
            Q(T, ( 0,+7, 0, 0), (+1,-3,+6,30)),
            Q(L, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_S,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(I, (+1,+7, 0, 0), ( 0,+5, 0, 0)),
            Q(R, ( 0,+3, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-7, 0, 0), (-1,-3,+6,30)),
            Q(L, ( 0,+7, 0, 0), (+1,-3,+6,30)),
            Q(L, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
            Q(R, ( 0,-3, 0, 0), (-2,-3, 0, 0)),
            Q(O, (+1,-7, 0, 0), ( 0,-2, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_T,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
            Q(B, ( 0,-8, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0,+8, 0, 0), (-2,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_U,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+7, 0, 0), ( 0,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_V,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(C, ( 0,-7, 0, 0), (-1,-3, 0, 0)),
            Q(P, ( 0,-7,+7,30), (-1,-3, 0, 0)),
            Q(C, ( 0,+7, 0, 0), (-1,-3, 0, 0)),
            Q(E, ( 0,+7, 0, 0), ( 0,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_W,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-9, 0, 0), ( 0,+6, 0, 0)),
            Q(L, ( 0,-9, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,-9, 0,30), (-2,-3, 0, 0)),
            Q(C, ( 0, 0, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-2,-2, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(B, ( 0,+9, 0, 0), ( 0,+6, 0, 0)),
            Q(L, ( 0,+9, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,+9, 0,30), (-2,-3, 0, 0)),
            Q(C, ( 0, 0, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-2,-2, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_X,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(C, ( 0,-7, 0, 0), ( 0,+6,-3,30)),
            Q(P, ( 0, 0, 0, 0), ( 0,+6,-3,30)),
            Q(C, ( 0,+7, 0, 0), ( 0,+6,-3,30)),
            Q(E, ( 0,+7, 0, 0), ( 0,+6, 0, 0)),

            Q(B, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),
            Q(C, ( 0,-7, 0, 0), ( 0,+6,-3,30)),
            Q(P, ( 0, 0, 0, 0), ( 0,+6,-3,30)),
            Q(C, ( 0,+7, 0, 0), ( 0,+6,-3,30)),
            Q(E, ( 0,+7, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_Y,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-1,+1, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-1,+1, 0, 0)),
            Q(E, ( 0,+7, 0, 0), ( 0,+6, 0, 0)),
            Q(B, ( 0, 0, 0, 0), ( 0,+1, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_Z,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, (+2,+7, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-8,-3, 0, 0)),
            Q(P, ( 0,+7, 0, 0), (-8,+6, 0, 0)),
            Q(P, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(E, (+2,-7, 0, 0), (-2,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_A,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(P, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(R, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(R, ( 0,-5, 0, 0), ( 0, 0, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0, 0, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_B,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,+6, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LETTER_B_WITH_SHORT_STEM,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), (+1,+3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_C,
        .draw = STROKE(
            Q(B, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_D,
        .draw = STROKE(
            Q(B, ( 0,+5, 0, 0), ( 0,+6, 0, 0)),
            Q(P, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LETTER_D_WITH_SHORT_STEM,
        .draw = STROKE(
            Q(B, ( 0,+5, 0, 0), (+1,+3, 0, 0)),
            Q(P, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_E,
        .draw = STROKE(
            Q(B, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(S, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), ( 0, 0, 0, 0)),
            Q(E, ( 0,-5, 0, 0), ( 0, 0, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_F,
        .draw = STROKE(
            Q(B, ( 0,+4, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-3, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0,-3, 0, 0), ( 0,-3, 0, 0)),
            Q(B, ( 0,-3, 0, 0), (-3,+3, 0, 0)),
            Q(E, ( 0,+4, 0, 0), (-3,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_G,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), (-2,-6, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,-6, 0, 0)),
            Q(P, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_H,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0,-5, 0, 0), ( 0,-3, 0, 0)),
            Q(B, ( 0,+5, 0, 0), ( 0,-3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_K,
        .max_coord = COORD( 0,+4, 0, 0),
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0,-5, 0, 0), ( 0,-3, 0, 0)),

            Q(B, ( 0,-5, 0, 0), (+1, 0, 0, 0)),
            Q(P, (-4,-3, 0, 0), (+1, 0, 0, 0)),
            Q(E, (-1,+4, 0, 0), ( 0,+3, 0, 0)),

            Q(B, ( 0,-5, 0, 0), (+1, 0, 0, 0)),
            Q(P, (-4,-3, 0, 0), (+1, 0, 0, 0)),
            Q(E, ( 0,+4, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LETTER_K_WITH_SHORT_STEM,
        .max_coord = COORD( 0,+4, 0, 0),
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), (+1,+3, 0, 0)),
            Q(E, ( 0,-5, 0, 0), ( 0,-3, 0, 0)),

            Q(B, ( 0,-5, 0, 0), (+1, 0, 0, 0)),
            Q(P, (-4,-3, 0, 0), (+1, 0, 0, 0)),
            Q(E, (-1,+4, 0, 0), ( 0,+3, 0, 0)),

            Q(B, ( 0,-5, 0, 0), (+1, 0, 0, 0)),
            Q(P, (-4,-3, 0, 0), (+1, 0, 0, 0)),
            Q(E, ( 0,+4, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_L,
        .max_coord = COORD(+3, 0, 0, 0),
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
            Q(R, ( 0, 0, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+3, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_M,
        .draw = STROKE(
            Q(B, ( 0,+8, 0, 0), ( 0,-3, 0, 0)),
            Q(L, ( 0,+8, 0, 0), (-2,+3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(B, ( 0,-8, 0, 0), ( 0,-3, 0, 0)),
            Q(P, ( 0,-8, 0, 0), (-2,+3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_N,
        .draw = STROKE(
            Q(B, ( 0,+5, 0, 0), ( 0,-3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(P, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0,-5, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_ENG,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_N),
            XFORM2(xlat_relx, 0,+5, REF(UX_LEFT_HOOK_BELOW_IN))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_O,
        .draw = STROKE(
            Q(H, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(H, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(H, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_P,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,-6, 0, 0)),
            Q(P, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_Q,
        .draw = STROKE(
            Q(B, ( 0,+5, 0, 0), ( 0,-6, 0, 0)),
            Q(P, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        /* horizontal rounded on right, pointed left-top corner */
        .unicode = U_LATIN_SMALL_LETTER_R,
        .max_coord = COORD(+1,+4,0,0),
        .draw = STROKE(
            Q(B, ( 0,+4, 0, 0), ( 0,+1, 0, 0)),
            Q(L, ( 0,+4, 0, 0), (-2,+3, 0, 0)),
            Q(P, ( 0,-4, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0,-4, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        /* was alternative form for 'r', now used for IPA
         * wider: straight horizontal, rounded left-top corner */
        .unicode = U_LATIN_SMALL_LETTER_R_WITH_FISHHOOK,
        .draw = STROKE(
            Q(B, ( 0,+4, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,-4, 0, 0), (-2,+3, 0, 0)),
            Q(E, ( 0,-4, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_S,
#if 1
        .draw = SAME(U_LATIN_LETTER_SMALL_CAPITAL_S),
#else
        .draw = STROKE(
            Q(B, (+1,+5, 0, 0), (-2,+3, 0, 0)),
            Q(R, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(R, ( 0,-5, 0, 0), (+1, 0, 0, 0)),
            Q(R, ( 0,+5, 0, 0), (-1, 0, 0, 0)),
            Q(R, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(E, (+1,-5, 0, 0), (-2,-3, 0, 0)),
        ),
#endif
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_T,
        .draw = STROKE(
            Q(B, ( 0,+4, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,-3, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,-3, 0, 0), ( 0,+5,+6,20)),
            Q(B, ( 0,-3, 0, 0), (-3,+3, 0, 0)),
            Q(E, ( 0,+4, 0, 0), (-3,+3, 0, 0)),
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LETTER_T_WITH_SHORT_STEM,
        .draw = STROKE(
            Q(B, ( 0,+4, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,-3, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,-3, 0, 0), (+1,+3, 0, 0)),
            Q(B, ( 0,-3, 0, 0), (-3,+3, 0, 0)),
            Q(E, ( 0,+4, 0, 0), (-3,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_U,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,+3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_V,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,+3, 0, 0)),
            Q(C, ( 0,-5, 0, 0), (-1,-3, 0, 0)),
            Q(P, ( 0,-5,+5,30), (-1,-3, 0, 0)),
            Q(C, ( 0,+5, 0, 0), (-1,-3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_W,
        .draw = STROKE(
            Q(B, ( 0,-8, 0, 0), ( 0,+3, 0, 0)),
            Q(L, ( 0,-8, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,-8, 0,30), (-2,-3, 0, 0)),
            Q(C, ( 0, 0, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-2,-2, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+3, 0, 0)),

            Q(B, ( 0,+8, 0, 0), ( 0,+3, 0, 0)),
            Q(L, ( 0,+8, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,+8, 0,30), (-2,-3, 0, 0)),
            Q(C, ( 0, 0, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-2,-2, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_X,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,+3, 0, 0)),
            Q(C, ( 0,-5, 0, 0), ( 0, 0, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0, 0, 0, 0)),
            Q(C, ( 0,+5, 0, 0), ( 0, 0, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,+3, 0, 0)),
            Q(B, ( 0,-5, 0, 0), ( 0,-3, 0, 0)),
            Q(C, ( 0,-5, 0, 0), ( 0, 0, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0, 0, 0, 0)),
            Q(C, ( 0,+5, 0, 0), ( 0, 0, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_Y,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), (-2,-6, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,-6, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,+3, 0, 0)),
            Q(B, ( 0,-5, 0, 0), ( 0,+3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_Z,
        .draw = STROKE(
            Q(B, (+2,+5, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,-5, 0, 0), (-8,-3, 0, 0)),
            Q(P, ( 0,+5, 0, 0), (-8,+3, 0, 0)),
            Q(P, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(E, (+2,-5, 0, 0), (-2,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_AE,
        .draw = STROKE(
            Q(B, ( 0,-9, 0, 0), (-2,+3, 0, 0)),
            Q(T, ( 0, 0, 0, 0), (-2,+3, 0, 0)),
            Q(T, ( 0, 0, 0, 0), (-2,-3, 0, 0)),
            Q(R, ( 0,-9, 0, 0), (-2,-3, 0, 0)),
            Q(R, ( 0,-9, 0, 0), ( 0, 0, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0, 0, 0, 0)),

            Q(B, ( 0,+9, 0, 0), (-2,-3, 0, 0)),
            Q(T, ( 0, 0, 0, 0), (-2,-3, 0, 0)),
            Q(T, ( 0, 0, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,+9, 0, 0), (-2,+3, 0, 0)),
            Q(P, ( 0,+9, 0, 0), ( 0, 0, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0, 0, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LIGATURE_OE,
        .draw = STROKE(
            Q(H, ( 0,-10, 0, 0), (-2,+3, 0, 0)),
            Q(T, ( 0,  0, 0, 0), (-2,+3, 0, 0)),
            Q(T, ( 0,  0, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,-10, 0, 0), (-2,-3, 0, 0)),

            Q(N, ( 0, 0, 0, 0), ( 0, 0, 0, 0)),

            Q(B, ( 0,+10, 0, 0), (-2,-3, 0, 0)),
            Q(T, ( 0,  0, 0, 0), (-2,-3, 0, 0)),
            Q(T, ( 0,  0, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,+10, 0, 0), (-2,+3, 0, 0)),
            Q(P, ( 0,+10, 0, 0), ( 0, 0, 0, 0)),
            Q(E, ( 0,  0, 0, 0), ( 0, 0, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_AE,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-10, 0, 0), ( 0,-3, 0, 0)),
            Q(C, ( 0,-10, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-10, 0,30), (-2,+6, 0, 0)),
            Q(E, ( 0,  0, 0, 0), (-2,+6, 0, 0)),

            Q(B, ( 0,-10, 0, 0), ( 0, 0,+3,10)),
            Q(E, ( 0,  0, 0, 0), ( 0, 0,+3,10)),

            Q(B, ( 0,+10, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,  0, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,  0, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+10, 0, 0), (-2,-3, 0, 0)),

            Q(B, ( 0,  0, 0, 0), ( 0,+2, 0, 0)),
            Q(E, ( 0, +9, 0, 0), ( 0,+2, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LIGATURE_OE,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,+10, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,  0, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,  0, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+10, 0, 0), (-2,-3, 0, 0)),

            Q(B, ( 0,  0, 0, 0), ( 0,+2, 0, 0)),
            Q(E, ( 0, +9, 0, 0), ( 0,+2, 0, 0)),

            Q(B, ( 0,  0, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,-10, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,-10, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,  0, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_O_WITH_STROKE,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_O),
            XFORM(ls_thin,
                STROKE(
                    Q(I, ( 0,+5, 0, 0), ( 0,+3,+6,20)),
                    Q(O, ( 0,-5, 0, 0), ( 0,-3,-6,20)),
                )
            ),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_O_WITH_STROKE,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_O),
            XFORM(ls_thin,
                STROKE(
                    Q(I, ( 0,+7, 0, 0), ( 0,+6,+10,20)),
                    Q(O, ( 0,-7, 0, 0), ( 0,-3,-6,20)),
                )
            ),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_ETH,
        //.min_coord = COORD(+3,-8,0,0),
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(H, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,-3, 0, 0)),

            Q(N, (0), (0)),

            Q(B, ( 0,-10, 0, 0), ( 0,-3,+6,30)),
            Q(E, ( 0,  0, 0, 0), ( 0,-3,+6,30)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_ETH,
        .draw = MERGE(
            STROKE(
                Q(I, ( 0,-2, 0, 0), ( 0,+7, 0, 0)),
                Q(P, ( 0,+5, 0, 0), (+5,+2,+3,30)),
                Q(P, ( 0,+5, 0, 0), ( 0,+2,+3,30)),
                Q(L, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
                Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
                Q(L, ( 0,-5, 0, 0), (-2,+2,+3,30)),
                Q(E, ( 0,+5, 0, 0), (-2,+2,+3,30)),
            ),
            XFORM(ls_thinner,
                STROKE(
                    Q(I, ( 0,+4,+5,30), ( 0,+7, 0, 0)),
                    Q(O, ( 0,-3, 0, 0), ( 0,+4, 0,0)),
                )
            ),
        )
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_D_WITH_STROKE,
        .draw = SAME(U_LATIN_CAPITAL_LETTER_ETH),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_AFRICAN_D,
        .draw = SAME(U_LATIN_CAPITAL_LETTER_ETH),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_D_WITH_STROKE,
        //.max_coord = COORD(+3,+5,0,0),
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_D),
            XFORM(ls_thinner,
                STROKE(
                    Q(B, ( 0, 0, 0, 0), ( 0, +5, 0, 0)),
                    Q(E, (+2,+8, 0, 0), ( 0, +5, 0, 0)),
                )
            ),
        ),
    },

    {
        .unicode = U_LATIN_CAPITAL_LETTER_G_WITH_STROKE,
        .line_step = LS_UPPER,
        .draw = MERGE(
          REF(U_LATIN_CAPITAL_LETTER_G),
          XFORM(ls_thinner,
            STROKE(
              Q(B, ( 0, 0, 0, 0), (-1, 0,-1,25)),
              Q(E, ( 0,+5, 0, 0), (-1, 0,-1,25)),
            )
          )
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_G_WITH_STROKE,
        .draw = MERGE(
          REF(U_LATIN_SMALL_LETTER_G),
          XFORM(ls_thinner,
            STROKE(
              Q(B, ( 0,-3, 0, 0), (-1,-4,-5,20)),
              Q(E, ( 0,+5, 0, 0), (-1,-4,-5,20)),
            )
          )
        ),
    },

    /* special letters */
    {
        .unicode = U_LATIN_SMALL_LETTER_DOTLESS_J,
        .map = MAP(
            WITH_ABOVE(U_LATIN_SMALL_LETTER_J),
            WITH_BOTH (U_LATIN_SMALL_LETTER_J),
        ),
        //.min_coord = COORD(-3,0,0,0),
        .draw = STROKE(
            Q(B, ( 0,-6, 0, 0), (-2,-6, 0, 0)),
            Q(R, ( 0, 0, 0, 0), (-2,-6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_DOTLESS_I,
        .map = MAP(
            WITH_ABOVE(U_LATIN_SMALL_LETTER_I),
            WITH_BOTH (U_LATIN_SMALL_LETTER_I),
        ),
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_IOTA,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,-3, 0, 0)),
            Q(E, ( 0,+6, 0, 0), (-3,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_IOTA,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+3, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,-3, 0, 0)),
            Q(E, ( 0,+6, 0, 0), (-3,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_SHARP_S,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-1,+6, 0, 0)),
            Q(H, (-2,+7, 0, 0), (-1,+6, 0, 0)),
            Q(P, (-2,+7, 0, 0), ( 0,+3, 0, 0)),
            Q(P, (-2,+7, 0, 0), (-2,+3, 0, 0)),
            Q(R, ( 0,-1, 0, 0), (-2,+3, 0, 0)),
            Q(R, ( 0,-1, 0, 0), (+2, 0, 0, 0)),
            Q(R, ( 0,+7, 0, 0), ( 0, 0, 0, 0)),
            Q(R, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
            Q(E, (+4,-1, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_SHARP_S,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,-3, 0, 0)),
            Q(H, ( 0,-5, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,+5, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,+5, 0, 0), ( 0,+3, 0, 0)),
            Q(P, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(R, (-2,-1, 0, 0), (-2,+3, 0, 0)),
            Q(R, (-2,-1, 0, 0), (+2, 0, 0, 0)),
            Q(R, (+3,+5, 0, 0), ( 0, 0, 0, 0)),
            Q(R, (+3,+5, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,-1, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_LONG_S,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,-3, 0, 0)),
            Q(H, ( 0,-5, 0, 0), (-2,+6, 0, 0)),
            Q(P, ( 0,+1, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,+3,+4,30), (-2,+6, 0, 0)),
            Q(O, ( 0,+5, 0, 0), ( 0,+5, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_THORN,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), (-2,+6,+3,40)),
            Q(H, ( 0,+7, 0, 0), (-2,+6,+3,40)),
            Q(H, ( 0,+7, 0, 0), (-2,-3, 0,40)),
            Q(E, ( 0,-7, 0, 0), (-2,-3, 0,40)),

            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_THORN,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,-6, 0, 0)),
            Q(E, ( 0,-5, 0, 0), ( 0,+6, 0, 0)),
            Q(B, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_REVERSED_E,
        .draw = XFORM(swap_x, REF(U_LATIN_CAPITAL_LETTER_E)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_REVERSED_E,
        .draw = XFORM(invert_lc, REF(U_LATIN_SMALL_LETTER_SCHWA)),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_SCHWA,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,+6, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,-7, 0, 0), ( 0,-3,+6,30)),
            Q(E, ( 0,+7, 0, 0), ( 0,-3,+6,30)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_SCHWA,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,+3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0,-5, 0, 0), ( 0, 0, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0, 0, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_E,
        .draw = SAME(U_LATIN_SMALL_LETTER_SCHWA),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_OPEN_O,
        .min_coord = COORD(+1,-7,0,0),
        .line_step = LS_UPPER,
        .draw = XFORM(swap_x, REF(U_LATIN_CAPITAL_LETTER_C)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_OPEN_O,
        .draw = XFORM(swap_x, REF(U_LATIN_LETTER_SMALL_CAPITAL_C)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_Y_WITH_DOT_BELOW,
        .draw = MERGE(
          STROKE(
            Q(I, ( 0,-5, 0, 0), (-2,-6, 0, 0)),
            Q(H, ( 0,+5, 0, 0), ( 0,-5, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,+3, 0, 0)),
            Q(B, ( 0,-5, 0, 0), ( 0,+3, 0, 0)),
            Q(L, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
          ),
          XFORM2(xlat_relx, 0,+5,
            XFORM2(xlat_rely, -5,-6.2,
              STROKE(
                Q(B, ( 0, 0, 0, 0), ( 0,-5,-6,20)),
                Q(E, ( 0, 0, 0, 0), ( 0,-5,-6,20, -60)),
              )
            )
          ),
        ),
    },

    {
        .unicode = U_MODIFIER_LETTER_PRIME,
        .draw = STROKE(
            Q(I, ( 0,+1, 0, 0), ( 0,+6, 0, 0)),
            Q(O, ( 0,-1, 0, 0), ( 0,+3, 0, 0)),
        )
    },
    {
        .unicode = U_MODIFIER_LETTER_DOUBLE_PRIME,
        .draw = MERGE(
            XFORM2(xlat_relx, 0,+2,
              STROKE(
                Q(I, (+2,+1, 0, 0), ( 0,+6, 0, 0)),
                Q(O, (-2,-1, 0, 0), ( 0,+3, 0, 0)),
              )
            ),
            XFORM2(xlat_relx, 0,-2,
              STROKE(
                Q(I, (-2,+1, 0, 0), ( 0,+6, 0, 0)),
                Q(O, (+2,-1, 0, 0), ( 0,+3, 0, 0)),
              )
            ),
        )
    },
    {
        .unicode = U_LATIN_LETTER_DENTAL_CLICK,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-6, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_LETTER_LATERAL_CLICK,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, (+3, -2, 0, 0), ( 0,+6, 0, 0)),
            Q(E, (+3, -2, 0, 0), ( 0,-6, 0, 0)),
            Q(B, (+3, +2, 0, 0), ( 0,+6, 0, 0)),
            Q(E, (+3, +2, 0, 0), ( 0,-6, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_LETTER_ALVEOLAR_CLICK,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,-6, 0, 0)),
            Q(B, (+2,-5, 0, 0), (-3,-6,+6,33)),
            Q(E, (+2,+5, 0, 0), (-3,-6,+6,33)),
            Q(B, (+2,-5, 0, 0), (+3,-6,+6,27)),
            Q(E, (+2,+5, 0, 0), (+3,-6,+6,27)),
        ),
    },
    {
        .unicode = U_LATIN_LETTER_RETROFLEX_CLICK,
        .line_step = LS_UPPER,
        .draw = REF(U_EXCLAMATION_MARK),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_ESH,
        .draw = STROKE(
            Q(B, ( 0,+6, 0, 0), (-3,+6, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,+6, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,-6, 0, 0)),
            Q(E, ( 0,-6, 0, 0), (-3,-6, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_UPSILON,
        .line_step = LS_UPPER,
        .draw = XFORM(invert_uc, REF(U_GREEK_CAPITAL_LETTER_OMEGA)),
    },
    {
        .unicode = U_GREEK_LETTER_SMALL_CAPITAL_OMEGA,
        .draw = XFORM(smallcap, REF(U_GREEK_CAPITAL_LETTER_OMEGA)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_UPSILON,
        .draw = XFORM(invert_lc, REF(U_GREEK_LETTER_SMALL_CAPITAL_OMEGA)),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_V_WITH_HOOK,
        .line_step = LS_UPPER,
        .draw = MERGE(
          STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(S, ( 0,-7, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,+7, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0,+7, 0, 0), ( 0,+3, 0, 0)),
          ),
          XFORM2(xlat_relx, 0,+7, REF(UX_CAPITAL_LEFT_HOOK_ABOVE_IN)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_V_WITH_HOOK,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,+3, 0, 0)),
            Q(S, ( 0,-5, 0, 0), (-2,-3, 0, 0)),
            Q(H, ( 0,+5, 0, 0), (-2,-3, 0, 0)),
            Q(L, ( 0,+5, 0, 0), (-3,+3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), (-3,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_GAMMA,
        .line_step = LS_UPPER,
        .draw = MERGE(
          STROKE(
            Q(C, ( 0,-4, 0, 0), (-3,-2, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-3,-2, 0, 0)),
            Q(C, ( 0,+4, 0, 0), (-3,-2, 0, 0)),
            Q(H, ( 0,+4, 0, 0), (-3,-6, 0, 0)),
            Q(H, ( 0,-4, 0, 0), (-3,-6, 0, 0)),
          ),
          STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(C, ( 0,-7, 0, 0), (-1,-2, 0, 0)),
            Q(P, ( 0,-7,+7,30), (-1,-2, 0, 0)),
            Q(C, ( 0,+7, 0, 0), (-1,-2, 0, 0)),
            Q(E, ( 0,+7, 0, 0), ( 0,+6, 0, 0)),
          ),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_GAMMA,
        .draw = MERGE(
          STROKE(
            Q(C, ( 0,-4, 0, 0), (-3,-2, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-3,-2, 0, 0)),
            Q(C, ( 0,+4, 0, 0), (-3,-2, 0, 0)),
            Q(H, ( 0,+4, 0, 0), (-3,-6, 0, 0)),
            Q(H, ( 0,-4, 0, 0), (-3,-6, 0, 0)),
          ),
          STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,+3, 0, 0)),
            Q(C, ( 0,-5, 0, 0), (-1,-2, 0, 0)),
            Q(P, ( 0,-5,+5,30), (-1,-2, 0, 0)),
            Q(C, ( 0,+5, 0, 0), (-1,-2, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,+3, 0, 0)),
          ),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_RAMS_HORN,
        .draw = MERGE(
          STROKE(
            Q(P, ( 0,-4, 0, 0), ( 0,-1, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0, 0,+1,30)),
            Q(P, ( 0,+4, 0, 0), ( 0,-1, 0, 0)),
            Q(H, ( 0,+4, 0, 0), (-3,-3, 0, 0)),
            Q(H, ( 0,-4, 0, 0), (-3,-3, 0, 0)),
          ),
          STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,+3, 0, 0)),
            Q(C, ( 0,-5, 0, 0), (-1, 0,+1,30)),
            Q(P, ( 0,-5,+5,30), (-1, 0,+1,30)),
            Q(C, ( 0,+5, 0, 0), (-1, 0,+1,30)),
            Q(E, ( 0,+5, 0, 0), ( 0,+3, 0, 0)),
          ),
        ),
    },

    /* letters with hooks and/or tails */
    {
        .unicode = UX_LEFT_HOOK_BELOW_OUT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-2, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,-6, 0, 0)),
            Q(E, ( 0,-5, 0, 0), (-3,-6, 0, 0)),
        ),
    },
    {
        .unicode = UX_RIGHT_HOOK_BELOW_OUT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-2, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,-6, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-3,-6, 0, 0)),
        ),
    },
#if 0
    {
        .unicode = UX_LEFT_HOOK_ABOVE_OUT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,+6, 0, 0)),
            Q(E, ( 0,-5, 0, 0), (-3,+6, 0, 0)),
        ),
    },
#endif
    {
        .unicode = UX_RIGHT_HOOK_ABOVE_OUT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,+6, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-3,+6, 0, 0)),
        ),
    },
    {
        .unicode = UX_CAPITAL_LEFT_HOOK_BELOW_OUT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-2, 0, 0)),
            Q(H, ( 0, 0, 0, 0), (-3,-6, 0, 0)),
            Q(E, ( 0,-5, 0, 0), (-3,-6, 0, 0)),
        ),
    },
#if 0
    {
        .unicode = UX_CAPITAL_RIGHT_HOOK_BELOW_OUT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-2, 0, 0)),
            Q(H, ( 0, 0, 0, 0), (-3,-6, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-3,-6, 0, 0)),
        ),
    },
#endif
#if 0
    {
        .unicode = UX_CAPITAL_LEFT_HOOK_ABOVE_OUT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(H, ( 0, 0, 0, 0), (-3,+6, 0, 0)),
            Q(E, ( 0,-5, 0, 0), (-3,+6, 0, 0)),
        ),
    },
#endif
#if 0
    {
        .unicode = UX_CAPITAL_RIGHT_HOOK_ABOVE_OUT,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(H, ( 0, 0, 0, 0), (-3,+6, 0, 0)),
            Q(E, ( 0,+5, 0, 0), (-3,+6, 0, 0)),
        ),
    },
#endif
    {
        .unicode = UX_LEFT_HOOK_BELOW_IN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-2, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,-6, 0, 0)),
            Q(E, ( 0,-6, 0, 0), (-3,-6, 0, 0)),
        ),
    },
    {
        .unicode = UX_RIGHT_HOOK_BELOW_IN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-2, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,-6, 0, 0)),
            Q(E, ( 0,+6, 0, 0), (-3,-6, 0, 0)),
        ),
    },
#if 0
    {
        .unicode = UX_LEFT_HOOK_ABOVE_IN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,+6, 0, 0)),
            Q(E, ( 0,-6, 0, 0), (-3,+6, 0, 0)),
        ),
    },
#endif
    {
        .unicode = UX_RIGHT_HOOK_ABOVE_IN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(L, ( 0, 0, 0, 0), (-3,+6, 0, 0)),
            Q(E, ( 0,+6, 0, 0), (-3,+6, 0, 0)),
        ),
    },
    {
        .unicode = UX_CAPITAL_LEFT_HOOK_BELOW_IN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-2, 0, 0)),
            Q(H, ( 0, 0, 0, 0), (-3,-6, 0, 0)),
            Q(E, ( 0,-7, 0, 0), (-3,-6, 0, 0)),
        ),
    },
    {
        .unicode = UX_CAPITAL_RIGHT_HOOK_BELOW_IN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-2, 0, 0)),
            Q(H, ( 0, 0, 0, 0), (-3,-6, 0, 0)),
            Q(E, ( 0,+7, 0, 0), (-3,-6, 0, 0)),
        ),
    },
    {
        .unicode = UX_CAPITAL_LEFT_HOOK_ABOVE_IN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(H, ( 0, 0, 0, 0), (-3,+6, 0, 0)),
            Q(E, ( 0,-7, 0, 0), (-3,+6, 0, 0)),
        ),
    },
#if 0
    {
        .unicode = UX_CAPITAL_RIGHT_HOOK_ABOVE_IN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+2, 0, 0)),
            Q(H, ( 0, 0, 0, 0), (-3,+6, 0, 0)),
            Q(E, ( 0,+7, 0, 0), (-3,+6, 0, 0)),
        ),
    },
#endif
    {
        .unicode = U_LATIN_CAPITAL_LETTER_B_WITH_HOOK,
        .line_step = LS_UPPER,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_B),
            STROKE(
                Q(B, ( 0,-5,  0, 0), (-2,+6, 0, 0)),
                Q(L, (+2,-11, 0, 0), (-2,+6, 0, 0)),
                Q(E, (+2,-11, 0, 0), ( 0,+4, 0, 0)),
            ),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_B_WITH_HOOK,
        .draw = MERGE(
            REF(UX_LATIN_SMALL_LETTER_B_WITH_SHORT_STEM),
            XFORM2(xlat_relx, 0,-5, REF(UX_RIGHT_HOOK_ABOVE_IN))
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_D_WITH_HOOK,
        .line_step = LS_UPPER,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_D),
            STROKE(
                Q(B, ( 0,-5,  0, 0), (-2,+6, 0, 0)),
                Q(L, (+2,-11, 0, 0), (-2,+6, 0, 0)),
                Q(E, (+2,-11, 0, 0), ( 0,+4, 0, 0)),
            ),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_D_WITH_HOOK,
        .draw = MERGE(
            REF(UX_LATIN_SMALL_LETTER_D_WITH_SHORT_STEM),
            XFORM2(xlat_relx, 0,+5, REF(UX_RIGHT_HOOK_ABOVE_OUT))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_D_WITH_TAIL,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_D),
            XFORM2(xlat_relx, 0,+5, REF(UX_RIGHT_HOOK_BELOW_OUT))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_D_WITH_HOOK_AND_TAIL,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_D_WITH_HOOK),
            XFORM2(xlat_relx, 0,+5, REF(UX_RIGHT_HOOK_BELOW_OUT))
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_F_WITH_HOOK,
        .line_step = LS_UPPER,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_F),
            XFORM2(xlat_relx, 0,-7, REF(UX_CAPITAL_LEFT_HOOK_BELOW_OUT))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_F_WITH_HOOK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_F),
            XFORM2(xlat_relx, 0,-3, REF(UX_LEFT_HOOK_BELOW_OUT))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_G_WITH_HOOK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_G),
            XFORM2(xlat_relx, 0,+5, REF(UX_RIGHT_HOOK_ABOVE_OUT))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_SCRIPT_G,
        .draw = SAME(U_LATIN_SMALL_LETTER_G),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_H_WITH_HOOK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_N),
            XFORM2(xlat_relx, 0,-5, REF(UX_RIGHT_HOOK_ABOVE_IN))
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_HENG,
        .line_step = LS_UPPER,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_H),
            XFORM2(xlat_relx, 0,+7, REF(UX_CAPITAL_LEFT_HOOK_BELOW_IN))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_HENG,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_H),
            XFORM2(xlat_relx, 0,+5, REF(UX_LEFT_HOOK_BELOW_IN))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_HENG_WITH_HOOK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_N),
            XFORM2(xlat_relx, 0,-5, REF(UX_RIGHT_HOOK_ABOVE_IN)),
            XFORM2(xlat_relx, 0,+5, REF(UX_LEFT_HOOK_BELOW_IN))
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_K_WITH_HOOK,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0,-7, 0, 0), ( 0,+6, 0, 0)),
            Q(E, ( 0,-7, 0, 0), ( 0,-3, 0, 0)),

            Q(B, ( 0,-7, 0, 0), ( 0,-3,+6,30)),
            Q(P, (-2,-3, 0, 0), ( 0,-3,+6,30)),
            Q(L, ( 0,+3, 0, 0), (-3,+6, 0, 0)),
            Q(L, ( 0,+8, 0, 0), (-3,+6, 0, 0)),
            Q(E, ( 0,+8, 0, 0), ( 0,+4, 0, 0)),

            Q(B, ( 0,-7, 0, 0), ( 0,-3,+6,30)),
            Q(P, (-2,-3, 0, 0), ( 0,-3,+6,30)),
            Q(E, ( 0,+5, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_K_WITH_HOOK,
        .draw = MERGE(
            REF(UX_LATIN_SMALL_LETTER_K_WITH_SHORT_STEM),
            XFORM2(xlat_relx, 0,-5, REF(UX_RIGHT_HOOK_ABOVE_IN))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_L_WITH_RETROFLEX_HOOK,
        .draw = MERGE(
            STROKE(
              Q(B, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
              Q(E, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            ),
            REF(UX_RIGHT_HOOK_BELOW_OUT)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_M_WITH_HOOK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_M),
            XFORM2(xlat_relx, 0,+8, REF(UX_LEFT_HOOK_BELOW_IN))
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_N_WITH_LEFT_HOOK,
        .line_step = LS_UPPER,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_N),
            XFORM2(xlat_relx, 0,-7, REF(UX_CAPITAL_LEFT_HOOK_BELOW_OUT))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_N_WITH_LEFT_HOOK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_N),
            XFORM2(xlat_relx, 0,-5, REF(UX_LEFT_HOOK_BELOW_OUT))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_N_WITH_RETROFLEX_HOOK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_N),
            XFORM2(xlat_relx, 0,+5, REF(UX_RIGHT_HOOK_BELOW_OUT))
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_P_WITH_HOOK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_P),
            XFORM2(xlat_relx, 0,-5, REF(UX_RIGHT_HOOK_ABOVE_IN)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_Q_WITH_HOOK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_Q),
            XFORM2(xlat_relx, 0,+5, REF(UX_RIGHT_HOOK_ABOVE_OUT)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_R_WITH_TAIL,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_R),
            XFORM2(xlat_relx, 0,-4, REF(UX_RIGHT_HOOK_BELOW_IN))
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_T_WITH_RETROFLEX_HOOK,
        .line_step = LS_UPPER,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_T),
            REF(UX_CAPITAL_RIGHT_HOOK_BELOW_IN)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_T_WITH_HOOK,
        .draw = MERGE(
            REF(UX_LATIN_SMALL_LETTER_T_WITH_SHORT_STEM),
            XFORM2(xlat_relx, 0,-3, REF(UX_RIGHT_HOOK_ABOVE_IN))
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_Y_WITH_HOOK,
        .line_step = LS_UPPER,
        .draw = STROKE(
            Q(B, ( 0, -7, 0, 0), ( 0,+6, 0, 0)),
            Q(H, ( 0, -7, 0, 0), (-1,+1, 0, 0)),
            Q(H, ( 0, +7, 0, 0), (-1,+1, 0, 0)),
            Q(L, ( 0, +7, 0, 0), (-3,+7, 0, 0)),
            Q(E, ( 0,+11, 0, 0), (-3,+7, 0, 0)),
            Q(B, ( 0,  0, 0, 0), ( 0,+1, 0, 0)),
            Q(E, ( 0,  0, 0, 0), ( 0,-3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_Y_WITH_HOOK,
        .draw = STROKE(
            Q(B, ( 0, -5, 0, 0), (-2,-6, 0, 0)),
            Q(L, ( 0, +5, 0, 0), (-2,-6, 0, 0)),
            Q(L, ( 0, +5, 0, 0), (-3,+4, 0, 0)),
            Q(E, ( 0,+10, 0, 0), (-3,+4, 0, 0)),
            Q(B, ( 0, -5, 0, 0), ( 0,+3, 0, 0)),
            Q(L, ( 0, -5, 0, 0), (-2,-3, 0, 0)),
            Q(E, ( 0, +5, 0, 0), (-2,-3, 0, 0)),
        ),
    },

    /* ligatures */
    {
        .unicode = UX_LATIN_SMALL_LETTER_F_WITH_LONG_MIDDLE_STROKE,
        .rpad_abs = -(PAD_DEFAULT + 0.75),
        .draw = STROKE(
            Q(B, ( 0,+4, 0, 0), (-2,+6, 0, 0)),
            Q(L, ( 0,-3, 0, 0), (-2,+6, 0, 0)),
            Q(E, ( 0,-3, 0, 0), ( 0,-3, 0, 0)),
            Q(B, ( 0,-3, 0, 0), (-3,+3, 0, 0)),
            Q(E, ( 0,+6, 0, 0), (-3,+3, 0, 0)),
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LETTER_F_WITH_LONG_TOP_STROKE,
        .rpad_abs = -PAD_DEFAULT,
        .draw = STROKE(
            Q(B, ( 0,+5, 0, 0), (-3,+6, 0, 0)),
            Q(L, ( 0,-3, 0, 0), (-3,+6, 0, 0)),
            Q(E, ( 0,-3, 0, 0), ( 0,-3, 0, 0)),
            Q(B, ( 0,-3, 0, 0), (-3,+3, 0, 0)),
            Q(E, ( 0,+3, 0, 0), (-3,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LIGATURE_FL,
        .map = MAP(
            LIGA(
               U_LATIN_SMALL_LETTER_F,
               U_LATIN_SMALL_LETTER_L
            )
        ),
        .draw = SEQ(
            SUBGLYPH(0, UX_LATIN_SMALL_LETTER_F_WITH_LONG_TOP_STROKE),
            SUBGLYPH(0, U_LATIN_SMALL_LETTER_L)
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LIGATURE_FT,
        .map = MAP(
            LIGA(
               U_LATIN_SMALL_LETTER_F,
               U_LATIN_SMALL_LETTER_T
            )
        ),
        .draw = SEQ(
            SUBGLYPH(0, UX_LATIN_SMALL_LETTER_F_WITH_LONG_MIDDLE_STROKE),
            SUBGLYPH(0, U_LATIN_SMALL_LETTER_T)
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LIGATURE_FJ,
        .map = MAP(
            LIGA(
               U_LATIN_SMALL_LETTER_F,
               U_LATIN_SMALL_LETTER_J
            )
        ),
        .draw = SEQ(
            SUBGLYPH(0,    UX_LATIN_SMALL_LETTER_F_WITH_LONG_MIDDLE_STROKE),
            SUBGLYPH(-7.5, U_LATIN_SMALL_LETTER_J)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LIGATURE_FI,
        .map = MAP(
            LIGA(
               U_LATIN_SMALL_LETTER_F,
               U_LATIN_SMALL_LETTER_I
            )
        ),
        .draw = SEQ(
            SUBGLYPH(0, UX_LATIN_SMALL_LETTER_F_WITH_LONG_MIDDLE_STROKE),
            SUBGLYPH(0, U_LATIN_SMALL_LETTER_I)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LIGATURE_FF,
        .map = MAP(
            LIGA(
               U_LATIN_SMALL_LETTER_F,
               U_LATIN_SMALL_LETTER_F
            )
        ),
        .draw = SEQ(
            SUBGLYPH(0, UX_LATIN_SMALL_LETTER_F_WITH_LONG_MIDDLE_STROKE),
            SUBGLYPH(0, U_LATIN_SMALL_LETTER_F)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LIGATURE_FFI,
        .map = MAP(
            LIGA(
               U_LATIN_SMALL_LIGATURE_FF,
               U_LATIN_SMALL_LETTER_I
            ),
            LIGA(
               U_LATIN_SMALL_LETTER_F,
               U_LATIN_SMALL_LIGATURE_FI
            ),
        ),
        .draw = SEQ(
            SUBGLYPH(0, UX_LATIN_SMALL_LETTER_F_WITH_LONG_MIDDLE_STROKE),
            SUBGLYPH(0, U_LATIN_SMALL_LIGATURE_FI)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LIGATURE_FFL,
        .map = MAP(
            LIGA(
               U_LATIN_SMALL_LIGATURE_FF,
               U_LATIN_SMALL_LETTER_L
            ),
            LIGA(
               U_LATIN_SMALL_LETTER_F,
               U_LATIN_SMALL_LIGATURE_FL
            ),
        ),
        .draw = SEQ(
            SUBGLYPH(0, UX_LATIN_SMALL_LETTER_F_WITH_LONG_MIDDLE_STROKE),
            SUBGLYPH(0, U_LATIN_SMALL_LIGATURE_FL)
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LIGATURE_FFT,
        .map = MAP(
            LIGA(
               U_LATIN_SMALL_LIGATURE_FF,
               U_LATIN_SMALL_LETTER_T
            ),
            LIGA(
               U_LATIN_SMALL_LETTER_F,
               UX_LATIN_SMALL_LIGATURE_FT
            ),
        ),
        .draw = SEQ(
            SUBGLYPH(0, UX_LATIN_SMALL_LETTER_F_WITH_LONG_MIDDLE_STROKE),
            SUBGLYPH(0, UX_LATIN_SMALL_LIGATURE_FT)
        ),
    },



    /* combined letters (these are kept for ligatures) */
    {
        .unicode = U_LATIN_SMALL_LETTER_I,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_DOTLESS_I),
            REF_DOT_ABOVE,
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_J,
        .draw = MERGE(
            WIDTH(U_LATIN_SMALL_LETTER_DOTLESS_J),
            REF(U_LATIN_SMALL_LETTER_DOTLESS_J),
            REF_DOT_ABOVE,
        ),
    },

    /* horn */
    {
        .unicode = U_COMBINING_HORN,
        .draw = MERGE(
          STROKE(
            Q(B, ( 0,+5, 0, 0), ( 0,+1,+2,20)),
            Q(S, ( 0,+9, 0, 0), ( 0,+1,+2,20)),
            Q(E, ( 0,+9, 0, 0), ( 0,+3, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_O_WITH_HORN,
        .draw = MERGE(
          REF(U_LATIN_SMALL_LETTER_O),
          REF(U_COMBINING_HORN),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_U_WITH_HORN,
        .draw = MERGE(
          REF(U_LATIN_SMALL_LETTER_U),
          STROKE(
            Q(B, (+2,+5, 0, 0), ( 0,+1,+2,20)),
            Q(S, (+2,+9, 0, 0), ( 0,+1,+2,20)),
            Q(E, (+2,+9, 0, 0), ( 0,+3, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_O_WITH_HORN,
        .line_step = LS_UPPER,
        .draw = MERGE(
          REF(U_LATIN_CAPITAL_LETTER_O),
          STROKE(
            Q(B, ( 0, +7, 0, 0), ( 0,+4,+5,20)),
            Q(S, ( 0,+10, 0, 0), ( 0,+4,+5,20)),
            Q(E, ( 0,+10, 0, 0), ( 0,+6, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_U_WITH_HORN,
        .line_step = LS_UPPER,
        .draw = MERGE(
          REF(U_LATIN_CAPITAL_LETTER_U),
          STROKE(
            Q(B, (+2, +7, 0, 0), ( 0,+4,+5,20)),
            Q(S, (+2,+10, 0, 0), ( 0,+4,+5,20)),
            Q(E, (+2,+10, 0, 0), ( 0,+6, 0, 0)),
          )
        ),
    },

    /* dot above */
    {
        .unicode = U_COMBINING_DOT_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DOT_ABOVE,
        .line_step = LS_LOWER,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+6,+5,20)),
            Q(E, ( 0, 0, 0, 0), ( 0,+6,+5,20, -60)),
        ),
    },
    {
        .unicode = U_DOT_ABOVE,
        .draw = SAME(U_COMBINING_DOT_ABOVE),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DOT_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+8,+7,30)),
            Q(E, ( 0, 0, 0, 0), ( 0,+8,+7,30, +60)),
        ),
    },

    /* diaeresis */
    {
        .unicode = U_COMBINING_DIAERESIS,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DIAERESIS,
        .line_step = LS_LOWER,
        .draw = STROKE(
            Q(B, (+3,-2, 0, 0), ( 0,+6,+5,20)),
            Q(E, (+3,-2, 0, 0), ( 0,+6,+5,20, -60)),
            Q(B, (+3,+2, 0, 0), ( 0,+6,+5,20)),
            Q(E, (+3,+2, 0, 0), ( 0,+6,+5,20, -60)),
        ),
    },
    {
        .unicode = U_DIAERESIS,
        .draw = SAME(U_COMBINING_DIAERESIS),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DIAERESIS,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_LOWER,
        .draw = STROKE(
            Q(B, (+3,-2, 0, 0), ( 0,+8,+7,30)),
            Q(E, (+3,-2, 0, 0), ( 0,+8,+7,30, +60)),
            Q(B, (+3,+2, 0, 0), ( 0,+8,+7,30)),
            Q(E, (+3,+2, 0, 0), ( 0,+8,+7,30, +60)),
        ),
    },
    /* corrected forms */
    {
        .unicode = U_LATIN_SMALL_LETTER_T_WITH_DIAERESIS,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_T),
            XFORM2(xlat, 0,7, REF_DIAERESIS),
        ),
    },

    /* acute */
    {
        .unicode = U_COMBINING_ACUTE_ACCENT,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_ACUTE,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,+4, 0, 0), ( 0,+7, 0, 0)),
            Q(O, ( 0,-1, 0, 0), ( 0,+5, 0, 0)),
        ),
    },
    {
        .unicode = U_ACUTE_ACCENT,
        .max_coord = COORD(0,+6, 0, 0),
        .draw = STROKE(
            Q(I, ( 0, 0, 0, 0), ( 0,+3, 0, 0)),
            Q(O, ( 0,+6, 0, 0), (-1,+6, 0, 0)),
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,+5, 0, 0), ( 0,+10, 0, 0)),
            Q(O, ( 0,-2, 0, 0), ( 0,+8, 0, 0)),
        ),
    },

    /* grave */
    {
        .unicode = U_COMBINING_GRAVE_ACCENT,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_GRAVE,
        .line_step = LS_THIN,
        .draw = XFORM(swap_x, REF(U_COMBINING_ACUTE_ACCENT)),
    },
    {
        .unicode = U_GRAVE_ACCENT,
        //.min_coord = COORD(0,-6, 0, 0),
        .draw = XFORM(swap_x, REF(U_ACUTE_ACCENT)),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = XFORM(swap_x, REF(UX_COMBINING_CAPITAL_ACUTE)),
    },

    /* caron below */
    {
        .unicode = U_COMBINING_CARON_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), ( 0,-5,-4,30)),
            Q(P, ( 0, 0, 0, 0), ( 0,-6, 0, 0)),
            Q(O, ( 0,+4, 0, 0), ( 0,-5,-4,30)),
        ),
    },

    /* caron / hacek */
    {
        .unicode = U_COMBINING_CARON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_CARON,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), ( 0,+7,+6,30)),
            Q(P, ( 0, 0, 0, 0), ( 0,+5, 0, 0)),
            Q(O, ( 0,+4, 0, 0), ( 0,+7,+6,30)),
        ),
    },
    {
        .unicode = U_CARON,
        .min_coord = COORD(0,-6, 0, 0),
        .max_coord = COORD(0,+6, 0, 0),
        .draw = STROKE(
            Q(I, ( 0,-6, 0, 0), (-1,+6, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0,+3, 0, 0)),
            Q(O, ( 0,+6, 0, 0), (-1,+6, 0, 0)),
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_CARON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), ( 0,+10, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0,+8, 0, 0)),
            Q(O, ( 0,+4, 0, 0), ( 0,+10, 0, 0)),
        ),
    },
    /* caron special forms: */
    {
        .unicode = U_LATIN_SMALL_LETTER_D_WITH_CARON,
        //.max_coord = COORD(+2,+9,0,0),
        .draw = MERGE(
          REF(U_LATIN_SMALL_LETTER_D),
          STROKE(
            Q(I, (+2,+10, 0, 0), ( 0,+6, 0, 0)),
            Q(O, (+2, +9, 0, 0), ( 0,+4, 0, 0)),
          ),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_T_WITH_CARON,
        .draw = MERGE(
          WIDTH(U_LATIN_SMALL_LETTER_T),
          REF(U_LATIN_SMALL_LETTER_T),
          STROKE(
            Q(I, (+3, +2, 0, 0), ( 0,+7, 0, 0)),
            Q(O, (+3, +1, 0, 0), ( 0,+5, 0, 0)),
          ),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_L_WITH_CARON,
        //.max_coord = COORD(0,+5,0,0),
        .draw = MERGE(
          REF(U_LATIN_SMALL_LETTER_L),
          STROKE(
            Q(I, (+2,+7, 0, 0), ( 0,+6, 0, 0)),
            Q(O, (+2,+4, 0, 0), ( 0,+4, 0, 0)),
          ),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_L_WITH_CARON,
        .draw = MERGE(
          REF(U_LATIN_CAPITAL_LETTER_L),
          STROKE(
            Q(I, (+2,+3, 0, 0), ( 0,+6, 0, 0)),
            Q(O, (+2, 0, 0, 0), ( 0,+4, 0, 0)),
          ),
        ),
    },
    /* explicit caron carriers for ligatures (cannot currently be emulated
     * by renderer) */
    {
        .unicode = U_LATIN_SMALL_LETTER_Z_WITH_CARON,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_Z),
            REF(U_COMBINING_CARON),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_Z_WITH_CARON,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_Z),
            REF(UX_COMBINING_CAPITAL_CARON),
        ),
    },

    /* MISSING: */
    /* 01EE;LATIN CAPITAL LETTER EZH WITH CARON */
    /* 01EF;LATIN SMALL LETTER EZH WITH CARON */

    /* circumflex below */
    {
        .unicode = U_COMBINING_CIRCUMFLEX_ACCENT_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), ( 0,-6, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0,-5,-4,30)),
            Q(O, ( 0,+4, 0, 0), ( 0,-6, 0, 0)),
        ),
    },

    /* circumflex */
    {
        .unicode = U_COMBINING_CIRCUMFLEX_ACCENT,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_CIRCUMFLEX,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), ( 0,+5, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0,+7,+6,30)),
            Q(O, ( 0,+4, 0, 0), ( 0,+5, 0, 0)),
        ),
    },
    {
        .unicode = U_CIRCUMFLEX_ACCENT,
        .min_coord = COORD(0,-6, 0, 0),
        .max_coord = COORD(0,+6, 0, 0),
        .draw = STROKE(
            Q(I, ( 0,-6, 0, 0), ( 0,+3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-1,+6, 0, 0)),
            Q(O, ( 0,+6, 0, 0), ( 0,+3, 0, 0)),
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_CIRCUMFLEX,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), ( 0, +8, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0,+10, 0, 0)),
            Q(O, ( 0,+4, 0, 0), ( 0, +8, 0, 0)),
        ),
    },

    /* tilde below */
    {
        .unicode = U_COMBINING_TILDE_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,+5, 0, 0), ( 0,-4,-6, 20)),
            Q(L, ( 0,+6,-6,20), ( 0,-4,-6, 80)),
            Q(L, ( 0,-6,+6,20), ( 0,-4,-6,  0)),
            Q(O, ( 0,-5, 0, 0), ( 0,-4,-6, 60)),
        ),
    },

    /* tilde */
    {
        .unicode = U_COMBINING_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_TILDE,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,+5, 0, 0), ( 0,+5,+6, 60)),
            Q(L, ( 0,+6,-6,20), ( 0,+5,+6,-20)),
            Q(L, ( 0,-6,+6,20), ( 0,+5,+6, 80)),
            Q(O, ( 0,-5, 0, 0), ( 0,+5,+6,  0)),
        ),
    },
    {
        .unicode = U_SMALL_TILDE,
        .draw = SAME(U_COMBINING_TILDE),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,+5, 0, 0), ( 0,+8,+10, 60)),
            Q(L, ( 0,+6,-6,20), ( 0,+8,+10,-20)),
            Q(L, ( 0,-6,+6,20), ( 0,+8,+10, 80)),
            Q(O, ( 0,-5, 0, 0), ( 0,+8,+10,  0)),
        ),
    },

    /* ring below */
    {
        .unicode = U_COMBINING_RING_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(L, ( 0,-3, 0, 0), (+2,-4, 0, 0)),
            Q(L, ( 0,-3, 0, 0), (+3,-6, 0, 0)),
            Q(L, ( 0,+3, 0, 0), (+3,-6, 0, 0)),
            Q(L, ( 0,+3, 0, 0), (+2,-4, 0, 0)),
        ),
    },

    /* ring above */
    {
        .unicode = U_COMBINING_RING_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_RING_ABOVE,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(L, ( 0,-3, 0, 0), (+2,+4, 0, 0)),
            Q(L, ( 0,-3, 0, 0), ( 0,+7,+6,30)),
            Q(L, ( 0,+3, 0, 0), ( 0,+7,+6,30)),
            Q(L, ( 0,+3, 0, 0), (+2,+4, 0, 0)),
        ),
    },
    {
        .unicode = U_RING_ABOVE,
        .draw = SAME(U_COMBINING_RING_ABOVE),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_RING_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(L, ( 0,-4,-3,60), (-2,+6,  0, 0)),
            Q(L, ( 0,-4,-3,60), ( 0,+9,+10,10)),
            Q(L, ( 0,+4,+3,60), ( 0,+9,+10,10)),
            Q(L, ( 0,+4,+3,60), (-2,+6,  0, 0)),
        ),
    },

    /* vertical line above */
    {
        .unicode = U_COMBINING_VERTICAL_LINE_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_VERTICAL_LINE_ABOVE,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,+5, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+7,+8,40)),
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_VERTICAL_LINE_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0, +7, 0,  0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+10,+11,40)),
        ),
    },

    /* vertical line below */
    {
        .unicode = U_COMBINING_VERTICAL_LINE_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-6, 0, 0, +40)),
            Q(E, ( 0, 0, 0, 0), ( 0,-5,-6,20, -60)),
        ),
    },

    /* double vertical line below */
    {
        .unicode = U_COMBINING_DOUBLE_VERTICAL_LINE_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(B, (+3,-2, 0, 0), ( 0,-6, 0, 0, +40)),
            Q(E, (+3,-2, 0, 0), ( 0,-5,-6,20, -60)),
            Q(B, (+3,+2, 0, 0), ( 0,-6, 0, 0, +40)),
            Q(E, (+3,+2, 0, 0), ( 0,-5,-6,20, -60)),
        ),
    },

    /* dot below */
    {
        .unicode = U_COMBINING_DOT_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-5,-6,20)),
            Q(E, ( 0, 0, 0, 0), ( 0,-5,-6,20, -60)),
        ),
    },

    /* diaeresis below */
    {
        .unicode = U_COMBINING_DIAERESIS_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_LOWER,
        .draw = STROKE(
            Q(B, (+3,-2, 0, 0), ( 0,-5,-6,20)),
            Q(E, (+3,-2, 0, 0), ( 0,-5,-6,20, -60)),
            Q(B, (+3,+2, 0, 0), ( 0,-5,-6,20)),
            Q(E, (+3,+2, 0, 0), ( 0,-5,-6,20, -60)),
        ),
    },

    /* comma above */
    {
        .unicode = U_COMBINING_COMMA_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_COMMA_ABOVE,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,+2,+1,30), ( 0,+6, 0, 0, 0)),
         /* Q(P, ( 0,+2,+1,30), ( 0,+4,+6,30, 0)), */
            Q(O, ( 0,-1,-2,30), ( 0,+4, 0, 0, +10)),
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_COMMA_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,+2,+1,30), ( 0,+10,  0, 0, 0)),
         /* Q(P, ( 0,+2,+1,30), ( 0, +7,+10,30, 0)), */
            Q(O, ( 0,-1,-2,30), ( 0, +7,  0, 0, +10)),
        ),
    },

    /* reversed comma above */
    {
        .unicode = U_COMBINING_REVERSED_COMMA_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_REVERSED_COMMA_ABOVE,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,-2,-1,30), ( 0,+6, 0, 0, 0)),
         /* Q(P, ( 0,-2,-1,30), ( 0,+4,+6,30, 0)), */
            Q(O, ( 0,+1,+2,30), ( 0,+4, 0, 0, +10)),
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_REVERSED_COMMA_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,-2,-1,30), ( 0,+10,  0, 0, 0)),
         /* Q(P, ( 0,-2,-1,30), ( 0, +7,+10,30, 0)), */
            Q(O, ( 0,+1,+2,30), ( 0, +7,  0, 0, +10)),
        ),
    },

    /* turned comma above */
    {
        .unicode = U_COMBINING_TURNED_COMMA_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_TURNED_COMMA_ABOVE,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,+2,+1,30), ( 0,+6, 0, 0, 0)),
         /* Q(P, ( 0,-1,-2,30), ( 0,+4,+6,35, 0)), */
            Q(O, ( 0,-1,-2,30), ( 0,+4, 0, 0, +10)),
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_TURNED_COMMA_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,+2,+1,30), ( 0,+10,  0, 0, 0)),
         /* Q(P, ( 0,-1,-2,30), ( 0, +7,+10,35, 0)), */
            Q(O, ( 0,-1,-2,30), ( 0, +7,  0, 0, +10)),
        ),
    },

    /* cedilla and comma below */
    {
        .unicode = U_COMBINING_COMMA_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(I, ( 0,+2,+1,30), ( 0,-4, 0, 0, -10)),
         /* Q(P, ( 0,+2,+1,30), ( 0,-4,-6,35, 0)), */
            Q(O, ( 0,-1,-2,30), ( 0,-6, 0, 0, 0)),
        ),
    },
    {
        .unicode = U_COMBINING_CEDILLA,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THIN,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), (-2,-3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0,-3,-4,20)),
            Q(S, ( 0,+3, 0, 0), ( 0,-4, 0, 0)),
            Q(S, ( 0,+3, 0, 0), (-1,-6, 0, 0)),
            Q(E, ( 0,-3, 0, 0), (-1,-6, 0, 0)),
        ),
    },
    {
        .unicode = U_CEDILLA,
        .draw = SAME(U_COMBINING_CEDILLA),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_T_WITH_CEDILLA,
        //.max_coord = COORD(0,+4,0,0),
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_T),
            XFORM2(xlat_relx, 0,+1, REF(U_COMBINING_CEDILLA)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_H_WITH_CEDILLA,
        .draw = MERGE(
            WIDTH(U_LATIN_SMALL_LETTER_H),
            REF(U_LATIN_SMALL_LETTER_H),
            XFORM2(xlat_relx, 0,-5, REF(U_COMBINING_CEDILLA)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_H_WITH_CEDILLA,
        //.min_coord = COORD(+3,-7,0,0),
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_H),
            XFORM2(xlat_relx, 0,-7, REF(U_COMBINING_CEDILLA)),
        ),
    },
    {
        .unicode = UX_LATIN_CAPITAL_LETTER_N_WITH_REAL_CEDILLA,
        //.min_coord = COORD(+3,-7,0,0),
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_N),
            XFORM2(xlat_relx, 0,-7, REF(U_COMBINING_CEDILLA)),
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LETTER_D_WITH_COMMA_BELOW,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_D),
            REF(U_COMBINING_COMMA_BELOW),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_D_WITH_CEDILLA,
        .map = MAP(
            LANG_REPLACE(lang_LIV, UX_LATIN_SMALL_LETTER_D_WITH_COMMA_BELOW),
        ),
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_D),
            REF(U_COMBINING_CEDILLA),
        ),
    },
    {
        /* misnomer: this is comma below, not cedilla */
        .unicode = U_LATIN_SMALL_LETTER_K_WITH_CEDILLA,
        .draw = MERGE(
            WIDTH(U_LATIN_SMALL_LETTER_K),
            REF(U_LATIN_SMALL_LETTER_K),
            XFORM2(xlat_relx, 0,-1, REF(U_COMBINING_COMMA_BELOW)),
        ),
    },
    {
        /* misnomer: this is comma below, not cedilla */
        .map = MAP(
            /* in Marshallese, not a misnomer, but real cedilla */
            LANG_REPLACE(lang_MAH, UX_LATIN_SMALL_LETTER_L_WITH_REAL_CEDILLA)
        ),
        .unicode = U_LATIN_SMALL_LETTER_L_WITH_CEDILLA,
        .draw = MERGE(
            WIDTH(U_LATIN_SMALL_LETTER_L),
            REF(U_LATIN_SMALL_LETTER_L),
            REF(U_COMBINING_COMMA_BELOW),
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LETTER_L_WITH_REAL_CEDILLA,
        .draw = MERGE(
            WIDTH(U_LATIN_SMALL_LETTER_L),
            REF(U_LATIN_SMALL_LETTER_L),
            XFORM2(xlat_relx, 0,+2, REF(U_COMBINING_CEDILLA)),
        ),
    },
    {
        /* misnomer: this is comma below, not cedilla */
        .map = MAP(
            /* in Marshallese, not a misnomer, but real cedilla */
            LANG_REPLACE(lang_MAH, UX_LATIN_SMALL_LETTER_N_WITH_REAL_CEDILLA)
        ),
        .unicode = U_LATIN_SMALL_LETTER_N_WITH_CEDILLA,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_N),
            REF(U_COMBINING_COMMA_BELOW),
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LETTER_N_WITH_REAL_CEDILLA,
        .draw = MERGE(
            WIDTH(U_LATIN_SMALL_LETTER_N),
            REF(U_LATIN_SMALL_LETTER_N),
            XFORM2(xlat_relx, 0,-5, REF(U_COMBINING_CEDILLA)),
        ),
    },
    {
        .unicode = UX_LATIN_SMALL_LETTER_M_WITH_CEDILLA,
        .map = MAP(
            CANON(
                U_LATIN_SMALL_LETTER_M,
                U_COMBINING_CEDILLA
            )
        ),
        .draw = MERGE(
            WIDTH(U_LATIN_SMALL_LETTER_M),
            REF(U_LATIN_SMALL_LETTER_M),
            XFORM2(xlat_relx, 0,-8, REF(U_COMBINING_CEDILLA)),
        ),
    },
    {
        .unicode = UX_LATIN_CAPITAL_LETTER_M_WITH_CEDILLA,
        .map = MAP(
            CANON(
                U_LATIN_CAPITAL_LETTER_M,
                U_COMBINING_CEDILLA
            )
        ),
        .draw = MERGE(
            WIDTH(U_LATIN_CAPITAL_LETTER_M),
            REF(U_LATIN_CAPITAL_LETTER_M),
            XFORM2(xlat_relx, 0,-8.7, REF(U_COMBINING_CEDILLA)),
        ),
    },
    {
        /* misnomer: this is comma below, not cedilla */
        .unicode = U_LATIN_SMALL_LETTER_R_WITH_CEDILLA,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_R),
            XFORM2(xlat_relx, 0,-2, REF(U_COMBINING_COMMA_BELOW)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_D_WITH_CEDILLA,
        .map = MAP(
            /* Livonian wants a comma below glyph. */
            LANG_REPLACE(lang_LIV, UX_LATIN_CAPITAL_LETTER_D_WITH_COMMA_BELOW),
        ),
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_D),
            REF(U_COMBINING_CEDILLA),
        ),
    },
    {
        .unicode = UX_LATIN_CAPITAL_LETTER_D_WITH_COMMA_BELOW,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_D),
            REF(U_COMBINING_COMMA_BELOW),
        ),
    },
    {
        /* misnomer: this is comma below, not cedilla */
        .map = MAP(
            /* in Marshallese, not a misnomer, but real cedilla */
            LANG_REPLACE(lang_MAH, UX_LATIN_SMALL_LETTER_L_WITH_REAL_CEDILLA),
        ),
        .unicode = U_LATIN_CAPITAL_LETTER_L_WITH_CEDILLA,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_L),
            REF(U_COMBINING_COMMA_BELOW),
        ),
    },
    {
        .unicode = UX_LATIN_CAPITAL_LETTER_L_WITH_REAL_CEDILLA,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_L),
            REF(U_COMBINING_CEDILLA),
        ),
    },
    {
        /* misnomer: this is comma below, not cedilla */
        .unicode = U_LATIN_CAPITAL_LETTER_K_WITH_CEDILLA,
        .draw = MERGE(
            WIDTH(U_LATIN_CAPITAL_LETTER_K),
            REF(U_LATIN_CAPITAL_LETTER_K),
            XFORM2(xlat_relx, 0,-1, REF(U_COMBINING_COMMA_BELOW)),
        ),
    },
    {
        /* misnomer: this is comma below, not cedilla */
        .map = MAP(
            /* in Marshallese, not a misnomer, but real cedilla */
            LANG_REPLACE(lang_MAH, UX_LATIN_SMALL_LETTER_L_WITH_REAL_CEDILLA),
        ),
        .unicode = U_LATIN_CAPITAL_LETTER_N_WITH_CEDILLA,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_N),
            REF(U_COMBINING_COMMA_BELOW),
        ),
    },
    {
        /* misnomer: this is comma below, not cedilla */
        .unicode = U_LATIN_CAPITAL_LETTER_R_WITH_CEDILLA,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_R),
            REF(U_COMBINING_COMMA_BELOW),
        ),
    },
    {
        /* misnomer: this is comma below, not cedilla */
        .unicode = U_LATIN_CAPITAL_LETTER_G_WITH_CEDILLA,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_G),
            REF(U_COMBINING_COMMA_BELOW),
        ),
    },
    {
        /* misnomer: this is turned comma above, not a cedilla */
        .unicode = U_LATIN_SMALL_LETTER_G_WITH_CEDILLA,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_G),
            XFORM2(xlat_relx, 0,+1, REF(U_COMBINING_TURNED_COMMA_ABOVE)),
        ),
    },

    /* line below */
    {
        .unicode = U_COMBINING_MACRON_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(B, ( 0,-5,-4,30), (-2,-5, 0, 0)),
            Q(E, ( 0,+5,+4,30), (-2,-5, 0, 0)),
        ),
    },

    /* macron */
    {
        .unicode = U_COMBINING_MACRON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_MACRON,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(B, ( 0,-5,-4,30), (+2,+5, 0, 0)),
            Q(E, ( 0,+5,+4,30), (+2,+5, 0, 0)),
        ),
    },
    {
        .unicode = U_MACRON,
        .draw = SAME(U_COMBINING_MACRON),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_MACRON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(B, ( 0,-5,-4,30), (+2,+8, 0, 0)),
            Q(E, ( 0,+5,+4,30), (+2,+8, 0, 0)),
        ),
    },

    /* enclosing circle */
    {
        .unicode = U_COMBINING_ENCLOSING_CIRCLE,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(L, (.olen={-4,+7,-30}), (0,-4,+7,30)),
            Q(L, (.olen={-4,+7,-21}), (0,-4,+7,51)),
            Q(L, (.olen={-4,+7,  0}), (0,-4,+7,60)),
            Q(L, (.olen={-4,+7,+21}), (0,-4,+7,51)),
            Q(L, (.olen={-4,+7,+30}), (0,-4,+7,30)),
            Q(L, (.olen={-4,+7,+21}), (0,-4,+7, 9)),
            Q(L, (.olen={-4,+7,  0}), (0,-4,+7, 0)),
            Q(L, (.olen={-4,+7,-21}), (0,-4,+7, 9)),
        ),
    },

    /* breve below */
    {
        .unicode = U_COMBINING_BREVE_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(B, (0,-4, 0, 0), (+2,-4, 0, 0)),
            Q(L, (0,-4, 0, 0), (+2,-6, 0, 0)),
            Q(L, (0,+4, 0, 0), (+2,-6, 0, 0)),
            Q(E, (0,+4, 0, 0), (+2,-4, 0, 0)),
        ),
    },

    /* inverted breve below */
    {
        .unicode = U_COMBINING_INVERTED_BREVE_BELOW,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(B, (0,-4, 0, 0), (+2,-6, 0, 0)),
            Q(L, (0,-4, 0, 0), (+2,-4, 0, 0)),
            Q(L, (0,+4, 0, 0), (+2,-4, 0, 0)),
            Q(E, (0,+4, 0, 0), (+2,-6, 0, 0)),
        ),
    },

    /* breve */
    {
        .unicode = U_COMBINING_BREVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_BREVE,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(B, ( 0,-4, 0, 0), ( 0,+7, 0, 0)),
            Q(L, ( 0,-4, 0, 0), (+2,+5, 0, 0)),
            Q(L, ( 0,+4, 0, 0), (+2,+5, 0, 0)),
            Q(E, ( 0,+4, 0, 0), ( 0,+7, 0, 0)),
        ),
    },
    {
        .unicode = U_BREVE,
        .draw = SAME(U_COMBINING_BREVE),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_BREVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(B, (-2,-5, 0, 0), ( 0,+10, 0, 0)),
            Q(L, (-2,-5, 0, 0), (-2, +8, 0, 0)),
            Q(L, (-2,+5, 0, 0), (-2, +8, 0, 0)),
            Q(E, (-2,+5, 0, 0), ( 0,+10, 0, 0)),
        ),
    },

    /* inverted breve */
    {
        .unicode = U_COMBINING_INVERTED_BREVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_INVERTED_BREVE,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(B, ( 0,-4, 0, 0), ( 0,+5, 0, 0)),
            Q(L, ( 0,-4, 0, 0), (-2,+7, 0, 0)),
            Q(L, ( 0,+4, 0, 0), (-2,+7, 0, 0)),
            Q(E, ( 0,+4, 0, 0), ( 0,+5, 0, 0)),
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_INVERTED_BREVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(B, (-2,-5, 0, 0), (-3, +8, 0, 0)),
            Q(L, (-2,-5, 0, 0), ( 0,+10, 0, 0)),
            Q(L, (-2,+5, 0, 0), ( 0,+10, 0, 0)),
            Q(E, (-2,+5, 0, 0), (-3, +8, 0, 0)),
        ),
    },

    /* hook above */
    {
        .unicode = U_COMBINING_HOOK_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_HOOK_ABOVE,
        .line_step = LS_THINNER,
        .draw = STROKE(
            Q(I, (0,-4,4, +9), (0,5,7,+50)),
            Q(L, (0,-4,4,+26), (0,5,7,+64)),
            Q(L, (0,-4,4,+44), (0,5,7,+58)),
            Q(L, (0,-4,4,+54), (0,5,7,+35)),
            Q(L, (0,-4,4,+50), (0,5,7, +9)),
            Q(O, (0,-4,4,+33), (0,5,7, -4)),
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_HOOK_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = XFORM2(xlat_rely, +5,+7, REF(U_COMBINING_HOOK_ABOVE)),
    },

    /* ogonek */
    {
        .unicode = U_COMBINING_OGONEK,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .is_below = true,
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0,-3,-4,20)),
            Q(S, ( 0,-3, 0, 0), ( 0,-4, 0, 0)),
            Q(S, ( 0,-3, 0, 0), (-1,-6, 0, 0)),
            Q(E, ( 0,+3, 0, 0), (-1,-6, 0, 0)),
        ),
    },
    {
        .unicode = U_OGONEK,
        .draw = SAME(U_COMBINING_OGONEK),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_O_WITH_OGONEK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_O),
            XFORM2(xlat_relx, 0,+1, REF(U_COMBINING_OGONEK)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_U_WITH_OGONEK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_U),
            XFORM2(xlat_relx, 0,+1, REF(U_COMBINING_OGONEK)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_E_WITH_OGONEK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_E),
            XFORM2(xlat_relx, 0,+1.5, REF(U_COMBINING_OGONEK)),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_A_WITH_OGONEK,
        .draw = MERGE(
            REF(U_LATIN_SMALL_LETTER_A),
            XFORM2(xlat_relx, +3,+7, REF(U_COMBINING_OGONEK)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_E_WITH_OGONEK,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_E),
            XFORM2(xlat_relx, +3,+7, REF(U_COMBINING_OGONEK)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_U_WITH_OGONEK,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_U),
            XFORM2(xlat_relx, 0,+1, REF(U_COMBINING_OGONEK)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_O_WITH_OGONEK,
        .draw = MERGE(
            REF(U_LATIN_CAPITAL_LETTER_O),
            XFORM2(xlat_relx, 0,+1, REF(U_COMBINING_OGONEK)),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_A_WITH_OGONEK,
        .draw = MERGE(
            WIDTH(U_LATIN_CAPITAL_LETTER_A),
            REF(U_LATIN_CAPITAL_LETTER_A),
            XFORM2(xlat_relx, 0,+6, REF(U_COMBINING_OGONEK)),
        ),
    },

    /* double acute */
    {
        .unicode = U_COMBINING_DOUBLE_ACUTE_ACCENT,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DOUBLE_ACUTE,
        .line_step = LS_THIN,
        .draw = XFORM2(xlat_relx, 0,+2,
          STROKE(
            Q(I, (+2,+4, 0, 0), ( 0,+7, 0, 0)),
            Q(O, (+2, 0, 0, 0), ( 0,+5, 0, 0)),
            Q(I, (-2, 0, 0, 0), ( 0,+7, 0, 0)),
            Q(O, (+2,-4, 0, 0), ( 0,+5, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_DOUBLE_ACUTE_ACCENT,
        .draw = SAME(U_COMBINING_DOUBLE_ACUTE_ACCENT),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DOUBLE_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = XFORM2(xlat_relx, 0,+2,
          STROKE(
            Q(I, (+2,+5, 0, 0), ( 0,+10, 0, 0)),
            Q(O, (+2, 0, 0, 0), ( 0, +8,+7,30)),
            Q(I, (-2, 0, 0, 0), ( 0,+10, 0, 0)),
            Q(O, (+2,-5, 0, 0), ( 0, +8,+7,30)),
          )
        ),
    },

    /* double grave */
    {
        .unicode = U_COMBINING_DOUBLE_GRAVE_ACCENT,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DOUBLE_GRAVE,
        .line_step = LS_THIN,
        .draw = XFORM2(xlat_relx, 0,-2,
          STROKE(
            Q(I, (+2, 0, 0, 0), ( 0,+7, 0, 0)),
            Q(O, (+2,+4, 0, 0), ( 0,+5, 0, 0)),
            Q(I, (+2,-4, 0, 0), ( 0,+7, 0, 0)),
            Q(O, (-2, 0, 0, 0), ( 0,+5, 0, 0)),
          )
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DOUBLE_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THIN,
        .draw = XFORM2(xlat_relx, 0,-2,
          STROKE(
            Q(I, (+2, 0, 0, 0), ( 0,+10, 0, 0)),
            Q(O, (+2,+5, 0, 0), ( 0, +8,+7,30)),
            Q(I, (+2,-5, 0, 0), ( 0,+10, 0, 0)),
            Q(O, (-2, 0, 0, 0), ( 0, +8,+7,30)),
          )
        ),
    },

    /* diaeresis with acute */
    {
        .unicode = UX_COMBINING_DIAERESIS_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DIAERESIS_WITH_ACUTE,
        .map = MAP(
            CANON(
                U_COMBINING_DIAERESIS,
                U_COMBINING_ACUTE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF_DIAERESIS),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_ACUTE_ACCENT))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DIAERESIS_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF_CAPITAL_DIAERESIS),
          XFORM2(xlat_relx, 0,+1,
              XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_ACUTE)))
        ),
    },

    /* diaeresis with grave */
    {
        .unicode = UX_COMBINING_DIAERESIS_WITH_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DIAERESIS_WITH_GRAVE,
        .map = MAP(
            CANON(
                U_COMBINING_DIAERESIS,
                U_COMBINING_GRAVE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF_DIAERESIS),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_GRAVE_ACCENT))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DIAERESIS_WITH_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF_CAPITAL_DIAERESIS),
          XFORM2(xlat_relx, 0,-1,
              XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_GRAVE)))
        ),
    },

    /* diaeresis with caron */
    {
        .unicode = UX_COMBINING_DIAERESIS_WITH_CARON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DIAERESIS_WITH_CARON,
        .map = MAP(
            CANON(
                U_COMBINING_DIAERESIS,
                U_COMBINING_CARON
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF_DIAERESIS),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_CARON))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DIAERESIS_WITH_CARON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF_CAPITAL_DIAERESIS),
          XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_CARON))
        ),
    },

    /* diaeresis with tilde */
    {
        .unicode = UX_COMBINING_DIAERESIS_WITH_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DIAERESIS_WITH_TILDE,
        .map = MAP(
            CANON(
                U_COMBINING_DIAERESIS,
                U_COMBINING_TILDE
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF_DIAERESIS),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_TILDE))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DIAERESIS_WITH_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF_CAPITAL_DIAERESIS),
          XFORM2(xlat_rely, +8,+9.8, REF(UX_COMBINING_CAPITAL_TILDE))
        ),
    },

    /* diaeresis with macron */
    {
        .unicode = UX_COMBINING_DIAERESIS_WITH_MACRON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DIAERESIS_WITH_MACRON,
        .map = MAP(
            CANON(
                U_COMBINING_DIAERESIS,
                U_COMBINING_MACRON
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF_DIAERESIS),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_MACRON))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DIAERESIS_WITH_MACRON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8, +7.5, REF_CAPITAL_DIAERESIS),
          XFORM2(xlat_rely, +8,+10.2, REF(UX_COMBINING_CAPITAL_MACRON))
        ),
    },

    /* dot above with macron */
    {
        .unicode = UX_COMBINING_DOT_ABOVE_WITH_MACRON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DOT_ABOVE_WITH_MACRON,
        .map = MAP(
            CANON(
                U_COMBINING_DOT_ABOVE,
                U_COMBINING_MACRON
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF_DOT_ABOVE),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_MACRON))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DOT_ABOVE_WITH_MACRON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8, +7.5, REF_CAPITAL_DOT_ABOVE),
          XFORM2(xlat_rely, +8,+10.2, REF(UX_COMBINING_CAPITAL_MACRON))
        ),
    },

    /* dot above with acute */
    {
        .unicode = UX_COMBINING_DOT_ABOVE_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DOT_ABOVE_WITH_ACUTE,
        .map = MAP(
            CANON(
                U_COMBINING_DOT_ABOVE,
                U_COMBINING_ACUTE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF_DOT_ABOVE),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_ACUTE_ACCENT))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DOT_ABOVE_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF_CAPITAL_DOT_ABOVE),
          XFORM2(xlat_relx, 0,-2,
              XFORM2(xlat_rely, +8,+9.7, REF(UX_COMBINING_CAPITAL_ACUTE)))
        ),
    },

    /* dot above with grave */
    {
        .unicode = UX_COMBINING_DOT_ABOVE_WITH_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DOT_ABOVE_WITH_GRAVE,
        .map = MAP(
            CANON(
                U_COMBINING_DOT_ABOVE,
                U_COMBINING_GRAVE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF_DOT_ABOVE),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_GRAVE_ACCENT))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DOT_ABOVE_WITH_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF_CAPITAL_DOT_ABOVE),
          XFORM2(xlat_relx, 0,+2,
              XFORM2(xlat_rely, +8,+9.7, REF(UX_COMBINING_CAPITAL_GRAVE)))
        ),
    },

    /* dot above with tilde */
    {
        .unicode = UX_COMBINING_DOT_ABOVE_WITH_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_DOT_ABOVE_WITH_TILDE,
        .map = MAP(
            CANON(
                U_COMBINING_DOT_ABOVE,
                U_COMBINING_TILDE
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF_DOT_ABOVE),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_TILDE))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_DOT_ABOVE_WITH_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF_CAPITAL_DOT_ABOVE),
          XFORM2(xlat_rely, +8,+9.8, REF(UX_COMBINING_CAPITAL_TILDE))
        ),
    },

    /* macron with acute */
    {
        .unicode = UX_COMBINING_MACRON_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_MACRON_WITH_ACUTE,
        .map = MAP(
            CANON(
                U_COMBINING_MACRON,
                U_COMBINING_ACUTE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          REF(U_COMBINING_MACRON),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_ACUTE_ACCENT))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_MACRON_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF(UX_COMBINING_CAPITAL_MACRON)),
          XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_ACUTE))
        ),
    },

    /* macron with grave */
    {
        .unicode = UX_COMBINING_MACRON_WITH_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_MACRON_WITH_GRAVE,
        .map = MAP(
            CANON(
                U_COMBINING_MACRON,
                U_COMBINING_GRAVE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          REF(U_COMBINING_MACRON),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_GRAVE_ACCENT))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_MACRON_WITH_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF(UX_COMBINING_CAPITAL_MACRON)),
          XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_GRAVE))
        ),
    },

    /* macron with diaeresis */
    {
        .unicode = UX_COMBINING_MACRON_WITH_DIAERESIS,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_MACRON_WITH_DIAERESIS,
        .map = MAP(
            CANON(
                U_COMBINING_MACRON,
                U_COMBINING_DIAERESIS
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          REF(U_COMBINING_MACRON),
          XFORM2(xlat_rely, +5,+6.7, REF_DIAERESIS)
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_MACRON_WITH_DIAERESIS,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8, +7.5, REF(UX_COMBINING_CAPITAL_MACRON)),
          XFORM2(xlat_rely, +8,+10.3, REF_CAPITAL_DIAERESIS)
        ),
    },

    /* macron with tilde */
    {
        .unicode = UX_COMBINING_MACRON_WITH_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_MACRON_WITH_TILDE,
        .map = MAP(
            CANON(
                U_COMBINING_MACRON,
                U_COMBINING_TILDE
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          REF(U_COMBINING_MACRON),
          XFORM2(xlat_rely, +5,+6.7, REF(U_COMBINING_TILDE))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_MACRON_WITH_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8, +7.5, REF(UX_COMBINING_CAPITAL_MACRON)),
          XFORM2(xlat_rely, +8, +9.5, REF(UX_COMBINING_CAPITAL_TILDE))
        ),
    },

    /* breve with acute */
    {
        .unicode = UX_COMBINING_BREVE_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_BREVE_WITH_ACUTE,
        .map = MAP(
            CANON(
                U_COMBINING_BREVE,
                U_COMBINING_ACUTE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF(U_COMBINING_BREVE)),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_ACUTE_ACCENT))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_BREVE_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF(UX_COMBINING_CAPITAL_BREVE)),
          XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_ACUTE))
        ),
    },

    /* breve with grave */
    {
        .unicode = UX_COMBINING_BREVE_WITH_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_BREVE_WITH_GRAVE,
        .map = MAP(
            CANON(
                U_COMBINING_BREVE,
                U_COMBINING_GRAVE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF(U_COMBINING_BREVE)),
          XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_GRAVE_ACCENT))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_BREVE_WITH_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.5, REF(UX_COMBINING_CAPITAL_BREVE)),
          XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_GRAVE))
        ),
    },

    /* breve with hook */
    {
        .unicode = UX_COMBINING_BREVE_WITH_HOOK_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_BREVE_WITH_HOOK_ABOVE,
        .map = MAP(
            CANON(
                U_COMBINING_BREVE,
                U_COMBINING_HOOK_ABOVE
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF(U_COMBINING_BREVE)),
          XFORM2(xlat_relx, 0,-1,
              XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_HOOK_ABOVE)))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_BREVE_WITH_HOOK_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.2, REF(UX_COMBINING_CAPITAL_BREVE)),
          XFORM2(xlat_relx, 0,-1.5,
              XFORM2(xlat_rely, +8,+9.7, REF(UX_COMBINING_CAPITAL_HOOK_ABOVE)))
        ),
    },

    /* breve with tilde */
    {
        .unicode = UX_COMBINING_BREVE_WITH_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_BREVE_WITH_TILDE,
        .map = MAP(
            CANON(
                U_COMBINING_BREVE,
                U_COMBINING_TILDE
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.2, REF(U_COMBINING_BREVE)),
          XFORM2(xlat_rely, +5,+6.8, REF(U_COMBINING_TILDE))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_BREVE_WITH_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.2, REF(UX_COMBINING_CAPITAL_BREVE)),
          XFORM2(xlat_rely, +8,+9.8, REF(UX_COMBINING_CAPITAL_TILDE))
        ),
    },

    /* circumflex with acute */
    {
        .unicode = UX_COMBINING_CIRCUMFLEX_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_ACUTE,
        .map = MAP(
            CANON(
                U_COMBINING_CIRCUMFLEX_ACCENT,
                U_COMBINING_ACUTE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF(U_COMBINING_CIRCUMFLEX_ACCENT)),
          XFORM2(xlat_relx, 0,-4,
              XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_ACUTE_ACCENT)))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.2, REF(UX_COMBINING_CAPITAL_CIRCUMFLEX)),
          XFORM2(xlat_relx, 0,-4,
              XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_ACUTE)))
        ),
    },

    /* circumflex with grave */
    {
        .unicode = UX_COMBINING_CIRCUMFLEX_WITH_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_GRAVE,
        .map = MAP(
            CANON(
                U_COMBINING_CIRCUMFLEX_ACCENT,
                U_COMBINING_GRAVE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF(U_COMBINING_CIRCUMFLEX_ACCENT)),
          XFORM2(xlat_relx, 0,+4,
              XFORM2(xlat_rely, +5,+6.5, REF(U_COMBINING_GRAVE_ACCENT)))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_GRAVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8,+7.2, REF(UX_COMBINING_CAPITAL_CIRCUMFLEX)),
          XFORM2(xlat_relx, 0,+4,
              XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_GRAVE)))
        ),
    },

    /* circumflex with hook */
    {
        .unicode = UX_COMBINING_CIRCUMFLEX_WITH_HOOK_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_HOOK_ABOVE,
        .map = MAP(
            CANON(
                U_COMBINING_CIRCUMFLEX_ACCENT,
                U_COMBINING_HOOK_ABOVE
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF(U_COMBINING_CIRCUMFLEX_ACCENT)),
          XFORM2(xlat_relx, 0,+4,
              XFORM2(xlat_rely, +5,+6, REF(U_COMBINING_HOOK_ABOVE)))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_HOOK_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_relx, 0,-1,
              XFORM2(xlat_rely, +8,+7.2, REF(UX_COMBINING_CAPITAL_CIRCUMFLEX))),
          XFORM2(xlat_relx, 0,+4.2,
              XFORM2(xlat_rely, +8,+9.7, REF(UX_COMBINING_CAPITAL_HOOK_ABOVE)))
        ),
    },

    /* circumflex with tilde */
    {
        .unicode = UX_COMBINING_CIRCUMFLEX_WITH_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_TILDE,
        .map = MAP(
            CANON(
                U_COMBINING_CIRCUMFLEX_ACCENT,
                U_COMBINING_TILDE
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.2, REF(U_COMBINING_CIRCUMFLEX_ACCENT)),
          XFORM2(xlat_rely, +5,+6.8, REF(U_COMBINING_TILDE))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_TILDE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_relx, 0,-1,
              XFORM2(xlat_rely, +8,+7.1, REF(UX_COMBINING_CAPITAL_CIRCUMFLEX))),
          XFORM2(xlat_relx, 0,+1,
              XFORM2(xlat_rely, +8,+9.8, REF(UX_COMBINING_CAPITAL_TILDE)))
        ),
    },

    /* circumflex with macron */
    {
        .unicode = UX_COMBINING_CIRCUMFLEX_WITH_MACRON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_MACRON,
        .map = MAP(
            CANON(
                U_COMBINING_CIRCUMFLEX_ACCENT,
                U_COMBINING_MACRON
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.2, REF(U_COMBINING_CIRCUMFLEX_ACCENT)),
          XFORM2(xlat_rely, +5,+7.3, REF(U_COMBINING_MACRON))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_MACRON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
           XFORM2(xlat_rely, +8, +7.1, REF(UX_COMBINING_CAPITAL_CIRCUMFLEX)),
           XFORM2(xlat_rely, +8,+10.2, REF(UX_COMBINING_CAPITAL_MACRON))
        ),
    },

    /* circumflex with caron */
    {
        .unicode = UX_COMBINING_CIRCUMFLEX_WITH_CARON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_CARON,
        .map = MAP(
            CANON(
                U_COMBINING_CIRCUMFLEX_ACCENT,
                U_COMBINING_CARON
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.2, REF(U_COMBINING_CIRCUMFLEX_ACCENT)),
          XFORM2(xlat_rely, +5,+6.8, REF(U_COMBINING_CARON))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_CIRCUMFLEX_WITH_CARON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_relx, 0,-1,
              XFORM2(xlat_rely, +8,+7.1, REF(UX_COMBINING_CAPITAL_CIRCUMFLEX))),
          XFORM2(xlat_relx, 0,+1,
              XFORM2(xlat_rely, +8,+9.8, REF(UX_COMBINING_CAPITAL_CARON)))
        ),
    },

    /* acute with dot above */
    {
        .unicode = UX_COMBINING_ACUTE_WITH_DOT_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_ACUTE_WITH_DOT_ABOVE,
        .map = MAP(
            CANON(
                U_COMBINING_ACUTE_ACCENT,
                U_COMBINING_DOT_ABOVE
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          REF(U_COMBINING_ACUTE_ACCENT),
          XFORM2(xlat_rely, +5,+7.5, REF_DOT_ABOVE)
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_ACUTE_WITH_DOT_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8, +7.5, REF(UX_COMBINING_CAPITAL_ACUTE)),
          XFORM2(xlat_rely, +8,+10.3, REF_CAPITAL_DOT_ABOVE)
        ),
    },

    /* caron with dot above */
    {
        .unicode = UX_COMBINING_CARON_WITH_DOT_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_CARON_WITH_DOT_ABOVE,
        .map = MAP(
            CANON(
                U_COMBINING_CARON,
                U_COMBINING_DOT_ABOVE
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          REF(U_COMBINING_CARON),
          XFORM2(xlat_rely, +5,+7.5, REF_DOT_ABOVE)
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_CARON_WITH_DOT_ABOVE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8, +7.5, REF(UX_COMBINING_CAPITAL_CARON)),
          XFORM2(xlat_rely, +8,+10.3, REF_CAPITAL_DOT_ABOVE)
        ),
    },

    /* tilde with diaeresis */
    {
        .unicode = UX_COMBINING_TILDE_WITH_DIAERESIS,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_TILDE_WITH_DIAERESIS,
        .map = MAP(
            CANON(
                U_COMBINING_TILDE,
                U_COMBINING_DIAERESIS
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF(U_COMBINING_TILDE)),
          XFORM2(xlat_rely, +5,+7.5, REF_DIAERESIS)
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_TILDE_WITH_DIAERESIS,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8, +7.0, REF(UX_COMBINING_CAPITAL_TILDE)),
          XFORM2(xlat_rely, +8,+10.3, REF_CAPITAL_DIAERESIS)
        ),
    },

    /* tilde with macron */
    {
        .unicode = UX_COMBINING_TILDE_WITH_MACRON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_TILDE_WITH_MACRON,
        .map = MAP(
            CANON(
                U_COMBINING_TILDE,
                U_COMBINING_MACRON
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF(U_COMBINING_TILDE)),
          XFORM2(xlat_rely, +5,+7.5, REF(U_COMBINING_MACRON))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_TILDE_WITH_MACRON,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +8, +7.0, REF(UX_COMBINING_CAPITAL_TILDE)),
          XFORM2(xlat_rely, +8,+10.3, REF(UX_COMBINING_CAPITAL_MACRON))
        ),
    },

    /* tilde with acute */
    {
        .unicode = UX_COMBINING_TILDE_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_TILDE_WITH_ACUTE,
        .map = MAP(
            CANON(
                U_COMBINING_TILDE,
                U_COMBINING_ACUTE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF(U_COMBINING_TILDE)),
          XFORM2(xlat_rely, +5,+7.0, REF(U_COMBINING_ACUTE_ACCENT))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_TILDE_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_relx, 0,-0.5,
              XFORM2(xlat_rely, +8,+6.8, REF(UX_COMBINING_CAPITAL_TILDE))),
          XFORM2(xlat_relx, 0,+1.5,
              XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_ACUTE)))
        ),
    },

    /* ring above with acute */
    {
        .unicode = UX_COMBINING_RING_ABOVE_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .high_above = UX_COMBINING_CAPITAL_RING_ABOVE_WITH_ACUTE,
        .map = MAP(
            CANON(
                U_COMBINING_RING_ABOVE,
                U_COMBINING_ACUTE_ACCENT
            )
        ),
        .line_step = LS_THINNER,
        .draw = MERGE(
          XFORM2(xlat_rely, +5,+4.5, REF(U_COMBINING_RING_ABOVE)),
          XFORM2(xlat_rely, +5,+7.0, REF(U_COMBINING_ACUTE_ACCENT))
        ),
    },
    {
        .unicode = UX_COMBINING_CAPITAL_RING_ABOVE_WITH_ACUTE,
        .lpad_abs = PAD_DIA,
        .rpad_abs = PAD_DIA,
        .line_step = LS_THINNER,
        .draw = MERGE(
          REF(UX_COMBINING_CAPITAL_RING_ABOVE),
          XFORM2(xlat_relx, 0,+1,
              XFORM2(xlat_rely, +8,+9.5, REF(UX_COMBINING_CAPITAL_ACUTE)))
        ),
    },

    /* other leters */
    {
        .unicode = U_LATIN_SMALL_LETTER_N_PRECEDED_BY_APOSTROPHE,
        .draw = SEQ(
          SUBGLYPH(0, U_APOSTROPHE),
          SUBGLYPH(0, U_LATIN_SMALL_LETTER_N)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_L_WITH_STROKE,
        //.min_coord = COORD(+1,-3,0,0),
        //.max_coord = COORD(+1,+3,0,0),
        .draw = MERGE(
          REF(U_LATIN_SMALL_LETTER_L),
          XFORM(ls_thin, STROKE(
            Q(I, (+3, -4, 0, 0), ( 0, 0, 0, 0)),
            Q(O, (+3, +4, 0, 0), ( 0,+2, 0, 0)),
          )),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_L_WITH_STROKE,
        //.min_coord = COORD(+1,-9,0,0),
        .draw = MERGE(
          REF(U_LATIN_CAPITAL_LETTER_L),
          XFORM(ls_thin, STROKE(
            Q(I, (+3,-10, 0, 0), ( 0, 0, 0, 0)),
            Q(O, (+3,  0, 0, 0), ( 0,+2, 0, 0)),
          )),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_H_WITH_STROKE,
        .draw = MERGE(
          REF(U_LATIN_SMALL_LETTER_H),
          XFORM(ls_thinner, STROKE(
            Q(I, (+1, -9, 0, 0), (+2,+4,+5,30)),
            Q(O, ( 0, +3, 0, 0), (+2,+4,+5,30)),
          )),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_H_WITH_STROKE,
        .draw = MERGE(
          WIDTH(U_LATIN_CAPITAL_LETTER_H),
          REF(U_LATIN_CAPITAL_LETTER_H),
          STROKE(
            Q(I, (+1,-10, 0, 0), (+3,+4, 0, 0)),
            Q(O, (+1,+10, 0, 0), (+3,+4, 0, 0)),
          ),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_T_WITH_STROKE,
        .draw = MERGE(
          WIDTH(U_LATIN_SMALL_LETTER_T),
          REF(U_LATIN_SMALL_LETTER_T),
          STROKE(
            Q(I, (0, -3, 0, 0), (-2,+1,0,0)),
            Q(O, (0, +4, 0, 0), (-2,+1,0,0)),
          ),
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_T_WITH_STROKE,
        .draw = MERGE(
          WIDTH(U_LATIN_CAPITAL_LETTER_T),
          REF(U_LATIN_CAPITAL_LETTER_T),
          XFORM(ls_thin, STROKE(
            Q(I, (0,-6, 0, 0), (0,+2, 0, 0)),
            Q(O, (0,+6, 0, 0), (0,+2, 0, 0)),
          )),
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_L_WITH_MIDDLE_DOT,
        .draw = SEQ(
          SUBGLYPH(0, U_LATIN_SMALL_LETTER_L),
          SUBGLYPH(0, U_MIDDLE_DOT)
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_L_WITH_MIDDLE_DOT,
        .draw = MERGE(
          REF(U_LATIN_CAPITAL_LETTER_L),
          XFORM2(xlat_relx, 0,+3, REF(U_MIDDLE_DOT))
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LIGATURE_IJ,
        .map = MAP(
            LANG_LIGA(lang_NLD,
                U_LATIN_CAPITAL_LETTER_I,
                U_LATIN_CAPITAL_LETTER_J
            )
        ),
        .draw = SEQ(
          SUBGLYPH(0,  U_LATIN_CAPITAL_LETTER_I),
          SUBGLYPH(-6, U_LATIN_CAPITAL_LETTER_J)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LIGATURE_IJ,
        .map = MAP(
            LANG_LIGA(lang_NLD,
                U_LATIN_SMALL_LETTER_I,
                U_LATIN_SMALL_LETTER_J
            )
        ),
        .draw = SEQ(
          SUBGLYPH(0,  U_LATIN_SMALL_LETTER_I),
          SUBGLYPH(-6, U_LATIN_SMALL_LETTER_J)
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_DZ_WITH_CARON,
        .draw = SEQ(
          SUBGLYPH(0, U_LATIN_CAPITAL_LETTER_D),
          SUBGLYPH(0, U_LATIN_CAPITAL_LETTER_Z_WITH_CARON)
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_D_WITH_SMALL_LETTER_Z_WITH_CARON,
        .draw = SEQ(
          SUBGLYPH(0, U_LATIN_CAPITAL_LETTER_D),
          SUBGLYPH(0, U_LATIN_SMALL_LETTER_Z_WITH_CARON)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_DZ_WITH_CARON,
        .draw = SEQ(
          SUBGLYPH(0, U_LATIN_SMALL_LETTER_D),
          SUBGLYPH(0, U_LATIN_SMALL_LETTER_Z_WITH_CARON)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_DZ,
        .draw = SEQ(
          SUBGLYPH(0, U_LATIN_SMALL_LETTER_D),
          SUBGLYPH(0, U_LATIN_SMALL_LETTER_Z)
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_DZ,
        .draw = SEQ(
          SUBGLYPH(0, U_LATIN_CAPITAL_LETTER_D),
          SUBGLYPH(0, U_LATIN_CAPITAL_LETTER_Z)
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_D_WITH_SMALL_LETTER_Z,
        .draw = SEQ(
          SUBGLYPH(0, U_LATIN_CAPITAL_LETTER_D),
          SUBGLYPH(0, U_LATIN_SMALL_LETTER_Z)
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_LJ,
        .draw = SEQ(
          SUBGLYPH(0,  U_LATIN_CAPITAL_LETTER_L),
          SUBGLYPH(-8, U_LATIN_CAPITAL_LETTER_J)
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_L_WITH_SMALL_LETTER_J,
        .draw = SEQ(
          SUBGLYPH(0,  U_LATIN_CAPITAL_LETTER_L),
          SUBGLYPH(-8, U_LATIN_SMALL_LETTER_J)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_LJ,
        .draw = SEQ(
          SUBGLYPH(0,  U_LATIN_SMALL_LETTER_L),
          SUBGLYPH(-6, U_LATIN_SMALL_LETTER_J)
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_NJ,
        .draw = SEQ(
          SUBGLYPH(0,  U_LATIN_CAPITAL_LETTER_N),
          SUBGLYPH(-8, U_LATIN_CAPITAL_LETTER_J)
        ),
    },
    {
        .unicode = U_LATIN_CAPITAL_LETTER_N_WITH_SMALL_LETTER_J,
        .draw = SEQ(
          SUBGLYPH(0,  U_LATIN_CAPITAL_LETTER_N),
          SUBGLYPH(-8, U_LATIN_SMALL_LETTER_J)
        ),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_NJ,
        .draw = SEQ(
          SUBGLYPH(0,  U_LATIN_SMALL_LETTER_N),
          SUBGLYPH(-8, U_LATIN_SMALL_LETTER_J)
        ),
    },

    /* small caps (note: X is not encoded, but is like lowcase x) */
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_A,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_A)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_B,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_B)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_C,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_C)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_D,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_D)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_E,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_E)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_F,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_F)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_G,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_G)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_H,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_H)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_I,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_I)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_J,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_J)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_K,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_K)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_L,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_L)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_M,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_M)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_N,
        .line_step = LS_LOWER,
        .draw = STROKE(
            Q(B, ( 0,-5, 0, 0), ( 0,-3, 0, 0)),
            Q(P, ( 0,-5, 0, 0), (-3,+3, 0, 0)),
            Q(P, ( 0,-3, 0, 0), (-3,+3, 0, 0)),
            Q(P, ( 0,+3, 0, 0), (-3,-3, 0, 0)),
            Q(P, ( 0,+5, 0, 0), (-3,-3, 0, 0)),
            Q(E, ( 0,+5, 0, 0), ( 0,+3, 0, 0)),
        ),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_O,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_O)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_P,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_P)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_Q,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_Q)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_R,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_R)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_S,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_S)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_T,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_T)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_U,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_U)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_V,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_V)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_W,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_W)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_Y,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_Y)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_Z,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_Z)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_AE,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_AE)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_OE,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LIGATURE_OE)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_ETH,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_ETH)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_OPEN_O,
        .draw = SAME(U_LATIN_SMALL_LETTER_OPEN_O),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_OPEN_E,
        .line_step = LS_LOWER,
        .draw = XFORM(smallcap, REF(U_LATIN_CAPITAL_LETTER_OPEN_E)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_REVERSED_OPEN_E,
        .line_step = LS_LOWER,
        .draw = XFORM(swap_x, REF(U_LATIN_SMALL_LETTER_OPEN_E)),
    },
    /* MISSING smallcaps: */
    /* LATIN LETTER SMALL CAPITAL EZH             1D23 */
    /* LATIN LETTER SMALL CAPITAL RUM             A776 */
    /* LATIN LETTER SMALL CAPITAL BARRED B        1D03 */
    /* LATIN LETTER SMALL CAPITAL OU              1D15 */
    /* LATIN CAPITAL LETTER SMALL CAPITAL I       A7AE */

    /* turned, inverted, reversed letters */
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_INVERTED_R,
        .draw = XFORM(invert_lc, REF(U_LATIN_LETTER_SMALL_CAPITAL_R)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_REVERSED_R,
        .draw = XFORM(reverse_lc, REF(U_LATIN_LETTER_SMALL_CAPITAL_R)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_TURNED_R,
        .draw = XFORM(turn_lc, REF(U_LATIN_LETTER_SMALL_CAPITAL_R)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_TURNED_E,
        .draw = XFORM(turn_lc, REF(U_LATIN_LETTER_SMALL_CAPITAL_E)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_TURNED_M,
        .draw = XFORM(turn_lc, REF(U_LATIN_LETTER_SMALL_CAPITAL_M)),
    },
    {
        .unicode = U_LATIN_LETTER_SMALL_CAPITAL_REVERSED_N,
        .draw = XFORM(reverse_lc, REF(U_LATIN_LETTER_SMALL_CAPITAL_N)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_A,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_A)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_M,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_M)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_R,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_R)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_V,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_V)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_W,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_W)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_AE,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_AE)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_I,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_I)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_OE,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LIGATURE_OE)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_H,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_H)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_T,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_T)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_Y,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_Y)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_K,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_K)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_G,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_G)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_L,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_L)),
    },
    {
        .unicode = U_LATIN_SMALL_LETTER_TURNED_OPEN_E,
        .draw = XFORM(turn_lc, REF(U_LATIN_SMALL_LETTER_OPEN_E)),
    },

    /* 0252;LATIN SMALL LETTER TURNED ALPHA */
    /* 018D;LATIN SMALL LETTER TURNED DELTA */
    /* 0270;LATIN SMALL LETTER TURNED M WITH LONG LEG */
    /* 027A;LATIN SMALL LETTER TURNED R WITH LONG LEG */
    /* 027B;LATIN SMALL LETTER TURNED R WITH HOOK */
    /* 02AE;LATIN SMALL LETTER TURNED H WITH FISHHOOK */
    /* 02AF;LATIN SMALL LETTER TURNED H WITH FISHHOOK AND TAIL */
    /* 1D1F;LATIN SMALL LETTER SIDEWAYS TURNED M */
    /* 2C79;LATIN SMALL LETTER TURNED R WITH TAIL */
    /* A77F;LATIN SMALL LETTER TURNED INSULAR G */
    /* AB41;LATIN SMALL LETTER TURNED OE WITH STROKE */
    /* AB42;LATIN SMALL LETTER TURNED OE WITH HORIZONTAL STROKE */
    /* AB44;LATIN SMALL LETTER TURNED O OPEN-O WITH STROKE */
    /* AB51;LATIN SMALL LETTER TURNED UI */
    /* AB43;LATIN SMALL LETTER TURNED O OPEN-O */

    {
        .unicode = U_LATIN_SMALL_LETTER_KRA,
        .draw = SAME(U_LATIN_LETTER_SMALL_CAPITAL_K),
    },

    /* misc symbols */
    {
        .unicode = U_LEFTWARDS_ARROW,
        .min_coord = COORD(0,-8,0,0),
        .draw = STROKE(
            Q(B, ( 0,+8, 0, 0), ( 0,-3,+4,30)),
            Q(E, ( 0,-8, 0, 0), ( 0,-3,+4,30)),
            Q(I, ( 0,-3, 0, 0), ( 0,-3,+4,10)),
            Q(P, ( 0,-8, 0, 0), ( 0,-3,+4,30)),
            Q(O, ( 0,-3, 0, 0), ( 0,-3,+4,50)),
        ),
    },
    {
        .unicode = U_RIGHTWARDS_ARROW,
        .draw = MERGE(
            WIDTH(U_LEFTWARDS_ARROW),
            XFORM(swap_x, REF(U_LEFTWARDS_ARROW))
        ),
    },
    {
        .unicode = U_UPWARDS_ARROW,
        .draw = MERGE(
          WIDTH(U_LEFTWARDS_ARROW),
          STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), (-1,+6, 0, 0)),
            Q(I, ( 0,-7, 0, 0), ( 0,+3, 0, 0)),
            Q(P, ( 0, 0, 0, 0), (-1,+6, 0, 0)),
            Q(O, ( 0,+7, 0, 0), ( 0,+3, 0, 0)),
          )
        ),
    },
    {
        .unicode = U_DOWNWARDS_ARROW,
        .draw = MERGE(
          WIDTH(U_LEFTWARDS_ARROW),
          XFORM(invert_uc,REF(U_UPWARDS_ARROW))
        ),
    },

    {
        /* This symbol is added solely to fulfill MES-1 -- this program is not
         * meant to draw symbols like this, but is for stroke based fonts.
         * Outlines cannot be defined, so this really looks a bit weird. */
        .unicode = U_EIGHTH_NOTE,
        .draw = STROKE(
            Q(I, ( 0,+7, 0, 0), ( 0, +3, 0, 0)),
            Q(P, ( 0,+7, 0, 0), ( 0, +5, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0, +6, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0, -1, 0, 0)),
            Q(P, ( 0,-7, 0, 0), ( 0, -3, 0, 0)),
            Q(S, ( 0,-7, 0,25), ( 0, -1, 0, 0)),
            Q(P, ( 0, 0, 0, 0), ( 0, -1, 0, 0)),
            Q(S, ( 0,-7, 0,45), (-2, -3, 0, 0)),
            Q(O, ( 0,-7, 0, 0), (-2, -3, 0, 0)),
        ),
    },

    /* tone contours */
    {
        .unicode = U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR,
        .draw = STROKE(
            Q(B, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(B, ( 0,+4, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0,+4, 0, 0), ( 0,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_HIGH_TONE_BAR,
        .draw = STROKE(
            Q(B, ( 0,-4, 0, 0), Y_CONTOUR_HIGH),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_HIGH),
            Q(B, ( 0,+4, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0,+4, 0, 0), ( 0,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_MID_TONE_BAR,
        .draw = STROKE(
            Q(B, ( 0,-4, 0, 0), Y_CONTOUR_MID),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_MID),
            Q(B, ( 0,+4, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0,+4, 0, 0), ( 0,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_LOW_TONE_BAR,
        .draw = STROKE(
            Q(B, ( 0,-4, 0, 0), Y_CONTOUR_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_LOW),
            Q(B, ( 0,+4, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0,+4, 0, 0), ( 0,+6, 0, 0)),
        ),
    },
    {
        .unicode = U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR,
        .draw = STROKE(
            Q(B, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(B, ( 0,+4, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0,+4, 0, 0), ( 0,+6, 0, 0)),
        ),
    },

    /* contours: extra-high */
    {
        .unicode = UX_TONE_CONTOUR_EXTRA_HIGH_EXTRA_HIGH,
        .rpad_abs = -PAD_DEFAULT,
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR,
                U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(O, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_EXTRA_HIGH_HIGH,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR,
                U_MODIFIER_LETTER_HIGH_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_HIGH),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_HIGH),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_EXTRA_HIGH_MID,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR,
                U_MODIFIER_LETTER_MID_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_MID),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_MID),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_EXTRA_HIGH_LOW,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR,
                U_MODIFIER_LETTER_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_LOW),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_EXTRA_HIGH_EXTRA_LOW,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR,
                U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_LOW),
        ),
    },

    /* contours: high */
    {
        .unicode = UX_TONE_CONTOUR_HIGH_EXTRA_HIGH,
        .rpad_abs = -PAD_DEFAULT,
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_HIGH_TONE_BAR,
                U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_HIGH),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(O, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_HIGH_HIGH,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_HIGH_TONE_BAR,
                U_MODIFIER_LETTER_HIGH_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_HIGH),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_HIGH),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_HIGH_MID,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_HIGH_TONE_BAR,
                U_MODIFIER_LETTER_MID_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_HIGH),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_MID),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_MID),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_HIGH_LOW,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_HIGH_TONE_BAR,
                U_MODIFIER_LETTER_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_HIGH),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_LOW),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_HIGH_EXTRA_LOW,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_HIGH_TONE_BAR,
                U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_HIGH),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_LOW),
        ),
    },

    /* contours: mid */
    {
        .unicode = UX_TONE_CONTOUR_MID_EXTRA_HIGH,
        .rpad_abs = -PAD_DEFAULT,
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_MID_TONE_BAR,
                U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_MID),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(O, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_MID_HIGH,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_MID_TONE_BAR,
                U_MODIFIER_LETTER_HIGH_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_MID),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_HIGH),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_HIGH),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_MID_MID,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_MID_TONE_BAR,
                U_MODIFIER_LETTER_MID_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_MID),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_MID),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_MID_LOW,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_MID_TONE_BAR,
                U_MODIFIER_LETTER_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_MID),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_LOW),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_MID_EXTRA_LOW,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_MID_TONE_BAR,
                U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_MID),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_LOW),
        ),
    },

    /* contours: low */
    {
        .unicode = UX_TONE_CONTOUR_LOW_EXTRA_HIGH,
        .rpad_abs = -PAD_DEFAULT,
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_LOW_TONE_BAR,
                U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_LOW),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(O, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_LOW_HIGH,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_LOW_TONE_BAR,
                U_MODIFIER_LETTER_HIGH_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_LOW),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_HIGH),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_HIGH),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_LOW_MID,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_LOW_TONE_BAR,
                U_MODIFIER_LETTER_MID_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_LOW),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_MID),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_MID),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_LOW_LOW,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_LOW_TONE_BAR,
                U_MODIFIER_LETTER_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_LOW),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_LOW_EXTRA_LOW,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_LOW_TONE_BAR,
                U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_LOW),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_LOW),
        ),
    },

    /* contours: extra-low */
    {
        .unicode = UX_TONE_CONTOUR_EXTRA_LOW_EXTRA_HIGH,
        .rpad_abs = -PAD_DEFAULT,
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR,
                U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_EXTRA_HIGH),
            Q(O, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_HIGH),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_EXTRA_LOW_HIGH,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR,
                U_MODIFIER_LETTER_HIGH_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_HIGH),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_HIGH),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_EXTRA_LOW_MID,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR,
                U_MODIFIER_LETTER_MID_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_MID),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_MID),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_EXTRA_LOW_LOW,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR,
                U_MODIFIER_LETTER_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(P, ( 0,+3, 0, 0), Y_CONTOUR_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_LOW),
        ),
    },
    {
        .unicode = UX_TONE_CONTOUR_EXTRA_LOW_EXTRA_LOW,
        .rpad_abs = -PAD_DEFAULT,
        .min_coord = COORD(-2,-4,0,0),
        .map = MAP(
            MAND_KEEP(
                U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR,
                U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(I, ( 0,-4, 0, 0), Y_CONTOUR_EXTRA_LOW),
            Q(E, ( 0,+4, 0, 0), Y_CONTOUR_EXTRA_LOW),
        ),
    },


    /* contours: terminal */
    {
        .unicode = UX_TONE_CONTOUR_TERMINAL,
        .map = MAP(
            CONTEXT(UX_TONE_CONTOUR_EXTRA_HIGH_HIGH,       U_MODIFIER_LETTER_HIGH_TONE_BAR),          CONTEXT(UX_TONE_CONTOUR_EXTRA_HIGH_MID,        U_MODIFIER_LETTER_MID_TONE_BAR),           CONTEXT(UX_TONE_CONTOUR_EXTRA_HIGH_LOW,        U_MODIFIER_LETTER_LOW_TONE_BAR),           CONTEXT(UX_TONE_CONTOUR_EXTRA_HIGH_EXTRA_LOW,  U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_EXTRA_HIGH_MID,        U_MODIFIER_LETTER_MID_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_EXTRA_HIGH_LOW,        U_MODIFIER_LETTER_LOW_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_EXTRA_HIGH_EXTRA_LOW,  U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR),

            CONTEXT(UX_TONE_CONTOUR_HIGH_EXTRA_HIGH,       U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_HIGH_MID,              U_MODIFIER_LETTER_MID_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_HIGH_LOW,              U_MODIFIER_LETTER_LOW_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_HIGH_EXTRA_LOW,        U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR),

            CONTEXT(UX_TONE_CONTOUR_MID_EXTRA_HIGH,        U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_MID_HIGH,              U_MODIFIER_LETTER_HIGH_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_MID_LOW,               U_MODIFIER_LETTER_LOW_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_MID_EXTRA_LOW,         U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR),

            CONTEXT(UX_TONE_CONTOUR_LOW_EXTRA_HIGH,        U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_LOW_HIGH,              U_MODIFIER_LETTER_HIGH_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_LOW_MID,               U_MODIFIER_LETTER_MID_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_LOW_EXTRA_LOW,         U_MODIFIER_LETTER_EXTRA_LOW_TONE_BAR),

            CONTEXT(UX_TONE_CONTOUR_EXTRA_LOW_EXTRA_HIGH,  U_MODIFIER_LETTER_EXTRA_HIGH_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_EXTRA_LOW_HIGH,        U_MODIFIER_LETTER_HIGH_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_EXTRA_LOW_MID,         U_MODIFIER_LETTER_MID_TONE_BAR),
            CONTEXT(UX_TONE_CONTOUR_EXTRA_LOW_LOW,         U_MODIFIER_LETTER_LOW_TONE_BAR),
        ),
        .draw = STROKE(
            Q(B, ( 0, 0, 0, 0), ( 0,-3, 0, 0)),
            Q(E, ( 0, 0, 0, 0), ( 0,+6, 0, 0)),
        ),
    },

};

/*
 * Naming Pattern:
 *    Name:
 *       Kind:     [Serif]        Sans
 *       Width:    [Proportional] Mono
 *    Style:
 *       Weight:   [Book]         Ultra-Light Thin Light [] Medium Bold Heavy Black (4)
 *       Slope:    [Roman]        [] Oblique Italic                                 (2)
 *       Stretch:  [Regular]      Wide [] Semi-Condensed Condensed                  (1)
 *       Size:     [Normal]       Poster Display Subhead [] Small Caption           (3)
 */
static font_def_t f1_font_book = {
    .family_name = FAMILY_NAME,
    .weight   = CP_FONT_WEIGHT_BOOK,
    .slope    = CP_FONT_SLOPE_ROMAN,
    .stretch  = CP_FONT_STRETCH_REGULAR,
    .min_size = 10,
    .max_size = 12,

    .box = {
        .lo = { .x = -14, .y = -9 },
        .hi = { .x = +14, .y = +12 },
    },
    .cap_y    = +6,
    .xhi_y    = +3,
    .base_y   = -3,
    .dec_y    = -6,
    .top_y    = +10,
    .bottom_y = -9,
    .line_width = { 3.5, 3.8, 3.1, 2.5 },
    .slant = 0,
    .radius = { 4, 8, 12, 24 }, // SMALL, LARGE, HUGE, GIANT
    .angle = { 5, 8 },          // TIGHT, ANGLED
    .min_dist = 1,

    /* integer->float coordinate translations */
    .coord_x = VEC(((double const []){
    /*  -14-13-12-11 -10 -9  -8 -7 -6 -5 -4 -3 -2 -1  0 +1 +2 +3 +4 +5 +6 +7 +8 +9+10+11+12+13+14 */
        -32,0,-16,-10,-5,-0., 6,10,12,14,18,22,26,29,32,35,38,42,46,50,52,54,58,64,69,74,80, 0,96
    })),
    .coord_y = VEC(((double const []){
    /* -9  -8 -7 -6 -5 -4 -3 -2 -1  0 +1 +2 +3 +4 +5 +6 +7 +8 +9+10+11+12 */
       -0., 0, 0, 8,12,16,20,25,29,33,37,41,46,49,52,58,61,64,66,70,74,78
    })),
    .highlight_y = VEC(((int const []) {
       /* baseline */
       -3,
       /* cap height */
       +6,
       /* median */
       +3,
    })),
    .dot_size = 5.0,

    .sub_x = { 0, 0.5, 0.75, 1, 1.25, 1.5, 2, 2.5, 3, 3.5 },
    .sub_y = { 0, 0.5, 0.75, 1, 1.25, 1.5, 2, 2.5, 3, 3.5 },
    .scale_x = 0.5 * 0.95,

    .round_tension = 0.4,
    .round_step_cnt = 8,
    .corner_type = {
#if 1
        [FONT_BOTTOM_LEFT]  = FONT_VERTEX_SMALL,
        [FONT_BOTTOM_RIGHT] = FONT_VERTEX_HUGE,
        [FONT_TOP_LEFT]     = FONT_VERTEX_HUGE,
        [FONT_TOP_RIGHT]    = FONT_VERTEX_SMALL,
#elif 0
        [FONT_BOTTOM_LEFT]  = FONT_VERTEX_SMALL,
        [FONT_BOTTOM_RIGHT] = FONT_VERTEX_LARGE,
        [FONT_TOP_LEFT]     = FONT_VERTEX_LARGE,
        [FONT_TOP_RIGHT]    = FONT_VERTEX_SMALL,
#else
        [FONT_BOTTOM_LEFT]  = FONT_VERTEX_HUGE,
        [FONT_BOTTOM_RIGHT] = FONT_VERTEX_HUGE,
        [FONT_TOP_LEFT]     = FONT_VERTEX_HUGE,
        [FONT_TOP_RIGHT]    = FONT_VERTEX_HUGE,
#endif
    },

    .lpad_default = PAD_DEFAULT,
    .rpad_default = PAD_DEFAULT,

    .glyph = VEC(f1_a_glyph),
};

/* ********************************************************************** */

typedef CP_ARR_T(unicode_t) font_a_unicode_t;

typedef struct {
    char const *name;
    char const *abbrev;
    size_t have_cnt;
    double have_ratio;
    font_a_unicode_t cp;
} unicode_set_t;

#include "unisets.inc"

/* ********************************************************************** */

#include "unicomp.inc"

/* ********************************************************************** */

typedef struct {
    char const *ott;
    char const *iso;
} lang_name_t;

static lang_name_t lang_name_data[] = {
#include "langname.inc"
};

typedef CP_ARR_T(lang_name_t) cp_a_lang_name_t;

static cp_a_lang_name_t lang_name =
    CP_A_INIT_WITH(lang_name_data, cp_countof(lang_name_data));

/* ********************************************************************** */
/* from this point, UNICODE() just returns the number */

#undef UNICODE
#define UNICODE(X,Y) (X)

/* ********************************************************************** */

static void ls_lower(
    font_t const *font, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    gc->line_width_defined = true;
    gc->line_width = font->def->line_width[0];
}

static void ls_thin(
    font_t const *font, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    gc->line_width_defined = true;
    gc->line_width = font->def->line_width[LS_THIN];
}

static void ls_thinner(
    font_t const *font, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    gc->line_width_defined = true;
    gc->line_width = font->def->line_width[LS_THINNER];
}

static void swap_x(
    font_t const *font CP_UNUSED, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    gc->swap_x = !gc->swap_x;
}

static void slant1(
    font_t const *font CP_UNUSED, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    cp_mat2w_t m = CP_MAT2W(
        1, 0.15, 0,
        0, 1,    0);
    cp_mat2w_mul(&gc->xform, &m, &gc->xform);
}

static double coord_y_abs(font_def_t const *def, int i)
{
    return cp_v_nth(&def->coord_y, (size_t)(i - def->box.lo.y));
}

static double coord_y_rel(font_def_t const *def, int i, int j)
{
    return coord_y_abs(def, i) - coord_y_abs(def, j);
}

static double coord_x_abs(font_def_t const *def, int i)
{
    return cp_v_nth(&def->coord_x, (size_t)(i - def->box.lo.x)) * def->scale_x;
}

static double coord_x_rel(font_def_t const *def, int i, int j)
{
    return coord_x_abs(def, i) - coord_x_abs(def, j);
}

static double coord_x_fabs(font_def_t const *def, double i)
{
    return cp_lerp(
        coord_x_abs(def, (int)lrint(floor(i))),
        coord_x_abs(def, (int)lrint(ceil(i))),
        i - floor(i));
}

static double coord_y_fabs(font_def_t const *def, double i)
{
    return cp_lerp(
        coord_y_abs(def, (int)lrint(floor(i))),
        coord_y_abs(def, (int)lrint(ceil(i))),
        i - floor(i));
}

static double coord_x_frel(font_def_t const *def, double i, double j)
{
    return coord_x_fabs(def, i) - coord_x_fabs(def, j);
}

static double coord_y_frel(font_def_t const *def, double i, double j)
{
    return coord_y_fabs(def, i) - coord_y_fabs(def, j);
}

static void superscript_lc(
    font_t const *font, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    if (!gc->line_width_defined) {
        gc->line_width_defined = true;
        gc->line_width = font->def->line_width[LS_THIN];
    }

    cp_mat2w_t m;
    cp_mat2w_xlat(&m, 0, -coord_y_rel(font->def, -3, +6));
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);

    cp_mat2w_scale(&m, 0.85, 0.7);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);

    cp_mat2w_xlat(&m, 0, +coord_y_rel(font->def, -3, +3));
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);
}

static void superscript(
    font_t const *font, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    if (!gc->line_width_defined) {
        gc->line_width_defined = true;
        gc->line_width = font->def->line_width[LS_THIN];
    }

    cp_mat2w_t m;
    cp_mat2w_xlat(&m, 0, -coord_y_rel(font->def, -3, +6));
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);

    cp_mat2w_scale(&m, 0.8, 0.6);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);

    cp_mat2w_xlat(&m, 0, +coord_y_rel(font->def, -3, +6));
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);
}

static void subscript(
    font_t const *font CP_UNUSED, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    if (!gc->line_width_defined) {
        gc->line_width_defined = true;
        gc->line_width = font->def->line_width[LS_THIN];
    }

    cp_mat2w_t m;
    cp_mat2w_scale(&m, 0.8, 0.6);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);
}

static void enclosed(
    font_t const *font CP_UNUSED, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    if (!gc->line_width_defined) {
        gc->line_width_defined = true;
        gc->line_width = font->def->line_width[LS_THIN];
    }

    cp_mat2w_t m;

    cp_mat2w_xlat(&m, 0, -coord_y_rel(font->def, -3, +6)/2);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);

    cp_mat2w_scale(&m, 0.8, 0.6);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);

    cp_mat2w_xlat(&m, 0, +coord_y_rel(font->def, -3, +6)/2);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);
}

CP_UNUSED
static void xlat(
    font_t const *font CP_UNUSED, font_gc_t *gc, font_draw_xform_t const *p)
{
    cp_mat2w_t m;
    cp_mat2w_xlat(&m, p->a, p->b);
    cp_mat2w_mul(&gc->xform, &m, &gc->xform);
}

static void xlat_relx(
    font_t const *font CP_UNUSED, font_gc_t *gc, font_draw_xform_t const *p)
{
    cp_mat2w_t m;
    cp_mat2w_xlat(&m, coord_x_frel(font->def, p->b, p->a), 0);
    cp_mat2w_mul(&gc->xform, &m, &gc->xform);
}

static void xlat_rely(
    font_t const *font CP_UNUSED, font_gc_t *gc, font_draw_xform_t const *p)
{
    cp_mat2w_t m;
    cp_mat2w_xlat(&m, 0, coord_y_frel(font->def, p->b, p->a));
    cp_mat2w_mul(&gc->xform, &m, &gc->xform);
}

static void smallcap(
    font_t const *font CP_UNUSED, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    if (!gc->line_width_defined) {
        gc->line_width_defined = true;
        gc->line_width = font->def->line_width[0];
    }

    cp_mat2w_t m;
    cp_mat2w_scale(&m,
        coord_x_rel(font->def, -5,+5) / coord_x_rel(font->def, -7,+7),
        coord_y_rel(font->def, -3,+3) / coord_y_rel(font->def, -3,+6));
    cp_mat2w_mul(&gc->pre_xform, &gc->pre_xform, &m);
}

static void invert_uc(
    font_t const *font, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    cp_mat2w_t m;
    cp_mat2w_xlat(&m, 0, -coord_y_rel(font->def, -3,+6)/2);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);

    cp_mat2w_scale(&m, +1, -1);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);

    cp_mat2w_xlat(&m, 0, +coord_y_rel(font->def, -3,+6)/2);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);
}

static void invert_lc(
    font_t const *font, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    cp_mat2w_t m;
    cp_mat2w_xlat(&m, 0, -coord_y_rel(font->def, -3,+3)/2);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);

    cp_mat2w_scale(&m, +1, -1);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);

    cp_mat2w_xlat(&m, 0, +coord_y_rel(font->def, -3,+3)/2);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);
}

static void reverse_lc(
    font_t const *font CP_UNUSED, font_gc_t *gc, font_draw_xform_t const *p CP_UNUSED)
{
    cp_mat2w_t m;
    cp_mat2w_scale(&m, -1, +1);
    cp_mat2w_mul(&gc->xform, &gc->xform, &m);
}

static void turn_lc(
    font_t const *font, font_gc_t *gc, font_draw_xform_t const *p)
{
    reverse_lc(font, gc, p);
    invert_lc (font, gc, p);
}

CP_NORETURN
CP_VPRINTF(5)
static void vdie_(
    char const *file,
    int line,
    font_glyph_t const *out,
    font_t const *font,
    char const *form,
    va_list va)
{
    fprintf(stderr, "%s:%d: Error: ", file, line);
    fprintf(stderr, "font '%s': ", font->name.data);
    if (out != NULL) {
        fprintf(stderr, "glyph U+%04X '%s': ", out->unicode.code_point, out->unicode.name);
    }
    vfprintf(stderr, form, va);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

CP_NORETURN
CP_PRINTF(5,6)
static void die_(
    char const *file,
    int line,
    font_glyph_t const *out,
    font_t const *font,
    char const *form,
    ...)
{
    va_list va;
    va_start(va, form);
    vdie_(file, line, out, font, form, va);
    va_end(va);
}

#define die(out, font, ...) die_(__FILE__, __LINE__, out, font, __VA_ARGS__)

static unsigned my_signbit(double d)
{
    union {
        double d;
        int64_t i;
    } u;
    u.d = d;
    return u.i < 0;
}

/**
 * Is it positive zero?
 *
 * We take that as 'undefined', too, because it is the default init value
 * in C.  A defined 0 can be written as '-0'.
 */
static bool is_pos0(double x)
{
    return cp_eq(x,0) && (my_signbit(x) == 0);
}

static bool is_defined(double x)
{
    return !is_pos0(x);
}

static double coord_x(
    font_glyph_t *out,
    font_t const *font,
    int i,
    cp_mat2w_t const *pre_xform)
{
    font_def_t const *def = font->def;
    if ((i < def->box.lo.x) || (i > def->box.hi.x)) {
        die(out, font, "x coord %+d out of range %+d..%+d",
            i, def->box.lo.x, def->box.hi.x);
    }
    int idx = i - def->box.lo.x;
    assert(idx >= 0);
    assert(idx <= (def->box.hi.x - def->box.lo.x));
    if (out != NULL) {
        out->used_x[idx] = 1;
    }
    double d = cp_v_nth(&def->coord_x, (size_t)idx);
    if (!is_defined(d)) {
        die(out, font, "x coord %+d refers to undefined coord_x[%d]=%+g", i, idx, d);
    }
    cp_vec2_t v = {{ d * def->scale_x, 0 }};
    if (pre_xform != NULL) {
        v.x -= coord_x_abs(def,0);
        cp_vec2w_xform(&v, pre_xform, &v);
        v.x += coord_x_abs(def,0);
    }
    return v.x;
}

static double coord_y(
    font_glyph_t *out,
    font_t const *font,
    int i,
    cp_mat2w_t const *pre_xform)
{
    font_def_t const *def = font->def;
    if ((i < def->box.lo.y) || (i > def->box.hi.y)) {
        die(out, font, "y coord %+d out of range %+d..%+d",
            i, def->box.lo.y, def->box.hi.y);
    }
    int idx = i - def->box.lo.y;
    assert(idx >= 0);
    assert(idx <= (def->box.hi.y - def->box.lo.y));
    if (out != NULL) {
        out->used_y[idx] = 1;
    }
    double d = cp_v_nth(&def->coord_y, (size_t)idx);
    if (!is_defined(d)) {
        die(out, font, "y coord %+d refers to undefined coord_y[%d]=%+g", i, idx, d);
    }
    cp_vec2_t v = {{ 0, d }};
    if (pre_xform != NULL) {
        v.y -= font->base_y;
        cp_vec2w_xform(&v, pre_xform, &v);
        v.y += font->base_y;
    }
    return v.y;
}

static double slant_x(font_t const *font, double x, double y)
{
     return x + (font->def->slant * (y - font->base_y));
}

static double unslant_x(font_t const *font, double x, double y)
{
     return x - (font->def->slant * (y - font->base_y));
}

static double get_x(
    font_glyph_t *out,
    font_t const *font,
    font_def_coord_t const *x,
    bool swap_x,
    double line_width,
    cp_mat2w_t const *pre_xform)
{
    font_def_t const *def = font->def;
    int pri = swap_x ? -x->primary   : +x->primary;
    int sec = swap_x ? -x->secondary : +x->secondary;

    int sub_cnt = cp_countof(def->sub_x);
    if (abs(x->sub) >= sub_cnt) {
        die(out, font, "x sub %+d is out of range %+d..%+d\n", x->sub, -sub_cnt, +sub_cnt);
    }
    double d1 = 0;
    if (x->interpol != 60) {
        d1 = coord_x(out, font, pri, pre_xform);
    }
    double d2 = 0;
    if (x->interpol != 0) {
        d2 = coord_x(out, font, sec, pre_xform);
    }
    double len = 0;
    if (x->len.frac != 0) {
        len += (x->len.frac / 60.) * (
            coord_x(out, font, swap_x ? -x->len.to   : +x->len.to,   pre_xform) -
            coord_x(out, font, swap_x ? -x->len.from : +x->len.from, pre_xform));
    }
    if (x->olen.frac != 0) {
        len += (x->olen.frac / 60.) * (
            coord_y(out, font, x->olen.to,   pre_xform) -
            coord_y(out, font, x->olen.from, pre_xform));
    }
    double f = pri < 0 ? -1 : +1;
    return
        cp_lerp(d1, d2, x->interpol/60.)
        + (f * 0.5 * line_width * cp_cmp(x->sub,0) * def->sub_x[abs(x->sub)])
        + (f * font->def->dot_size * (x->dot_rel / 60.))
        + len;
}

static double get_y(
    font_glyph_t *out,
    font_t const *font,
    font_def_coord_t const *y,
    double line_width,
    cp_mat2w_t const *pre_xform)
{
    font_def_t const *def = font->def;
    int pri = y->primary;
    int sec = y->secondary;

    int sub_cnt = cp_countof(def->sub_y);
    if ((y->sub >= +sub_cnt) || (y->sub <= -sub_cnt)) {
        die(out, font, "y sub %+d is out of range %+d..%+d\n", y->sub, -sub_cnt, +sub_cnt);
    }
    double d1 = 0;
    if (y->interpol != 60) {
        d1 = coord_y(out, font, pri, pre_xform);
    }
    double d2 = 0;
    if (y->interpol != 0) {
        d2 = coord_y(out, font, sec, pre_xform);
    }
    double f = (pri < 0) ? -1 : +1;
    double len = 0;
    if (y->len.frac != 0) {
        len += (y->len.frac / 60.) * (
            coord_y(out, font, y->len.to,   pre_xform) -
            coord_y(out, font, y->len.from, pre_xform));
    }
    if (y->olen.frac != 0) {
        assert(0 && "currently not used, think about whether you really need this");
        len += (y->olen.frac / 60.) * (
            coord_x(out, font, y->olen.to,   pre_xform) -
            coord_x(out, font, y->olen.from, pre_xform));
    }
    return
        cp_lerp(d1, d2, y->interpol/60.)
        + (f * 0.5 * line_width * cp_cmp(y->sub,0) * def->sub_y[abs(y->sub)])
        + (f * font->def->dot_size * (y->dot_rel / 60.))
        + len;
}

static void poly_push_path_(
    font_draw_poly_t *poly,
    font_glyph_t const *out,
    cp_vec2_t const *data,
    size_t size)
{
    /* update bb */
    for (cp_size_each(i, size)) {
        cp_vec2_t d = data[i];
        d.x = unslant_x(out->font, d.x, d.y);
        cp_vec2_minmax(&poly->box, &d);
    }

    /* push path into poly */
    font_draw_path_t *path = cp_v_push0(&poly->path);
    path->point.data = CP_CLONE_ARR(*path->point.data, data, size);
    path->point.size = size;
}

#define poly_push_path(out, poly, ...) \
    ({ \
        cp_vec2_t _data[] = { __VA_ARGS__ }; \
        poly_push_path_(out, poly, _data, cp_countof(_data)); \
    })

static void draw_line(
    font_draw_poly_t *poly,
    font_glyph_t const *out,
    font_stroke_line_t const *l)
{
    poly_push_path(poly, out, l->src.left, l->dst.left, l->dst.right, l->src.right);
}

static void stroke_line(
    font_stroke_line_t *line,
    cp_vec2_t const *src,
    cp_vec2_t const *dst,
    double line_width)
{
    cp_vec2_t n;
    cp_vec2_normal(&n, src, dst);
    assert(!cp_vec2_has_len0(&n));
    double lw2 = line_width/2;
    line->src.left.x  = src->x - (n.x * lw2);
    line->src.left.y  = src->y - (n.y * lw2);
    line->dst.left.x  = dst->x - (n.x * lw2);
    line->dst.left.y  = dst->y - (n.y * lw2);
    line->src.right.x = src->x + (n.x * lw2);
    line->src.right.y = src->y + (n.y * lw2);
    line->dst.right.x = dst->x + (n.x * lw2);
    line->dst.right.y = dst->y + (n.y * lw2);
}

static void draw_corner3(
    font_draw_poly_t *poly,
    font_glyph_t const *out,
    cp_vec2_t const *c,
    cp_vec2_t const *u,
    cp_vec2_t const *v)
{
    poly_push_path(poly, out, *c, *u, *v);
}

static void draw_corner(
    font_draw_poly_t *poly,
    font_glyph_t const *gl,
    cp_vec2_t const *vc,
    font_stroke_line_t const *in,
    font_stroke_line_t const *out,
    int dir)
{
    switch (dir) {
    default:
        assert(0);
    case 0:
        return;

    case -1:
        draw_corner3(poly, gl, vc, &out->src.right, &in->dst.right);
        return;

    case +1:
        draw_corner3(poly, gl, vc, &in->dst.left, &out->src.left);
        return;
    }
}

static void get_intersection(
    cp_vec2_t *i,
    cp_vec2_t const *a,
    cp_vec2_t const *b,
    cp_vec2_t const *c,
    cp_vec2_t const *d)
{
    double p = (a->x * b->y) - (a->y * b->x);
    double q = (c->x * d->y) - (c->y * d->x);

    double z = ((a->x - b->x)*(c->y - d->y)) - ((a->y - b->y)*(c->x - d->x));
    assert(!cp_eq(z,0));

    i->x = ((p*(c->x - d->x)) - (q*(a->x - b->x))) / z;
    i->y = ((p*(c->y - d->y)) - (q*(a->y - b->y))) / z;
}

static void build_link(
    cp_a_vec2_t *pt,
    font_def_t const *def,
    cp_vec2_t const *va,
    cp_vec2_t const *vb,
    cp_vec2_t const *vc,
    cp_vec2_t const *vd)
{
    pt->size = 2 + def->round_step_cnt;
    pt->data = CP_NEW_ARR(*pt->data, pt->size);
    cp_v_nth(pt, 0) = *vb;
    cp_v_nth(pt, pt->size-1) = *vc;
    if (def->round_step_cnt == 0) {
        return;
    }

    cp_vec2_t vi;
    get_intersection(&vi, va, vb, vc, vd);

    cp_vec2_t vp;
    cp_vec2_lerp(&vp, &vi, vb, def->round_tension);

    cp_vec2_t vq;
    cp_vec2_lerp(&vq, &vi, vc, def->round_tension);

    for (cp_v_each(i, pt, 1, 1)) {
        double t = cp_f(i) / cp_f(pt->size - 1);
        cp_vec2_t *pi = cp_v_nth_ptr(pt, i);
        pi->x = cp_interpol3(vb->x, vp.x, vq.x, vc->x, t);
        pi->y = cp_interpol3(vb->y, vp.y, vq.y, vc->y, t);
    }
}

static void end_stroke(
    font_stroke_end_t *e,
    cp_vec2_t const *a,
    cp_vec2_t const *b)
{
    cp_vec2_t d;
    cp_vec2_sub(&d, b, a);
    if (fabs(d.x) >= fabs(d.y)) {
        /* more horizontal than vertical */
        return;
    }

    /* vertical */
    double k = d.x / d.y;
    double c = e->left.y - b->y;
    e->left.y = b->y;
    e->left.x -= c * k;

    c = e->right.y - b->y;
    e->right.y = b->y;
    e->right.x -= c * k;
}

/**
 * Draws the line vp-vc.
 * Draws the connector between vp-vc and vc-vn.
 */
static void convert_draw_segment(
    font_draw_poly_t *poly,
    font_glyph_t const *gl,
    font_def_t const *def,
    double lw,
    font_vertex_type_t ptype,
    font_vertex_type_t type,
    cp_vec2_t const *vp,
    cp_vec2_t const *vpn,
    cp_vec2_t const *vcp,
    cp_vec2_t const *vc,
    cp_vec2_t const *vcn,
    cp_vec2_t const *vnp,
    cp_vec2_t const *vn)
{
    switch (type) {
    case FONT_VERTEX_ROUND:
        assert(0);
    default:
        break;

    case FONT_VERTEX_GIANT:
    case FONT_VERTEX_HUGE:
    case FONT_VERTEX_LARGE:
    case FONT_VERTEX_SMALL:{
        /* round */
        /* vp vpn vcp vc
         *            vcn
         *            vnp
         *            vp
         */
        cp_a_vec2_t link = {0};
        build_link(&link, def, vpn, vcp, vcn, vnp);
        assert(link.size >= 2);

        cp_vec2_t const *a;
        cp_vec2_t const *b = cp_v_nth_ptr(&link, 0);
        cp_vec2_t const *c = cp_v_nth_ptr(&link, 1);
        convert_draw_segment(poly, gl, def, lw, ptype, FONT_VERTEX_POINTED, vp, vpn, b,b,b, c,c);

        for(cp_v_each(i, &link, 1, 1)) {
            a = cp_v_nth_ptr(&link, cp_wrap_sub1(i, link.size));
            b = cp_v_nth_ptr(&link, i);
            c = cp_v_nth_ptr(&link, cp_wrap_add1(i, link.size));
            convert_draw_segment(poly, gl, def, lw,
                FONT_VERTEX_POINTED, FONT_VERTEX_POINTED, a,a, b,b,b, c,c);
        }

        b = cp_v_nth_ptr(&link, link.size-2);
        c = cp_v_nth_ptr(&link, link.size-1);
        convert_draw_segment(poly, gl, def, lw,
            FONT_VERTEX_POINTED, FONT_VERTEX_POINTED, b,b, c,c,c, vnp, vn);

        CP_FREE(link.data);
        return;}

    case FONT_VERTEX_ANGLED:
    case FONT_VERTEX_TIGHT:
        /* same as 'LARGE/SMALL', but angled (usually at 2*135 deg to get 90 deg, or wider) */
        convert_draw_segment(poly, gl, def, lw,
            ptype, FONT_VERTEX_POINTED, vp, vpn, vcp, vcp, vcp, vcn, vcn);

        convert_draw_segment(poly, gl, def, lw,
            FONT_VERTEX_POINTED, FONT_VERTEX_POINTED, vcp, vcp, vcn, vcn, vcn, vnp, vn);
        return;
    }

    font_stroke_line_t in, out;
    stroke_line(&in,  vpn, vcp, lw);
    stroke_line(&out, vcn, vnp, lw);

    if (ptype == FONT_VERTEX_BEGIN) {
        end_stroke(&in.src, vcp, vpn);
    }
    if (type == FONT_VERTEX_END) {
        end_stroke(&in.dst, vpn, vcp);
    }

    switch (type) {
    case FONT_VERTEX_ROUND:
    case FONT_VERTEX_GIANT:
    case FONT_VERTEX_HUGE:
    case FONT_VERTEX_LARGE:
    case FONT_VERTEX_ANGLED:
    case FONT_VERTEX_TIGHT:
    case FONT_VERTEX_SMALL:
    case FONT_VERTEX_CHAMFER:
    case FONT_VERTEX_NEW:
    case FONT_VERTEX_DENT:
        assert(0);
    case FONT_VERTEX_BEGIN:
    case FONT_VERTEX_IN:
        break;

    case FONT_VERTEX_END:
    case FONT_VERTEX_OUT:
    case FONT_VERTEX_MIRROR:
        draw_line(poly, gl, &in);
        break;

    case FONT_VERTEX_POINTED:
        draw_line(poly, gl, &in);
        draw_corner(poly, gl, vc, &in, &out, cp_vec2_right_normal3_z(vp, vc, vn));
        break;
    }
}

static font_corner_type_t get_corner_type(
    cp_vec2_t const *u,
    cp_vec2_t const *c,
    cp_vec2_t const *v)
{
    int uvx = cp_cmp(u->x, v->x);
    if (uvx > 0) {
        CP_SWAP(&u, &v);
    }

    int uvy = cp_cmp(u->y, v->y);
    if (uvy == 0) {
        /* This could be distinguished further, like:
         *      c
         *                  => TOP_LEFT
         *    u      v
         * But this is would be a strange design.
         */
        assert(0);
        return FONT_STRAIGHT;
    }

    int bend = cp_vec2_right_normal3_z(u, c, v);
    if (uvy < 0) {
        /* u is below v */
        switch (bend) {
        case 0:  return FONT_STRAIGHT;
        case -1: return FONT_BOTTOM_RIGHT;
        case +1: return FONT_TOP_LEFT;
        }
    }
    else {
        /* u is above v */
        switch (bend) {
        case 0:  return FONT_STRAIGHT;
        case -1: return FONT_BOTTOM_LEFT;
        case +1: return FONT_TOP_RIGHT;
        }
    }
    CP_DIE();
}

static font_vertex_type_t resolve_vertex_type(
    font_def_t const *def,
    font_vertex_type_t t,
    cp_vec2_t const *p,
    cp_vec2_t const *c,
    cp_vec2_t const *n)
{
    if (t != FONT_VERTEX_ROUND) {
        return t;
    }
    return def->corner_type[get_corner_type(p, c, n)];
}

static void convert_draw_vertex_arr(
    font_draw_poly_t *poly,
    font_glyph_t const *out,
    font_vertex_t *v,
    size_t sz)
{
    if (sz == 0) {
        return;
    }
    assert(sz >= 2);

    font_t const *font = out->font;
    font_def_t const *def = font->def;

    /* resolve vertex type before slanting */
    for (cp_size_each(i, sz)) {
        size_t p = cp_wrap_sub1(i, sz);
        size_t n = cp_wrap_add1(i, sz);
        v[i].type = resolve_vertex_type(def, v[i].type, &v[p].coord, &v[i].coord, &v[n].coord);
    }

    /* slant coordinates */
    for (cp_size_each(i, sz)) {
        v[i].coord.x = slant_x(font, v[i].coord.x, v[i].coord.y);
    }

    /* compute length of lines */
    double *l = CP_NEW_ARR(*l, sz);
    for (cp_size_each(i, sz)) {
        size_t n = cp_wrap_add1(i, sz);
        l[i] = cp_vec2_dist(&v[i].coord, &v[n].coord);
    }

    /* resolve DENT into POINTED */
    for (cp_size_each(i, sz)) {
        if (v[i].type != FONT_VERTEX_DENT) {
            continue;
        }
        size_t p = cp_wrap_sub1(i, sz);
        size_t n = cp_wrap_add1(i, sz);
        int c = cp_vec2_right_normal3_z(&v[p].coord, &v[i].coord, &v[n].coord);
        if (c == 0) {
            die(out, font, "Found dent without indication of direction");
        }
        double d = cp_vec2_dist(&v[p].coord, &v[n].coord) / 2;
        cp_vec2_t k;
        cp_vec2_normal(&k, &v[p].coord, &v[n].coord);
        cp_vec2_t e;
        cp_vec2_lerp(&e, &v[p].coord, &v[n].coord, 0.5);
        v[i].type = FONT_VERTEX_POINTED;
        v[i].coord.x = e.x - ((k.x * d) * c);
        v[i].coord.y = e.y - ((k.y * d) * c);
        /* recompute lengths */
        l[p] = cp_vec2_dist(&v[p].coord, &v[i].coord);
        l[i] = cp_vec2_dist(&v[i].coord, &v[n].coord);
    }

    /* resolve CHAMFER into POINTED */
    for (cp_size_each(i, sz)) {
        if (v[i].type != FONT_VERTEX_CHAMFER) {
            continue;
        }
        v[i].type = FONT_VERTEX_POINTED;
        size_t p = cp_wrap_sub1(i, sz);
        size_t n = cp_wrap_add1(i, sz);
        if (cp_eq(l[i], l[p])) {
            die(out, out->font, "Around chamfer, both edges have the same length");
        }
        if (l[i] < l[p]) {
            cp_vec2_lerp(&v[i].coord, &v[i].coord, &v[p].coord, l[i]/l[p]);
            l[p] = cp_vec2_dist(&v[p].coord, &v[i].coord);
            l[i] = cp_vec2_dist(&v[i].coord, &v[n].coord);
        }
        else {
            cp_vec2_lerp(&v[i].coord, &v[i].coord, &v[n].coord, l[p]/l[i]);
            l[p] = cp_vec2_dist(&v[p].coord, &v[i].coord);
            l[i] = cp_vec2_dist(&v[i].coord, &v[n].coord);
        }
    }

    /* resolve vertex type and set initial corner radius */
    double *r = CP_NEW_ARR(*r, sz);
    for (cp_size_each(i, sz)) {
        size_t p = cp_wrap_sub1(i, sz);
        switch (v[i].type) {
        case FONT_VERTEX_SMALL:
            r[i] = def->radius[0] * v[i].radius_mul;
            break;
        case FONT_VERTEX_LARGE:
            r[i] = def->radius[1] * v[i].radius_mul;
            break;
        case FONT_VERTEX_HUGE:
            r[i] = def->radius[2] * v[i].radius_mul;
            break;
        case FONT_VERTEX_GIANT:
            r[i] = def->radius[3] * v[i].radius_mul;
            break;

        case FONT_VERTEX_TIGHT:
            r[i] = def->angle[0] * v[i].radius_mul;
            break;
        case FONT_VERTEX_ANGLED:
            r[i] = def->angle[1] * v[i].radius_mul;
            break;

        case FONT_VERTEX_CHAMFER:
        case FONT_VERTEX_DENT:
        case FONT_VERTEX_ROUND:
            assert(0);
        default:
            break;
        }
        r[i] = cp_max(0.0, cp_min(r[i], cp_min(l[i] - def->min_dist, l[p] - def->min_dist)));
    }

    /* reduce corner radii if the line is too short */
    for (cp_size_each(i, sz)) {
        size_t n = cp_wrap_add1(i, sz);
        if (v[i].type == FONT_VERTEX_POINTED) {
            continue;
        }
        double c = l[i] - def->min_dist;
        assert(cp_ge(c,0) && "Line is too short");
        if (c < (r[i] + r[n])) {
            double *small = &r[i];
            double *large = &r[n];
            if (*small > *large) {
                CP_SWAP(&small, &large);
            }
            assert(*small <= *large);
            if (c > (2 * *small)) {
                *large = c - *small;
            }
            else {
                *small = *large = c/2;
            }
        }
        assert(cp_ge(l[i] - (r[i] + r[n]), def->min_dist));
    }

    /* compute previous and next vertices */
    cp_vec2_t *wp = CP_NEW_ARR(*wp, sz);
    cp_vec2_t *wn = CP_NEW_ARR(*wn, sz);
    for (cp_size_each(i, sz)) {
        size_t n = cp_wrap_add1(i, sz);
        font_vertex_t *vi = &v[i];
        font_vertex_t *vn = &v[n];
        cp_vec2_lerp(&wn[i], &vi->coord, &vn->coord, r[i] / l[i]);
        cp_vec2_lerp(&wp[n], &vn->coord, &vi->coord, r[n] / l[i]);
    }

    /* draw segments */
    for (cp_size_each(i, sz)) {
        size_t p = cp_wrap_sub1(i, sz);
        size_t n = cp_wrap_add1(i, sz);
        assert(!cp_vec2_eq(&wn[i], &wp[n]));
        assert(!cp_vec2_eq(&wn[p], &wp[i]));
        convert_draw_segment(poly, out, def, v[i].line_width, v[p].type, v[i].type,
            &v[p].coord, &wn[p], &wp[i], &v[i].coord, &wn[i], &wp[n], &v[n].coord);
    }

    CP_FREE(wn);
    CP_FREE(wp);
    CP_FREE(r);
    CP_FREE(l);
}

static font_draw_poly_t *convert_draw_v_vertex(
    font_glyph_t *out,
    font_v_vertex_t *v)
{
    font_draw_poly_t *poly = CP_NEW(*poly);
    poly->box = (cp_vec2_minmax_t)CP_VEC2_MINMAX_EMPTY;
    size_t start = 0;
    for (cp_v_each(i, v)) {
        if (cp_v_nth(v,i).type == FONT_VERTEX_NEW) {
            convert_draw_vertex_arr(poly, out, v->data + start, (i - start));
            start = i+1;
        }
    }
    convert_draw_vertex_arr(poly, out, v->data + start, (v->size - start));
    return poly;
}

static void get_glyph(
    font_v_vertex_t *vo,
    font_glyph_t *out,
    font_gc_t const *gc,
    font_draw_t const *vi);

static void get_glyph_rec(
    font_v_vertex_t *vo,
    font_glyph_t *out,
    font_gc_t const *gc,
    font_draw_t const *vi);

static void get_glyph_merge(
    font_v_vertex_t *vo,
    font_glyph_t *out,
    font_gc_t const *gc,
    font_draw_t const *_vi)
{
    font_draw_merge_t const *vi = font_draw_cast(*vi, _vi);
    for (cp_v_each(i, &vi->child)) {
        font_draw_t const *ii = cp_v_nth(&vi->child, i);
        get_glyph(vo, out, gc, ii);

        font_vertex_t *sep = cp_v_push0(vo);
        sep->type = FONT_VERTEX_NEW;
    }
}

static void get_glyph_xform(
    font_v_vertex_t *vo,
    font_glyph_t *out,
    font_gc_t const *go,
    font_draw_t const *_vi)
{
    font_draw_xform_t const *vi = font_draw_cast(*vi, _vi);

    font_gc_t gn = *go;
    vi->xform(out->font, &gn, vi);

    get_glyph(vo, out, &gn, vi->child);
}

static int cmp_glyph_unicode_ref(
    unsigned const *a,
    font_glyph_t const *b,
    void *u CP_UNUSED)
{
    unsigned acp = *a;
    unsigned bcp = b->unicode.code_point;
    return acp == bcp ? 0 : acp < bcp ? -1 : +1;
}

static font_glyph_t *find_glyph0(
    font_t const *font, unsigned cp)
{
    size_t j = cp_v_bsearch(&cp, &font->glyph, cmp_glyph_unicode_ref, NULL);
    if (j >= font->glyph.size) {
        return NULL;
    }
    return cp_v_nth_ptr(&font->glyph, j);
}

static font_glyph_t *find_glyph(
    font_glyph_t const *out, unicode_t const *unicode)
{
    font_glyph_t *gj = find_glyph0(out->font, unicode->code_point);
    if (gj == NULL) {
        die(out, out->font, "Referenced glyph U+%04X '%s' not found in font",
            unicode->code_point,  unicode->name);
    }
    return gj;
}

static double line_width(
    font_def_t const *def,
    double step)
{
    double lo = def->line_width[lrint(floor(step))];
    double hi = def->line_width[lrint(ceil(step))];
    double fr = step - floor(step);
    return cp_lerp(lo, hi, fr);
}

static void get_glyph_ref(
    font_v_vertex_t *vo,
    font_glyph_t *out,
    font_gc_t const *ogc,
    font_draw_t const *_vi)
{
    font_draw_ref_t const *vi = font_draw_cast(*vi, _vi);
    font_glyph_t const *gj = find_glyph(out, &vi->unicode);

    /* by default, use first REF glyph for lpad and rpad, too */
    if (out->lpad_of == NULL) {
        out->lpad_of = gj;
    }
    if (out->rpad_of == NULL) {
        out->rpad_of = gj;
    }
    if (out->line_step_of == NULL) {
        out->line_step_of = gj;
    }

    font_gc_t gn = *ogc;
    if (!gn.line_width_defined) {
        gn.line_width_defined = is_defined(gj->def->line_step);
        gn.line_width = line_width(out->font->def, gj->def->line_step);
    }
    font_glyph_t *width_of = out->width_of;
    get_glyph_rec(vo, out, &gn, gj->def->draw);
    out->width_of = width_of;
}

static void get_glyph_width(
    font_glyph_t *out,
    font_draw_t const *_vi)
{
    font_draw_width_t const *vi = font_draw_cast(*vi, _vi);
    font_glyph_t *gj = find_glyph(out, &vi->unicode);
    if (out->width_of == NULL) {
        out->width_of = gj;
    }
}

static void get_glyph_lpad(
    font_glyph_t *out,
    font_draw_t const *_vi)
{
    font_draw_lpad_t const *vi = font_draw_cast(*vi, _vi);
    font_glyph_t const *gj = find_glyph(out, &vi->unicode);
    if (out->lpad_of == NULL) {
        out->lpad_of = gj;
    }
}

static void get_glyph_rpad(
    font_glyph_t *out,
    font_draw_t const *_vi)
{
    font_draw_rpad_t const *vi = font_draw_cast(*vi, _vi);
    font_glyph_t const *gj = find_glyph(out, &vi->unicode);
    if (out->rpad_of == NULL) {
        out->rpad_of = gj;
    }
}

static void get_glyph_stroke(
    font_v_vertex_t *vo,
    font_glyph_t *out,
    font_gc_t const *gc,
    font_draw_t const *_vi)
{
    font_t const *font = out->font;
    font_def_t const *def = font->def;
    font_draw_stroke_t const *vi = font_draw_cast(*vi, _vi);

    for (cp_v_each(i, &vi->vertex)) {
        font_def_vertex_t const *ii = cp_v_nth_ptr(&vi->vertex, i);
        font_vertex_t *oi = cp_v_push0(vo);
        oi->type = ii->type;
        oi->radius_mul =
            cp_min(fabs(gc->pre_xform.b.m[0][0]), fabs(gc->pre_xform.b.m[1][1])) *
            cp_min(fabs(gc->xform.b.m[0][0]), fabs(gc->xform.b.m[1][1]));
        oi->line_width = gc->line_width;
        oi->coord.x = get_x(out, font, &ii->x, gc->swap_x, gc->line_width, &gc->pre_xform);
        oi->coord.y = get_y(out, font, &ii->y,             gc->line_width, &gc->pre_xform);

        oi->coord.x -= coord_x_abs(def,0);
        oi->coord.y -= font->base_y;
        cp_vec2w_xform(&oi->coord, &gc->xform, &oi->coord);
        oi->coord.y += font->base_y;
        oi->coord.x += coord_x_abs(def,0);
    }
}

static void get_glyph_aux(
    font_v_vertex_t *vo,
    font_glyph_t *out,
    font_gc_t const *gc,
    font_draw_t const *vi)
{
    switch (vi->type) {
    case FONT_DRAW_MERGE:
        return get_glyph_merge(vo, out, gc, vi);
    case FONT_DRAW_XFORM:
        return get_glyph_xform(vo, out, gc, vi);
    case FONT_DRAW_REF:
        return get_glyph_ref(vo, out, gc, vi);
    case FONT_DRAW_WIDTH:
        return get_glyph_width(out, vi);
    case FONT_DRAW_LPAD:
        return get_glyph_lpad(out, vi);
    case FONT_DRAW_RPAD:
        return get_glyph_rpad(out, vi);
    case FONT_DRAW_STROKE:
        return get_glyph_stroke(vo, out, gc, vi);
    case FONT_DRAW_SEQUENCE:
        CP_DIE();
    }
    CP_DIE();
}

static void get_glyph(
    font_v_vertex_t *vo,
    font_glyph_t *out,
    font_gc_t const *gc,
    font_draw_t const *vi)
{
    if (vi == NULL) {
        return;
    }
    static size_t incarn = 0;
    incarn++;
    if (incarn >= 100) {
        die(out, out->font, "deep recursion");
    }
    get_glyph_aux(vo, out, gc, vi);
    incarn--;
}

static void get_glyph_rec(
    font_v_vertex_t *vo,
    font_glyph_t *out,
    font_gc_t const *gc,
    font_draw_t const *vi)
{
    if (vi == NULL) {
        return;
    }
    if (vi->type != FONT_DRAW_SEQUENCE) {
        get_glyph(vo, out, gc, vi);
        return;
    }

    assert(0);
    /* what is this for? */
#if 0
    font_draw_sequence_t const *dec = font_draw_cast(*dec,vi);
    if (dec->second.code_point != 0) {
        die(out, out->font,
            "recursive reference to sequenced glyph is currently not supported");
    }
    font_glyph_t const *gj = find_glyph(out, &dec->first);
    font_gc_t gn = *gc;
    if (!gn.line_width_defined) {
        gn.line_width_defined = is_defined(gj->def->line_step);
        gn.line_width = line_width(out->font->def, gj->def->line_step);
    }
    get_glyph_rec(vo, out, &gn, gj->def->draw);
#endif
}

static font_draw_poly_t *convert_draw(
    font_glyph_t *out)
{
    font_t const *font = out->font;
    font_def_t const *def = font->def;
    font_def_glyph_t const *dglyph = out->def;
    font_draw_t const *in = dglyph->draw;

    font_v_vertex_t vertex = {0};

    font_gc_t gc = {0};
    cp_mat2w_unit(&gc.xform);
    cp_mat2w_unit(&gc.pre_xform);
    gc.line_width_defined = is_defined(dglyph->line_step);
    gc.line_width = line_width(def, dglyph->line_step);

    get_glyph(&vertex, out, &gc, in);

    font_draw_poly_t *poly = convert_draw_v_vertex(out, &vertex);

    cp_v_fini(&vertex);

    return poly;
}

static void convert_glyph(
    font_glyph_t *out)
{
    font_def_glyph_t const *in CP_UNUSED = out->def;
    assert(in->unicode.code_point == out->unicode.code_point);

    if ((out->def->draw != NULL) &&
        (out->def->draw->type == FONT_DRAW_SEQUENCE))
    {
        return; /* handled later */
    }

    /* recurse */
    out->draw = convert_draw(out);

    /* bounding box and padding */
    out->box = out->draw->box;

    /* line_step (for possible min_coord/max_coord override) */
    if (out->line_step_of == NULL) {
        out->line_step_of = out;
    }

    double lw = line_width(out->font->def, out->line_step_of->def->line_step);
    if (out->def->min_coord != NULL) {
        out->box.min.x = get_x(out, out->font, out->def->min_coord, false, lw, NULL);
    }
    if (out->def->max_coord != NULL) {
        out->box.max.x = get_x(out, out->font, out->def->max_coord, false, lw, NULL);
    }

    if (out->def->min_coord_from_y != NULL) {
        out->box.min.x = get_y(out, out->font, out->def->min_coord_from_y, lw, NULL);
    }
    if (out->def->max_coord_from_y != NULL) {
        out->box.max.x = get_y(out, out->font, out->def->max_coord_from_y, lw, NULL);
    }

    if (out->def->center_coord != NULL) {
        if (out->box.min.x > out->box.max.x) {
            die(out, out->font, "center_coord without defined X min/max");
        }
        double width = out->box.max.x - out->box.min.x;
        double center_x = get_x(out, out->font, out->def->center_coord, false, lw, NULL);
        out->box.min.x = center_x - width/2;
        out->box.max.x = center_x + width/2;
    }
}

static void compute_glyph_width(
    font_glyph_t *out);

static void compute_glyph_width_sequence(
    font_glyph_t *out,
    font_draw_t const *draw)
{
    font_draw_sequence_t const *d = font_draw_cast(*d, draw);
    assert(d->seq.size >= 1);

    font_subglyph_t const *sg = cp_v_nth(&d->seq,0);
    font_glyph_t *first = find_glyph(out, &sg->unicode);
    compute_glyph_width(first);

    out->box = first->box;
    out->dim = first->dim;

    /* kern first glyph */
    out->dim.min.x -= sg->kern;

    for (cp_v_each(i, &d->seq, 1)) {
        sg = cp_v_nth(&d->seq,i);
        font_glyph_t *second = find_glyph(out, &sg->unicode);
        compute_glyph_width(second);

        /* merge Y */
        out->box.min.y = cp_min(out->box.min.y, second->box.min.y);
        out->box.max.y = cp_max(out->box.max.y, second->box.max.y);
        out->dim.min.y = cp_min(out->dim.min.y, second->dim.min.y);
        out->dim.max.y = cp_max(out->dim.max.y, second->dim.max.y);

        /* compute X */
        out->dim.max.x += second->dim.max.x - second->dim.min.x;
        out->box.max.x = out->dim.max.x - (second->dim.max.x - second->box.max.x);

        /* kern */
        out->box.max.x += sg->kern;
        out->dim.max.x += sg->kern;
    }
}

static void compute_glyph_width(
    font_glyph_t *out)
{
    /* already done? */
    if (cp_vec2_minmax_valid(&out->dim)) {
        return;
    }

    /* sequence: do nothing, the finalisation step will handle this */
    if ((out->def->draw != NULL) &&
        (out->def->draw->type == FONT_DRAW_SEQUENCE))
    {
        compute_glyph_width_sequence(out, out->def->draw);

        double mid = coord_x_abs(out->font->def, 0);
        double w = out->dim.max.x - out->dim.min.x;
        double l = mid - w/2;
        double a = l - out->dim.min.x;
        out->dim.min.x += a;
        out->dim.max.x += a;
        out->box.min.x += a;
        out->box.max.x += a;

        return;
    }

    /* lpad */
    if ((out->lpad_of == NULL) || is_defined(out->def->lpad_abs)) {
        out->lpad_of = out;
    }
    assert(out->lpad_of != NULL);
    out->lpad = out->lpad_of->def->lpad_abs;

    /* rpad */
    if ((out->rpad_of == NULL) || is_defined(out->def->rpad_abs)) {
        out->rpad_of = out;
    }
    assert(out->rpad_of != NULL);
    out->rpad = out->rpad_of->def->rpad_abs;

    /* copy width and height;
     * height is copied from box, or set equal to font_y */
    out->dim.min.y = out->dim.max.y = out->font->base_y;

    if (out->width_of != NULL) {
        /* get width from other glyph */
        compute_glyph_width(out->width_of);
        out->dim.min.x = out->width_of->dim.min.x;
        out->dim.max.x = out->width_of->dim.max.x;
        if (!is_defined(out->lpad)) {
            out->lpad = out->width_of->lpad;
        }
        if (!is_defined(out->rpad)) {
            out->rpad = out->width_of->rpad;
        }
    }
    else {
        double min_x = out->box.min.x;
        double max_x = out->box.max.x;

        /* get width from box + pad */
        if (min_x > max_x) {
            die(out, out->font,
                "Empty glyph without reference width glyph or manual width setting");
        }

        /* finalise padding */
        if (!is_defined(out->lpad)) {
            out->lpad = out->font->def->lpad_default + out->lpad_of->def->lpad_add;
        }
        if (!is_defined(out->rpad)) {
            out->rpad = out->font->def->rpad_default + out->rpad_of->def->rpad_add;
        }

        /* compute dim */
        out->dim.min.x = min_x - out->lpad;
        out->dim.max.x = max_x + out->rpad;
    }
    assert(cp_le(out->dim.min.x, out->dim.max.x));

    /* in any case, apply the multiplier, if given */
    if (is_defined(out->def->width_mul)) {
        double w = (out->dim.max.x - out->dim.min.x);
        double ws = w * out->def->width_mul;
        double wd = (ws - w)/2;
        out->dim.max.x += wd;
        out->dim.min.x -= wd;

        /* padding with scaled width makes no sense => clear */
        out->lpad = 0;
        out->rpad = 0;
    }
}

static void normalise_filename(
    cp_vchar_t *s)
{
    char *t = s->data;
    for (cp_v_each(i, s)) {
        char c = cp_v_nth(s, i);
        c = (char)tolower((unsigned char)c);
        if (c == '-') {
            continue;
        }
        if (c == ' ') {
            c = '_';
        }
        *(t++) = c;
    }
    *t = 0;
    s->size = CP_PTRDIFF(t, s->data);
}

static void normalise_c_name_lc(
    cp_vchar_t *s)
{
    normalise_filename(s);
}

static void normalise_c_name_uc(
    cp_vchar_t *s)
{
    normalise_c_name_lc(s);
    for (cp_v_each(i, s)) {
        char *c = cp_v_nth_ptr(s, i);
        *c = (char)toupper((unsigned char)*c);
    }
}

static void convert_coll_box_range(
    font_t *font,
    size_t ia,
    size_t ib)
{
    double range = cp_f(ib) - cp_f(ia);
    for (cp_size_each(i, ib, ia+1)) {
        font->coll_box_y[i] =
           cp_lerp(
               font->coll_box_y[ia],
               font->coll_box_y[ib],
               cp_f(i - ia) / range);
    }
}

static void convert_coll_box(
    font_t *font)
{
    unsigned const base = 4;
    unsigned const cnt = CP_FONT_GLYPH_LAYER_COUNT;
    unsigned const inter = cnt - base;
    unsigned const top  = ((inter + 1) / 5);
    unsigned const hi   = ((inter + 3) / 5);
    unsigned const mid  = ((inter + 4) / 5);
    unsigned const lo   = ((inter + 2) / 5);
    unsigned const bot  = ((inter + 0) / 5);
    assert(cnt == (base + bot + lo + mid + hi + top));

    unsigned b2 = bot + 1 + lo;
    unsigned b3 = b2  + 1 + mid;
    unsigned b4 = b3  + 1 + hi;

    double ls = font->def->line_width[LS_UPPER];
    double ls1 = ls * 0.126;
    double ls2 = ls1 + (ls * 1.005);

    font->coll_box_y[0]     = coord_y_abs(font->def, font->def->box.lo.y);

    font->coll_box_y[bot]   = coord_y_abs(font->def, font->def->dec_y) - ls1;
    font->coll_box_y[bot+1] = font->coll_box_y[bot] + ls2;

    font->coll_box_y[b2]    = coord_y_abs(font->def, font->def->base_y) - ls1;
    font->coll_box_y[b2+1]  = font->coll_box_y[b2] + ls2;

    font->coll_box_y[b3+1]  = coord_y_abs(font->def, font->def->xhi_y) + ls1;
    font->coll_box_y[b3]    = font->coll_box_y[b3+1] - ls2;

    font->coll_box_y[b4+1]  = coord_y_abs(font->def, font->def->cap_y) + ls1;
    font->coll_box_y[b4]    = font->coll_box_y[b4+1] - ls2;

    font->coll_box_y[cnt]   = coord_y_abs(font->def, font->def->box.hi.y);

    convert_coll_box_range(font, 0,     bot);
    convert_coll_box_range(font, bot+1, b2);
    convert_coll_box_range(font, b2+1,  b3);
    convert_coll_box_range(font, b3+1,  b4);
    convert_coll_box_range(font, b4+1,  cnt);
}

static double get_x_at_y(
    cp_vec2_t const *a,
    cp_vec2_t const *b,
    double y)
{
    assert(cp_le(a->y, y));
    assert(cp_ge(b->y, y));
    double t = cp_t01(a->y, y, b->y);
    return cp_lerp(a->x, b->x, t);
}

static void get_coll_lohi_line(
    minmax_t *x,
    minmax_t const *y,
    cp_vec2_t const *a,
    cp_vec2_t const *b)
{
    int y_cmp = cp_cmp(a->y, b->y);
    if (y_cmp > 0) {
        CP_SWAP(&a, &b);
    }
    assert(cp_cmp(a->y, b->y) <= 0);
    if (cp_lt(b->y, y->lo)) {
        return;
    }
    if (cp_gt(a->y, y->hi)) {
        return;
    }
    double top_x = b->x;
    double bot_x = a->x;
    if (y_cmp != 0) {
        if (cp_lt(a->y, y->lo)) {
            bot_x = get_x_at_y(a, b, y->lo);
        }
        if (cp_gt(b->y, y->hi)) {
            top_x = get_x_at_y(a, b, y->hi);
        }
    }
    if (top_x < x->lo) { x->lo = top_x; }
    if (top_x > x->hi) { x->hi = top_x; }
    if (bot_x < x->lo) { x->lo = bot_x; }
    if (bot_x > x->hi) { x->hi = bot_x; }
}

static void get_coll_lohi_glyph(
    minmax_t *xb,
    minmax_t const *y,
    font_glyph_t *g)
{
    minmax_t xr;
    xr.lo = +CP_F_MAX;
    xr.hi = -CP_F_MAX;

    assert(g->draw != NULL);
    for (cp_v_each(j, &g->draw->path)) {
        font_draw_path_t const *p = cp_v_nth_ptr(&g->draw->path,j);
        for (cp_v_each(k, &p->point)) {
            size_t l = cp_wrap_add1(k, p->point.size);
            cp_vec2_t const *a = cp_v_nth_ptr(&p->point, k);
            cp_vec2_t const *b = cp_v_nth_ptr(&p->point, l);
            get_coll_lohi_line(&xr, y, a, b);
        }
    }

    if (xr.lo < xr.hi) {
        assert(cp_vec2_minmax_valid(&g->dim));
        xb->lo = cp_max(0.0, xr.lo - g->box.min.x);
        xb->hi = cp_max(0.0, g->box.max.x - xr.hi);
    }
}

static void get_coll_box_glyph(
    font_glyph_t *g)
{
    /* reset box */
    for (cp_arr_each(i, g->coll_box)) {
        minmax_t *xb = &g->coll_box[i];
        xb->lo = -1;
        xb->hi = -1;
    }

    /* only have kerning for glyphs w draw info */
    if (g->draw == NULL) {
        return;
    }

    /* draw? */
    for (cp_arr_each(i, g->coll_box)) {
        minmax_t y = {
            .lo = g->font->coll_box_y[i],
            .hi = g->font->coll_box_y[i + 1],
        };
        get_coll_lohi_glyph(&g->coll_box[i], &y, g);
    }
}

/**
 * Convert a font into polygons.
 *
 * This replaces FONT_DRAW_STROKE by FONT_DRAW_POLY.
 */
static font_t *convert_font(
    font_def_t const *def)
{
    /* straight must be 'pointed' */
    assert(def->corner_type[FONT_STRAIGHT] == FONT_VERTEX_POINTED);

    /* make new font */
    font_t *font = CP_NEW(*font);
    font->def = def;
    font->slant = def->slant;

    /* make kerning boxes */
    convert_coll_box(font);

    /* derives names */
    font->family_name = def->family_name;

    cp_vchar_printf(&font->name, "%s", font->family_name);

    if (def->weight_name != NULL) {
        cp_vchar_printf(&font->style_name, "%s ", def->weight_name);
    }
    if (def->slope_name != NULL) {
        cp_vchar_printf(&font->style_name, "%s ", def->slope_name);
    }
    if (def->stretch_name != NULL) {
        cp_vchar_printf(&font->style_name, "%s ", def->stretch_name);
    }
    if (def->size_name != NULL) {
        cp_vchar_printf(&font->style_name, "%s ", def->size_name);
    }

    if (font->style_name.size == 0) {
        cp_vchar_printf(&font->style_name, "%s", DEFAULT_STYLE);
    }
    else {
        cp_vchar_pop(&font->style_name);
        cp_vchar_printf(&font->name, " %s", cp_vchar_cstr(&font->style_name));
    }

    cp_vchar_printf(&font->filename, "%s", cp_vchar_cstr(&font->name));
    normalise_filename(&font->filename);

    /* font parameters */
    font->base_y = coord_y(NULL, font, def->base_y, NULL);
    font->cap_y  = coord_y(NULL, font, def->cap_y,  NULL);
    font->xhi_y  = coord_y(NULL, font, def->xhi_y,  NULL);
    font->dec_y  = coord_y(NULL, font, def->dec_y,  NULL);

    size_t cxm = INTV_SIZE(def->box.lo.x, def->box.hi.x);
    size_t cym = INTV_SIZE(def->box.lo.y, def->box.hi.y);
    assert(is_defined(coord_x(NULL, font, def->box.lo.x, NULL)));
    assert(is_defined(coord_x(NULL, font, def->box.hi.x, NULL)));
    assert(is_defined(coord_y(NULL, font, def->box.lo.y, NULL)));
    assert(is_defined(coord_y(NULL, font, def->box.hi.y, NULL)));
    double lw2 = def->line_width[0]/2;
    font->box_max.min.x = coord_x(NULL, font, def->box.lo.x, NULL) -lw2;
    font->box_max.max.x = coord_x(NULL, font, def->box.hi.x, NULL) +lw2;
    font->box_max.min.y = coord_y(NULL, font, def->box.lo.y, NULL);
    font->box_max.max.y = coord_y(NULL, font, def->box.hi.y, NULL);
    font->top_y    = coord_y(NULL, font, def->top_y, NULL);
    font->bottom_y = coord_y(NULL, font, def->bottom_y, NULL);

    /* glyphs */

    /* pre-initialise glyph array */
    for (cp_v_each(i, &def->glyph)) {
        font_glyph_t *glyph = cp_v_push0(&font->glyph);
        font_def_glyph_t const *gd = cp_v_nth_ptr(&def->glyph, i);
        glyph->font = font;
        glyph->def = gd;
        glyph->used_x = CP_NEW_ARR(*glyph->used_x, cxm);
        glyph->used_y = CP_NEW_ARR(*glyph->used_y, cym);
        glyph->unicode = gd->unicode;
        glyph->box = (cp_vec2_minmax_t)CP_VEC2_MINMAX_EMPTY;
        glyph->dim = (cp_vec2_minmax_t)CP_VEC2_MINMAX_EMPTY;
    }

    /* generate polygons */
    for (cp_v_each(i, &def->glyph)) {
        font_glyph_t *glyph = cp_v_nth_ptr(&font->glyph, i);
        convert_glyph(glyph);
    }

    /* compute widths */
    for (cp_v_each(i, &def->glyph)) {
        font_glyph_t *glyph = cp_v_nth_ptr(&font->glyph, i);
        compute_glyph_width(glyph);
    }

    /* compute coll_box */
    for (cp_v_each(i, &def->glyph)) {
        font_glyph_t *glyph = cp_v_nth_ptr(&font->glyph, i);
        get_coll_box_glyph(glyph);
    }

    /* get em size */
    font_glyph_t const *em = find_glyph0(font, U_EM_SPACE);
    if (em == NULL) {
        die(NULL, font, "Font has no EM SPACE, so em width cannot be determined.");
    }
    font->em = em->dim.max.x - em->dim.min.x;

    return font;
}

static int cmp_glyph_unicode(
    font_def_glyph_t const *a,
    font_def_glyph_t const *b,
    void *u CP_UNUSED)
{
    unsigned acp = a->unicode.code_point;
    unsigned bcp = b->unicode.code_point;
    return acp == bcp ? 0 : acp < bcp ? -1 : +1;
}

static void sort_font_def(
    font_a_def_glyph_const_t *glyph)
{
    cp_v_qsort(glyph, 0, CP_SIZE_MAX, cmp_glyph_unicode, NULL);
}

/* ********************************************************************** */

static long rasterize_x_long(cp_mat2w_t const *ram, double x)
{
    double r = (x * ram->b.m[0][0]) + ram->w.v[0];
    return lrint(r);
}

static uint16_t rasterize_x(cp_mat2w_t const *ram, double x)
{
    long i = rasterize_x_long(ram, x);
    assert((i >= 0) && (i <= 0xfffe));
    return (uint16_t)i;
}

static uint16_t rasterize_y(cp_mat2w_t const *ram, double y)
{
    double r = (y * ram->b.m[1][1]) + ram->w.v[1];
    long i = lrint(r);
    assert((i >= 0) && (i <= 0xfffe));
    return (uint16_t)i;
}

static bool coord_is_end(cp_font_xy_t const *w)
{
    return (w->x == CP_FONT_X_SPECIAL) && (w->y == CP_FONT_Y_END);
}

typedef struct {
    cp_dict_t node;
    size_t idx;
} coord_cell_t;

static void finalise_save_path1(
    font_t *f,
    cp_dict_ref_t *ref,
    size_t idx)
{
    coord_cell_t *u = CP_NEW(*u);
    u->idx = idx;
    cp_dict_insert_ref(&u->node, ref, &f->coord_dict);
}

static int cmp_coord2(
    cp_font_xy_t const *a,
    cp_font_xy_t const *b)
{
    for (;;) {
        if (coord_is_end(a)) {
            return coord_is_end(b) ? 0 : +1;
        }
        if (coord_is_end(b)) {
            return -1;
        }

        if (a->x != b->x) {
            return a->x < b->x ? -1 : +1;
        }
        if (a->y != b->y) {
            return a->y < b->y ? -1 : +1;
        }
        a++;
        b++;
    }
}

static int cmp_coord(
    cp_font_xy_t const *a,
    cp_dict_t *b2,
    cp_v_font_xy_t *coord)
{
    coord_cell_t const *b1 = CP_BOX_OF(b2, coord_cell_t, node);
    cp_font_xy_t const *b = cp_v_nth_ptr(coord, b1->idx);
    return cmp_coord2(a, b);
}

static size_t finalise_find_path(
    cp_dict_ref_t *ref,
    font_t *f,
    cp_font_xy_t const *w)
{
    cp_dict_t *_o = cp_dict_find_ref(ref, w, f->coord_dict, cmp_coord, &f->final->coord, 0);
    if (_o == NULL) {
        return CP_SIZE_MAX;
    }
    coord_cell_t *o = CP_BOX_OF(_o, coord_cell_t, node);
    return o->idx;
}

static void finalise_save_path(
    font_t *f,
    cp_dict_ref_t *ref,
    cp_font_xy_t const *w,
    size_t idx)
{
    for(;;) {
        finalise_save_path1(f, ref, idx);
        if (coord_is_end(w)) {
            break;
        }
        assert(idx < f->final->coord.size);
        w++;
        idx++;
        if (finalise_find_path(ref, f, w) < f->final->coord.size) {
            break;
        }
    }
}

static void finalise_find_or_save_path(
    font_t *f)
{
    cp_font_t *c = f->final;
    /* find path */
    size_t idx = cp_v_last(&c->path);
    cp_font_xy_t const *w = cp_v_nth_ptr(&c->coord, idx);
    cp_dict_ref_t ref = {0};
    size_t idx2 = finalise_find_path(&ref, f, w);
    if (idx2 < idx) {
        /* replace by link to existing path */
        cp_v_last(&c->path) = idx2 & ~0U;
        cp_v_set_size(&c->coord, idx);
        return;
    }

    /* save path and suffixes */
    finalise_save_path(f, &ref, w, idx);
}

#if FLATTEN_POLY == 0
/* 1:1 copy of polygon data */

static void finalise_path(
    font_t *f,
    cp_mat2w_t const *ram,
    font_draw_path_t const *p)
{
    cp_font_t *c = f->final;
    cp_v_push(&c->path, c->coord.size & ~0U);
    for (cp_v_each(i, &p->point)) {
        cp_vec2_t const *v = &cp_v_nth(&p->point, i);
        cp_font_xy_t *w = cp_v_push0(&c->coord);
        w->x = rasterize_x(ram, v->x);
        w->y = rasterize_y(ram, v->y);
    }
    cp_font_xy_t *w = cp_v_push0(&c->coord);
    w->x = CP_FONT_X_SPECIAL;
    w->y = CP_FONT_Y_END;

    finalise_find_or_save_path(f);
}

static void finalise_poly(
    cp_mat2w_t const *ram,
    font_glyph_t const *g)
{
    font_draw_poly_t const *p = g->draw;
    for (cp_v_each(i, &p->path)) {
        finalise_path(g->font, ram, &cp_v_nth(&p->path, i));
    }
}

static void finalise_prepare_glyph(
    cp_vec2_minmax_t *box,
    font_glyph_t *g)
{
    if (g->draw != NULL) {
        for (cp_v_each(j, &g->draw->path)) {
            font_draw_path_t const *p = cp_v_nth_ptr(&g->draw->path,j);
            for (cp_v_each(k, &p->point)) {
                cp_vec2_minmax(box, cp_v_nth_ptr(&p->point, k));
            }
        }
        cp_vec2_minmax_or(box, box, &g->dim);
    }
}

#else /* FLATTEN_POLY == 1 */
/* flattened polygon data */

static void finalise_path(
    font_t *f,
    cp_mat2w_t const *ram,
    cp_csg2_poly_t const *f,
    cp_csg2_path_t const *q)
{
    cp_font_t *c = f->final;
    cp_v_push(&c->path, c->coord.size & ~0U);
    for (cp_v_each(j, &q->point_idx)) {
        size_t i = cp_v_nth(&q->point_idx, j);
        cp_vec2_loc_t const *v = &cp_v_nth(&f->point, i);
        cp_font_xy_t *w = cp_v_push0(&c->coord);
        w->x = rasterize_x(ram, v->coord.x);
        w->y = rasterize_y(ram, v->coord.y);
    }
    cp_font_xy_t *w = cp_v_push0(&c->coord);
    w->x = CP_FONT_X_SPECIAL;
    w->y = CP_FONT_Y_END;

    finalise_find_or_save_path(f);
}

static void finalise_poly(
    cp_mat2w_t const *ram,
    font_glyph_t const *g)
{
    cp_font_t *c = g->font->final;
    cp_csg2_poly_t *f = g->final_poly;
    if ((f == NULL) || (f->path.size == 0)) {
        return;
    }

    /* construct polygon off of f */
    for (cp_v_each(i, &f->path)) {
        finalise_path(g->font, ram, f, &cp_v_nth(&f->path, i));
    }
}

static void finalise_flatten_poly(
    font_glyph_t *g)
{
    font_draw_poly_t const *p = g->draw;

    /* make list of polygons from path data */
    cp_v_obj_p_t rc = {0};
    for (cp_v_each(i, &p->path)) {
        font_draw_path_t const *q = cp_v_nth_ptr(&p->path, i);
        cp_csg2_poly_t *a = cp_csg2_new(*a, 0);
        cp_csg2_path_t *b = cp_v_push0(&a->path);
        for (cp_v_each(j, &q->point)) {
            cp_v_push(&b->point_idx, a->point.size);
            cp_vec2_loc_t *v = cp_v_push0(&a->point);
            v->coord = cp_v_nth(&q->point, j);
        }
        cp_v_push(&rc, cp_obj(a));
    }

    /* flatten */
    cp_csg_opt_t opt = CP_CSG_OPT_DEFAULT;
    cp_pool_t tmp = {0};

    g->final_poly = cp_csg2_flatten(&opt, &tmp, &rc);

    cp_pool_fini(&tmp);
}

static void finalise_prepare_glyph(
    cp_vec2_minmax_t *box,
    font_glyph_t *g)
{
    if (g->draw == NULL) {
        return;
    }

    /* get flattened glyph */
    finalise_flatten_poly(g);

    /* get bounding box */
    cp_csg2_poly_t *f = g->final_poly;
    if (f != NULL) {
        for (cp_v_each(j, &f->path)) {
            cp_csg2_path_t const *q = cp_v_nth_ptr(&f->path,j);
            for (cp_v_each(k, &q->point_idx)) {
                size_t i = cp_v_nth(&q->point_idx, k);
                cp_vec2_minmax(box, &cp_v_nth(&f->point, i).coord);
            }
        }
    }
    cp_vec2_minmax_or(box, box, &g->dim);
}

#endif

static unsigned finalise_get_kern(
    font_glyph_t const *g,
    double x)
{
    if ((x < 0) || (x > g->font->kern_max)) {
        return CP_FONT_PROFILE_COUNT-1;
    }
    long l = lrint((x / g->font->kern_max) * (CP_FONT_PROFILE_COUNT - 1));
    assert(l >= 0);
    assert(l < CP_FONT_PROFILE_COUNT);
    return l & 0xffff;
}

static void finalise_glyph_draw(
    cp_font_glyph_t *k,
    cp_font_t *c,
    cp_mat2w_t const *ram,
    font_glyph_t const *g)
{
    assert(k->id == g->unicode.code_point);
    assert(c->path.size <= CP_FONT_ID_MASK);
    k->first = c->path.size & CP_FONT_ID_MASK;

    cp_font_path_t *p;
    for (size_t i = 0; i < sizeof(*p)/sizeof(c->path.data[0]); i++) {
        cp_v_push(&c->path, 0);
    }
    p = (cp_font_path_t*)cp_v_nth_ptr(&c->path, k->first);
    p->border_x.left  = rasterize_x(ram, g->dim.min.x);
    p->border_x.right = rasterize_x(ram, g->dim.max.x);

    /* set the TALL flag if the glyph occupies anything above the xhi line
     * (with some slack). */
    font_def_t const *fd = g->font->def;
    if (g->box.max.y >= (coord_y_abs(fd, fd->xhi_y) + (1.5 * fd->line_width[0]))) {
        k->flags |= CP_FONT_GF_TALL;
    }
    if (g->def->mono) {
        k->flags |= CP_FONT_GF_MONO;
    }

    /* set kerning info: empty glyphs chars have no kerning (kept at all 0). */
    bool have_kern = false;
    for (cp_arr_each(i, g->coll_box)) {
        minmax_t const *cb = &g->coll_box[i];
        if ((cb->lo >= 0) || (cb->hi >= 0)) {
            have_kern = true;
            break;
        }
    }
    if (have_kern) {
        for (cp_arr_each(i, g->coll_box)) {
            minmax_t const *cb = &g->coll_box[i];
            p->profile.x[i] = CP_FONT_PROFILE(
                finalise_get_kern(g, cb->lo),
                finalise_get_kern(g, cb->hi));
        }
    }

    size_t path_a = c->path.size;
    finalise_poly(ram, g);
    size_t path_z = c->path.size;

    size_t count = path_z - path_a;
    assert(count <= CP_FONT_ID_MASK);
    k->second = count & CP_FONT_ID_MASK;
}

static void finalise_glyph(
    cp_font_t *c,
    cp_mat2w_t const *ram,
    font_glyph_t const *g);

static void finalise_glyph_sequence(
    cp_font_glyph_t *k,
    cp_font_t *c,
    cp_mat2w_t const *ram,
    font_glyph_t const *g)
{
    font_draw_sequence_t const *d = font_draw_cast(*d, g->def->draw);
    font_subglyph_t const *sg0 = cp_v_nth(&d->seq, 0);
    if ((d->seq.size == 1) && cp_eq(sg0->kern,0)) {
        /* This is an equivalence that is implemented by pointing at
         * the same glyph data. */
        font_glyph_t *same = find_glyph(g, &sg0->unicode);
        finalise_glyph(c, ram, same);
        k->flags = same->final->flags;
        k->first = same->final->first;
        k->second = same->final->second;
        return;
    }

    /* first generate the sub-glyphs (all of them, so c->path is filled) */
    for (cp_v_each(i, &d->seq)) {
        font_subglyph_t const *sgi = cp_v_nth(&d->seq, i);
        assert(sgi->unicode.code_point <= CP_FONT_ID_MASK);
        font_glyph_t *sg = find_glyph(g, &sgi->unicode);
        finalise_glyph(c, ram, sg);
    }

    /* generate reference list */
    k->flags |= CP_FONT_GF_SEQUENCE;
    k->first =  c->path.size & CP_FONT_ID_MASK;
    k->second = d->seq.size  & CP_FONT_ID_MASK;
    for (cp_v_each(i, &d->seq)) {
        font_subglyph_t const *sgi = cp_v_nth(&d->seq, i);
        assert(sgi->unicode.code_point <= CP_FONT_ID_MASK);

        font_glyph_t *sg = find_glyph(g, &sgi->unicode);
        if (sg->final->flags & CP_FONT_GF_TALL) {
            k->flags |= CP_FONT_GF_TALL;
        }

        if ((sgi->kern > g->font->em) || (sgi->kern < -g->font->em)) {
            die(g, g->font, "seq kerning out of range: %g, expected -em..+em, where em=%g",
                sgi->kern, g->font->em);
        }
        long ki = lrint((sgi->kern / g->font->em) * CP_FONT_KERN_EM_MASK);

        cp_font_subglyph_t *fgl = (cp_font_subglyph_t*)cp_v_push0(&c->path);
        fgl->glyph = cp_v_idx(&g->font->glyph, sg) & CP_FONT_ID_MASK;

        unsigned long kiu = (unsigned long)labs(ki);
        fgl->kern_em = kiu & CP_FONT_KERN_EM_MASK;
        fgl->kern_sub = ki < 0;
    }
}

static void finalise_glyph(
    cp_font_t *c,
    cp_mat2w_t const *ram,
    font_glyph_t const *g)
{
    cp_font_glyph_t *k = g->final;
    if (k->id != 0) {
        return;
    }
    assert(g->unicode.code_point <= CP_FONT_ID_MASK);
    k->id = (g->unicode.code_point & CP_FONT_ID_MASK);

    if ((g->draw == NULL) &&
        (g->def->draw != NULL) &&
        (g->def->draw->type == FONT_DRAW_SEQUENCE))
    {
        finalise_glyph_sequence(k, c, ram, g);
        return;
    }

    assert(g->draw != NULL);
    finalise_glyph_draw(k, c, ram, g);
}

static int cmp_font_map1(
    cp_font_map_t const *a,
    cp_font_map_t const *b,
    void *u CP_UNUSED)
{
    if (a->first != b->first) {
        return a->first < b->first ? -1 : +1;
    }
    return 0;
}

static int cmp_font_map2(
    cp_font_map_t const *a,
    cp_font_map_t const *b,
    void *u CP_UNUSED)
{
    if (a->first != b->first) {
        return a->first < b->first ? -1 : +1;
    }
    if (a->second != b->second) {
        return a->second < b->second ? -1 : +1;
    }
    return 0;
}

static int cmp_per_lang(
    font_def_map_t const *a,
    font_def_map_t const *b,
    void *u CP_UNUSED)
{
    assert(a->lang != NULL);
    assert(b->lang != NULL);
    int i = strcmp(a->lang, b->lang);
    if (i != 0) {
        return i;
    }
    if (a->a.code_point != b->a.code_point) {
        return a->a.code_point < b->a.code_point ? -1 : +1;
    }
    if (a->b.code_point != b->b.code_point) {
        return a->b.code_point < b->b.code_point ? -1 : +1;
    }
    return 0;
}

static unsigned mof_flags_from_type(
    unsigned type)
{
    switch (type) {
    case MAP_TYPE_MANDATORY:      return CP_FONT_MOF_MANDATORY;
    case MAP_TYPE_LIGATURE:       return CP_FONT_MOF_LIGATURE;
    case MAP_TYPE_JOINING:        return CP_FONT_MOF_JOINING;
    case MAP_TYPE_OPTIONAL:       return CP_FONT_MOF_OPTIONAL;
    case MAP_TYPE_MANDATORY_KEEP: return CP_FONT_MOF_MANDATORY | CP_FONT_MOF_KEEP_SECOND;
    case MAP_TYPE_LIGATURE_KEEP:  return CP_FONT_MOF_LIGATURE  | CP_FONT_MOF_KEEP_SECOND;
    case MAP_TYPE_JOINING_KEEP:   return CP_FONT_MOF_JOINING   | CP_FONT_MOF_KEEP_SECOND;
    case MAP_TYPE_OPTIONAL_KEEP:  return CP_FONT_MOF_OPTIONAL  | CP_FONT_MOF_KEEP_SECOND;

    default:
        CP_DIE("Unexpected type for MOF table.");
    }
}

static int cmp_comp_equiv1(
    unsigned const *_a,
    unsigned const *const *_b,
    void *u CP_UNUSED)
{
    unsigned a = *_a;
    unsigned b = (*_b)[1];
    if (a != b) {
        return a < b ? -1 : +1;
    }
    return 0;
}

static unsigned equiv_decompose(
    unsigned const **seq,
    unsigned cp)
{
    cp_a_unsigned_const_p_t a =
        CP_A_INIT_WITH(unicode_comp_equiv, cp_countof(unicode_comp_equiv));
    size_t i = cp_v_bsearch(&cp, &a, cmp_comp_equiv1, NULL);
    unsigned const *c = cp_v_nth0(&a, i);
    if (c == NULL) {
        return 0;
    }
    assert(c[1] == cp);
    assert(c[0] >= 1);
    assert(c[0] <= 2);
    *seq = &c[2];
    return c[0];
}

static bool have_glyph_aux(
    font_t const *f,
    unsigned cp,
    bool combining)
{
    font_glyph_t const *g = find_glyph0(f, cp);
    if (g != NULL) {
        if (combining &&
            (g->def->high_above.code_point == 0) &&
            (!g->def->is_below))
        {
            return false;
        }
        return true;
    }

    unsigned const *seq;
    unsigned cnt = equiv_decompose(&seq, cp);
    if (cnt == 0) {
        return false;
    }
    if (!have_glyph_aux(f, seq[0], combining)) {
        return false;
    }
    for (size_t j = 1; j < cnt; j++) {
        if (!have_glyph_aux(f, seq[j], true)) {
            return false;
        }
    }
    return true;
}

/**
 * Checks whether the glyph is either present or can be automatically
 * combined by the combining above/below mechanism.  This will check that
 * all glyphs of the equivalence decomposition exist in the font, and that
 * all but the first have a combining class set, which indicates that the
 * font assumes that it can handle the diacritic generically.
 *
 * FIXME (MAYBE): This is currently overly optimistic for multi-diacritic stuff,
 * which, for working properly, would require the font to define a combined
 * diacritic as the composer can only handle a single diacritic above and
 * below.  E.g. for 'S WITH ACUTE AND DOT ABOVE', even if ACUTE and DOT ABOVE
 * are supported, this only works if the font has a combined
 * 'ACUTE WITH DOT ABOVE' diacritic.
 * Well, this is only used for building the decomposition table, where it is
 * OK if more is decomposed than what actually works, and for the coverage
 * tables, which now might look optimistic.  There are a few cases where we
 * need to check this manually and try to fulfil what is claimed (e.g.,
 * Vietnamese).
 */
static bool have_glyph(
    font_t const *f,
    unsigned cp)
{
    return have_glyph_aux(f, cp, false);
}

static void add_lang_map1(
    cp_font_t *c,
    char const *cur_lang,
    size_t lang_idx)
{
    cp_font_lang_map_t *m = cp_v_push0(&c->lang_map);

    assert(strlen(cur_lang) <= sizeof(m->id));
    strncpy(m->id, cur_lang, sizeof(m->id));
    assert(lang_idx <= 0x7fffffff);
    m->lang_idx = lang_idx & 0x7fffffff;
}

static int cmp_lang_name(
    char const *a,
    lang_name_t const *b,
    void *u CP_UNUSED)
{
    return strcmp(a, b->ott);
}

static void add_lang_map(
    cp_font_t *c,
    char const *name,
    size_t lang_idx)
{
    add_lang_map1(c, name, lang_idx);

    size_t i = cp_v_bsearch(name, &lang_name, cmp_lang_name, NULL);
    if (cp_v_nth_ptr0(&lang_name, i) == NULL) {
        return;
    }

    /* find first entry */
    for (;;) {
        lang_name_t const *p = cp_v_nth_ptr0(&lang_name, i - 1);
        if ((p == NULL) || !strequ(p->ott, name)) {
            break;
        }
        i--;
    }

    /* add each entry until we reach one that is a different language */
    for (;;) {
        lang_name_t const *p = cp_v_nth_ptr0(&lang_name, i);
        if ((p == NULL) || !strequ(p->ott, name)) {
            break;
        }
        add_lang_map1(c, p->iso, lang_idx);
        i++;
    }
}

static int cmp_lang_map2(
    cp_font_lang_map_t const *a,
    cp_font_lang_map_t const *b,
    void *u CP_UNUSED)
{
    for (cp_arr_each(i, a->id)) {
        if (a->id[i] != b->id[i]) {
            return a->id[i] < b->id[i] ? -1 : +1;
        }
    }
    return 0;
}

static void finalise_font(
    cp_font_t *c,
    font_t *f)
{
    f->final = c;

    c->name = strcpy(malloc(f->name.size + 1), f->name.data);

    c->family_name = f->family_name;

    c->weight_name = f->def->weight_name;
    c->slope_name = f->def->slope_name;
    c->stretch_name = f->def->stretch_name;
    c->size_name = f->def->size_name;

    if (c->weight_name == NULL) {
        c->weight_name = "Book";
    }
    if (c->slope_name == NULL) {
        c->slope_name = "Roman";
    }
    if (c->stretch_name == NULL) {
        c->stretch_name = "Regular";
    }
    if (c->size_name == NULL) {
        c->size_name = "Normal";
    }

    c->weight = f->def->weight;
    c->slope = f->def->slope;
    c->stretch = f->def->stretch;
    c->min_size = f->def->min_size;
    c->max_size = f->def->max_size;

    /* get min/max coordinate globally; we compute it again for each
     * glyph so that slanting is considered. */
    cp_vec2_minmax_t box = CP_VEC2_MINMAX_EMPTY;
    for (cp_v_each(i, &f->glyph)) {
        font_glyph_t *g = &cp_v_nth(&f->glyph, i);
        // fprintf(stderr, "Info: U+%04X...\n", g->unicode.code_point);
        finalise_prepare_glyph(&box, g);
    }
    cp_vec2_minmax(&box, (cp_vec2_t[]){{{ box.min.x + f->em, f->base_y }}});
    cp_vec2_minmax(&box, (cp_vec2_t[]){{{ 0, f->top_y    }}});
    cp_vec2_minmax(&box, (cp_vec2_t[]){{{ 0, f->bottom_y }}});
    assert(cp_vec2_minmax_valid(&box));

    /* compute coordinate mapping matrix */
    cp_mat2w_t ram = CP_MAT2W(1,0,0, 0,1,0);
    ram.b.m[0][0] = 0xfffe / (box.max.x - box.min.x);
    ram.w.v[0] = -box.min.x * ram.b.m[0][0];
    ram.b.m[1][1] = 0xfffe / (box.max.y - box.min.y);
    ram.w.v[1] = -box.min.y * ram.b.m[1][1];

    assert(rasterize_x(&ram, box.min.x) == 0);
    assert(rasterize_x(&ram, box.max.x) == 0xfffe);
    assert(rasterize_y(&ram, box.min.y) == 0);
    assert(rasterize_y(&ram, box.max.y) == 0xfffe);

    /* dimensions */
    c->center_x = rasterize_x(&ram, coord_x_abs(f->def, 0));
    c->em_x     = rasterize_x(&ram, f->em + box.min.x);
    c->em_y     = rasterize_y(&ram, f->em + box.min.y);
    c->top_y    = rasterize_y(&ram, f->top_y);
    c->bottom_y = rasterize_y(&ram, f->bottom_y);
    c->base_y   = rasterize_y(&ram, f->base_y);
    c->cap_y    = rasterize_y(&ram, f->cap_y);
    c->xhi_y    = rasterize_y(&ram, f->xhi_y);
    c->dec_y    = rasterize_y(&ram, f->dec_y);

    /* step table for kerning: we use em/2 as a maximum amount of kerning */
    f->kern_max = f->em/2;
    double ai = f->kern_max / (cp_countof(c->space_x) - 1);
    for (cp_arr_each(i, c->space_x)) {
        double a = cp_f(i) * ai;
        c->space_x[i] = rasterize_x(&ram, a + box.min.x);
    }

    /* glyphs */
    cp_v_init0(&c->glyph, f->glyph.size);
    for (cp_v_each(i, &f->glyph)) {
        cp_v_nth(&f->glyph, i).final = cp_v_nth_ptr(&c->glyph, i);
    }
    for (cp_v_each(i, &f->glyph)) {
        finalise_glyph(c, &ram, &cp_v_nth(&f->glyph, i));
    }

    /* (1) Equivalencs compositions for composed glyphs that the font has.
     * (2) Equivalence decompositions for glyphs that the font does not have,
     *     but for which it has all the components. */
    for (cp_arr_each(i, unicode_comp_equiv)) {
        unsigned const *comp = unicode_comp_equiv[i];
        assert(comp[0] <= 2);
        /* Only 1:2 mappings are handled in compositions. */
        /* 1:1 mappings are handled as decompositions. */
        font_glyph_t *g = find_glyph0(f, comp[1]);
        if (g != NULL) {
            assert(comp[2] <= CP_FONT_ID_MASK);
            assert(comp[3] <= CP_FONT_ID_MASK);
            assert(comp[1] <= CP_FONT_ID_MASK);
            if (comp[0] == 2) {
                /* have glyph => compose */
                cp_font_map_t *m = cp_v_push0(&c->compose);
                m->first =  comp[2] & CP_FONT_ID_MASK;
                m->second = comp[3] & CP_FONT_ID_MASK;
                m->result = comp[1] & CP_FONT_ID_MASK;
            }
            else
            if (find_glyph0(f, comp[2]) == NULL) {
                /* warn that an easy opportunity to have a glyph is missed */
                die(g, f,
                    "decomposes into U+%04X; font has former but not latter.\n",
                    comp[2]);
            }
        }
        else
        if (have_glyph(f, comp[2])) {
            if (comp[0] == 1) {
                cp_font_map_t *m = cp_v_push0(&c->decompose);
                m->first =  comp[1] & CP_FONT_ID_MASK;
                m->result = comp[2] & CP_FONT_ID_MASK;
            }
            else
            if (have_glyph(f, comp[3])) {
                /* have components => decompose */
                cp_font_map_t *m = cp_v_push0(&c->decompose);
                m->first =  comp[1] & CP_FONT_ID_MASK;
                m->result = comp[2] & CP_FONT_ID_MASK;
                m->second = comp[3] & CP_FONT_ID_MASK;
            }
        }
    }

    /* composition from font data */
    font_v_def_map_t per_lang = {0};
    for (cp_v_each(i, &f->glyph)) {
        font_glyph_t const *g = &cp_v_nth(&f->glyph, i);
        assert(g->unicode.code_point <= CP_FONT_ID_MASK);

        if (g->def->high_above.code_point != 0) {
            assert(!g->def->is_below);
            assert(g->def->high_above.code_point <= CP_FONT_ID_MASK);
            cp_font_map_t *m = cp_v_push0(&c->comb_type);
            m->first = g->unicode.code_point & CP_FONT_ID_MASK;
            m->result = CP_FONT_CT_ABOVE;
            m->second = g->def->high_above.code_point & CP_FONT_ID_MASK;
        }
        if (g->def->is_below) {
            assert(g->def->high_above.code_point == 0);
            cp_font_map_t *m = cp_v_push0(&c->comb_type);
            m->first = g->unicode.code_point & CP_FONT_ID_MASK;
            m->result = CP_FONT_CT_BELOW;
        }

        for (cp_v_each(j, &g->def->map)) {
            font_def_map_t *comp = cp_v_nth_ptr(&g->def->map, j);
            comp->glyph = g;
            switch (comp->type) {
            case MAP_TYPE_CANON:{
                if (comp->lang != NULL) {
                    die(g, g->font, "No language specific canonical replacement is possible");
                }
                assert(comp->a.code_point <= CP_FONT_ID_MASK);
                assert(comp->b.code_point <= CP_FONT_ID_MASK);
                cp_font_map_t *m = cp_v_push0(&c->compose);
                m->first =  comp->a.code_point    & CP_FONT_ID_MASK;
                m->second = comp->b.code_point    & CP_FONT_ID_MASK;
                m->result = g->unicode.code_point & CP_FONT_ID_MASK;
                break;}

            case MAP_TYPE_BASE_REPLACE:{
                if (comp->lang != NULL) {
                    die(g, g->font, "No language specific base replacement is possible");
                }
                assert(comp->a.code_point <= CP_FONT_ID_MASK);
                cp_font_map_t *m = cp_v_push0(&c->base_repl);
                m->first =  comp->a.code_point & CP_FONT_ID_MASK;
                m->second = comp->value & CP_FONT_ID_MASK;
                m->result = g->unicode.code_point & CP_FONT_ID_MASK;
                break;}

            case MAP_TYPE_MANDATORY:
            case MAP_TYPE_JOINING:
            case MAP_TYPE_LIGATURE:
            case MAP_TYPE_OPTIONAL:
            case MAP_TYPE_MANDATORY_KEEP:
            case MAP_TYPE_JOINING_KEEP:
            case MAP_TYPE_LIGATURE_KEEP:
            case MAP_TYPE_OPTIONAL_KEEP:
                if (comp->lang == NULL) { // global
                    assert(comp->a.code_point <= CP_FONT_ID_MASK);
                    assert(comp->b.code_point <= CP_FONT_ID_MASK);
                    cp_font_map_t *m = cp_v_push0(&c->optional);
                    m->flags = mof_flags_from_type(comp->type) & CP_FONT_FLAG_MASK;
                    m->first =  comp->a.code_point    & CP_FONT_ID_MASK;
                    m->second = comp->b.code_point    & CP_FONT_ID_MASK;
                    m->result = g->unicode.code_point & CP_FONT_ID_MASK;
                }
                else {
                    /* lang specific combination */
                    cp_v_push(&per_lang, *comp);
                }
                break;

            case MAP_TYPE_KERNING:{
                assert((comp->lang == NULL) && "cannot have language specific kerning");
                assert(comp->a.code_point <= CP_FONT_ID_MASK);
                cp_font_map_t *m = cp_v_push0(&c->context);
                (void)find_glyph(g, &comp->a);
                m->flags |= CP_FONT_MXF_KERNING;
                m->first  = g->unicode.code_point & CP_FONT_ID_MASK; /* current */
                m->second = comp->a.code_point    & CP_FONT_ID_MASK; /* prev */
                long k =
                    rasterize_x_long(&ram, comp->amount) -
                    rasterize_x_long(&ram, 0);
                long lo = -(1L << (CP_FONT_ID_WIDTH-1));
                long hi = +(1L << (CP_FONT_ID_WIDTH-1));
                if ((k < lo) || (k > hi)) {
                    die (g, g->font, "Kerning out of range: %g becomes %ld, range is %ld..%ld\n",
                        comp->amount, k, lo, hi);
                }
                m->result = ((unsigned)k) & CP_FONT_ID_MASK;
                break;}

            case MAP_TYPE_CONTEXT:{
                assert((comp->lang == NULL) && "cannot have lang specific context substitution");
                assert(comp->a.code_point <= CP_FONT_ID_MASK);
                assert(comp->b.code_point <= CP_FONT_ID_MASK);
                cp_font_map_t *m = cp_v_push0(&c->context);
                (void)find_glyph(g, &comp->a);
                (void)find_glyph(g, &comp->b);
                m->result = g->unicode.code_point & CP_FONT_ID_MASK; /* substitution */
                m->first  = comp->b.code_point    & CP_FONT_ID_MASK; /* current */
                m->second = comp->a.code_point    & CP_FONT_ID_MASK; /* prev */
                break;}

            case MAP_TYPE_REPLACE:
                assert((comp->lang != NULL) && "cannot have global replacement");
                cp_v_push(&per_lang, *comp);
                break;
            }
        }
    }

    /* sort */
    cp_v_qsort(&c->decompose, 0, CP_SIZE_MAX, cmp_font_map1, NULL);
    cp_v_qsort(&c->compose,   0, CP_SIZE_MAX, cmp_font_map2, NULL);
    cp_v_qsort(&c->optional,  0, CP_SIZE_MAX, cmp_font_map2, NULL);
    cp_v_qsort(&c->comb_type, 0, CP_SIZE_MAX, cmp_font_map1, NULL);
    cp_v_qsort(&c->context,   0, CP_SIZE_MAX, cmp_font_map2, NULL);
    cp_v_qsort(&c->base_repl, 0, CP_SIZE_MAX, cmp_font_map2, NULL);

#if 0
    /* Make decompose entries unique...  Currently not needed because
     * we have no manually defined decompositions. */
#endif

    /* Make compose entries unique the composition map in case entries are
     * redundantly given in unicodedata and font data or duplicate in font
     * data.  Also check that there are no contradictions. */
    if (c->compose.size > 0) {
        size_t last = 0;
        for (cp_v_each(i, &c->compose, 1)) {
            assert(last != i);
            cp_font_map_t *a = cp_v_nth_ptr(&c->compose, last);
            cp_font_map_t *b = cp_v_nth_ptr(&c->compose, i);
            if (cmp_font_map2(a, b, NULL) != 0) {
                last++;
            }
            else {
                if (a->result != b->result) {
                    die(NULL, f,
                        "Equiv mapping is ambiguous:\n"
                        "    U+%04X + U+%04X => U+%04X or U+%04X\n",
                        a->first,
                        a->second,
                        a->result,
                        b->result);
                }
            }
        }
        c->compose.size = last + 1;
    }

    /* sort language specific stuff */
    cp_v_qsort(&per_lang, 0, CP_SIZE_MAX, cmp_per_lang, NULL);

    /* process language specific stuff lang by lang (the array is sorted,
     * so it can be split easily) */
    char const *cur_lang = "";
    cp_font_lang_t *lang = NULL;
    for (cp_v_each(i, &per_lang)) {
        font_def_map_t *comp = cp_v_nth_ptr(&per_lang, i);

        if (!strequ(comp->lang, cur_lang)) {
            /* new entry for other language */
            cur_lang = comp->lang;

            size_t lang_idx = c->lang.size;
            lang = cp_v_push0(&c->lang);

            add_lang_map(c, cur_lang, lang_idx);
        }

        switch (comp->type) {
            case MAP_TYPE_MANDATORY:
            case MAP_TYPE_JOINING:
            case MAP_TYPE_LIGATURE:
            case MAP_TYPE_OPTIONAL:
            case MAP_TYPE_MANDATORY_KEEP:
            case MAP_TYPE_JOINING_KEEP:
            case MAP_TYPE_LIGATURE_KEEP:
            case MAP_TYPE_OPTIONAL_KEEP: {
                assert(comp->a.code_point  <= CP_FONT_ID_MASK);
                assert(comp->b.code_point  <= CP_FONT_ID_MASK);
                assert(comp->glyph->unicode.code_point <= CP_FONT_ID_MASK);
                (void)find_glyph(comp->glyph, &comp->a);
                (void)find_glyph(comp->glyph, &comp->b);
                cp_font_map_t *m = cp_v_push0(&lang->optional);
                m->flags = mof_flags_from_type(comp->type) & CP_FONT_FLAG_MASK;
                m->first =  comp->a.code_point  & CP_FONT_ID_MASK;
                m->second = comp->b.code_point  & CP_FONT_ID_MASK;
                m->result = comp->glyph->unicode.code_point & CP_FONT_ID_MASK;
                break;}

            case MAP_TYPE_REPLACE: {
                assert(comp->a.code_point  <= CP_FONT_ID_MASK);
                assert(comp->b.code_point  <= CP_FONT_ID_MASK);
                assert(comp->glyph->unicode.code_point <= CP_FONT_ID_MASK);
                (void)find_glyph(comp->glyph, &comp->a);
                cp_font_map_t *m = cp_v_push0(&lang->one2one);
                m->first =  comp->glyph->unicode.code_point & CP_FONT_ID_MASK;
                m->result = comp->a.code_point  & CP_FONT_ID_MASK;
                break;}

            case MAP_TYPE_CANON:
            case MAP_TYPE_KERNING:
            case MAP_TYPE_CONTEXT:
            case MAP_TYPE_BASE_REPLACE:
                CP_DIE();
        }
    }

    /* sort language map */
    cp_v_qsort(&c->lang_map, 0, CP_SIZE_MAX, cmp_lang_map2, NULL);

}

static void finalise_family(
    cp_v_font_p_t *cpfont,
    font_v_font_p_t const *vfont)
{
    for (cp_v_each(i, vfont)) {
        cp_font_t *c = CP_NEW(*c);
        cp_v_push(cpfont, c);
        finalise_font(c, cp_v_nth(vfont, i));
    }
}

/* ********************************************************************** */
/* PS generator */

#define PS_INCH(x) ((x) * 72)
#define PS_CM(x)   ((PS_INCH(x) * 100)/ 254)
#define PS_MM(x)   (PS_CM(x) / 10)

#define PS_PAPER_NAME "a4"
#define PS_PAPER_X 595
#define PS_PAPER_Y 842
#define PS_PAPER_MARGIN_X PS_MM(10)
#define PS_PAPER_MARGIN_Y PS_MM(10)

#define PS_GRID_MARGIN_X PS_PAPER_MARGIN_X
#define PS_GRID_MARGIN_Y (PS_PAPER_MARGIN_Y + PS_MM(15))

#define PS_GRID_X_ ((PS_PAPER_X - (2*PS_GRID_MARGIN_X)) / 16)
#define PS_GRID_Y_ ((PS_PAPER_Y - (2*PS_GRID_MARGIN_Y)) / 16)

#if (PS_GRID_X_ * 3) < (PS_GRID_Y_ * 2)
#  define PS_GRID_X PS_GRID_X_
#  define PS_GRID_Y ((PS_GRID_X * 3) / 2)
#else
#  define PS_GRID_Y PS_GRID_Y_
#  define PS_GRID_X ((PS_GRID_Y * 2) / 3)
#endif

typedef struct {
    FILE *f;
    size_t page;
    bool in_page;
} ps_t;

static void ps_doc_begin(
    ps_t *ps,
    FILE *f)
{
    *ps = (ps_t){0};
    ps->f = f;
    fprintf(f,
        "%%!PS-Adobe-3.0\n"
        "%%%%Creator: hob3l fontgen\n"
        "%%%%Orientation: Portrait\n"
        "%%%%Pages: atend\n"
        "%%%%BoundingBox: 0 0 %u %u\n"
        "%%%%DocumentPaperSizes: "PS_PAPER_NAME"\n"
        "%%Magnification: 1.0000\n"
        "%%%%EndComments\n",
        PS_PAPER_X, PS_PAPER_Y);
}

static void ps_doc_end(ps_t *ps)
{
    fprintf(ps->f,
        "%%%%Trailer\n"
        "%%%%Pages: %"CP_Z"u\n"
        "%%%%EOF\n",
        ps->page);
}

static void ps_page_end(ps_t *ps)
{
    if(!ps->in_page) {
        return;
    }
    ps->in_page = false;
    fprintf(ps->f,
        "restore\n"
        "showpage\n");
}

static void ps_page_begin(ps_t *ps, char const *label)
{
    if (ps->in_page) {
        ps_page_end(ps);
    }
    ps->page++;
    ps->in_page = true;
    if (label != NULL) {
        fprintf(ps->f, "%%%%Page: %s %"CP_Z"u\n", label, ps->page);
    }
    else {
        fprintf(ps->f, "%%%%Page: %"CP_Z"u %"CP_Z"u\n", ps->page, ps->page);
    }

    fprintf(ps->f,
        "save\n"
        "1 setlinecap\n"
        "1 setlinejoin\n"
        "1 setlinewidth\n"
        "0 setgray\n");
}

/* ********************************************************************** */
static void ps_glyph_draw(
    ps_t *ps,
    double x,
    double y,
    font_glyph_t const *glyph);

static void ps_glyph_sequence(
    ps_t *ps,
    double x,
    double y,
    font_glyph_t const *glyph,
    font_draw_t const *draw)
{
    font_draw_sequence_t const *d = font_draw_cast(*d, draw);

    x += glyph->dim.min.x;
    for (cp_v_each(i, &d->seq)) {
        font_subglyph_t const *sgi = cp_v_nth(&d->seq, i);
        font_glyph_t const *sg = find_glyph(glyph, &sgi->unicode);

        /* kern */
        x += sgi->kern;

        /* render */
        x -= sg->dim.min.x;
        ps_glyph_draw(ps, x, y, sg);
        x += sg->dim.max.x;
    }
}

static void ps_glyph_draw(
    ps_t *ps,
    double x,
    double y,
    font_glyph_t const *glyph)
{
    font_draw_poly_t const *draw = glyph->draw;

    if ((draw == NULL) &&
        (glyph->def->draw != NULL) &&
        (glyph->def->draw->type == FONT_DRAW_SEQUENCE))
    {
        ps_glyph_sequence(ps, x, y, glyph, glyph->def->draw);
        return;
    }
    assert(draw != NULL);

    for (cp_v_each(i, &draw->path)) {
        fprintf(ps->f, "    newpath");
        font_draw_path_t *path = cp_v_nth_ptr(&draw->path, i);
        char const *cmd = "moveto";
        for (cp_v_each(j, &path->point)) {
            cp_vec2_t *p = cp_v_nth_ptr(&path->point, j);
            fprintf(ps->f, " %g %g %s", p->x + x, p->y + y, cmd);
            cmd = "lineto";
        }
        fprintf(ps->f, " closepath fill\n");
    }
}

/**
 * Move to top left corner of grid box for x=0..15
 * Add .5 for center.  Add 1 for next grid line.
 */
static double ps_coord_grid_x(double x)
{
    assert(x >= 0);
    assert(x <= 16);
    return PS_PAPER_X/2 + ((x - 8) * PS_GRID_X);
}

/**
 * Move to top left corner of grid box for y=0..15
 * Add .5 for center.  Add 1 for next grid line.
 */
static double ps_coord_grid_y(double y)
{
    assert(y >= 0);
    assert(y <= 16);
    return PS_PAPER_Y/2 + ((8 - y) * PS_GRID_Y);
}

static void ps_line(ps_t *ps, double x1, double y1, double x2, double y2)
{
    fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
        x1, y1, x2, y2);
}

static void ps_chart_grid(ps_t *ps, char const *label)
{
    double x0  = ps_coord_grid_x(0);
    double x16 = ps_coord_grid_x(16);
    double y0  = ps_coord_grid_y(0);
    double y16 = ps_coord_grid_y(16);
    fprintf(ps->f, "2 setlinewidth\n");
    ps_line(ps, x0,  y0 + PS_MM(6), x0,  y16);
    ps_line(ps, x16, y0 + PS_MM(6), x16, y16);
    ps_line(ps, x0,  y0,  x16, y0);
    ps_line(ps, x0,  y16, x16, y16);
    fprintf(ps->f, "1 setlinewidth\n");
    for (cp_size_each(i, 16, 1)) {
        double x = ps_coord_grid_x(cp_f(i));
        double y = ps_coord_grid_y(cp_f(i));
        ps_line(ps, x0, y,  x16, y);
        ps_line(ps, x,  y0, x,   y16);
    }

    fprintf(ps->f, "/Helvetica findfont 10 scalefont setfont\n");
    for (cp_size_each(i, 16)) {
        fprintf(ps->f, "%g %g moveto (%s%"CP_Z"X) dup stringwidth pop neg 2 div 0 rmoveto show\n",
            ps_coord_grid_x(cp_f(i) + .5), y0 + PS_MM(3),
            label, i);
        fprintf(ps->f, "%g %g moveto (%"CP_Z"X) dup stringwidth pop neg 0 rmoveto show\n",
            x0 - PS_MM(3), ps_coord_grid_y(cp_f(i)+0.5) - PS_MM(2),
            i);
    }

    fprintf(ps->f, "/Helvetica findfont 7 scalefont setfont\n");
    for (cp_size_each(x, 16)) {
        for (cp_size_each(y, 16)) {
            fprintf(ps->f,
                "%g %g moveto (%s%"CP_Z"X%"CP_Z"X) dup stringwidth pop neg 2 div 0 rmoveto show\n",
                ps_coord_grid_x(cp_f(x) + .5), ps_coord_grid_y(cp_f(y) + 1) + PS_MM(1.2),
                label, x, y);
        }
    }
}

static void ps_render_path(
    ps_t *ps,
    double x,
    double y,
    cp_csg2_poly_t *p,
    cp_csg2_path_t *q)
{
    fprintf(ps->f, "newpath");
    char const *cmd = "moveto";
    for (cp_v_each(i, &q->point_idx)) {
        size_t j = cp_v_nth(&q->point_idx, i);
        cp_vec2_loc_t const *v = cp_v_nth_ptr(&p->point, j);
        fprintf(ps->f, " %g %g %s", x + v->coord.x, y + v->coord.y, cmd);
        cmd = "lineto";
    }
    fprintf(ps->f, " closepath fill\n");
}

static void ps_render_poly(
    ps_t *ps,
    double x,
    double y,
    cp_csg2_poly_t *p)
{
    for (cp_v_each(i, &p->path)) {
        ps_render_path(ps, x, y, p, cp_v_nth_ptr(&p->path, i));
    }
}

static void ps_render_v_poly(
    ps_t *ps,
    double x,
    double y,
    cp_v_obj_p_t const *v)
{
    for (cp_v_each(i, v)) {
        cp_csg2_poly_t *p = cp_csg2_cast(*p, cp_v_nth(v,i));
        ps_render_poly(ps, x, y, p);
    }
}

static size_t ps_chart_font(
    ps_t *ps,
    font_t const *font)
{
    unsigned prev_page = ~0U;
    size_t count = 0;
    for (unsigned cp = 0; cp <= 0x10ffff; cp++) {
         /* skip huge block of unassigned characters (once this
          * starts being assignned, we'll notice...) */
         if ((cp >= 0x30000) && (cp < 0xe0000)) {
             /* spare ~3s of runtime... */
             continue;
         }
         if (!have_glyph(font, cp)) {
             continue;
         }
         count++;
         unsigned page   = cp >> 8;
         unsigned grid_x = (cp & 0xf0) >> 4;
         unsigned grid_y = (cp & 0x0f);
         if (page != prev_page) {
             char label[20];
             snprintf(label, sizeof(label), "%02X", page);
             ps_page_begin(ps, label);
             ps_chart_grid(ps, label);
         }

         fprintf(ps->f, "save\n");
         fprintf(ps->f, "%g %g translate\n",
             ps_coord_grid_x(grid_x + 0.5),
             ps_coord_grid_y(grid_y + 0.55));
         cp_font_gc_t gc = {0};
         cp_font_gc_set_font(&gc, font->final, 20, 1);

         unsigned str[2];
         str[0] = cp;
         str[1] = 0;
         cp_v_obj_p_t out = {0};
         cp_font_print_str32(&out, &gc, str);

         fprintf(ps->f, "%g 0 translate\n", -gc.state.cur_x/2);
         ps_render_v_poly(ps, 0, 0, &out);

         fprintf(ps->f, "restore\n");

         prev_page = page;
    }
    ps_page_end(ps);
    return count;
}

/* ********************************************************************** */

#define CXY(f,X,Y) slant_x(f, (X), (Y)), (Y)

static void ps_detail_grid(
    ps_t *ps,
    font_t const *font,
    font_def_t const *def,
    font_glyph_t const *glyph,
    cp_vec2_minmax_t const *box,
    double scale)
{
    bool valid_box = cp_vec2_minmax_valid(&glyph->box);
    /* background indicating glyph box size */
    if (valid_box) {
        fprintf(ps->f, "0.9 1 0.9 setrgbcolor\n");
        if (glyph->draw != NULL) {
            fprintf(ps->f,
                "newpath %g %g moveto %g %g lineto %g %g lineto %g %g lineto closepath fill\n",
                CXY(font, glyph->draw->box.min.x, glyph->draw->box.min.y),
                CXY(font, glyph->draw->box.max.x, glyph->draw->box.min.y),
                CXY(font, glyph->draw->box.max.x, glyph->draw->box.max.y),
                CXY(font, glyph->draw->box.min.x, glyph->draw->box.max.y));
        }

        fprintf(ps->f, "0.8 1 0.8 setrgbcolor\n");
        fprintf(ps->f,
            "newpath %g %g moveto %g %g lineto %g %g lineto %g %g lineto closepath fill\n",
            CXY(font, glyph->box.min.x, glyph->box.min.y),
            CXY(font, glyph->box.max.x, glyph->box.min.y),
            CXY(font, glyph->box.max.x, glyph->box.max.y),
            CXY(font, glyph->box.min.x, glyph->box.max.y));
    }

    /* coll_box_y */
    fprintf(ps->f, "0.6 0.8 0.6 setrgbcolor\n");
    for (cp_arr_each(i, glyph->coll_box)) {
        double y1 = font->coll_box_y[i];
        double y2 = font->coll_box_y[i+1];
        double x1r = glyph->coll_box[i].lo;
        double x2r = glyph->coll_box[i].hi;
        if ((x1r >= 0) || (x2r >= 0)) {
            double x1 = glyph->box.min.x + cp_max(0.0, x1r);
            double x2 = glyph->box.max.x - cp_max(0.0, x2r);
            fprintf(ps->f, "newpath");
            fprintf(ps->f, " %g %g moveto", CXY(font, x1, y1));
            fprintf(ps->f, " %g %g lineto", CXY(font, x1, y2));
            fprintf(ps->f, " %g %g lineto", CXY(font, x2, y2));
            fprintf(ps->f, " %g %g lineto", CXY(font, x2, y1));
            fprintf(ps->f, " closepath fill\n");
        }
    }

    /* background indicating glyph width */
    fprintf(ps->f, "1 0.8 0.8 setrgbcolor\n");
    double yy = font->base_y;
    double o1 = 1.0;
    double o2 = 2.0;
    fprintf(ps->f, "newpath");
    fprintf(ps->f, " %g %g moveto", CXY(font, glyph->dim.min.x - o2, yy - o2));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.min.x - o2, yy + o2));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.min.x,      yy));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.min.x,      yy + o1));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.max.x,      yy + o1));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.max.x,      yy));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.max.x + o2, yy + o2));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.max.x + o2, yy - o2));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.max.x,      yy));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.max.x,      yy - o1));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.min.x,      yy - o1));
    fprintf(ps->f, " %g %g lineto", CXY(font, glyph->dim.min.x,      yy));
    fprintf(ps->f, " closepath fill\n");

    /* settings */
    fprintf(ps->f, "/Helvetica findfont %g scalefont setfont\n", 14./scale);
    fprintf(ps->f, "%g setlinewidth\n", 1./scale);
    fprintf(ps->f, "0.8 setgray\n");

    /* show box_max */
    fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
        CXY(font, box->min.x, box->min.y),
        CXY(font, box->max.x, box->min.y));
    fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
        CXY(font, box->min.x, box->max.y),
        CXY(font, box->max.x, box->max.y));
    fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
        CXY(font, box->min.x, box->min.y),
        CXY(font, box->min.x, box->max.y));
    fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
        CXY(font, box->max.x, box->min.y),
        CXY(font, box->max.x, box->max.y));

    /* highlighted y lines */
    fprintf(ps->f, "%g setlinewidth\n", 4./scale);
    fprintf(ps->f, "%g setgray\n", 0.8);
    for (cp_v_each(i, &def->highlight_y)) {
        int idx = cp_v_nth(&def->highlight_y, i);
        double d = cp_v_nth(&def->coord_y, (size_t)(idx - def->box.lo.y));
        assert(is_defined(d));
        fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
            CXY(font, box->min.x, d),
            CXY(font, box->max.x, d));
    }

    /* coordinate grid */
    fprintf(ps->f, "%g setlinewidth\n", 1./scale);
    for (unsigned pass = 0; pass < 2; pass++) {
        fprintf(ps->f, "%g setgray\n", (pass == 1) ? 0 : 0.8);
        for (cp_size_each(i, INTV_SIZE(def->box.lo.y, def->box.hi.y))) {
            double d = cp_v_nth(&def->coord_y, i);
            if (!is_defined(d)) {
                continue;
            }
            if ((pass == 1) && !glyph->used_y[i]) {
                continue;
            }
            fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
                CXY(font, box->min.x, d),
                CXY(font, box->max.x, d));
            fprintf(ps->f,
                "%g %g moveto %g %g rmoveto (%d) dup stringwidth pop neg 0 rmoveto show\n",
                CXY(font, box->min.x, d),
                -PS_MM(2.)/scale,
                -PS_MM(1.)/scale,
                def->box.lo.y + (int)i);
        }
        for (cp_size_each(i, INTV_SIZE(def->box.lo.x, def->box.hi.x))) {
            double d = cp_v_nth(&def->coord_x, i);
            if (!is_defined(d)) {
                continue;
            }
            if ((pass == 1) && !glyph->used_x[i]) {
                continue;
            }
            d *= def->scale_x;
            fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
                CXY(font, d, box->min.y),
                CXY(font, d, box->max.y));
            fprintf(ps->f,
                "%g %g moveto 0 %g rmoveto (%d) dup stringwidth pop 2 div neg 0 rmoveto show\n",
                CXY(font, d, box->max.y),
                +PS_MM(2.)/scale,
                def->box.lo.x + (int)i);
        }
    }

    /* width/height indicators for box: */
    fprintf(ps->f, "save\n");
    fprintf(ps->f, "0 0.8 0 setrgbcolor\n");
    fprintf(ps->f, "[0.2 0.5] 0 setdash\n");
    double over = PS_MM(10.) / scale;
    double d;
    if (valid_box) {
        d = glyph->box.min.y;
        fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
            CXY(font, box->min.x, d),
            CXY(font, box->max.x + over, d));

        d = glyph->box.max.y;
        fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
            CXY(font, box->min.x, d),
            CXY(font, box->max.x + over, d));
    }

    if (cp_lt(glyph->box.min.x, glyph->box.max.x)) {
        d = glyph->box.min.x;
        fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
            CXY(font, d, box->min.y - over),
            CXY(font, d, box->max.y));

        d = glyph->box.max.x;
        fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
            CXY(font, d, box->min.y - over),
            CXY(font, d, box->max.y));

        fprintf(ps->f, "restore\n");
    }

    /* dimension indicators for box: */
    fprintf(ps->f, "save\n");
    fprintf(ps->f, "0.8 0 0 setrgbcolor\n");
    fprintf(ps->f, "[0.2 0.5] 0 setdash\n");

    d = glyph->dim.min.x;
    fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
        CXY(font, d, box->min.y - over),
        CXY(font, d, box->max.y));

    d = glyph->dim.max.x;
    fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
        CXY(font, d, box->min.y - over),
        CXY(font, d, box->max.y));

    d = glyph->dim.max.x - glyph->rpad;
    if (!cp_eq(glyph->rpad, 0) && !cp_eq(glyph->box.max.x, d)) {
        fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
            CXY(font, d, box->min.y - over),
            CXY(font, d, box->max.y));
    }

    d = glyph->dim.min.x + glyph->lpad;
    if (!cp_eq(glyph->lpad, 0) && !cp_eq(glyph->box.min.x, d)) {
        fprintf(ps->f, "newpath %g %g moveto %g %g lineto stroke\n",
            CXY(font, d, box->min.y - over),
            CXY(font, d, box->max.y));
    }

    fprintf(ps->f, "restore\n");
}

static void ps_detail_font(
    ps_t *ps,
    font_t const *font)
{
    font_def_t const *def = font->def;
    cp_vec2_minmax_t const *box = &font->box_max;

    double margin_x = PS_PAPER_MARGIN_X;
    double margin_y = PS_PAPER_MARGIN_Y + PS_MM(15.0);

    double view_x = PS_PAPER_X - (2 * margin_x);
    double view_y = PS_PAPER_Y - (2 * margin_y);

    double min_x = box->min.x;
    min_x = cp_min(min_x, slant_x(font, box->min.x, box->min.y));
    min_x = cp_min(min_x, slant_x(font, box->min.x, box->max.y));

    double max_x = box->max.x;
    max_x = cp_max(max_x, slant_x(font, box->max.x, box->min.y));
    max_x = cp_max(max_x, slant_x(font, box->max.x, box->max.y));

    double scale_x = view_x / (max_x - min_x);
    double scale_y = view_y / (box->max.y - box->min.y);

    double scale = cp_min(scale_x, scale_y);

    for (cp_v_each(i, &font->glyph)) {
         font_glyph_t const *glyph = cp_v_nth_ptr(&font->glyph, i);

         char label[20];
         snprintf(label, sizeof(label), "%04X", glyph->unicode.code_point);
         ps_page_begin(ps, label);

         char long_label[80];
         snprintf(long_label, sizeof(long_label), "U+%04X %s",
             glyph->unicode.code_point,
             glyph->unicode.name);

         fprintf(ps->f, "/Helvetica findfont 14 scalefont setfont\n");
         fprintf(ps->f, "%g %g moveto (%s) show\n",
             margin_x, (PS_PAPER_Y - margin_y) + PS_MM(10),
             long_label);

         fprintf(ps->f, "save\n");
         fprintf(ps->f, "%g %g translate\n",
             PS_PAPER_X/2.,
             PS_PAPER_Y - margin_y);
         fprintf(ps->f, "%g dup scale\n", scale);
         fprintf(ps->f, "%g %g translate\n",
             -(box->min.x + box->max.x) / 2.,
             -box->max.y);

         ps_detail_grid(ps, font, def, glyph, box, scale);
         fprintf(ps->f, "0 setgray\n");
         ps_glyph_draw(ps, 0, 0, glyph);
         fprintf(ps->f, "restore\n");

         ps_page_end(ps);
    }
}

/* ********************************************************************** */

static void ps_writeln_str7(
    ps_t *ps,
    cp_font_t *font,
    double pt_size,
    double *y,
    char const *s)
{
    cp_v_obj_p_t out = {0};
    cp_font_gc_t gc = {0};
    cp_font_gc_set_font(&gc, font, pt_size, 1.0);
    cp_font_print_str_latin1(&out, &gc, s);
    ps_render_v_poly(ps, 0, *y, &out);
    *y -= (font->top_y - font->bottom_y) * gc.scale_y;
}

static void ps_writeln_str32(
    ps_t *ps,
    cp_font_t *font,
    double pt_size,
    double *y,
    unsigned const *s)
{
    cp_v_obj_p_t out = {0};
    cp_font_gc_t gc = {0};
    cp_font_gc_set_font(&gc, font, pt_size, 1.0);
    cp_font_print_str32(&out, &gc, s);
    ps_render_v_poly(ps, 0, *y, &out);
    *y -= (font->top_y - font->bottom_y) * gc.scale_y;
}

static void ps_proof_sheet(
    ps_t *ps,
    cp_font_t *font)
{
    ps_page_begin(ps, NULL);

    double x = PS_MM(10);
    double y = PS_PAPER_Y - PS_MM(25);

    fprintf(ps->f, "save %g %g translate\n", x, y);
    double yy = 0;
    ps_writeln_str7(ps, font, 20, &yy, font->name);
    fprintf(ps->f, "restore\n");

    yy += PS_MM(3);
    fprintf(ps->f, "save %g %g translate\n", x, y);
    ps_writeln_str32(ps, font, 14, &yy,
        U"Cwm fjord bank glyphs vext quiz. pr\x30ci\x301s\x30cti\x301 Svi\x301\xfejo\x301\xf0"
         "?!.;:\xb7\xbfs\x153ur'`/fox-like ");

    ps_writeln_str32(ps, font, 14, &yy,
        U"\x201e""Fix, Schwyz!\x201c qu\u00E4kt J\u00FCrgen bl\u00F6d vom Pa\u00DF. "
         "\x201aN\xe3o.\x2018\x2013\x152uvre\\f\xe6r pTo//.\\");

    ps_writeln_str32(ps, font, 14, &yy,
        U"ABCDEFGHIJKLMNOPQRSTUVWXYZA\x308O\x308U\x308N\x308\x1E9E \xa9""ht "
         "\x2e9\xfeff\x2e9\x2e5\xfeff\x2e9\x2e6\xfeff\x2e9\x2e7\xfeff\x2e9\x2e8\xfeff\x2e9\x2e9"
         "\x17e\x307z\x307\x30cz\x30c\x307\x304\x327"
         "\x105j");

    ps_writeln_str32(ps, font, 14, &yy,
        U"abcdefghijklmnopqrstuvwxyza\x308o\x200d\x308u\x308n\x308\xDF\x149 0123456789 "
        "['t\x2b0""a:l\x250] i\x307\x303\x328"
        "e\x307"
        "j\x307\x303""E\x307\x301""\x14a");

    ps_writeln_str32(ps, font, 14, &yy,
        U"Poj\x10fte! Pe\x165""a ve\x13e""k\xfd fjo\x308r\xf0 segja z\x142oty "
        "ce\x140la CE\x13fLA muffig flo\xdf Gift The");
    {
        cp_v_obj_p_t out = {0};
        cp_font_gc_t gc = {0};
        cp_font_gc_set_font(&gc, font, 14, 1.0);
        cp_font_print_str32(&out, &gc, U"\x1f2u");
        cp_font_print_str32(&out, &gc, U"d\x327 ");

        cp_font_gc_set_lang(&gc, "Mah");
        cp_font_print_str32(&out, &gc, U"M\x327""ajel\x327 ");

        cp_font_gc_set_lang(&gc, "LIV");
        cp_font_print_str32(&out, &gc, U"ne\x304""d\x327i ");

        cp_font_gc_set_lang(&gc, "LAT");
        cp_font_print_str32(&out, &gc, U"vil\x327n\x327u ");

        gc.tracking = 2.0;
        cp_font_gc_set_lang(&gc, "nl");
        cp_font_print_str32(&out, &gc, U"\x132\x301_IJssel ij\x30cq\x30c");
        cp_font_print_str32(&out, &gc, U"i\x200cjiji\x200bj");

        cp_font_gc_enable_ligature(&gc, false);
        cp_font_print_str32(&out, &gc, U"i\x200djij");

        cp_font_gc_enable_ligature(&gc, true);
        cp_font_gc_set_lang(&gc, "DEU");
        cp_font_print_str32(&out, &gc, U"i\x200dj bijektiv");
        ps_render_v_poly(ps, 0, yy, &out);

        yy -= (font->top_y - font->bottom_y) * gc.scale_y;
    }

    unsigned comb[][3] = {
        { U_COMBINING_ACUTE_ACCENT,        U_COMBINING_DOT_BELOW },
        { U_COMBINING_GRAVE_ACCENT,        U_COMBINING_DIAERESIS_BELOW },
        { U_COMBINING_CIRCUMFLEX_ACCENT,   U_COMBINING_MACRON_BELOW },
        { U_COMBINING_CARON,               U_COMBINING_CIRCUMFLEX_ACCENT_BELOW },
        { U_COMBINING_TILDE,               U_COMBINING_CARON_BELOW },
        { U_COMBINING_DOT_ABOVE,           U_COMBINING_CEDILLA },
        { U_COMBINING_DIAERESIS,           U_COMBINING_OGONEK },
        { U_COMBINING_BREVE,               U_COMBINING_RING_BELOW },
        { U_COMBINING_INVERTED_BREVE,      U_COMBINING_COMMA_BELOW },
        { U_COMBINING_MACRON,              U_COMBINING_TILDE_BELOW },
        { U_COMBINING_DOUBLE_ACUTE_ACCENT, U_COMBINING_VERTICAL_LINE_BELOW },
        { U_COMBINING_DOUBLE_GRAVE_ACCENT, U_COMBINING_BREVE_BELOW },
        { U_COMBINING_RING_ABOVE,          U_COMBINING_INVERTED_BREVE_BELOW },
        { U_COMBINING_HOOK_ABOVE,          U_COMBINING_DOUBLE_VERTICAL_LINE_BELOW },
        { U_COMBINING_COMMA_ABOVE,         },
        { U_COMBINING_REVERSED_COMMA_ABOVE,},
        { U_COMBINING_TURNED_COMMA_ABOVE,  },
        { U_COMBINING_VERTICAL_LINE_ABOVE, },
        { U_COMBINING_CIRCUMFLEX_ACCENT,   U_COMBINING_ACUTE_ACCENT,    },
        { U_COMBINING_CIRCUMFLEX_ACCENT,   U_COMBINING_GRAVE_ACCENT,    },
        { U_COMBINING_CIRCUMFLEX_ACCENT,   U_COMBINING_CARON,           },
        { U_COMBINING_CIRCUMFLEX_ACCENT,   U_COMBINING_MACRON,          },
        { U_COMBINING_CIRCUMFLEX_ACCENT,   U_COMBINING_HOOK_ABOVE,      },
        { U_COMBINING_CIRCUMFLEX_ACCENT,   U_COMBINING_TILDE,           },
        { U_COMBINING_BREVE,               U_COMBINING_ACUTE_ACCENT,    },
        { U_COMBINING_BREVE,               U_COMBINING_GRAVE_ACCENT,    },
        { U_COMBINING_BREVE,               U_COMBINING_HOOK_ABOVE,      },
        { U_COMBINING_BREVE,               U_COMBINING_TILDE,           },
        { U_COMBINING_DIAERESIS,           U_COMBINING_ACUTE_ACCENT,    },
        { U_COMBINING_DIAERESIS,           U_COMBINING_GRAVE_ACCENT,    },
        { U_COMBINING_DIAERESIS,           U_COMBINING_CARON,           },
        { U_COMBINING_DIAERESIS,           U_COMBINING_MACRON,          },
        { U_COMBINING_DIAERESIS,           U_COMBINING_TILDE,           },
        { U_COMBINING_ACUTE_ACCENT,        U_COMBINING_DOT_ABOVE,       },
        { U_COMBINING_CARON,               U_COMBINING_DOT_ABOVE,       },
        { U_COMBINING_DOT_ABOVE,           U_COMBINING_MACRON,          },
        { U_COMBINING_DOT_ABOVE,           U_COMBINING_ACUTE_ACCENT,    },
        { U_COMBINING_DOT_ABOVE,           U_COMBINING_GRAVE_ACCENT,    },
        { U_COMBINING_DOT_ABOVE,           U_COMBINING_TILDE,           },
        { U_COMBINING_MACRON,              U_COMBINING_ACUTE_ACCENT,    },
        { U_COMBINING_MACRON,              U_COMBINING_GRAVE_ACCENT,    },
        { U_COMBINING_MACRON,              U_COMBINING_DIAERESIS,       },
        { U_COMBINING_MACRON,              U_COMBINING_TILDE,           },
        { U_COMBINING_RING_ABOVE,          U_COMBINING_ACUTE_ACCENT,    },
        { U_COMBINING_TILDE,               U_COMBINING_ACUTE_ACCENT,    },
        { U_COMBINING_TILDE,               U_COMBINING_MACRON,          },
        { U_COMBINING_TILDE,               U_COMBINING_DIAERESIS,       },
    };
    cp_v_unsigned_t str = {0};
    for (cp_arr_each(i, comb)) {
        cp_v_push(&str, 'u');
        for (cp_arr_each(j, comb[0])) {
            if (comb[i][j]) {
                cp_v_push(&str, comb[i][j]);
            }
        }
    }
    cp_v_push(&str, 0);
    ps_writeln_str32(ps, font, 14, &yy, str.data);

    cp_v_clear(&str, 0);
    for (cp_arr_each(i, comb)) {
        cp_v_push(&str, 'h');
        for (cp_arr_each(j, comb[0])) {
            if (comb[i][j]) {
                cp_v_push(&str, comb[i][j]);
            }
        }
    }
    cp_v_push(&str, 0);
    ps_writeln_str32(ps, font, 14, &yy, str.data);

    ps_writeln_str32(ps, font, 14, &yy,
        U"a[k] foo_bar __LINE__ hsn{xy} x*(y+5)<78 a\u2212b\u00b1c b=(1+*a) x||y");

    ps_writeln_str32(ps, font, 14, &yy,
        U"#define TE \"ta\"#5 '$45' \x1e68S$s 50% \xb7+~g &a o<a X@x 5/8");

    ps_writeln_str32(ps, font, 14, &yy,
        U"5\x20aC 6$ \x2039o\x203a\xabo\xbb \x00B2\x2154\x2083 3x\xb2+4x $\xa4 1\xb5""F 2k\x2126 "
         "5\xaa 6\xba 2\xB0""C 0\x212a");

    ps_writeln_str32(ps, font, 11, &yy,
        U"\xd4 s\x14f\x324h hu\xf2i, B\xe1""e\x324k-h\x16dng g\xe2""e\x324ng "
        "N\x12dk-t\xe0u du\x14fh h\x12b d\xf3\x324i c\x103ng, k\xe1ng "
        "di\xea-n\xe8\x324ng bu\x14dng-s\xea\x324u\x324 du\xe2i.");

    ps_writeln_str32(ps, font, 11, &yy,
        U"\x110\x1ebf qu\x1ed1""c La M\xe3, hay c\xf2n g\x1ecdi l\xe0 \x110\x1ebf "
        "qu\x1ed1""c Roma l\xe0 th\x1eddi k\x1ef3 h\x1eadu C\x1ed9ng h\xf2""a c\x1ee7""a "
        "n\x1ec1n v\x103n minh..."); // La M\xe3 c\x1ed5 \x111\x1ea1i.");

    ps_writeln_str32(ps, font, 11, &yy,
#if 0
        U"P\x25br\x269l\x25b\x25b k\x25bna Caama \x25bja\x256\x25b taa t\x25bt\x28b "
        "nd\x28b t\x269k\x269la pa\x263l\x28b\x28b y\x254 n\x25b t\x25bt\x28b "
        "s\x28bz\x254t\x28b."
#endif
        U"L\x254ndr\x269 k\x25b\x14bna Kewiya\x263 K\x269kp\x25bnda\x263 \x25bja\x256\x25b "
        "taa t\x25bt\x28b s\x28bz\x254t\x28b n\x25b t\x25bt\x28b t\x28bn\x25b t\x269w\x25b "
        "Pireetaa\xf1\x269..."
        // S\x254s\x254\x28b w\x269s\x269 \x256\x269l\x269y\x25b n\x25b had\x25b ki\x14b."
    );

    fprintf(ps->f, "restore\n");

    ps_page_end(ps);
}

/* ********************************************************************** */

static FILE *fopen_or_fail(char const *filename, char const *mode)
{
    FILE *f = fopen(filename, mode);
    if (f == NULL) {
        fprintf(stderr, "Error: Unable to open file '%s' for writing: %s\n",
            filename, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return f;
}

static void convert_family_push(
    font_v_font_p_t *vfont,
    font_def_t const *def)
{
    font_t *font = convert_font(def);
    cp_v_push(vfont, font);
}

static void convert_family_all_sizes(
    font_v_font_p_t *vfont,
    font_def_t const *def)
{
    /* Normal */
    convert_family_push(vfont, def);

#if 0
    /* Caption */
    {
        font_def_t *def2 = CP_CLONE(*def2, def);
        def2->size_name = "Caption";
        for (cp_arr_each(i, def2->line_width)) {
            def2->line_width[i] *= 5./4.;
        }
        def2->scale_x *= 1.3;
        convert_family_push(vfont, def2);
    }
    /* Small */
    {
        font_def_t *def2 = CP_CLONE(*def2, def);
        def2->size_name = "Small";
        for (cp_arr_each(i, def2->line_width)) {
            def2->line_width[i] *= 4.5/4.;
        }
        def2->scale_x *= 1.15;
        convert_family_push(vfont, def2);
    }
    /* Display */
    {
        font_def_t *def2 = CP_CLONE(*def2, def);
        def2->size_name = "Display";
        for (cp_arr_each(i, def2->line_width)) {
            def2->line_width[i] *= 3.5/4.;
        }
        def2->scale_x *= 0.95;
        convert_family_push(vfont, def2);
    }
#endif
}

static void convert_family_all_stretches(
    font_v_font_p_t *vfont,
    font_def_t const *def)
{
    convert_family_all_sizes(vfont, def);
}

static void convert_family_all_slopes(
    font_v_font_p_t *vfont,
    font_def_t const *def)
{
    /* Roman */
    convert_family_all_stretches(vfont, def);

    /* Oblique */
    font_def_t *def2 = CP_CLONE(*def2, def);
    def2->slope_name = "Oblique";
    def2->slope = CP_FONT_SLOPE_OBLIQUE;
    def2->slant = (def2->slope - 100) / 100.0;
    convert_family_all_stretches(vfont, def2);
}

static void convert_family_all_weights(
    font_v_font_p_t *vfont,
    font_def_t const *def)
{
    /* Book */
    convert_family_all_slopes(vfont, def);

    /* Medium */
    {
        font_def_t *def2 = CP_CLONE(*def2, def);
        def2->weight_name = "Medium";
        def2->weight = CP_FONT_WEIGHT_MEDIUM;
        for (cp_arr_each(i, def2->line_width)) {
            def2->line_width[i] *= 5./4.;
        }
        convert_family_all_slopes(vfont, def2);
    }

    /* Bold */
    {
        font_def_t *def2 = CP_CLONE(*def2, def);
        def2->weight_name = "Bold";
        def2->weight = CP_FONT_WEIGHT_BOLD;
        for (cp_arr_each(i, def2->line_width)) {
            def2->line_width[i] *= 6./4.;
        }
        convert_family_all_slopes(vfont, def2);
    }

    /* Black */
    {
        font_def_t *def2 = CP_CLONE(*def2, def);
        def2->weight_name = "Black";
        def2->weight = CP_FONT_WEIGHT_BLACK;
        for (cp_arr_each(i, def2->line_width)) {
            def2->line_width[i] *= 8./4.;
        }
        convert_family_all_slopes(vfont, def2);
    }

    /* Light */
    {
        font_def_t *def2 = CP_CLONE(*def2, def);
        def2->weight_name = "Light";
        def2->weight = CP_FONT_WEIGHT_LIGHT;
        for (cp_arr_each(i, def2->line_width)) {
            def2->line_width[i] *= 3./4.;
        }
        convert_family_all_slopes(vfont, def2);
    }
}

static void convert_family(
    font_v_font_p_t *vfont,
    font_def_t const *def)
{
    convert_family_all_weights(vfont, def);
}

/* ********************************************************************** */

static size_t ps_font(font_t const *font)
{
    /* output font */
    cp_vchar_t fn[1] = {0};
    FILE *f;
    ps_t ps[1];

    /* print chart document */
    cp_vchar_printf(fn, "out-font/%s-chart.ps", font->filename.data);
    f = fopen_or_fail(cp_vchar_cstr(fn), "wt");
    ps_doc_begin(ps, f);
    size_t count = ps_chart_font(ps, font);
    ps_doc_end(ps);
    fclose(f);

    /* print chart document */
    cp_vchar_clear(fn);
    cp_vchar_printf(fn, "out-font/%s-detail.ps", font->filename.data);
    f = fopen_or_fail(cp_vchar_cstr(fn), "wt");
    ps_doc_begin(ps, f);
    ps_detail_font(ps, font);
    ps_doc_end(ps);
    fclose(f);
    return count;
}

static void ps_font_family(cp_v_font_p_t const *vfont)
{
    cp_font_t const *font0 = cp_v_nth(vfont,0);

    cp_vchar_t filename = {0};
    cp_vchar_printf(&filename, "%s", font0->family_name);
    normalise_filename(&filename);

    /* output font */
    cp_vchar_t fn[1] = {0};
    FILE *f;
    ps_t ps[1];

    /* print chart document */
    cp_vchar_printf(fn, "out-font/%s-family.ps", filename.data);
    f = fopen_or_fail(cp_vchar_cstr(fn), "wt");
    ps_doc_begin(ps, f);

    for (cp_v_each(i, vfont)) {
        cp_font_t *font = cp_v_nth(vfont, i);
        ps_proof_sheet(ps, font);
    }

    ps_doc_end(ps);
    fclose(f);
}

/* ********************************************************************** */

static int cmp_cp_set_by_coverage(void const *_a, void const *_b)
{
    unicode_set_t const *a = _a;
    unicode_set_t const *b = _b;
    int i = cp_cmp(rint(100 * b->have_ratio), rint(100 * a->have_ratio));
    if (i != 0) {
        return i;
    }
    return strcmp(a->name, b->name);
}

static void doc_coverage(
    font_t *font0)
{
    /* count glyphs */
    for (cp_arr_each(i, cp_set)) {
        unicode_set_t *s = &cp_set[i];
        assert(s->cp.size > 0);
        s->have_cnt = 0;
        for (cp_v_each(j, &s->cp)) {
            if (have_glyph(font0, cp_v_nth(&s->cp, j).code_point)) {
                s->have_cnt++;
            }
        }
        s->have_ratio = cp_f(s->have_cnt) / cp_f(s->cp.size);

        /* coverage below 50% is counted as 0.  This helps sorting. */
        if (cp_lt(s->have_ratio, 0.5)) {
            s->have_ratio = 0;
        }
    }

    /* sort by coverage */
    qsort(cp_set, cp_countof(cp_set), sizeof(cp_set[0]), cmp_cp_set_by_coverage);

    /* output font */
    cp_vchar_t fn[1] = {0};
    FILE *f;

    /* print chart document */
    cp_vchar_printf(fn, "out-font/%s-coverage.tex", font0->filename.data);
    f = fopen_or_fail(cp_vchar_cstr(fn), "wt");
    fprintf(f, "\\documentclass[12pt,a4paper]{article}\n");
    fprintf(f, "\\usepackage{a4wide}\n");
    fprintf(f, "\\usepackage{longtable}\n");
    fprintf(f, "\\parindent0pt\n");
    fprintf(f, "\\parskip1ex\n");
    fprintf(f, "\\begin{document}\n");
    fprintf(f, "\\sloppy\n");
    fprintf(f, "\\section*{%s}\n", font0->family_name);

    /* fully covered */
    char const *sep = NULL;
    for (cp_arr_each(i, cp_set)) {
        unicode_set_t const *s = &cp_set[i];
        if (cp_eq(s->have_ratio, 1)) {
            if (sep == NULL) {
                fprintf(f, "\\subsection*{Fully Covered}\n");
                sep = "";
            }
            fprintf(f, "%s%s", sep, s->name);
            sep = ",\n";
        }
    }
    if (sep != NULL) {
        fprintf(f, ".\n\n");
    }

    /* low coverage */
    sep = NULL;
    for (cp_arr_each(i, cp_set)) {
        unicode_set_t const *s = &cp_set[i];
        if (cp_eq(s->have_ratio, 0)) {
            if (sep == NULL) {
                fprintf(f, "\\subsection*{Coverage Below 50\\%%}\n");
                sep = "";
            }
            fprintf(f, "%s%s", sep, s->name);
            sep = ",\n";
        }
    }
    if (sep != NULL) {
        fprintf(f, ".\n\n");
    }

    fprintf(f, "\\subsection*{Almost Covered (Max. 5 Code Points Missing)}\n");
    fprintf(f, "\\begin{longtable}[l]{lrrr}\n");
    for (cp_arr_each(i, cp_set)) {
        unicode_set_t const *s = &cp_set[i];
        if (cp_eq(s->have_ratio, 1)) {
            continue;
        }
        if (cp_eq(s->have_ratio, 0)) {
            continue;
        }
        if ((s->cp.size - s->have_cnt) <= 5) {
            fprintf(f, "%s\\\\\n", s->name);
            for (cp_v_each(j, &s->cp)) {
                unicode_t const *u = &cp_v_nth(&s->cp, j);
                if (!have_glyph(font0, u->code_point)) {
                    fprintf(f, "\\qquad{\\small U+%04X %s}\\\\\n", u->code_point, u->name);
                }
            }
        }
    }
    fprintf(f, "\\end{longtable}\n");

    fprintf(f, "\\subsection*{Partially Covered}\n");
    fprintf(f, "\\begin{longtable}[l]{lrrr}\n");
    fprintf(f, "\\textbf{Set}&\\textbf{Coverage}&\\textbf{Missing}&\\textbf{Glyph Count}\\\\\n");
    for (cp_arr_each(i, cp_set)) {
        unicode_set_t const *s = &cp_set[i];
        if (cp_eq(s->have_ratio, 1)) {
            continue;
        }
        if (cp_eq(s->have_ratio, 0)) {
            continue;
        }
        if ((s->cp.size - s->have_cnt) <= 5) {
            continue;
        }
        fprintf(f, "%s & %3.0f\\%% & %"CP_Z"u & %"CP_Z"u\\\\\n",
            s->name, 100*s->have_ratio, s->cp.size - s->have_cnt, s->cp.size);
    }
    fprintf(f, "\\end{longtable}\n");

    fprintf(f, "\\end{document}\n");
    fclose(f);
}

/* ********************************************************************** */
/* save font family as C data structure file */

static void save_c_head(
    FILE *f)
{
    fprintf(f,
        "/* -*- Mode: C -*- */\n"
        "/* Automatically generated by Hob3l fontgen; DO NOT EDIT */\n"
        "/* Copyright (C) 2018 by Henrik Theiling, Licence: GPLv3, see LICENSE file */\n"
        "\n");
}

static void save_h_head(
    FILE *f,
    char const *filename)
{
    save_c_head(f);
    fprintf(f,
        "#ifndef CP_FONT_%s_H_\n"
        "#define CP_FONT_%s_H_\n\n",
        filename, filename);
}

static void save_h_tail(
    FILE *f,
    char const *filename)
{
    fprintf(f,
        "\n"
        "#endif /* CP_FONT_%s_H_ */\n",
        filename);
}

static void save_c_family(
    cp_v_font_p_t const *vfont)
{
    cp_font_t const *font0 = cp_v_nth(vfont,0);

    cp_vchar_t c_name_uc = {0};
    cp_vchar_printf(&c_name_uc, "%s", font0->family_name);
    normalise_c_name_uc(&c_name_uc);

    cp_vchar_t h_name = {0};
    cp_vchar_printf(&h_name, "%s", font0->family_name);
    normalise_filename(&h_name);

    /* header */
    cp_vchar_t fn[1] = {0};
    cp_vchar_printf(fn, "include/hob3l/font-%s.h", h_name.data);
    FILE *f = fopen_or_fail(cp_vchar_cstr(fn), "wt");
    save_h_head(f, c_name_uc.data);
    fprintf(f, "#include <hob3l/font_tam.h>\n\n");
    for (cp_v_each(i, vfont)) {
        cp_font_t const *font = cp_v_nth(vfont, i);

        cp_vchar_t c_name_lc = {0};
        cp_vchar_printf(&c_name_lc, "%s", font->name);
        normalise_c_name_lc(&c_name_lc);

        fprintf(f, "extern cp_font_t const cp_font_%s;\n", c_name_lc.data);
    }
    save_h_tail(f, c_name_uc.data);
    fclose(f);

    /* implementation */
    for (cp_v_each(i, vfont)) {
        cp_font_t const *font = cp_v_nth(vfont, i);

        cp_vchar_t c_name = {0};
        cp_vchar_printf(&c_name, "%s", font->name);
        normalise_filename(&c_name);

        cp_vchar_clear(fn);
        cp_vchar_printf(fn, "src/font-%s.c", c_name.data);
        f = fopen_or_fail(cp_vchar_cstr(fn), "wt");
        save_c_head(f);
        fprintf(f, "#include <hob3l/font-%s.h>\n", h_name.data);

        cp_vchar_t c_name_lc = {0};
        cp_vchar_printf(&c_name_lc, "cp_font_%s", font->name);
        normalise_c_name_lc(&c_name_lc);

        /* glyph array */
        if (font->glyph.size > 0) {
            fprintf(f, "\ncp_font_glyph_t %s_glyph[%"CP_Z"u] = {\n",
                c_name_lc.data, font->glyph.size);
            for (cp_v_each(j, &font->glyph)) {
                cp_font_glyph_t const *g = cp_v_nth_ptr(&font->glyph, j);
                fprintf(f, "{%u,%u,%u,%u},\n",
                    g->id, g->flags, g->first, g->second);
            }
            fprintf(f, "};\n");
        }

        /* path array */
        /* There is a type cast from entries in this array to cp_font_path_t
         * because that type ends in an open array of path start indices.
         * Emit a static assert to see that the font generator has the same
         * notion of size for cp_font_path_t object as the C compiler that
         * reads the font. */
        fprintf(f,
            "\n"
            "CP_STATIC_ASSERT(sizeof(cp_font_path_t) == %"CP_Z"u);\n"
            "CP_STATIC_ASSERT(cp_alignof(cp_font_path_t) == %"CP_Z"u);\n",
            sizeof(cp_font_path_t), cp_alignof(cp_font_path_t));
        if (font->path.size > 0) {
            fprintf(f, "\nuint32_t %s_path[%"CP_Z"u] = {",
                c_name_lc.data, font->path.size);
            for (cp_v_each(j, &font->path)) {
                unsigned u = cp_v_nth(&font->path, j);
                if ((j % 8) == 0) {
                    fprintf(f, "\n");
                }
                fprintf(f, "%u,", u);
            }
            fprintf(f, "\n};\n");
        }

        /* coord array */
        if (font->coord.size > 0) {
            fprintf(f, "\ncp_font_xy_t %s_coord[%"CP_Z"u] = {\n",
                c_name_lc.data, font->coord.size);
            for (cp_v_each(j, &font->coord)) {
                cp_font_xy_t *g = cp_v_nth_ptr(&font->coord, j);
                fprintf(f, "{%u,%u},\n", g->x, g->y);
            }
            fprintf(f, "};\n");
        }

        /* decompose array */
        if (font->decompose.size > 0) {
            fprintf(f, "\ncp_font_map_t %s_decompose[%"CP_Z"u] = {\n",
                c_name_lc.data, font->decompose.size);
            for (cp_v_each(j, &font->decompose)) {
                cp_font_map_t *g = cp_v_nth_ptr(&font->decompose, j);
                fprintf(f, "{%u,%u,%u,%u},\n",
                    g->first, g->flags, g->second, g->result);
            }
            fprintf(f, "};\n");
        }

        /* compose array */
        if (font->compose.size > 0) {
            fprintf(f, "\ncp_font_map_t %s_compose[%"CP_Z"u] = {\n",
                c_name_lc.data, font->compose.size);
            for (cp_v_each(j, &font->compose)) {
                cp_font_map_t *g = cp_v_nth_ptr(&font->compose, j);
                fprintf(f, "{%u,%u,%u,%u},\n",
                    g->first, g->flags, g->second, g->result);
            }
            fprintf(f, "};\n");
        }

        /* optional array */
        if (font->optional.size > 0) {
            fprintf(f, "\ncp_font_map_t %s_optional[%"CP_Z"u] = {\n",
                c_name_lc.data, font->optional.size);
            for (cp_v_each(j, &font->optional)) {
                cp_font_map_t *g = cp_v_nth_ptr(&font->optional, j);
                fprintf(f, "{%u,%u,%u,%u},\n",
                    g->first, g->flags, g->second, g->result);
            }
            fprintf(f, "};\n");
        }

        /* comb_type array */
        if (font->comb_type.size > 0) {
            fprintf(f, "\ncp_font_map_t %s_comb_type[%"CP_Z"u] = {\n",
                c_name_lc.data, font->comb_type.size);
            for (cp_v_each(j, &font->comb_type)) {
                cp_font_map_t *g = cp_v_nth_ptr(&font->comb_type, j);
                fprintf(f, "{%u,%u,%u,%u},\n",
                    g->first, g->flags, g->second, g->result);
            }
            fprintf(f, "};\n");
        }

        /* context array */
        if (font->context.size > 0) {
            fprintf(f, "\ncp_font_map_t %s_context[%"CP_Z"u] = {\n",
                c_name_lc.data, font->context.size);
            for (cp_v_each(j, &font->context)) {
                cp_font_map_t *g = cp_v_nth_ptr(&font->context, j);
                fprintf(f, "{%u,%u,%u,%u},\n",
                    g->first, g->flags, g->second, g->result);
            }
            fprintf(f, "};\n");
        }

        /* base_repl array */
        if (font->base_repl.size > 0) {
            fprintf(f, "\ncp_font_map_t %s_base_repl[%"CP_Z"u] = {\n",
                c_name_lc.data, font->base_repl.size);
            for (cp_v_each(j, &font->base_repl)) {
                cp_font_map_t *g = cp_v_nth_ptr(&font->base_repl, j);
                fprintf(f, "{%u,%u,%u,%u},\n",
                    g->first, g->flags, g->second, g->result);
            }
            fprintf(f, "};\n");
        }

        /* lang array */
        if (font->lang.size > 0) {
            for (cp_v_each(k, &font->lang)) {
                cp_font_lang_t *lang = cp_v_nth_ptr(&font->lang, k);
                if (lang->optional.size > 0) {
                    fprintf(f, "\ncp_font_map_t %s_%"CP_Z"u_optional[%"CP_Z"u] = {\n",
                        c_name_lc.data, k, lang->optional.size);
                    for (cp_v_each(j, &lang->optional)) {
                        cp_font_map_t *g = cp_v_nth_ptr(&lang->optional, j);
                        fprintf(f, "{%u,%u,%u,%u},\n",
                            g->first, g->flags, g->second, g->result);
                    }
                    fprintf(f, "};\n");
                }
                if (lang->one2one.size > 0) {
                    fprintf(f, "\ncp_font_map_t %s_%"CP_Z"u_one2one[%"CP_Z"u] = {\n",
                        c_name_lc.data, k, lang->one2one.size);
                    for (cp_v_each(j, &lang->one2one)) {
                        cp_font_map_t *g = cp_v_nth_ptr(&lang->one2one, j);
                        fprintf(f, "{%u,%u,0,%u},\n",
                            g->first, g->flags, g->result);
                    }
                    fprintf(f, "};\n");
                }
            }

            fprintf(f, "\ncp_font_lang_t %s_lang[%"CP_Z"u] = {\n",
                c_name_lc.data, font->lang.size);
            for (cp_v_each(k, &font->lang)) {
                cp_font_lang_t *lang = cp_v_nth_ptr(&font->lang, k);
                fprintf(f, "    {\n");
                if (lang->optional.size > 0) {
                    fprintf(f, "        .optional = { .data = %s_%"CP_Z"u_optional, .size = %"CP_Z"u },\n",
                        c_name_lc.data, k, lang->optional.size);
                }
                if (lang->one2one.size > 0) {
                    fprintf(f, "        .one2one = { .data = %s_%"CP_Z"u_one2one, .size = %"CP_Z"u },\n",
                        c_name_lc.data, k, lang->one2one.size);
                }
                fprintf(f, "    },\n");
            }
            fprintf(f, "};\n");

            fprintf(f, "\ncp_font_lang_map_t %s_lang_map[%"CP_Z"u] = {\n",
                c_name_lc.data, font->lang_map.size);
            for (cp_v_each(k, &font->lang_map)) {
                cp_font_lang_map_t *lang = cp_v_nth_ptr(&font->lang_map, k);
                fprintf(f, "{\"%.*s\", %u },\n",
                    (int)sizeof(lang->id), lang->id,
                    lang->lang_idx);
            }
            fprintf(f, "};\n");
        }

        /* main structure */
        fprintf(f, "\ncp_font_t const %s = {\n", c_name_lc.data);
        fprintf(f, "    .name = \"%s\",\n", font->name);
        fprintf(f, "    .family_name = \"%s\",\n", font->family_name);
        fprintf(f, "    .weight_name = \"%s\",\n", font->weight_name);
        fprintf(f, "    .slope_name = \"%s\",\n", font->slope_name);
        fprintf(f, "    .stretch_name = \"%s\",\n", font->stretch_name);
        fprintf(f, "    .size_name = \"%s\",\n", font->size_name);
        fprintf(f, "    .em_x = %u,\n", font->em_x);
        fprintf(f, "    .em_y = %u,\n", font->em_y);
        fprintf(f, "    .top_y = %u,\n", font->top_y);
        fprintf(f, "    .bottom_y = %u,\n", font->bottom_y);
        fprintf(f, "    .base_y = %u,\n", font->base_y);
        fprintf(f, "    .cap_y = %u,\n", font->cap_y);
        fprintf(f, "    .xhi_y = %u,\n", font->xhi_y);
        fprintf(f, "    .dec_y = %u,\n", font->dec_y);
        fprintf(f, "    .center_x = %u,\n", font->center_x);
        fprintf(f, "    .space_x = {");
        for (cp_arr_each(j, font->space_x)) {
            fprintf(f, "%s%u", j==0?"":",", font->space_x[j]);
        }
        fprintf(f, "},\n");
        fprintf(f, "    .flags = 0x%x,\n", font->flags);
        fprintf(f, "    .weight = %u,\n", font->weight);
        fprintf(f, "    .slope = %u,\n", font->slope);
        fprintf(f, "    .stretch = %u,\n", font->stretch);
        fprintf(f, "    .min_size = %u,\n", font->min_size);
        fprintf(f, "    .max_size = %u,\n", font->max_size);

        if (font->glyph.size > 0) {
            fprintf(f, "    .glyph = { .data = %s_glyph, .size = %"CP_Z"u },\n",
                c_name_lc.data, font->glyph.size);
        }
        if (font->path.size > 0) {
            fprintf(f, "    .path = { .data = %s_path, .size = %"CP_Z"u },\n",
                c_name_lc.data, font->path.size);
        }
        if (font->coord.size > 0) {
            fprintf(f, "    .coord = { .data = %s_coord, .size = %"CP_Z"u },\n",
                c_name_lc.data, font->coord.size);
        }
        if (font->decompose.size > 0) {
            fprintf(f, "    .decompose = { .data = %s_decompose, .size = %"CP_Z"u },\n",
                c_name_lc.data, font->decompose.size);
        }
        if (font->compose.size > 0) {
            fprintf(f, "    .compose = { .data = %s_compose, .size = %"CP_Z"u },\n",
                c_name_lc.data, font->compose.size);
        }
        if (font->optional.size > 0) {
            fprintf(f, "    .optional = { .data = %s_optional, .size = %"CP_Z"u },\n",
                c_name_lc.data, font->optional.size);
        }
        if (font->comb_type.size > 0) {
            fprintf(f, "    .comb_type = { .data = %s_comb_type, .size = %"CP_Z"u },\n",
                c_name_lc.data, font->comb_type.size);
        }
        if (font->context.size > 0) {
            fprintf(f, "    .context = { .data = %s_context, .size = %"CP_Z"u },\n",
                c_name_lc.data, font->context.size);
        }
        if (font->base_repl.size > 0) {
            fprintf(f, "    .base_repl = { .data = %s_base_repl, .size = %"CP_Z"u },\n",
                c_name_lc.data, font->base_repl.size);
        }
        if (font->lang.size > 0) {
            fprintf(f, "    .lang = { .data = %s_lang, .size = %"CP_Z"u },\n",
                c_name_lc.data, font->lang.size);
        }

        fprintf(f, "};\n");

        fclose(f);
    }
}


/* ********************************************************************** */

int main(void)
{
    font_v_font_p_t vfont = {0};

    font_def_t *def = &f1_font_book;
    sort_font_def(&def->glyph);
    printf("%s: %"CP_Z"u glyphs\n", def->family_name, def->glyph.size);

    /* convert from def to intermediate form */
    convert_family(&vfont, def);

    /* convert from intermediate to final form */
    cp_v_font_p_t cpfont = {0};
    finalise_family(&cpfont, &vfont);

    /* save font family */
    save_c_family(&cpfont);

    /* overview documents */
    size_t count = 0;
    for (cp_v_each(i, &vfont)) {
        font_t *font = cp_v_nth(&vfont, i);
        count = ps_font(font);
    }
    printf("%s: %"CP_Z"u code points\n", def->family_name, count);

    /* overview document */
    ps_font_family(&cpfont);

    /* coverage document */
    doc_coverage(cp_v_nth(&vfont, 0));

    exit(EXIT_SUCCESS);
}
