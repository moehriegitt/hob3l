/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_CSG2_H
#define __CP_CSG2_H

#include <cpmat/stream_tam.h>
#include <cpmat/pool_tam.h>
#include <cpmat/arith_tam.h>
#include <csg2plane/csg2_tam.h>
#include <csg2plane/csg3_tam.h>
#include <csg2plane/ps_tam.h>

/**
 * Allocate a new CSG2 object.
 */
#define cp_csg2_new(type, loc) __cp_csg2_new(CP_FILE, CP_LINE, type, loc)

/**
 * Internal: allocate a new CSG2 object.
 */
extern cp_csg2_t *__cp_csg2_new(
    char const *file,
    int line,
    cp_csg2_type_t type,
    cp_loc_t loc);

/**
 * Manual initialisation for stack allocated objects.
 *
 * Note: This does not zero the object, this has to be done before (with the
 * right size of the corresponding struct type).
 */
extern void cp_csg2_init(
    cp_csg2_t *r,
    cp_csg2_type_t type,
    cp_loc_t loc);

/**
 * Initialise a cp_csg2_add_t object unless it is initialised
 * already.
 *
 * For this to work, the data must be zeroed first, then this
 * function can be used to initialise it, if it is not yet
 * initialised.
 */
extern void cp_csg2_add_init_perhaps(
    cp_csg2_add_t *r,
    cp_loc_t loc);

/**
 * Get a point of a path in a poly.
 */
static inline cp_vec2_loc_t *cp_csg2_path_nth(
    cp_csg2_poly_t *poly,
    cp_csg2_path_t *path,
    size_t i)
{
    assert(i < path->point_idx.size);
    size_t j = path->point_idx.data[i];
    assert(j < poly->point.size);
    return &poly->point.data[j];
}

/**
 * Initialises a CSG2 structure with a tree derived from a CSG3
 * structure, and reserves, for each simple object in the tree, an
 * array of layers of size layer_cnt.
 *
 * This assumes a freshly zeroed r to be initialised.
 */
extern void cp_csg2_tree_from_csg3(
    cp_csg2_tree_t *r,
    cp_csg3_tree_t const *d,
    cp_range_t const *s);

/**
 * This generates stacks of polygon CSG2 trees.
 *
 * The tree must be either empty, or the root of the tree must have
 * type CP_CSG2_ADD.  If the result is non-empty, this will set the
 * root to a node of type CP_CSG2_ADD.
 *
 * In the polygons, only the 'path' entries are filled in, i.e.,
 * the 'triangle' entries are left empty.
 */
extern bool cp_csg2_tree_add_layer(
    cp_csg2_tree_t *r,
    cp_err_t *t,
    size_t zi);

/**
 * Print as scad file.
 *
 * Note that SCAD is actually 3D and OpenSCAD does not really
 * honor Z translations for 2D objects.  The F5 view is OK, but
 * the F6 view maps everything to z==0.
 *
 * This prefers the triangle info.  If not present, it uses the
 * polygon path info.
 *
 * This prints in 1:1 scale, i.e., if the finput is in MM,
 * the SCAD output is in MM, too.
 */
extern void cp_csg2_tree_put_scad(
    cp_stream_t *s,
    cp_csg2_tree_t *r);

/**
 * Print as STL file.
 *
 * This generates one 3D solid for each layer.
 *
 * This uses both the triangle and the polygon data for printing.  The
 * triangles are used for the xy plane (top and bottom) and the path
 * for connecting top and bottom at the edges of the slice.
 *
 * This prints in 1:10 scale, i.e., if the input in MM,
 * the STL output is in CM.
 */
extern void cp_csg2_tree_put_stl(
    cp_stream_t *s,
    cp_csg2_tree_t *t);

/**
 * Print as PS file.
 *
 * All layers is printed on a separate page.
 *
 * This prints both the triangle and the path data in different
 * colours, overlayed so that the shape can be debugged.
 *
 * If xform is NULL, this assumes that the input is in MM.  Otherwise,
 * any other scaling transformation can be applied.
 */
extern void cp_csg2_tree_put_ps(
    cp_stream_t *s,
    cp_ps_opt_t const *opt,
    cp_csg2_tree_t *t);

