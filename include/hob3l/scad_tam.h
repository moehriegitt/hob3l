/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_SCAD_TAM_H_
#define CP_SCAD_TAM_H_

#include <hob3lmat/mat_tam.h>
#include <hob3lbase/base-def.h>
#include <hob3lbase/err_tam.h>
#include <hob3lbase/obj_tam.h>
#include <hob3lbase/base-mat_tam.h>
#include <hob3l/scad_fwd.h>
#include <hob3l/gc_tam.h>
#include <hob3l/csg2.h>

/**
 * Map type to type ID */
#define cp_scad_typeof(type) \
    _Generic(type, \
        cp_obj_t:               CP_ABSTRACT, \
        cp_scad_t:              CP_SCAD_TYPE, \
        cp_scad_rec_t:          CP_SCAD_REC_TYPE, \
        cp_scad_union_t:        CP_SCAD_UNION, \
        cp_scad_difference_t:   CP_SCAD_DIFFERENCE, \
        cp_scad_intersection_t: CP_SCAD_INTERSECTION, \
        cp_scad_multmatrix_t:   CP_SCAD_MULTMATRIX, \
        cp_scad_translate_t:    CP_SCAD_TRANSLATE, \
        cp_scad_mirror_t:       CP_SCAD_MIRROR, \
        cp_scad_scale_t:        CP_SCAD_SCALE, \
        cp_scad_rotate_t:       CP_SCAD_ROTATE, \
        cp_scad_sphere_t:       CP_SCAD_SPHERE, \
        cp_scad_cylinder_t:     CP_SCAD_CYLINDER, \
        cp_scad_cube_t:         CP_SCAD_CUBE, \
        cp_scad_polyhedron_t:   CP_SCAD_POLYHEDRON, \
        cp_scad_import_t:       CP_SCAD_IMPORT, \
        cp_scad_surface_t:      CP_SCAD_SURFACE, \
        cp_scad_circle_t:       CP_SCAD_CIRCLE, \
        cp_scad_square_t:       CP_SCAD_SQUARE, \
        cp_scad_polygon_t:      CP_SCAD_POLYGON, \
        cp_scad_projection_t:   CP_SCAD_PROJECTION, \
        cp_scad_linext_t:       CP_SCAD_LINEXT, \
        cp_scad_rotext_t:       CP_SCAD_ROTEXT, \
        cp_scad_text_t:         CP_SCAD_TEXT, \
        cp_scad_hull_t:         CP_SCAD_HULL, \
        cp_scad_color_t:        CP_SCAD_COLOR)

/**
 * Type IDs for SCAD module */
typedef enum {
    CP_SCAD_UNION = CP_SCAD_REC_TYPE + 1,
    CP_SCAD_DIFFERENCE,
    CP_SCAD_INTERSECTION,

    CP_SCAD_MULTMATRIX,
    CP_SCAD_TRANSLATE,
    CP_SCAD_MIRROR,
    CP_SCAD_SCALE,
    CP_SCAD_ROTATE,

    CP_SCAD_SPHERE = CP_SCAD_TYPE + 1,
    CP_SCAD_CUBE,
    CP_SCAD_CYLINDER,
    CP_SCAD_POLYHEDRON,
    CP_SCAD_IMPORT,
    CP_SCAD_SURFACE,

    CP_SCAD_CIRCLE,
    CP_SCAD_SQUARE,
    CP_SCAD_POLYGON,
    CP_SCAD_PROJECTION,
    CP_SCAD_TEXT,

    CP_SCAD_LINEXT,
    CP_SCAD_ROTEXT,
    CP_SCAD_HULL,

    CP_SCAD_COLOR,
} cp_scad_type_t;

#define CP_SCAD_ \
    struct { CP_OBJ_(cp_scad_type_t) }; \
    unsigned modifier;

typedef struct {
    unsigned _fn;
    double _fs, _fa;
} cp_detail_t;

#define CP_DETAIL_INIT ((cp_detail_t){ ._fn = 0, ._fs = 2.0, ._fa = 12.0 })

typedef struct {
    CP_SCAD_
    double r;
    cp_detail_t detail;
} cp_scad_sphere_t;

typedef struct {
    CP_SCAD_
    char const *file_tok;
    cp_vchar_t file;
    bool center;
    char const *id_tok;
    cp_vchar_t id;
    char const *layer;
    cp_detail_t detail;
    double dpi;
} cp_scad_import_t;

