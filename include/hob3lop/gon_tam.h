/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef HOB3LOP_GON_TAM_H_
#define HOB3LOP_GON_TAM_H_

#include <hob3lmat/mat.h>
#include <hob3lbase/err_tam.h>
#include <hob3lbase/color_tam.h>
#include <hob3lop/def_tam.h>

#define CQ_VEC2_T \
    union { \
        cq_dim_t v[2]; \
        struct { \
            cq_dim_t x; \
            cq_dim_t y; \
        }; \
    }

/**
 * 2-dim vector
 */
typedef CQ_VEC2_T cq_vec2_t;

/**
 * Compound literal constructor for a cq_vec2_t */
#define CQ_VEC2(x_, y_) ((cq_vec2_t){ .x = (x_), .y = (y_) })

#define CQ_VEC2_MAX CQ_VEC2(CQ_DIM_MAX, CQ_DIM_MAX)
#define CQ_VEC2_MIN CQ_VEC2(CQ_DIM_MIN, CQ_DIM_MIN)

/**
 * A vector of points.
 * This also functions as a polygon.  I.e., a polygon
 * is stored as a set of lines, with no particular order.
 */
typedef CP_VEC_T(cq_vec2_t) cq_v_vec2_t;

#define CQ_LINE2_T \
    union { \
        cq_vec2_t p[2]; \
        struct { \
            cq_vec2_t a; \
            cq_vec2_t b; \
        }; \
    } \


/**
 * 2-dim line */
typedef CQ_LINE2_T cq_line2_t;

#define CQ_LINE2(a_, b_)  ((cq_line2_t){ .a = (a_), .b = (b_) })

#if 0
/* maybe later, we might have a better way of storing edges and crossings */

/**
 * An edge is like a line, but its second vector is the difference,
 * not the right point. */
#define CQ_EDGE2_T \
    struct { \
        cq_vec2_t org; \
        cq_vec2_t add; \
    }

typedef CQ_EDGE2_T cq_edge2_t;

/**
 * An unsigned fraction n/d \in [0..1] in double precision.
 * Probably we can make this smaller, maybe just one cq_udimw_t.
 * For now, this is exact arith.
 * FIXME: LATER.
 */
#define CQ_UFRAC_T \
    struct { \
        cq_udimw_t n, d; \
    }

typedef CQ_UFRAC_T cq_ufrac_t;

/**
 * A vertext is a point on an edge given by a pointer to an edge
 * and the position on the edge.
 */
#define CQ_VERTEX2_T \
    struct { \
        cq_edge2_t const *edge; \
        cq_ufrac_t pos; \
    }

typedef CQ_VERTEX2_T cq_vertex2_t;

/**
 * A segment is a piece of an edge delimited by two positions. */
#define CQ_SEG2_T \
    union { \
        cq_vertex2_t vertex2; \
        struct { \
            cq_edge2_t const *edge; \
            union { \
                cq_ufrac_t pos[2]; \
                struct { \
                    cq_ufrac_t a, b; \
                }; \
            }; \
        }; \
    }

typedef CQ_SEG2_T cq_seg2_t;
#endif /*0*/

/**
 * A vector of lines.
 * This also functions as a polygon.  I.e., a polygon
 * is stored as a set of lines, with no particular order.
 */
typedef CP_VEC_T(cq_line2_t) cq_v_line2_t;

#define CQ_TRI2_T \
    union { \
        cq_vec2_t p[3]; \
        struct { \
            cq_vec2_t a, b, c; \
        }; \
    }

/**
 * minmax values */
typedef struct {
    cq_vec2_t min;
    cq_vec2_t max;
} cq_vec2_minmax_t;

#define CQ_VEC2_MINMAX(lo_,hi_) ((cq_vec2_minmax_t){ .min = lo_, .max = hi_ })
#define CQ_VEC2_MINMAX_INIT     CQ_VEC2_MINMAX(CQ_VEC2_MAX, CQ_VEC2_MIN)

typedef void (*cq_vec2_put3_t)(
    cq_vec2_t const *a,
    cq_vec2_t const *b,
    cq_vec2_t const *c,
    void *user_put);

/**
 * An integer plus a fraction.
 *
 * This is a special type to store the exact coordinate of an intersection.
 * The algorithm uses degree 3 arithmetics and the result is a single
 * precision integer plus a fraction in double precision, so this is five
 * times larger than the original.
 *
 * The value of this is (i+(n/d)).
 *
 * FIXME:
 * This is exact, but unfortunately, fractions are expensive in comparisons,
 * so we will need something faster.
 */
#define CQ_DIMIF_T \
    struct { \
        cq_dim_t i; \
        cq_udimw_t n; \
        cq_udimw_t d; \
    }

typedef CQ_DIMIF_T cq_dimif_t;

