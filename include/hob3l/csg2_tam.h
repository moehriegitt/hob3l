/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG2_TAM_H
#define __CP_CSG2_TAM_H

#include <hob3lbase/mat_tam.h>
#include <hob3lbase/dict.h>
#include <hob3l/err_tam.h>
#include <hob3l/csg2_fwd.h>
#include <hob3l/csg3_fwd.h>
#include <hob3l/gc_tam.h>

/**
 * Whether to support circles in 2D engine.
 * FIXME: Not yet implemented.
 */
#define CP_CSG2_CIRCLULAR 0

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
    /**
     * Circle with radius 1, centered a [0,0] */
    CP_CSG2_CIRCLE = CP_CSG2_TYPE + 1,

    /**
     * Polygon */
    CP_CSG2_POLY,

    /**
     * Bool op: union (boolean '|') */
    CP_CSG2_ADD,

    /**
     * Bool op: difference (boolean '&~')*/
    CP_CSG2_SUB,

    /**
     * Bool op: cut (boolean '&') */
    CP_CSG2_CUT,

    /**
     * A stack of 2D layers */
    CP_CSG2_STACK,
} cp_csg2_type_t;

#define _CP_CSG2 \
    unsigned type; \
    char const *loc;

#define _CP_CSG2_OBJ_ \
    _CP_CSG2 \
    cp_gc_t gc;

#define _CP_CSG2_OBJ \
    union { \
        struct { \
            _CP_CSG2_OBJ_ \
        }; \
        struct { \
            _CP_CSG2_OBJ_ \
        } obj; \
    };

#define _CP_CSG2_SIMPLE \
    _CP_CSG2_OBJ \
    cp_mat2wi_t mat; \
    cp_f_t _fa, _fs; \
    size_t _fn;

typedef struct {
    /**
     * type is CP_CSG2_CIRCLE */
    _CP_CSG2_SIMPLE
} cp_csg2_circle_t;

typedef CP_VEC_T(cp_csg2_t*) cp_v_csg2_p_t;

typedef struct {
    /**
     * type is CP_CSG2_ADD */
    _CP_CSG2
    cp_v_csg2_p_t add;
} cp_csg2_add_t;

typedef CP_VEC_T(cp_csg2_add_t*) cp_v_csg2_add_p_t;

typedef struct {
    /**
     * type is CP_CSG2_SUB */
    _CP_CSG2
    cp_csg2_add_t add;
    cp_csg2_add_t sub;
} cp_csg2_sub_t;

typedef struct {
    /**
     * type is CP_CSG2_CUT */
    _CP_CSG2
    cp_v_csg2_add_p_t cut;
} cp_csg2_cut_t;

typedef struct {
    cp_csg2_add_t root;
    size_t zi;
} cp_csg2_layer_t;

typedef CP_VEC_T(cp_csg2_layer_t) cp_v_csg2_layer_t;
typedef CP_ARR_T(cp_csg2_layer_t) cp_a_csg2_layer_t;

typedef struct {
    /**
     * type is CP_CSG2_STACK */
    _CP_CSG2

    /**
     * Actual first global index at index[0] in \a layer */
    size_t idx0;

    /**
     * the actual layers */
    cp_v_csg2_layer_t layer;

    /**
     * the 3D object represented by this stack */
    cp_csg3_t const *csg3;
} cp_csg2_stack_t;

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
    _CP_CSG2_OBJ

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

union cp_csg2 {
    struct {
        _CP_CSG2
    };
    cp_csg2_circle_t _circle;
    cp_csg2_poly_t _poly;
    cp_csg2_add_t _add;
    cp_csg2_sub_t _sub;
    cp_csg2_cut_t _cut;
    cp_csg2_stack_t _stack;
};

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
     * How much to randomize colours in JS/WebGL output to
     * make the shapes easier to grasp.
     */
    unsigned char js_color_rand;
} cp_csg2_tree_opt_t;


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
    cp_csg2_tree_opt_t opt;
} cp_csg2_tree_t;

/**
 * Internal to algorithm: node list cell for triangulation algorithm.
 *
 * This whole type is internal, but needs to be exposed because
 * this is C and it is needed to define the cp_csg2_3edge_t
 * data type which has an inline object of this type.
 */
struct cp_csg2_3list {
    cp_csg2_3node_t *node;
    union {
        struct {
            cp_csg2_3list_t *next;
            cp_csg2_3list_t *prev;
        };
        cp_csg2_3list_t *step[2];
    };
};

/**
 * Node for triangulation algorithm.
 *
 * This exposes more than it should, but that's life with C.  This
 * is only needed for special purpose invocation of the triangulation
 * algorith where lowlevel nodes and edges are defined instead of
 * using the higher level types cp_csg2_poly_t etc.
 *
 * When preparing this for the triangulation algorithm, this needs to
 * be zeroed and then only the slots \a point, \a in, and \a out need
 * to be set up properly to define a set of polygons.
 */
struct cp_csg2_3node {
    /** internal to algorithm: node for 'X structure' */
    cp_dict_t node_nx;

    /** the coordinate of this point */
    cp_vec2_t *coord;

    /** incoming edge of node on polygon */
    cp_csg2_3edge_t *in;

    /** outgoing edge of node on polygon */
    cp_csg2_3edge_t *out;

    /**
     * Location of the point in the input file.
     *
     * If this point is not directly in the input, it is better
     * (= more useful to a user) to point to the enclosing object
     * than to set this it NULL.  NULL is not illegal, though.
     */
    cp_loc_t loc;
};

/**
 * Edge for triangulation algorithm
 *
 * This exposes more than it should, but that's life with C.  This
 * is only needed for special purpose invocation of the triangulation
 * algorith where lowlevel nodes and edges are defined instead of
 * using the higher level types cp_csg2_poly_t etc.
 *
 * When preparing this for the triangulation algorithm, this needs to
 * be zeroed and then only the slots \a src, and \a dst need
 * to be set up properly to define a set of polygons.
 */
struct cp_csg2_3edge {
    /** internal to algorithm: edge in 'Y structure */
    cp_dict_t node_ey;

    /** source node of edge */
    cp_csg2_3node_t *src;

    /** destination node of edge */
    cp_csg2_3node_t *dst;

    /** internal to algorithm: linked list nodes ('C structure') */
    unsigned type;
    cp_csg2_3list_t list;
    cp_csg2_3list_t *rm;
};

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

#endif /* __CP_CSG2_TAM_H */
