/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG2_TAM_H_
#define CP_CSG2_TAM_H_

#include <hob3lbase/mat_tam.h>
#include <hob3lbase/dict.h>
#include <hob3lbase/err_tam.h>
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
        cp_obj_t:         CP_ABSTRACT, \
        cp_csg2_t:        CP_CSG_TYPE, \
        cp_csg2_poly_t:   CP_CSG2_POLY, \
        cp_csg2_stack_t:  CP_CSG2_STACK)

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
} cp_csg2_type_t;

#define CP_CSG2_ \
    union { \
        struct { CP_OBJ_(cp_csg2_type_t) }; \
        struct { CP_OBJ_(cp_csg2_type_t) } obj; \
    };

#define CP_CSG2_SIMPLE_ \
    CP_CSG2_ \
    cp_mat2wi_t mat; \
    cp_color_rgba_t color; \
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

typedef struct {
    cp_v_size_t point_idx;
} cp_csg2_path_t;

typedef CP_VEC_T(cp_csg2_path_t) cp_v_csg2_path_t;

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
     * The vertices of the polygon.
     *
     * This stores both the coordinates and the location in the
     * input file (for error messages).
     *
     * Each point must be unique.  Paths and triangles refer to
     * this array.
     */
    cp_v_vec2_loc_t point;

    /**
     * Paths definingthe polygon.
     *
     * This should be equivalent information as in triangle.
     *
     * All paths should be clockwise.  Some processing stages
     * work without this (e.g., triangulation and bool operations
     * do not really care about point order), others require it
     * (like SCAD and STL output).  The bool operation output
     * will correctly fill this in clockwise order.  (I.e., polygon
     * paths subtracting from an outer path will have reverse
     * order.)
     */
    cp_v_csg2_path_t path;

    /**
     * Triangles defining the polygon.
     *
     * This should be equivalent information as in path.
     *
     * All triangles should be clockwise.  Whether this is
     * required depends on the step in the processing pipeline.
     * SCAD and STL output require this to work correctly.
     *
     * Without triangulation run, this is empty.
     */
    cp_v_size3_t triangle;

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
} cp_csg2_tree_t;

/**
 * Maximum number of polygons to delay.
 */
#define CP_CSG2_MAX_LAZY 10

/**
 * Bitmap to store boolean function
 */
typedef union {
    unsigned char      b[((1U << CP_CSG2_MAX_LAZY) +  7) /  8];
    unsigned short     s[((1U << CP_CSG2_MAX_LAZY) + 15) / 16];
    unsigned int       i[((1U << CP_CSG2_MAX_LAZY) + 31) / 32];
    unsigned long long w[((1U << CP_CSG2_MAX_LAZY) + 63) / 64];
} cp_csg2_op_bitmap_t;

/**
 * An unresolved polygon combination.
 */
typedef struct {
    /** Number of polygons to be combined (valid entries in \a data) */
    size_t size;
    /** Polygons to be combined */
    cp_csg2_poly_t *data[CP_CSG2_MAX_LAZY];
    /**
     * Boolean combination map to decide from a mask of inside bits for each
     * polygon whether the result is inside.  This is indexed bitwise with the
     * mask of bits.  The number of entries is (1U << size) bits. */
    cp_csg2_op_bitmap_t comb;
} cp_csg2_lazy_t;

#endif /* CP_CSG2_TAM_H_ */
