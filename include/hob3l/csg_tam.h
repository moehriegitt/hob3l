/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG_TAM_H_
#define CP_CSG_TAM_H_

#include <hob3lbase/obj_tam.h>
#include <hob3l/csg_fwd.h>
#include <hob3l/csg2_fwd.h>
#include <hob3l/csg3_fwd.h>

#define cp_csg_typeof(type) \
    _Generic(type, \
        cp_obj_t:     CP_ABSTRACT, \
        cp_csg_t:     CP_CSG_TYPE, \
        cp_csg_add_t: CP_CSG_ADD, \
        cp_csg_xor_t: CP_CSG_XOR, \
        cp_csg_sub_t: CP_CSG_SUB, \
        cp_csg_cut_t: CP_CSG_CUT)

/**
 * CSG basic objects that are shared by 2D and 3D subsystems.
 */
typedef enum {
    /**
     * Bool op: union (boolean '|') */
    CP_CSG_ADD = CP_CSG_TYPE + 1,

    /**
     * Bool op: difference (boolean '&~')*/
    CP_CSG_SUB,

    /**
     * Bool op: cut (boolean '&') */
    CP_CSG_CUT,

    /**
     * Bool op: xor.  This is used only internally, but not exported. */
    CP_CSG_XOR,
} cp_csg_type_t;

#define CP_CSG_ \
    CP_OBJ_(cp_csg_type_t)

typedef struct {
    /**
     * type is CP_CSG_ADD */
    CP_CSG_
    cp_v_obj_p_t add;
} cp_csg_add_t;

typedef struct {
    /**
     * type is CP_CSG_SUB */
    CP_CSG_
    cp_csg_add_t *add;
    cp_csg_add_t *sub;
} cp_csg_sub_t;

typedef CP_VEC_T(cp_csg_add_t*) cp_v_csg_add_p_t;

typedef struct {
    /**
     * type is CP_CSG_CUT */
    CP_CSG_
    cp_v_csg_add_p_t cut;
} cp_csg_cut_t;

typedef struct {
    /**
     * type is CP_CSG_XOR */
    CP_CSG_
    cp_v_csg_add_p_t xor;
} cp_csg_xor_t;

/**
 * cp_csg_t is basically a cp_obj_t indicating CSG handling.
 *
 * This is used to indicate that mainly CSG data is processed.
 * Using cp_obj_t would work, too, but this type is an additional
 * abstract type for clarity.
 */
struct cp_csg { CP_CSG_ };

/**
 * Empty polygon optimisation.
 */
#define CP_CSG2_OPT_SKIP_EMPTY 0x01

/**
 * Optimise base on bounding box.
 * FIXME: currently not implemented.
 */
#define CP_CSG2_OPT_DISJOINT_BB 0x02

/**
 * Bounding box x-coord check to terminate early
 * FIXME: currently not implemented.
 */
#define CP_CSG2_OPT_SWEEP_END 0x04

/**
 * Drop inner vertices of collinear lines
 */
#define CP_CSG2_OPT_DROP_COLLINEAR 0x08

/**
 * Default set of optimisations
 */
#define CP_CSG2_OPT_DEFAULT (CP_CSG2_OPT_SKIP_EMPTY | CP_CSG2_OPT_DROP_COLLINEAR)

/**
 * The default value for cp_csg_opt_t.
 * This also needs csg2.h
 */
#define CP_CSG_OPT_DEFAULT \
    { \
        .layer_gap = -1, \
        .max_simultaneous = CP_BOOL_BITMAP_MAX_LAZY, \
        .max_fn = 100, \
        .optimise = CP_CSG2_OPT_DEFAULT, \
        .color_rand = 0, \
    }

/**
 * Options for CSG rendering.
 *
 * Unified for both 2D and 3D parts.
 */
typedef struct {
    /**
     * Gap between layers in STL or SCAD output.
     *
     * This is to make the STL a valid 2-manifold, because without
     * the gaps, bottom and top faces of adjacent layers would be
     * coplanar, which is not well-formed.
     */
    double layer_gap;

    /**
     * How many polygons to process at once, maximally.
     * Must be at least 2.
     */
    size_t max_simultaneous;

    /**
     * Maximum number for $fn up to which to use polyhedron/polygons.
     *
     * For larger values, use round shapes, if available.
     */
    unsigned max_fn;

    /**
     * With which groups to tag the output file, space or comma separated */
    cp_vchar_t js_group;

    /**
     * Optimisation.  See CP_CSG2_OPT* constants. */
    unsigned optimise;

    /**
     * How much to randomize colours during CSG2 algorithm.
     */
    unsigned char color_rand;

    /**
     * Treatment of empty objects
     */
    unsigned err_empty;

    /**
     * Treatment of collapsed objects
     */
    unsigned err_collapse;

    /**
     * Treatment of 3D object in outside 3D context. (CP_ERR_*)
     */
    unsigned err_outside_3d;

    /**
     * Treatment of 2D object outside 2D context. (CP_ERR_*)
     */
    unsigned err_outside_2d;

    /**
     * Copy position and gc back to root of CSG2 tree, so that
     * the '!' does not change the position. */
    bool keep_ctxt;

    /**
     * When only a triangulation is needed, still also generate a path --
     * this is useful for debugging. */
    bool tri_add_path;
} cp_csg_opt_t;


#endif /* CP_CSG_TAM_H_ */