#define CQ_DIMIF(i_,n_,d_) ((cq_dimif_t){ .i=i_, .n=n_, .d=d_ })

#define CQ_DIMIF_0    CQ_DIMIF(0,0,1)
#define CQ_DIMIF_NAN  CQ_DIMIF(0,0,0)

/**
 * A 2D vector of exact intersection coordinates
 */
#define CQ_VEC2IF_T \
    union { \
        cq_dimif_t v[2]; \
        struct { \
            cq_dimif_t x; \
            cq_dimif_t y; \
        }; \
    }

typedef CQ_VEC2IF_T cq_vec2if_t;

#define CQ_VEC2IFI(x_, y_) ((cq_vec2if_t){ .x={ .i=(x_), .d=1 }, .y={ .i=(y_), .d=1 } })

#define CQ_VEC2IF(ix,nx,dx,iy,ny,dy) ((cq_vec2if_t){ .x={ix,nx,dx}, .y={iy,ny,dy} })

#define CQ_VEC2IF_0    CQ_VEC2IF(0,0,1,0,0,1)
#define CQ_VEC2IF_NAN  CQ_VEC2IF(0,0,0,0,0,0)

/**
 * Point array for higher level polygon representation.
 */
typedef struct {
    cp_v_size_t point_idx;
} cp_csg2_path_t;

typedef CP_VEC_T(cp_csg2_path_t) cp_v_csg2_path_t;

/**
 * Flags (bitmask of _) to characterize a triangle.
 */
typedef enum {
    /**
     * The edge v[0]--v[1] of the triangle is an outline of the polygon */
    CP_CSG2_TRI_OUTLINE_01 = (1 << 0),
    /**
     * The edge v[1]--v[2] of the triangle is an outline of the polygon */
    CP_CSG2_TRI_OUTLINE_12 = (1 << 1),
    /**
     * The edge v[2]--v[0] of the triangle is an outline of the polygon */
    CP_CSG2_TRI_OUTLINE_20 = (1 << 2),
} cp_csg2_tri_flags_t;

/**
 * Triangle for higher level polygon representation.
 */
typedef struct {
    union {
        CP_SIZE3_T;
        cp_size3_t size3;
    };
    cp_csg2_tri_flags_t flags;
} cp_csg2_tri_t;

typedef CP_VEC_T(cp_csg2_tri_t) cp_v_csg2_tri_t;


/**
 * Higher level 2D vector type with source location and color.
 */
typedef struct {
    cp_vec2_t coord;
    unsigned aux;
    cp_loc_t loc;
} cp_vec2_loc_t;

typedef CP_ARR_T(cp_vec2_loc_t) cp_a_vec2_loc_t;
typedef CP_VEC_T(cp_vec2_loc_t) cp_v_vec2_loc_t;

/**
 * The internal parts of cp_csg2_poly_t without the object box.
 *
 * point:
 *     The vertices of the polygon.
 *
 *     This stores both the coordinates and the location in the
 *     input file (for error messages).
 *
 *     Each point must be unique.  Paths and triangles refer to
 *     this array.
 *
 * path:
 *     Paths defining the polygon.
 *
 *     This should be equivalent information as in triangle.
 *
 *     All paths should be clockwise.  Some processing stages
 *     work without this (e.g., triangulation and bool operations
 *     do not really care about point order), others require it
 *     (like SCAD and STL output).  The bool operation output
 *     will correctly fill this in clockwise order.  (I.e., polygon
 *     paths subtracting from an outer path will have reverse
 *     order.)
 *
 * triangle:
 *     Triangles defining the polygon.
 *
 *     This should be equivalent information as in path.
 *
 *     All triangles should be clockwise.  Whether this is
 *     required depends on the step in the processing pipeline.
 *     SCAD and STL output require this to work correctly.
 *
 *     Without triangulation run, this is empty.
 */
#define CQ_CSG2_POLY_T \
    struct { \
        cp_v_vec2_loc_t point; \
        cp_v_csg2_path_t path; \
        cp_v_csg2_tri_t tri; \
    }

typedef CQ_CSG2_POLY_T cq_csg2_poly_t;

#define CQ_CSG2_POLY_INIT ((cq_csg2_poly_t){})

static inline void cq_csg2_poly_fini(
    cq_csg2_poly_t *p)
{
    for (cp_v_eachp(i, &p->path)) {
        cp_v_fini(i);
    }
    cp_v_fini(&p->point);
    cp_v_fini(&p->path);
    cp_v_fini(&p->tri);
}

static inline void cq_csg2_poly_delete(
    cq_csg2_poly_t *p)
{
    cq_csg2_poly_fini(p);
    CP_DELETE(p);
}

#endif /* HOB3LOP_GON_TAM_H_ */
