/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_SCAD_TAM_H
#define __CP_SCAD_TAM_H

#include <hob3lbase/def.h>
#include <hob3lbase/mat_tam.h>
#include <hob3lbase/err_tam.h>
#include <hob3l/scad_fwd.h>
#include <hob3l/gc_tam.h>

/**
 * Map type to type ID */
#define cp_scad_typeof(type) \
    _Generic(type, \
        cp_scad_union_t:        CP_SCAD_UNION, \
        cp_scad_difference_t:   CP_SCAD_DIFFERENCE, \
        cp_scad_intersection_t: CP_SCAD_INTERSECTION, \
        cp_scad_sphere_t:       CP_SCAD_SPHERE, \
        cp_scad_cylinder_t:     CP_SCAD_CYLINDER, \
        cp_scad_cube_t:         CP_SCAD_CUBE, \
        cp_scad_polyhedron_t:   CP_SCAD_POLYHEDRON, \
        cp_scad_multmatrix_t:   CP_SCAD_MULTMATRIX, \
        cp_scad_translate_t:    CP_SCAD_TRANSLATE, \
        cp_scad_mirror_t:       CP_SCAD_MIRROR, \
        cp_scad_scale_t:        CP_SCAD_SCALE, \
        cp_scad_rotate_t:       CP_SCAD_ROTATE, \
        cp_scad_circle_t:       CP_SCAD_CIRCLE, \
        cp_scad_square_t:       CP_SCAD_SQUARE, \
        cp_scad_polygon_t:      CP_SCAD_POLYGON, \
        cp_scad_linext_t:       CP_SCAD_LINEXT, \
        cp_scad_color_t:        CP_SCAD_COLOR)

/**
 * Type IDs for SCAD module */
typedef enum {
    CP_SCAD_UNION = CP_SCAD_TYPE + 1,
    CP_SCAD_DIFFERENCE,
    CP_SCAD_INTERSECTION,

    CP_SCAD_SPHERE,
    CP_SCAD_CUBE,
    CP_SCAD_CYLINDER,
    CP_SCAD_POLYHEDRON,

    CP_SCAD_MULTMATRIX,
    CP_SCAD_TRANSLATE,
    CP_SCAD_MIRROR,
    CP_SCAD_SCALE,
    CP_SCAD_ROTATE,

    CP_SCAD_CIRCLE,
    CP_SCAD_SQUARE,
    CP_SCAD_POLYGON,

    CP_SCAD_LINEXT,

    CP_SCAD_COLOR,
} cp_scad_type_t;

#define _CP_SCAD \
    unsigned type; \
    char const *loc; \
    unsigned modifier;

typedef struct {
    _CP_SCAD
    double r;
    double _fa;
    double _fs;
    unsigned _fn;
} cp_scad_sphere_t;

typedef struct {
    _CP_SCAD
    double r;
    double _fa;
    double _fs;
    unsigned _fn;
} cp_scad_circle_t;

typedef struct {
    _CP_SCAD
    double h;
    double r1;
    double r2;
    bool center;
    double _fa;
    double _fs;
    unsigned _fn;
} cp_scad_cylinder_t;

typedef struct {
    _CP_SCAD
    cp_vec3_t size;
    bool center;
} cp_scad_cube_t;

typedef struct {
    _CP_SCAD
    cp_vec2_t size;
    bool center;
} cp_scad_square_t;

typedef struct {
    cp_a_vec3_loc_ref_t points;
    cp_loc_t loc;
} cp_scad_face_t;

typedef CP_VEC_T(cp_scad_face_t) cp_a_scad_face_t;

typedef struct {
    _CP_SCAD
    cp_a_vec3_loc_t points;
    cp_a_scad_face_t faces;
} cp_scad_polyhedron_t;

typedef struct {
    cp_a_vec2_loc_ref_t points;
    cp_loc_t loc;
} cp_scad_path_t;

typedef CP_VEC_T(cp_scad_path_t) cp_a_scad_path_t;

typedef struct {
    _CP_SCAD
    cp_a_vec2_loc_t points;
    cp_a_scad_path_t paths;
    unsigned convexity;
} cp_scad_polygon_t;

