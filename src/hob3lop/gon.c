/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lop/gon.h>

/* On 32-bit floats, we have 23 mantissa bits.
 *
 * We want at least +-1m max. size on 32-bit floats, or 1024mm =
 * 10 bits integer.
 *
 * So this is set to 2^13, which means the smallest difference
 * is 1/8192mm.
 *
 * The integer part can do 30 unsigned bits, so the coordinate
 * range at integer precision is +-2^17mm or +-128m.
 */
double cq_dim_scale = 8192;

extern void cq_vec2_minmax(
    cq_vec2_minmax_t *r,
    cq_vec2_t const *v)
{
    cq_dim_minmax(&r->min.x, &r->max.x, v->x);
    cq_dim_minmax(&r->min.y, &r->max.y, v->y);
}

extern void cq_v_vec2_minmax(
    cq_vec2_minmax_t *r,
    cq_v_vec2_t const *v)
{
    for (cp_v_eachp(i, v)) {
        cq_vec2_minmax(r, i);
    }
}

extern void cq_v_line2_minmax(
    cq_vec2_minmax_t *r,
    cq_v_line2_t const *v)
{
    for (cp_v_eachp(i, v)) {
        cq_line2_minmax(r, i);
    }
}

extern int cq_import_dim(double v)
{
    /*
     * The '00001' in '0.500001' fixes test28a, which then correctly
     * rounds up, not down.  Cleanly rounding up at x.5 is desirable
     * so that the snap rounding's tolerance squares work the same way
     * as the rounding here.  Of course, there is no way to actually
     * do it completely correctly here, because there are many
     * opportunities for rounding errors before this code is even
     * reached, as everything before this uses 'double'.  In case of
     * test28a, it's a 45deg rotation that produces coordinates in
     * .499999something.  That '00001' is a good heuristics to
     * eliminate the typical and most frequent weirdnesses.  Strictly
     * speaking, the code is also correct with a simple '0.5', but
     * with '0.500001', the behaviour is a little less surprising.
     */
    double s = floor((v * cq_dim_scale) + 0.500001);
    cq_ovf_unless(fabs(s) <= CP_MAX_OF(int)); /* using 'unless<' so that NAN will trigger OVF */
    return (int)s; /* should be free of UB after OVF test */
}
