/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_CSG3_TAM_H_
#define CP_CSG3_TAM_H_

#include <hob3lbase/err_tam.h>
#include <hob3lbase/obj_tam.h>
#include <hob3lbase/base-mat.h>
#include <hob3l/gc_tam.h>
#include <hob3l/csg_tam.h>
#include <hob3l/csg2_tam.h>
#include <hob3l/csg3_fwd.h>

/**
 * Map type to type ID */
#define cp_csg3_typeof(type) \
    _Generic(type, \
        cp_obj_t:         CP_ABSTRACT, \
        cp_csg3_t:        CP_CSG_TYPE, \
        cp_csg3_sphere_t: CP_CSG3_SPHERE, \
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
    CP_CSG3_ADD = CP_CSG_ADD,
    CP_CSG3_SUB = CP_CSG_SUB,
    CP_CSG3_CUT = CP_CSG_CUT,
    CP_CSG3_XOR = CP_CSG_XOR,

    /**
     * Sphere with radius 1, centered a [0,0,0] */
    CP_CSG3_SPHERE = CP_CSG3_TYPE + 1,

    /**
     * Polyhedron */
    CP_CSG3_POLY,
} cp_csg3_type_t;

#define CP_CSG3_ \
    union { \
        struct { CP_OBJ_(cp_csg3_type_t) }; \
        struct { CP_OBJ_(cp_csg3_type_t) } obj; \
    }; \
    cp_gc_t gc;

#define CP_CSG3_SIMPLE_ \
    CP_CSG3_ \
    cp_mat3wi_t const *mat; \
    size_t _fn;

typedef struct {
    /**
     * type is CP_CSG3_SPHERE */
    CP_CSG3_SIMPLE_
} cp_csg3_sphere_t;

typedef struct {
    /**
     * type is CP_CSG3_2D */
    CP_CSG3_SIMPLE_
    cp_csg2_t *csg2;
} cp_csg3_2d_t;

struct cp_csg3_face {
    /**
     * Point array.  Uses the same index as the edge
     * in edge array starting at this point */
    cp_a_vec3_loc_ref_t point;

    /**
     * Source location of face. */
    cp_loc_t loc;
};

typedef CP_VEC_T(cp_csg3_face_t) cp_v_csg3_face_t;

typedef struct {
    /**
     * type is CP_CSG3_POLY */
    CP_CSG3_

    /**
     * All points in the polyhedron.
     * This is the first part of this poly that is constructed.  Edges are
     * constructed from this only after a polyhedron has been completely
     * defined in terms of edges and faces.  Note that each face also has
     * redundant storage of the shape in a point and an edge array.
     */
    cp_a_vec3_loc_t point;

    /**
     * The faces of the polyhedron. */
    cp_v_csg3_face_t face;
} cp_csg3_poly_t;

typedef cp_csg2_poly_t cp_csg3_poly2_t;

/**
 * CSG3 Version of an object.
 *
 * This indicates that (mainly) 2D objects are stored/processed.
 */
struct cp_csg3 { CP_CSG3_; };

typedef struct {
    cp_v_mat3wi_p_t mat;
    cp_csg_add_t *root;
    cp_csg_opt_t const *opt;
    cp_mat3wi_t const *root_xform;
} cp_csg3_tree_t;

#endif /* CP_CSG3_TAM_H_ */
