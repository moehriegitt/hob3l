/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_TAM_H_
#define CP_CSG2_TAM_H_

#include <hob3lmat/mat_tam.h>
#include <hob3lbase/dict.h>
#include <hob3lbase/err_tam.h>
#include <hob3lbase/bool-bitmap_tam.h>
#include <hob3lop/gon_tam.h>
#include <hob3l/csg_tam.h>
#include <hob3l/csg2_fwd.h>
#include <hob3l/csg3_fwd.h>
#include <hob3l/gc_tam.h>

/**
 * Map type to type ID.
 *
 * If this maps to CP_OBJ, then no object can be generated,
 * but the type is a supertype.
 */
#define cp_csg2_typeof(type) \
    _Generic(type, \
        cp_obj_t:          CP_ABSTRACT, \
        cp_csg2_t:         CP_CSG_TYPE, \
        cp_csg2_poly_t:    CP_CSG2_POLY, \
        cp_csg2_stack_t:   CP_CSG2_STACK, \
        cp_csg2_vline2_t:  CP_CSG2_VLINE2)

/**
 * 2D CSG basic shapes.
 *
 * The idea is that this is output from some other program that
 * generates normalised CSG objects.  Therefore, this has no
 * convenience shapes (like rectangles) for anthing that can be
 * reduced to a polygon.  Also, the basic shapes are normalised and
 * have as few parameters as possible.
 *
 * Note: the structure restricts how 'add' can be used: 'cut' and 'sub'
 * must have children of type 'add'.
 *
 * FIXME: We might need more shapes here, i.e., all shapes that can
 * emerge from cutting cylinders, cones, and frustrums with a plane.
 * If not, we'd have to convert them to polygons, which I'd like to
 * avoid as long as possible.
 */
typedef enum {
    CP_CSG2_ADD = CP_CSG_ADD,
    CP_CSG2_SUB = CP_CSG_SUB,
    CP_CSG2_CUT = CP_CSG_CUT,
    CP_CSG2_XOR = CP_CSG_XOR,

    /**
     * Polygon */
    CP_CSG2_POLY = CP_CSG2_TYPE + 1,

    /**
     * A stack of 2D layers */
    CP_CSG2_STACK,

    /**
     * A set of lines representing a polygon.  This is a simplified
     * representation with less order than CP_CSG2_POLY -- the internal
     * algorithms use this.  It has integer coordinates instead of
     * the FP that is used elsewhere.
     *
     * This polygon representation is used internally when no
     * CP_CSG2_POLY is needed and it can be produced more easily,
     * e.g., when slicing a polygon from a polyhedron.
     */
    CP_CSG2_VLINE2,

    /**
     * The working structure of combining multiple polygons into a
     * single one, e.g., a cq_sweep_t.  This is used when the output
     * format is not yet determined or for intermediate results
     * to avoid an export into a cp_csg2_vline_t or cp_csg2_poly_t.
     *
     * This is used only internally in the csg2-bool module as an
     * intermediate step during the recursive computation of a polygon
     * tree flattening, as part of a cp_csg2_lazy_t structure.
     */
    CP_CSG2_SWEEP = CQ_OBJ_TYPE_SWEEP,
} cp_csg2_type_t;

#define CP_CSG2_ \
    union { \
        struct { CP_OBJ_(cp_csg2_type_t) }; \
        struct { CP_OBJ_(cp_csg2_type_t) } obj; \
    };

#define CP_CSG2_SIMPLE_ \
    CP_CSG2_ \
    cp_mat2wi_t mat; \
    cp_f_t _fa, _fs; \
    size_t _fn;

struct cp_csg2_circle {
    /**
     * type is CP_CSG2_CIRCLE */
    CP_CSG2_SIMPLE_
};

typedef struct {
    cp_csg_add_t *root;
    size_t zi;
} cp_csg2_layer_t;

typedef CP_VEC_T(cp_csg2_layer_t) cp_v_csg2_layer_t;
typedef CP_ARR_T(cp_csg2_layer_t) cp_a_csg2_layer_t;

struct cp_csg2_stack {
    /**
     * type is CP_CSG2_STACK */
    CP_CSG2_

    /**
     * Actual first global index at index[0] in \a layer */
    size_t idx0;

    /**
     * the actual layers */
    cp_v_csg2_layer_t layer;

    /**
     * the 3D object represented by this stack */
    cp_csg3_t const *csg3;
};

/**
 * A 2D polygon is actually many polygons, called paths here.
 *
 * The semantics is that the area described is a XOR combination
 * of the area the paths describe.
 *
 * The algorithm for constructing the 2D paths will ensure that
 * the polygon outside is on the left of an edge (when src is back
 * and dst is fore).  With this info, polygons that are actually
 * subtracting can be identified.  By the intervals order, a tree
 * of 'sub' can be generated.  We might do this transformation later.
 *
 * This can also or alternatively store a triangulation.  Depending
 * on the function, either path or triangle will be filled in.
 */
struct cp_csg2_poly {
    /**
     * type is CP_CSG2_POLY */
    CP_CSG2_

    /**
     * The unboxed 2D polygon representation */
    union {
        cq_csg2_poly_t q;
        CQ_CSG2_POLY_T;
    };

    /**
     * If available, the result of subtracting the previous layer
     * from this one.  For output modules that support this,
     * this can be used to draw the bottom plane instead of drawing
     * the full polygon.
     */
    cp_csg2_poly_t *diff_below;

    /**
     * If available, the result of subtracting the next layer
     * from this one.  For output modules that support this,
     * this can be used to draw the top plane instead of drawing
     * the full polygon.
     */
    cp_csg2_poly_t *diff_above;
};

/**
 * A 2D polygon represented as a set of lines with integer coordinates.
 *
 * This is used for internal exact computations: slicing, 2D bool
 * operations, triangulation, and polygon reconstruction (VLINE2->POLY
 * reconstruction).  This is, thus, related to the 'cq_' library
 * operations using exact and robust arithmetics.
 */
struct cp_csg2_vline2 {
    /**
     * type is CP_CSG2_VLINE2 */
    CP_CSG2_

    /**
     * The unboxed 2D polygon representation as a set (~vector)
     * of lines. */
    cq_v_line2_t q;
};

/**
 * CSG2 Version of an object.
 *
 * This indicates that (mainly) 2D objects are stored/processed.
 */
struct cp_csg2 { CP_CSG2_ };


/**
 * Whether the layer polygon is empty
 */
#define CP_CSG2_FLAG_NON_EMPTY 1

typedef struct {
    /**
     * z coordinates of layers
     */
    cp_a_double_t z;

    /**
     * Bitmap of CP_CSG2_FLAG_* entries for each layer
     */
    cp_a_size_t flag;

    /**
     * The tree root
     */
    cp_csg2_t *root;

    /**
     * Global layer thickness
     */
    double thick;

    /**
     * Options for conversion or generation of output formats.
     */
    cp_csg_opt_t const *opt;

    /**
     * If non-NULL, the transformation of the root node.
     * This points into the CSG3 structure.
     */
    cp_mat3wi_t const *root_xform;
} cp_csg2_tree_t;

typedef struct {
    cp_vec2_loc_t *ref;
    cp_loc_t loc;
} cp_vec2_loc_ref_t;

typedef CP_ARR_T(cp_vec2_loc_ref_t) cp_a_vec2_loc_ref_t;

#endif /* CP_CSG2_TAM_H_ */
