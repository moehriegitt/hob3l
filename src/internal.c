/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <csg2plane/ps.h>
#include "internal.h"

#ifdef PSTRACE
FILE *cp_debug_ps_file = NULL;
cp_stream_t *cp_debug_ps = NULL;
size_t cp_debug_ps_page_cnt = 0;
cp_ps_xform_t cp_debug_ps_xform = CP_PS_XFORM_MM;
cp_ps_opt_t const *cp_debug_ps_opt = NULL;
size_t cp_debug_ps_page_skip = 0;
size_t cp_debug_ps_page_count = -1U;
cp_scale_t cp_debug_ps_scale = 1;
cp_scale_t cp_debug_ps_xlat_x = 0;
cp_scale_t cp_debug_ps_xlat_y = 0;
bool cp_debug_ps_dots = true;
#endif

extern int cp_trace_level(int i)
{
    static int level = 0;
    if (i > 0) { level += i; }
    int l = level;
    if (i < 0) { level += i; }
    return l;
}

#ifdef PSTRACE
extern bool cp_debug_ps_page_begin(void)
{
    if (cp_debug_ps == NULL) {
        return false;
    }
    if (cp_debug_ps_page_skip > 0) {
        cp_debug_ps_page_skip--;
        return false;
    }
    if (cp_debug_ps_page_count == 0) {
        return false;
    }
    if (cp_debug_ps_page_count != -1U) {
       cp_debug_ps_page_count--;
    }

    /* begin page */
    cp_ps_page_begin(cp_debug_ps, cp_debug_ps_opt, ++cp_debug_ps_page_cnt);
    cp_ps_clip_box(cp_debug_ps, 0, 0, CP_PS_PAPER_X, CP_PS_PAPER_Y);

    return true;
}

extern void cp_debug_ps_dot(double x, double y, double sz)
{
    if (cp_debug_ps_dots) {
        cp_printf(cp_debug_ps, "newpath %g %g %g 0 360 arc closepath fill\n",
            x, y, sz);
    }
}
#endif /* PSTRACE */
