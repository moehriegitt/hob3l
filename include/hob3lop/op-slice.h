/* -*- Mode: C -*- */

#ifndef HOB3LOP_OP_SLICE_H_
#define HOB3LOP_OP_SLICE_H_

#include <hob3lop/gon.h>
#include <hob3lop/hedron.h>
#include <hob3lbase/base-mat.h>

typedef struct cq_slice {
    cq_v_vec2_t *out;
    double z;
} cq_slice_t;

/**
 * Cuts a slice out of a polyhedron at a given \p z plane.
 * The slice is stored as a sequence of lines into \p gon.
 *
 * This does not care about forward or backward orientations
 * of triangles.  But it also does not produce ordered
 * output paths, but just a list of lines.
 *
 * If the input polyhedron is property closed, then the result
 * is guaranteed to be closed, too.  If it is not, the
 * result may be broken, especially if an edge is within
 * a selected Z plane in a triangle but the same edge in
 * the adjacent triangle is not.
 *
 * This cuts just above z, i.e., if there is an edge of a
 * triangle within the selected z plane, it will be added
 * if the triangle is below z, but not if it is above z.
 * (And a triangle fully within z is added if it is at
 * the bottom, but not at the top of the polyhedron.)
 *
 * This is O(n log k) where n = number of edges of the
 * polyhedron and k = convexity of the polygon faces.
 * (This needs to sort the edges to handle corner cases.)
 */
extern void cq_slice_init(
    cq_slice_t *slice,
    cq_v_line2_t *r,
    double z);

/**
 * Add one face to the slice */
extern void cq_slice_add_face(
    cq_slice_t *slice,
    cp_a_vec3_loc_ref_t const *hedron);

/**
 * Finalise slicing.
 *
 * After this, the output vector set in cq_slice_init()
 * is fully initialised with the result of the slice.
 */
extern void cq_slice_fini(
    cq_slice_t *slice);

#endif /* HOB3LOP_OP_SLICE_H_ */
