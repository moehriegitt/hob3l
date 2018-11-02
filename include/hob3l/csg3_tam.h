/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG3_TAM_H
#define __CP_CSG3_TAM_H

#include <hob3lbase/mat_tam.h>
#include <hob3lbase/err_tam.h>
#include <hob3l/gc_tam.h>
#include <hob3l/obj_tam.h>
#include <hob3l/csg_tam.h>
#include <hob3l/csg2_tam.h>
#include <hob3l/csg3_fwd.h>

/**
 * Whether to support fully circular cylinders.
 * FIXME: Not yet implemented.
 */
#define CP_CSG3_CIRCULAR_CYLINDER 0

/**
 * Map type to type ID */
#define cp_csg3_typeof(type) \
    _Generic(type, \
        cp_obj_t:         CP_ABSTRACT, \
        cp_csg_t:         CP_ABSTRACT, \
        cp_csg2_t:        CP_ABSTRACT, \
        cp_csg_add_t:     CP_CSG_ADD, \
        cp_csg_sub_t:     CP_CSG_SUB, \
        cp_csg_cut_t:     CP_CSG_CUT, \
        cp_csg2_poly_t:   CP_CSG2_POLY, \
        cp_csg3_sphere_t: CP_CSG3_SPHERE, \
        cp_csg3_cyl_t:    CP_CSG3_CYL, \
        cp_csg3_poly_t:   CP_CSG3_POLY)

/**
 * 3D CSG basic shapes and operations.
 *
 * The idea is that this is output from some other program that
 * generates normalised CSG objects.  Therefore, this has no
 * convenience shapes (like cubes) for anthing that can be reduced to
 * a polyhedron.  Also, the basic shapes are normalised and have as
 * few parameters as possible.
 *
 * Note: the structure restricts how 'add' can be used: 'cut' and 'sub'
 * must have children of type 'add'.
 */
typedef enum {
    /* Have add, sub, cut in this enum for switch() */
    CP_CSG3_ADD = CP_CSG_ADD,
    CP_CSG3_SUB = CP_CSG_SUB,
    CP_CSG3_CUT = CP_CSG_CUT,

    /**
     * Sphere with radius 1, centered a [0,0,0] */
    CP_CSG3_SPHERE = CP_CSG3_TYPE + 1,

    /**
     * Cylinder with length 1, radius 1, centered at [0,0,0], along z-axis.
     * The top radius can be set (so this also implements cone and frustrum). */
    CP_CSG3_CYL,

    /**
     * Polyhedron */
    CP_CSG3_POLY,
} cp_csg3_type_t;

#define _CP_CSG3 \
    union { \
        struct { _CP_OBJ }; \
        struct { _CP_OBJ } obj; \
    }; \
    cp_gc_t gc;

#define _CP_CSG3_SIMPLE \
    _CP_CSG3 \
    cp_mat3wi_t const *mat; \
    cp_f_t _fa, _fs; \
    size_t _fn;

typedef struct {
    /**
     * type is CP_CSG3_SPHERE */
    _CP_CSG3_SIMPLE
} cp_csg3_sphere_t;

typedef struct {
    /**
     * type is CP_CSG3_2D */
    _CP_CSG3_SIMPLE
    cp_csg2_t *csg2;
} cp_csg3_2d_t;

typedef struct {
    /**
     * type is CP_CSG3_CYL */
    _CP_CSG3_SIMPLE
    double r2;
} cp_csg3_cyl_t;

struct cp_csg3_edge {
    /**
     * Source point of foreward edge.
     *
     * Points to the source point ref in \a fore->point
     * This defines the index in fore->point and fore->edge arrays.
     * This also locates the foreword edge in the input code.
     */
    cp_vec3_loc_ref_t *src;

    /**
     * Destination point of foreward edge.
     *
     * Points to the source point ref in \a back->point.
     * This defines the index in back->point and back->edge arrays.
     * This also locates the backward edge in the input code.
     */
    cp_vec3_loc_ref_t *dst;

    /**
     * Face where this edge is used in forward direction
     * The index in fore->edge is cp_v_idx(&fore->point, src);
     */
    cp_csg3_face_t *fore;

    /**
     * Face where this edge is used in backward direction.
     * The index in back->edge is cp_v_idx(&back->point, dst);
     */
    cp_csg3_face_t *back;
};

typedef CP_ARR_T(cp_csg3_edge_t)  cp_a_csg3_edge_t;
typedef CP_ARR_T(cp_csg3_edge_t*) cp_a_csg3_edge_p_t;

struct cp_csg3_face {
    /**
     * Point array.  Uses the same index as the edge
     * in edge array starting at this point */
    cp_a_vec3_loc_ref_t point;

    /**
     * Edge array.  This is stored additional to the point array
     * both for checking that the definition is sound and because
     * it is a more useful representation for the transformation into
     * 2D space, as each edge that is cut will become one point in 2D.
     */
    cp_a_csg3_edge_p_t edge;

    /**
     * Source location of face. */
    cp_loc_t loc;
};

typedef CP_ARR_T(cp_csg3_face_t) cp_a_csg3_face_t;

typedef struct {
    /**
     * type is CP_CSG3_POLY */
    _CP_CSG3

    /**
     * All points in the polyhedron.
     * This is the first part of this poly that is constructed.  Edges are
     * constructed from this only after a polyhedron has been completely
     * defined in terms of edges and faces.  Note that each face also has
     * redundant storage of the shape in a point and an edge array.
     */
    cp_a_vec3_loc_t point;

    /**
     * All edges in the polyhedron.  This is allocated to double
     * the necessary size in order to do proper error handling.
     * Only the first half of it is used only foreward edges are
     * stored and backward edges are mapped to the same index.
     * edge->size, nevertheless, has the correct number, only
     * the array is allocated to edge->size*2 entries.  It cannot
     * currently be reclaimed because pointers into this array
     * are used and realloc() would invalidate them.
     */
    cp_a_csg3_edge_t edge;

    /**
     * The faces of the polyhedron. */
    cp_a_csg3_face_t face;

    /**
     * This is a full cube, i.e., the bounding box is exactly the
     * polyhedron.  This may have false negatives, e.g., if a cube is
     * defined by 'polyhedron' in SCAD instead of 'cube', then this
     * will be false.
     * (FIXME: not yet implemented)
     */
    bool is_cube;
} cp_csg3_poly_t;

typedef cp_csg2_poly_t cp_csg3_poly2_t;

/**
 * Any of the CP_CSG3_* objects and some CP_CSG2_*.
 */
union cp_csg3 {
    struct { _CP_CSG3 };
    cp_csg_t _csg;
    cp_csg3_sphere_t _sphere;
    cp_csg_add_t _add;
    cp_csg_sub_t _sub;
    cp_csg_cut_t _cut;
    cp_csg3_cyl_t _cyl;
    cp_csg3_poly_t _poly;
    cp_csg2_poly_t _poly2;
};

typedef struct {
    size_t max_fn;
} cp_csg3_opt_t;

typedef struct {
    cp_csg3_opt_t opt;
    cp_v_mat3wi_p_t mat;
    cp_csg_add_t *root;
} cp_csg3_tree_t;

#endif /* __CP_CSG3_TAM_H */
