/* -*- Mode: C -*- */

#include "internal.h"

#ifdef PSTRACE
FILE *cp_debug_ps_file = NULL;
cp_stream_t *cp_debug_ps = NULL;
size_t cp_debug_ps_page_cnt = 0;
cp_ps_xform_t cp_debug_ps_xform = CP_PS_XFORM_MM;
#endif

extern int cp_trace_level(int i)
{
    static int level = 0;
    if (i > 0) { level += i; }
    int l = level;
    if (i < 0) { level += i; }
    return l;
}
