/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG_TAM_H
#define __CP_CSG_TAM_H

#include <hob3l/obj_tam.h>
#include <hob3l/csg_fwd.h>
#include <hob3l/csg2_fwd.h>
#include <hob3l/csg3_fwd.h>

#define cp_csg_typeof(type) \
    _Generic(type, \
        cp_obj_t:     CP_ABSTRACT, \
        cp_csg_t:     CP_CSG_TYPE, \
        cp_csg_add_t: CP_CSG_ADD, \
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
} cp_csg_type_t;

typedef struct {
    /**
     * type is CP_CSG_ADD */
    _CP_OBJ
    cp_v_obj_p_t add;
} cp_csg_add_t;

typedef struct {
    /**
     * type is CP_CSG_SUB */
    _CP_OBJ
    cp_csg_add_t *add;
    cp_csg_add_t *sub;
} cp_csg_sub_t;

typedef CP_VEC_T(cp_csg_add_t*) cp_v_csg_add_p_t;

typedef struct {
    /**
     * type is CP_CSG_CUT */
    _CP_OBJ
    cp_v_csg_add_p_t cut;
} cp_csg_cut_t;

/**
 * cp_csg_t is basically a cp_obj_t indicating CSG handling.
 *
 * This is used to indicate that mainly CSG data is processed.
 * Using cp_obj_t would work, too, but this type is an additional
 * abstract type for clarity.
 */
struct cp_csg { _CP_OBJ };

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
     * Optimisation.  See CP_CSG2_OPT* constants. */
    unsigned optimise;

    /**
     * How much to randomize colours during CSG2 algorithm.
     */
    unsigned char color_rand;

    /**
     * Maximum number for $fn up to which to use polyhedron/polygons.
     *
     * For larger values, use round shapes, if available.
     */
    size_t max_fn;

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
} cp_csg_opt_t;


#endif /* __CP_CSG_TAM_H */