/**
 * This clears all 'triangle' vectors in all polygons of the tree and
 * refills them with a set of triangles derived from the 'path'
 * entries of the polygons.
 *
 * Note that this algorithm ignores the order of points on a path and
 * always produces clockwise triangles from any path.
 *
 * This uses cp_csg2_tri_set() internally for each polygon in the
 * tree, so the set of paths of each polygon in the tree is contrained
 * in the way described for that function.
 *
 * FIXME: Currently, all allocations of temporaries are on the stack.
 * This prevents heap fragmentation, but might not work well with
 * large polygons on OSes that have a fixed stack size for threads.
 *
 * Runtime: O(m * n log n)
 * Space: O(max(n))
 * Where
 *     m = number of polygons (=cp_csg2_poly_t objects),
 *     n = number of points of a polygon,
 *     max(n) = the maximum n among the polygons.
 */
extern bool cp_csg2_tri_tree(
    cp_err_t *t,
    cp_csg2_tree_t *r);

/**
 * Triangulate a single polygon.
 *
 * Note that a polygon may consist of multiple paths.
 *
 * This uses cp_csg2_tri_set() internally, it is invoked once with all
 * the paths in one data structure, so the set of paths of the given
 * polygon is contrained in the way described for that function.
 *
 * Runtime: O(n log n)
 * Space: O(n)
 * Where n = number of points.
 */
extern bool cp_csg2_tri_poly(
    cp_err_t *t,
    cp_csg2_poly_t *r);

/**
 * Triangulate a single path.
 *
 * This does not clear the list of triangles, but the new
 * triangles are appended to the polygon's triangle vector.
 *
 * This uses cp_csg2_tri_set() internally, so the path is contrained
 * in the way described for that function.
 *
 * Runtime: O(n log n)
 * Space: O(n)
 * Where n = number of points.
 */
extern bool cp_csg2_tri_path(
    cp_err_t *t,
    cp_csg2_poly_t *g,
    cp_csg2_path_t *s);

/**
 * Triangulate a set of polygons.
 *
 * Each polygon must be simple and there must be no intersecting edges
 * neither with the same polygon nor with any other polygon.
 * Polygons, however, may be fully contained with in other polygons,
 * i.e., they must not intersect, but may fully overlap.
 *
 * Polygons are defined by setting up an array of nodes \p node.  The
 * algorithm assumes that the structure was zeroed for initialisation
 * and then each node's \a in, \a out, and \p point slots need to be
 * initialised to represent the set of polygons.  The \p loc slot
 * is optional (meaning it may remain NULL), but highly recommended
 * for good error messages.
 *
 * Implicitly, edges need to be stored somewhere (they are pointed to
 * by each node).  Each edge is also assumed to having been zeroed for
 * initialisation.  The edges \p src and \p dst slots may be
 * initialised, but do not need to be, as they will be initialised by
 * the algorithm from each point's n->in and n->out information so
 * that n->in->dst = n->out->src = n.
 *
 * This uses the Hertel & Mehlhorn (1983) algorithm (non-optimised).
 *
 * The algorithm is extended in several ways:
 *
 * (1) It also handles subsequent collinear edges, i.e., three (and more)
 *     subsequent points in the polygon on the same line.  This
 *     situation will introduce more triangles than necessary, however,
 *     because each point will become a corner of a triangle.  This
 *     is implemented by applying a 2 dimensional lexicographical
 *     order to the points in the sweep line queue instead of the
 *     original x-only order.
 *
 * (2) It also handles coincident vertices in the same polygon.  This
 *     is what the CSG2 boolop algorithm outputs if the input is such
 *     that points coincide.  There is no way to fix this: it is just
 *     how the polygons are.  The boolop algorithm will never output
 *     vertices in the middle of an edge, so the triangulation does
 *     not need to handle that.  Also, I think no bends with coincident
 *     edges will ever be output, so this is untested (and probably
 *     will not work), only ends (proper and improper) and starts (proper
 *     and improper) are tested.
 *     This is implemented by again extending the sweep line point order
 *     by considering the type or corner (first ends, then starts).
 *     Additionally, the improper start case has a special case if vertices
 *     coincide.
 *
 * Runtime: O(n log n)
 * Space: O(n)
 * Where n = number of points.
 */
extern bool cp_csg2_tri_set(
    cp_err_t *t,
    cp_vec2_arr_ref_t *point_arr,
    cp_v_size3_t *tri,
    cp_csg2_3node_t *node,
    size_t node_count);

/**
 * Compute bounding box
 *
 * Runtime: O(n), n=size of vector
 */
extern void cp_v_vec2_loc_minmax(
    cp_vec2_minmax_t *m,
    cp_v_vec2_loc_t *o);

/**
 * Compute bounding box
 *
 * This uses only the points, neither the triangles nor the paths.
 *
 * Runtime: O(n), n=number of points
 */
