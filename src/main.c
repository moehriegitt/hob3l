/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <cpmat/mat.h>
#include <cpmat/pool.h>
#include <cpmat/alloc.h>
#include <csg2plane/syn.h>
#include <csg2plane/scad.h>
#include <csg2plane/csg3.h>
#include <csg2plane/csg2.h>
#include <csg2plane/ps.h>
#include "internal.h"

#ifndef CP_PROG_NAME
#define CP_PROG_NAME "csg2plane"
#endif

typedef struct {
    cp_dim_t z_min;
    cp_dim_t z_max;
    cp_dim_t z_step;
    bool have_z_min;
    bool have_z_max;
    bool dump_syn;
    bool dump_scad;
    bool dump_csg3;
    bool dump_csg2;
    bool dump_ps;
    bool dump_stl;
    bool have_dump;
    bool no_tri;
    bool no_csg;
    unsigned ps_scale_step; /* 0 = no change, 1 = normal bb, 2 = max bb */
    cp_ps_opt_t ps;
    cp_scale_t ps_persp;
    char const *out_file_name;
} cp_opt_t;

static void format_source_line(
    cp_vchar_t *out,
    size_t *pos,
    char const *loc,
    char const *src,
    size_t len)
{
    bool need_cr = true;
    size_t x = 0;
    for (char const *i = src, *e = i + len; i != e; i++) {
        if (i == loc) {
            *pos = x;
        }
        if (*i == '\t') {
            size_t new_x = (x + 4) & -4U;
            while (x < new_x) {
                cp_vchar_push(out, ' ');
                x++;
            }
        }
        else {
            cp_vchar_push(out, *i);
            x++;
        }
        if (*i == '\n') {
            need_cr = false;
        }
    }
    if (need_cr) {
        cp_vchar_push(out, '\n');
    }
}

static bool do_file(
    cp_stream_t *sout,
    cp_opt_t *opt,
    cp_syn_tree_t *r,
    const char *fn,
    FILE *f)
{
    /* stage 1: syntax tree */
    if (!cp_syn_parse(r, fn, f)) {
        return false;
    }
    if (opt->dump_syn) {
        cp_syn_tree_put_scad(sout, r);
        return true;
    }

    /* stage 2: SCAD */
    cp_scad_tree_t *scad = CP_NEW(*scad);
    if (!cp_v_scad_from_v_syn_func(scad, &r->err, &r->toplevel)) {
        return false;
    }
    if (opt->dump_scad) {
        cp_scad_tree_put_scad(sout, scad);
        return true;
    }

    /* stage 3: 3D CSG */
    cp_csg3_tree_t *csg3 = CP_NEW(*csg3);
    csg3->opt.max_fn = 200;
    if (!cp_csg3_from_scad_tree(csg3, &r->err, scad)) {
        return false;
    }

    cp_vec3_minmax_t full_bb;
    CP_ZERO(&full_bb);
    if (csg3->root != NULL) {
        fprintf(stderr, "DEBUG: bounding box: ("FD3")--("FD3"), valid=%s\n",
            CP_V012(csg3->root->bb.min),
            CP_V012(csg3->root->bb.max),
            csg3->root->non_empty ? "true" : "false");

        full_bb = csg3->root->bb;
        cp_csg3_tree_max_bb(&full_bb, csg3);
#ifdef PSTRACE
        cp_ps_xform_from_bb(&cp_debug_ps_xform,
            full_bb.min.x, full_bb.min.y, full_bb.max.x, full_bb.max.y);
        cp_debug_ps_opt = &opt->ps;
#endif
    }
    else {
        fprintf(stderr, "DEBUG: bounding box: EMPTY\n");
    }

    if (opt->dump_csg3) {
        cp_csg3_tree_put_scad(sout, csg3);
        return true;
    }

    /* stage 4: 2D CSG */
    cp_dim_t z_min = csg3->root->bb.min.z + opt->z_step/2;
    cp_dim_t z_max = csg3->root->bb.max.z;
    if (opt->have_z_min) {
        z_min = opt->z_min;
    }
    if (opt->have_z_max) {
        z_max = opt->z_max;
    }

    cp_range_t range;
    cp_range_init(&range, z_min, z_max, opt->z_step);
    if (range.cnt == 0) {
        range.cnt = 1;
    }
    fprintf(stderr, "DEBUG: z_min=%g, z_max=%g, z_step=%g, z_cnt=%"_Pz"u\n",
        range.min, range.min + (cp_dim(range.cnt) * range.step), range.step, range.cnt);

    /* step 1: slice leaf objects */
    /* step 2: apply 2D CSG to each layer */
    cp_csg2_tree_t *csg2 = CP_NEW(*csg2);
    cp_csg2_tree_from_csg3(csg2, csg3, &range);

    cp_csg2_tree_t *csg2b = CP_NEW(*csg2b);
    cp_csg2_op_tree_init(csg2b, csg2);

    cp_pool_t pool;
    cp_pool_init(&pool, 0);

    for (cp_size_each(i, range.cnt)) {
        if (!cp_csg2_tree_add_layer(csg2, &r->err, i)) {
            return false;
        }
        if (!opt->no_csg) {
            if (!cp_csg2_op_add_layer(&pool, &r->err, csg2b, csg2, i)) {
                return false;
            }
        }
    }

    /* step 2b: use reduced polygon stack */
    if (!opt->no_csg) {
        csg2 = csg2b;
    }

    /* step 3: triangulate */
    if (!opt->no_tri) {
        if (!cp_csg2_tri_tree(&r->err, csg2)) {
            return false;
        }
    }

    /* step 4: print */
    if (opt->dump_csg2) {
        cp_csg2_tree_put_scad(sout, csg2);
        return true;
    }
    if (opt->dump_stl) {
        cp_csg2_tree_put_stl(sout, csg2);
        return true;
    }
    if (opt->dump_ps) {
        cp_ps_xform_t xform = CP_PS_XFORM_MM;
        switch (opt->ps_scale_step) {
        default:
            break;

        case 1:
            cp_ps_xform_from_bb(&xform,
                csg3->root->bb.min.x, csg3->root->bb.min.y,
                csg3->root->bb.max.x, csg3->root->bb.max.y);
            break;

        case 2:
            cp_ps_xform_from_bb(&xform,
                full_bb.min.x, full_bb.min.y,
                full_bb.max.x, full_bb.max.y);
            break;
        }
        opt->ps.xform1 = &xform;
        cp_csg2_tree_put_ps(sout, &opt->ps, csg2);
        return true;
    }

    return true;
}

