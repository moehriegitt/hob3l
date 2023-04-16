/* -*- Mode: C -*- */

#ifndef CP_SRC_INTERNAL_H_
#define CP_SRC_INTERNAL_H_

#include <stdio.h>
#include <errno.h>
#include <hob3lbase/base-def.h>
#include <hob3lbase/stream.h>
#include <hob3l/ps_tam.h>

/* standard print parameters */

#define FF  CP_FF
#define FD  CP_FD
#define IND CP_IND

#define FD2  FD" "FD
#define FD3  FD" "FD" "FD
#define FD4  FD" "FD" "FD" "FD

#define FF2  FF" "FF
#define FF3  FF" "FF" "FF
#define FF4  FF" "FF" "FF" "FF

typedef struct {
    char const *func;
    char const *file;
    unsigned line;
    char msg[100];
} trace_func_t;

extern int cp_trace_level(int add);

#ifdef PSTRACE

#define IFPSTRACE(...) __VA_ARGS__

extern FILE *cp_debug_ps_file;
extern cp_stream_t *cp_debug_ps;
extern size_t cp_debug_ps_page_cnt;
extern cp_ps_xform_t cp_debug_ps_xform;
extern cp_ps_opt_t const *cp_debug_ps_opt;
extern size_t cp_debug_ps_page_skip;
extern size_t cp_debug_ps_page_count;
extern cp_scale_t cp_debug_ps_scale_x;
extern cp_scale_t cp_debug_ps_scale_y;
extern cp_scale_t cp_debug_ps_xlat_x;
extern cp_scale_t cp_debug_ps_xlat_y;
extern bool cp_debug_ps_dots;

/**
 * Transform using cp_debug_ps_xform.
 */
#define CP_PS_X(v) cp_ps_x(&cp_debug_ps_xform, v)
#define CP_PS_Y(v) cp_ps_y(&cp_debug_ps_xform, v)

/**
 * X,Y in one macro
 */
#define CP_PS_XY(v) CP_PS_X((v).x), CP_PS_Y((v).y)

extern bool cp_debug_ps_page_begin(void);
extern void cp_debug_ps_dot(double x, double y, double sz);

static inline double three_steps(size_t i)
{
    switch (i % 3) {
    case 0:  return 0;
    case 1:  return 0.75;
    default: return 1;
    }
}

#else

#define IFPSTRACE(...) ((void)0)

#endif /* PSTRACE */

#if defined(DEBUG) && DEBUG

#define IFDEBUG(...) __VA_ARGS__

#define LOG(...)  fprintf(stderr, __VA_ARGS__)
#define VLOG(...) vfprintf(stderr, __VA_ARGS__)

#define TRACE_AUX(_tf, ...) \
    __attribute__((__cleanup__(trace_func_leave))) \
    trace_func_t _tf = { __FUNCTION__, __FILE__, __LINE__, "" }; \
    snprintf(_tf.msg, sizeof(_tf.msg), " " __VA_ARGS__); \
    trace_func_enter(&_tf);

#define TRACE(...) \
    TRACE_AUX(CP_GENSYM(_tf), __VA_ARGS__)

#define TRACE_LOCUS 0

CP_UNUSED
static void trace_func_enter(trace_func_t *t)
{
#if TRACE_LOCUS
    fprintf(stderr, "%s:%u: ", t->file, t->line);
#endif
    fprintf(stderr, "TRACE: %2d ENTER: %s%s\n", cp_trace_level(+1), t->func, t->msg);
}

CP_UNUSED
static void trace_func_leave(trace_func_t *t)
{
#if TRACE_LOCUS
    fprintf(stderr, "%s:%u: ", t->file, t->line);
#endif
    fprintf(stderr, "TRACE: %2d LEAVE: %s%s\n", cp_trace_level(-1), t->func, t->msg);
}

#else /* !DEBUG */

#define TRACE(...)   ((void)0)
#define LOG(...)     ((void)0)
#define VLOG(...)    ((void)0)
#define IFDEBUG(...) ((void)0)

#endif

/**
 * To print info in an assert */
#define CONFESS(...) \
    (fprintf(stderr, "ASSERT FAIL: " __VA_ARGS__), \
     fprintf(stderr,"\n"), \
     0)

#endif /* CP_SRC_INTERNAL_H_ */