typedef struct {
    CP_SCAD_
    char const *file_tok;
    cp_vchar_t file;
    bool center;
} cp_scad_surface_t;

typedef struct {
    CP_SCAD_
    double r;
    cp_detail_t detail;
} cp_scad_circle_t;

typedef struct {
    CP_SCAD_
    double h;
    double r1;
    double r2;
    bool center;
    cp_detail_t detail;
} cp_scad_cylinder_t;

typedef struct {
    CP_SCAD_
    cp_vec3_t size;
    bool center;
} cp_scad_cube_t;

typedef struct {
    CP_SCAD_
    cp_vec2_t size;
    bool center;
} cp_scad_square_t;

typedef struct {
    cp_a_vec3_loc_ref_t points;
    cp_loc_t loc;
} cp_scad_face_t;

typedef CP_VEC_T(cp_scad_face_t) cp_a_scad_face_t;

typedef struct {
    CP_SCAD_
    cp_a_vec3_loc_t points;
    cp_a_scad_face_t faces;
} cp_scad_polyhedron_t;

typedef struct {
    cp_a_vec2_loc_ref_t points;
    cp_loc_t loc;
} cp_scad_path_t;

typedef CP_VEC_T(cp_scad_path_t) cp_a_scad_path_t;

typedef struct {
    CP_SCAD_
    cp_a_vec2_loc_t points;
    cp_a_scad_path_t paths;
} cp_scad_polygon_t;

typedef CP_VEC_T(cp_scad_t*) cp_v_scad_p_t;

#define CP_SCAD_GROUP_ \
    CP_SCAD_ \
    cp_v_scad_p_t child;

#define CP_SCAD_GROUP_XYZ_ \
    CP_SCAD_GROUP_ \
    cp_vec3_t v;

/**
 * Common to each recursive structure
 */
typedef struct {
    CP_SCAD_GROUP_
} cp_scad_rec_t;


/**
 * To store union.
 */
typedef struct {
    CP_SCAD_GROUP_
} cp_scad_union_t;

/**
 * To store intersection.
 */
typedef struct {
    CP_SCAD_GROUP_
} cp_scad_intersection_t;

/**
 * To store difference.
 */
typedef struct {
    CP_SCAD_GROUP_
} cp_scad_difference_t;

/**
 * To store translate */
typedef struct {
    CP_SCAD_GROUP_XYZ_
} cp_scad_translate_t;

/**
 * To store mirror */
typedef struct {
    CP_SCAD_GROUP_XYZ_
} cp_scad_mirror_t;

/**
 * To store scale */
typedef struct {
    CP_SCAD_GROUP_XYZ_
} cp_scad_scale_t;

typedef struct {
    CP_SCAD_GROUP_
    bool cut;
} cp_scad_projection_t;

typedef struct {
    CP_SCAD_GROUP_
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
    CP_SCAD_GROUP_
    cp_color_rgba_t rgba;
    /**
     * Whether rgba is valid or whether the
     * color setting is to be ignored (in case c was 'undef')
     */
    bool valid;
} cp_scad_color_t;

typedef struct {
    CP_SCAD_GROUP_
    cp_mat3w_t m;
} cp_scad_multmatrix_t;

typedef struct {
    CP_SCAD_GROUP_
    cp_f_t height;
    cp_f_t twist;
    cp_vec2_t scale;
    unsigned slices;
    bool center;
    cp_detail_t detail;
} cp_scad_linext_t;

typedef struct {
    CP_SCAD_GROUP_
    cp_angle_t angle;
    cp_detail_t detail;
} cp_scad_rotext_t;

typedef struct {
    CP_SCAD_GROUP_
    char const *text;
    cp_f_t size;
    char const *font;
    char const *halign;
    char const *valign;
    cp_f_t spacing;
    cp_f_t tracking;
    char const *direction;
    char const *language;
    char const *script;
    cp_detail_t detail;
} cp_scad_text_t;

typedef struct {
    CP_SCAD_GROUP_
} cp_scad_hull_t;

struct cp_scad { CP_SCAD_ };

typedef struct {
    /**
     * What to do with a recognised, but yet not implemented functor. */
    unsigned err_unsupported_functor;

    /**
     * What to do with an unknown functor */
    unsigned err_unknown_functor;

    /**
     * What to do with an unknown parameter. */
    unsigned err_unknown_param;
} cp_scad_opt_t;

typedef struct {
    /**
     * Options for processing */
    cp_scad_opt_t const *opt;

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

#endif /* CP_SCAD_TAM_H_ */