__attribute__((noreturn))
static void my_exit(int i)
{
#ifdef PSTRACE
    if (cp_debug_ps != NULL) {
        cp_ps_doc_end(cp_debug_ps, cp_debug_ps_page_cnt, 0, 0, -1, -1);
        CP_FREE(cp_debug_ps);
    }
    if (cp_debug_ps_file != NULL) {
        fclose(cp_debug_ps_file);
        cp_debug_ps_file = NULL;
    }
#endif
    exit(i);
}

static char const *cp_prog_name(void)
{
    return CP_PROG_NAME;
}

static char const *opt_help;

__attribute__((noreturn))
static void help(void)
{
#define PRI printf
    PRI("Usage: %s [Options] INFILE\n", cp_prog_name());
    PRI("\n");
    PRI("This reads 3D CSG models from (simple syntax) SCAD files, slices\n"
        "them into layers of 2D CSG models, applies 2D CSG boolean operations\n"
        "to the resulting polygon stack (instead of the 3D polyhedra), and outputs the\n"
        "result as STL file consisting of a (trivially extruded) polygon per slice.\n");
    PRI("\n");
    PRI("Options:\n");
    PRI("%s", opt_help);
    PRI("Note: --dump-csg2 without --no-tri does not work well with OpenSCAD, because\n"
        "the triangles are interpreted in a funny way.\n");
#undef PRI
    my_exit(0);
}

static void get_arg_bool(
    bool *v,
    char const *arg,
    char const *str)
{
    if ((str == NULL) ||
        strequ(str, "true") ||
        strequ(str, "1") ||
        strequ(str, "yes"))
    {
        *v = true;
        return;
    }
    if (strequ(str, "false") ||
        strequ(str, "0") ||
        strequ(str, "no"))
    {
        *v = false;
        return;
    }
    fprintf(stderr, "Error: %s: invalid boolean: '%s'\n", arg, str);
    my_exit(1);
}

static void get_arg_neg_bool(
    bool *v,
    char const *arg,
    char const *str)
{
    bool x = false;
    get_arg_bool(&x, arg, str);
    *v = !x;
}