typedef CP_VEC_T(cp_scad_t*) cp_v_scad_p_t;

#define _CP_SCAD_GROUP \
    _CP_SCAD \
    cp_v_scad_p_t child;

#define _CP_SCAD_GROUP_XYZ \
    _CP_SCAD_GROUP \
    cp_vec3_t v;

/**
 * To store union.
 */
typedef struct {
    _CP_SCAD_GROUP
} cp_scad_union_t;

/**
 * To store intersection.
 */
typedef struct {
    _CP_SCAD_GROUP
} cp_scad_intersection_t;

/**
 * To store difference.
 */
typedef struct {
    _CP_SCAD_GROUP
} cp_scad_difference_t;

/**
 * To store translate */
typedef struct {
    _CP_SCAD_GROUP_XYZ
} cp_scad_translate_t;

/**
 * To store mirror */
typedef struct {
    _CP_SCAD_GROUP_XYZ
} cp_scad_mirror_t;

/**
 * To store scale */
typedef struct {
    _CP_SCAD_GROUP_XYZ
} cp_scad_scale_t;

typedef struct {
    _CP_SCAD_GROUP
    /**
     * If true, rotate around n at angle a.
     * If false, rotate around all three axes at n.z, n.y, n.x.
     */
    bool around_n;
    cp_vec3_t n;
    double a;
} cp_scad_rotate_t;

/**
 * color()
 *
 * Tagged with CP_SCAD_COLOR.
 */
typedef struct {
    _CP_SCAD_GROUP
    cp_color_rgba_t rgba;
    /**
     * Whether rgba is valid or whether the
     * color setting is to be ignored (in case c was 'undef')
     */
    bool valid;
} cp_scad_color_t;

typedef struct {
    _CP_SCAD_GROUP
    cp_mat3w_t m;
} cp_scad_multmatrix_t;

typedef struct {
    _CP_SCAD_GROUP
    cp_f_t height;
    cp_f_t twist;
    cp_vec2_t scale;
    size_t slices;
    unsigned convexity;
    bool center;
} cp_scad_linext_t;

union cp_scad {
    struct {
        _CP_SCAD;
    };

    cp_scad_sphere_t _sphere;
    cp_scad_cylinder_t _cylinder;
    cp_scad_cube_t _cube;
    cp_scad_polyhedron_t _polyhedron;

    cp_scad_union_t _union;
    cp_scad_difference_t _difference;
    cp_scad_intersection_t _intersection;

    cp_scad_translate_t _translate;
    cp_scad_mirror_t _mirror;
    cp_scad_scale_t _scale;

    cp_scad_rotate_t _rotate;
    cp_scad_multmatrix_t _multmatrix;

    cp_scad_circle_t _circle;
    cp_scad_square_t _square;
    cp_scad_polygon_t _polygon;

    cp_scad_linext_t _linext;

    cp_scad_color_t _color;
};

typedef struct {
    /**
     * The top-level of the file.
     */
    cp_v_scad_p_t toplevel;

    /**
     * If any sub-tree is marked as 'root', this will be set to
     * non-NULL to mark that subtree.
     */
    cp_scad_t *root;
} cp_scad_tree_t;

/*
 * UNSUPPORTED:
 *
 * use
 * include
 * import
 *
 * linear_extrude (w/ limited settings, maybe)
 * offset
 * render
 * children
 *
 * rotate_extrude
 * function
 * module
 * var = value
 *
 * text
 *
 * for
 * intersection_for
 * echo
 * if
 *
 * Any functions (sin, cos, *, concat, ...).
 *
 * $fa        minimum angle
 * $fs        minimum size
 * $fn        number of fragments
 * $t         animation step
 * $vpr       viewport rotation angles in degrees
 * $vpt       viewport translation
 * $vpd       viewport camera distance
 * $children  number of module children
 *
 * LIKELY NEVER SUPPORTED:
 *
 * hull
 * minkowski
 * resize
 * surface
 * projection
 */

#endif /* __CP_SCAD_TAM_H */
