/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_SCAD_TAM_H
#define __CP_SCAD_TAM_H

#include <hob3lbase/def.h>
#include <hob3lbase/mat_tam.h>
#include <hob3l/err_tam.h>
#include <hob3l/scad_fwd.h>
#include <hob3l/gc_tam.h>

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

    CP_SCAD_COLOR,
} cp_scad_type_t;

#define _CP_SCAD \
    cp_scad_type_t type; \
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

/**
 * To store union, difference, or intersection.
 * Also the base for transformations.
 */
typedef struct {
    _CP_SCAD_GROUP
} cp_scad_combine_t;

/**
 * To store translate, scale, mirror */
typedef struct {
    _CP_SCAD_GROUP
    cp_vec3_t v;
} cp_scad_xyz_t;

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

typedef cp_scad_combine_t cp_scad_union_t;
typedef cp_scad_combine_t cp_scad_difference_t;
typedef cp_scad_combine_t cp_scad_intersection_t;

typedef cp_scad_xyz_t cp_scad_translate_t;
typedef cp_scad_xyz_t cp_scad_mirror_t;
typedef cp_scad_xyz_t cp_scad_scale_t;

union cp_scad {
    struct {
        _CP_SCAD;
    };
    cp_scad_sphere_t _sphere;
    cp_scad_cylinder_t _cylinder;
    cp_scad_cube_t _cube;
    cp_scad_polyhedron_t _polyhedron;

    cp_scad_combine_t _combine;
    cp_scad_combine_t _union;
    cp_scad_combine_t _difference;
    cp_scad_combine_t _intersection;

    cp_scad_xyz_t _xyz;
    cp_scad_xyz_t _translate;
    cp_scad_xyz_t _mirror;
    cp_scad_xyz_t _scale;

    cp_scad_rotate_t _rotate;
    cp_scad_multmatrix_t _multmatrix;

    cp_scad_circle_t _circle;
    cp_scad_square_t _square;
    cp_scad_polygon_t _polygon;

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