#define get_arg_angle get_arg_dim
#define get_arg_scale get_arg_dim

static void get_arg_dim(
    cp_dim_t *v,
    char const *arg,
    char const *str)
{
    char *r = NULL;
    *v = strtod(str, &r);
    if ((str == r) || (*r != '\0')) {
        fprintf(stderr, "Error: %s: invalid number: '%s'\n", arg, str);
        my_exit(1);
    }
}

__unused
static void get_arg_size(
    size_t *v,
    char const *arg,
    char const *str)
{
    char *r = NULL;
    unsigned long long val = strtoull(str, &r, 10);
    if ((str == r) || (*r != '\0')) {
        fprintf(stderr, "Error: %s: invalid number: '%s'\n", arg, str);
        my_exit(1);
    }
    *v = val & CP_MAX_OF(*v);
}

static void get_arg_rgb(
    cp_color_rgb_t *v,
    char const *arg,
    char const *str)
{
    char *r = NULL;
    unsigned long w = strtoul(str, &r, 16);
    if ((str == r) || (*r != '\0')) {
        fprintf(stderr, "Error: %s: invalid rgb color: '%s'\n", arg, str);
        my_exit(1);
    }

    v->r = (w >> 16) & 0xff;
    v->g = (w >> 8)  & 0xff;
    v->b = (w >> 0)  & 0xff;
}

typedef struct {
    char const *name;
    void (*func)(cp_opt_t *, char const *, char const *);
    unsigned need_arg;
} cp_get_opt_t;

#include "opt.inc"

typedef struct {
    char const *s;
    size_t n;
} cp_string_n_t;

static int opt_cmp(void const *_a, void const *_b)
{
    cp_string_n_t const *a = _a;
    cp_get_opt_t const *b = _b;
    int i = strncmp(a->s, b->name, a->n);
    if (i != 0) {
        return i;
    }
    if (b->name[a->n] != '\0') {
        return -1;
    }
    return 0;
}

static void parse_opt(
    cp_opt_t *opt,
    int *i,
    int argc,
    char **argv)
{
    char const *argvi = argv[*i];

    cp_string_n_t key = { .s = argvi };
    while (*key.s == '-') {
        key.s++;
    }
    key.n = strlen(key.s);
    char const *eq = strchr(key.s, '=');
    if (eq != NULL) {
        key.n = CP_PTRDIFF(eq, key.s);
    }

    cp_get_opt_t *g =
        bsearch(&key, opt_list, cp_countof(opt_list), sizeof(opt_list[0]), opt_cmp);
    if (g == NULL) {
        fprintf(stderr, "Error: Unrecognised option: '%s'\n", argvi);
        my_exit(1);
    }

    char const *arg = NULL;
    if (g->need_arg > 0) {
        if (key.s[key.n] == '=') {
            arg = key.s + key.n + 1;
        }
        else
        if (g->need_arg == 2) {
            if ((*i + 1) >= argc) {
                fprintf(stderr, "Error: Expected argument for '%s'\n", argvi);
                my_exit(1);
            }
            arg = argv[++*i];
        }
    }

    g->func(opt, argvi, arg);
}

static bool has_suffix(
    char const *haystack,
    char const *needle)
{
    size_t len1 = strlen(haystack);
    size_t len2 = strlen(needle);
    if (len1 < len2) {
        return false;
    }
    return strequ(haystack + len1 - len2, needle);
}