static inline void cp_csg2_poly_minmax(
    cp_vec2_minmax_t *m,
    cp_csg2_poly_t *o)
{
    cp_v_vec2_loc_minmax(m, &o->point);
}

/**
 * Append all paths from a into r, destroying a.
 *
 * This does no intersection test, but simply appends vectors
 * and adjusts indices.
 *
 * This moves both the paths and the triangulation.
 *
 * NOTE: If necessary for some future algorithm, this could be
 * rewritten to only clear a->path, but leave a->point and a->triangle
 * intact, because the latter vectors do not contain any allocated
 * substructures.  Currently, for consistency, the whole poly is
 * cleared.
 */
extern void cp_csg2_poly_merge(
    cp_csg2_poly_t *r,
    cp_csg2_poly_t *a);

/**
 * Boolean operation on two polygons.
 *
 * This uses the path information, not the triangles.
 *
 * 'r' will be overwritten and initialised (it may be passed uninitialised).
 *
 * \p a and/or \p b are reused and cleared to construct r.
 *
 * Uses \p pool for all temporary allocations (but not for constructing r).
 *
 * This uses the algorithm of Martinez, Rueda, Feito (2009), based on a
 * Bentley-Ottmann plain sweep.  The algorithm is modified:
 *
 * (1) The original algorithm (both paper and sample implementation)
 *     do not focus on reassembling into polygons the set of edges the
 *     algorithm produces.  The sample implementation uses a slow
 *     adhoc O(n**2) algorithm.  This library replaces the polygon
 *     reassembling by a O(n log n) algorithm that connects the points
 *     into rings and then outputs path.  It exploits the fact that
 *     the edges are produced in sweep line order.  The algorithm may
 *     generate paths with multiple identical points.  The
 *     triangulation algorithm needs to cope with this, so it was
 *     modified, too (see above).
 *
 * (2) Some memory safety violations in the sample implementation where
 *     fixed.
 *
 * (3) Float operations have all been mapped to epsilon aware versions.
 *
 * (4) Intersection points are always computed from the original line slope
 *     and offset to avoid adding up rounding errors for edges with many
 *     intersections.
 *
 * Runtime: O(k log k),
 * Space: O(k)
 * Where
 *     k = n + m + s,
 *     n = number of edges in a,
 *     m = number of edges in b,
 *     s = number of intersection points.
 */
extern bool cp_csg2_op_poly(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_poly_t *r,
    cp_loc_t loc,
    cp_csg2_poly_t *a,
    cp_csg2_poly_t *b,
    cp_bool_op_t op);

/**
 * Add a layer to a tree by reducing it from another tree.
 *
 * The tree must have been initialised by cp_csg2_op_tree_init(),
 * and the layer ID must be in range.
 *
 * r is filled from a.  In the process, a is cleared/reused, if necessary.
 *
 * Runtime: O(j * k log k)
 * Space O(k)
 *    k = see cp_csg2_op_poly()
 *    j = number of polygons + number of bool operations in tree
 */
extern bool cp_csg2_op_add_layer(
    cp_pool_t *pool,
    cp_err_t *t,
    cp_csg2_tree_t *r,
    cp_csg2_tree_t *a,
    size_t zi);

/**
 * Initialise a tree for cp_csg2_op_add_layer() operations.
 *
 * The tree is initialised with a single stack of layers of the given
 * size taken from \p a.  Also, the z values are copied from \p a.
 *
 * Runtime: O(n)
 * Space O(n)
 *    n = number of layers
 */
extern void cp_csg2_op_tree_init(
    cp_csg2_tree_t *r,
    cp_csg2_tree_t const *a);

/**
 * If the stack has the given layer, return it.
 * Otherwise, return NULL.
 */
extern cp_csg2_layer_t *cp_csg2_stack_get_layer(
    cp_csg2_stack_t *c,
    size_t zi);

/* Dynamic casts */
CP_DECLARE_CAST(csg2, circle, CP_CSG2_CIRCLE)
CP_DECLARE_CAST(csg2, poly,   CP_CSG2_POLY)
CP_DECLARE_CAST(csg2, add,    CP_CSG2_ADD)
CP_DECLARE_CAST(csg2, sub,    CP_CSG2_SUB)
CP_DECLARE_CAST(csg2, cut,    CP_CSG2_CUT)
CP_DECLARE_CAST(csg2, stack,  CP_CSG2_STACK)

#endif /* __CP_CSG2_H */
