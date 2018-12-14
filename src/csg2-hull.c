/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3l/csg2-hull.h>
#include <hob3l/ps.h>
#include "internal.h"

#if !defined(PSTRACE)

#  define debug_poly(...) ((void)0)

#else /* defined(PSTRACE) */

static void debug_poly(
    cp_v_vec2_loc_t *point,
    size_t last,
    size_t cur)
{
    (void)last;
    (void)cur;
    if (cp_debug_ps_page_begin()) {
        const cp_vec2_loc_t *first = &cp_v_nth(point, 0);
        const cp_vec2_loc_t *lastp = &cp_v_nth(point, last);

        cp_printf(cp_debug_ps, "0 setgray\n");

        /* print path we already have */
        if (last > 0) {
            for (cp_size_each(i, last+1, 1)) {
                cp_vec2_loc_t *p = &cp_v_nth(point, i);
                cp_debug_ps_dot(CP_PS_XY(p->coord), 3);
            }
            cp_printf(cp_debug_ps, "1 setlinewidth\n");
            cp_printf(cp_debug_ps, "newpath %g %g moveto\n", CP_PS_XY(first->coord));
            for (cp_size_each(i, last+1, 1)) {
                cp_vec2_loc_t *p = &cp_v_nth(point, i);
                cp_printf(cp_debug_ps, "%g %g lineto\n", CP_PS_XY(p->coord));
            }
            cp_printf(cp_debug_ps, "stroke\n");
        }

        /* print pivot point */
        cp_debug_ps_dot(CP_PS_XY(first->coord), 5);

        /* print all remaining points */
        for (cp_v_each(i, point, cur)) {
            cp_vec2_loc_t *p = &cp_v_nth(point, i);
            cp_debug_ps_dot(CP_PS_XY(p->coord), 3);
        }

        /* current line */
        const cp_vec2_loc_t *curp = &cp_v_nth(point, cur);
        cp_printf(cp_debug_ps, "2 setlinewidth\n");
        cp_printf(cp_debug_ps, "0.8 0 0 setrgbcolor\n");
        cp_printf(cp_debug_ps, "newpath %g %g moveto %g %g lineto stroke\n",
            CP_PS_XY(first->coord),
            CP_PS_XY(curp->coord));

        /* collecting line with last */
        cp_printf(cp_debug_ps, "2 setlinewidth\n");
        cp_printf(cp_debug_ps, "0 0.8 0 setrgbcolor\n");
        cp_printf(cp_debug_ps, "newpath %g %g moveto %g %g lineto stroke\n",
            CP_PS_XY(curp->coord),
            CP_PS_XY(lastp->coord));

        /* end page */
        cp_ps_page_end(cp_debug_ps);
    }
}

#endif /* PSTRACE */

static int pt_y_angle_cmp(
    const cp_vec2_loc_t *a,
    const cp_vec2_loc_t *b,
    cp_vec2_t *u)
{
    /* primary: sort by angle */
    int i = cp_vec2_right_normal3_z(&a->coord, u, &b->coord);
    if (i != 0) {
        return i;
    }

    /* secondary for collinear case: sort by distance from u */
    return cp_cmp(
        cp_vec2_sqr_dist(u, &a->coord),
        cp_vec2_sqr_dist(u, &b->coord));
}

/* ********************************************************************** */

/**
 * Compute the convex hull of a set of points.
 *
 * \a point will be rearranged to contain the points of the hull in
 * clockwise order.
 */
extern void cp_csg2_hull(
    cp_v_vec2_loc_t *point)
{
    /* triangles are trivially convex, but we promise clockwise order,
     * so only return for <=2 points and do process 3 points */
    if (point->size <= 2) {
        return;
    }

    /* This implements a Graham Scan, which is O(n log n), which is fast
     * enough, and the algorithm is very simple and does not even need
     * additional space, but runs in-place. */

    /* find lowest (left most) point */
    cp_vec2_t first = cp_v_nth(point, 0).coord;
    for (cp_v_each(i, point, 1)) {
        cp_vec2_loc_t *p = &cp_v_nth(point, i);
        if (cp_vec2_lex_cmp(&p->coord, &first) < 0) {
            first = p->coord;
        }
    }

    /* sort using the angle with y axis */
    cp_v_qsort(point, 0, CP_SIZE_MAX, pt_y_angle_cmp, &first);

    /* iterate */
    size_t last = 0;
    for (cp_v_each(i, point, 1)) {
        /* kick out concave ones */
        while (last > 0) {
            cp_vec2_t *cp = &cp_v_nth(point, last-1).coord;
            cp_vec2_t *cl = &cp_v_nth(point, last).coord;
            cp_vec2_t *cc = &cp_v_nth(point, i).coord;
            int k = cp_vec2_right_normal3_z(cp, cl, cc);
            if (k > 0) {
                break;
            }
            debug_poly(point, last, i);
            last--;
        }

        /* put in new point */
        last++;
        cp_v_nth(point, last) = cp_v_nth(point, i);

        debug_poly(point, last, i);
    }

    /* set new size */
    assert(last < point->size);
    point->size = last+1;
}