int main(int argc, char **argv)
{
    /* init options */
    cp_opt_t opt;
    CP_ZERO(&opt);
    opt.z_step = 0.2;
    opt.z_max = -1;
    cp_mat4_unit(&opt.ps.xform2);
    opt.ps.color_path   = (cp_color_rgb_t){   0,   0,   0 };
    opt.ps.color_tri    = (cp_color_rgb_t){ 102, 102, 102 };
    opt.ps.color_fill   = (cp_color_rgb_t){ 204, 204, 204 };
    opt.ps.color_vertex = (cp_color_rgb_t){ 255,   0,   0 };
    opt.ps.color_mark   = (cp_color_rgb_t){   0,   0, 255 };
    opt.ps.line_width = 0.4;

    /* parse command line */
    char const *in_file_name = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            parse_opt(&opt, &i, argc, argv);
        }
        else
        if (in_file_name == NULL) {
            in_file_name = argv[i];
        }
        else {
            fprintf(stderr, "Error: Multiple input files cannot be processed: '%s'\n",
                argv[i]);
            my_exit(1);
        }
    }

    /* post-process options */
    if (!cp_equ(opt.ps_persp,0)) {
        cp_mat4_t m;
        cp_mat4_unit(&m);
        m.m[3][2] = opt.ps_persp / -1000.0;
        cp_mat4_mul(&opt.ps.xform2, &m, &opt.ps.xform2);
    }

    /* output file: */
    cp_stream_t *sout = CP_STREAM_FROM_FILE(stdout);
    FILE *fout = NULL;
    if (opt.out_file_name) {
        fout = fopen(opt.out_file_name, "wt");
        if (fout == NULL) {
            fprintf(stderr, "Error: Unable to open '%s' for writing: %s\n",
                opt.out_file_name, strerror(errno));
            my_exit(1);
        }
        sout = CP_STREAM_FROM_FILE(fout);

        if (!opt.have_dump) {
            if (has_suffix(opt.out_file_name, ".stl")) {
                opt.dump_stl = true;
            }
            else if (has_suffix(opt.out_file_name, ".scad")) {
                opt.dump_csg2 = true;
            }
            else if (has_suffix(opt.out_file_name, ".ps")) {
                opt.dump_ps = true;
            }
            else {
                fprintf(stderr, "Error: Unrecognised file ending: '%s'.  Use --dump-...\n",
                    opt.out_file_name);
                my_exit(1);
            }
        }
    }

    /* process files */
    FILE *fin = fopen(in_file_name, "rt");
    if (fin == NULL) {
        fprintf(stderr, "Error: Unable to open '%s' for reading: %s\n",
            in_file_name, strerror(errno));
        my_exit(1);
    }

    cp_syn_tree_t *r = CP_NEW(*r);
    bool ok = do_file(sout, &opt, r, in_file_name, fin);
    fclose(fin);

    if (fout != NULL) {
        fclose(fout);
    }

    /* print error (FIXME: make this readable) */
    if (!ok) {
        cp_syn_loc_t loc;
        bool have_loc = cp_syn_get_loc(&loc, r, r->err.loc);

        cp_vchar_t src_line;
        cp_vchar_init(&src_line);
        size_t pos = CP_SIZE_MAX;

        if (have_loc) {
            format_source_line(
                &src_line,
                &pos,
                loc.orig + CP_PTRDIFF(r->err.loc, loc.copy),
                loc.orig,
                CP_PTRDIFF(loc.orig_end, loc.orig));
            if (pos != CP_SIZE_MAX) {
                fprintf(stderr, "%s:%"_Pz"u:%"_Pz"u: ",
                    loc.file->filename.data, loc.line+1, pos+1);
            }
            else {
                fprintf(stderr, "%s:%"_Pz"u: ", loc.file->filename.data, loc.line+1);
            }
        }

        if (r->err.msg.size > 0) {
            if (r->err.msg.data[r->err.msg.size-1] != '\n') {
                cp_vchar_push(&r->err.msg, '\n');
            }

            fprintf(stderr, "Error: %s", r->err.msg.data);
        }
        else {
            fprintf(stderr, "Error: Unknown failure.\n");
        }
        if (have_loc) {
            fprintf(stderr, " %s", src_line.data);
            if (pos != CP_SIZE_MAX) {
                fprintf(stderr, " %*s^\n", (int)pos, "");
            }
        }

        cp_syn_loc_t loc2;
        bool have_loc2 = cp_syn_get_loc(&loc2, r, r->err.loc2);
        if (have_loc2 && (r->err.loc2 != r->err.loc)) {
            cp_vchar_t src_line2;
            cp_vchar_init(&src_line2);
            size_t pos2 = CP_SIZE_MAX;
            format_source_line(
                &src_line2,
                &pos2,
                loc2.orig + CP_PTRDIFF(r->err.loc2, loc2.copy),
                loc2.orig,
                CP_PTRDIFF(loc2.orig_end, loc2.orig));
            if (pos2 != CP_SIZE_MAX) {
                fprintf(stderr, "%s:%"_Pz"u:%"_Pz"u: Info: See also here:\n",
                    loc2.file->filename.data, loc2.line+1, pos2+1);
                fprintf(stderr, " %s", src_line2.data);
                fprintf(stderr, " %*s^\n", (int)pos2, "");
            }
        }

        my_exit(1);
    }

    my_exit(0);
}
