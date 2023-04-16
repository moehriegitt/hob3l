/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <stdlib.h>

#include <hob3lop/hedron.h>
#include <hob3lop/op-ps.h>
#include <hob3lop/op-slice.h>
#include <hob3lop/op-sweep.h>
#include <hob3lop/op-poly.h>
#include <hob3lop/op-trianglify.h>

#define OUT_TEST "out/test/hob3lop/"

#define CQ_TYPE_VTRI3   1
#define CQ_TYPE_VVVEC3  2
#define CQ_TYPE_SUB     3
#define CQ_TYPE_ADD     4
#define CQ_TYPE_VLINE2  5

typedef struct {
    unsigned type;
    void *data;
    size_t size;
} cq_csg3_t;

#pragma GCC diagnostic ignored "-Wunused-variable"

#include "useless_box.inc"
#include "test1.inc"
#include "test2.inc"
#include "corner1.inc"
#include "corner1b.inc"
#include "corner2.inc"

#include "hob3lop-test-1.inc"

static cp_bool_bitmap_t comb = { .b = { 0x00 } };
static size_t comb_size = 8;


static void ps_page(
    cq_v_line2_t const *gon)
{
    FILE *f = cq_ps_file();
    if (f == NULL) {
        return;
    }
    cq_ps_page_begin();
    fprintf(f, "%g %g moveto (%zu lines) show\n",
        cq_ps_left(), cq_ps_bottom() - 14, gon->size);
    for (cp_v_eachp(i, gon)) {
        cq_ps_line(i->a.x, i->a.y, i->b.x, i->b.y);
    }
    cq_ps_page_end();
}

#if 0
/* 3d structures in integer are not supported anymore, the code now
 * uses cp_a_vec3_loc_ref_t as a base type.
 */
CP_UNUSED
static void test_slice(
    cp_pool_t *pool CP_UNUSED,
    char const *psfn CP_UNUSED,
    cq_v_v_vec3_t *hedron CP_UNUSED)
{
    cq_ps_doc_begin(psfn);

    cq_vec3_minmax_t bb3 = CQ_VEC3_MINMAX_INIT;
    cq_v_v_vec3_minmax(&bb3, hedron);

    cq_vec2_minmax_t bb2;
    cq_vec3_minmax_minmax2(&bb2, &bb3, 0, 1);
    ps_minmax2dim(&bb2);

    for (cq_dim_t z = bb3.min.z; z <= bb3.max.z; z += mm2rel(0.2)) {
        fprintf(stderr, "DEBUG: z=%d\n", z);
        cq_v_line2_t *gon = CP_NEW(*gon);
        cq_slice(gon, z, hedron);
        fprintf(stderr, "DEBUG: done: %zu lines\n", gon->size);
        ps_page(gon);

#if 1
        cq_v_line2_t *gon2 = cq_sweep_copy(pool, gon, CP_COLOR_RGBA_WHITE);
        assert(!cq_has_intersect(NULL, NULL, NULL, gon2));
        cp_v_delete(&gon2);
#endif

        cp_v_delete(&gon);
    }

    cq_ps_doc_end();
}
#endif

CP_UNUSED
static void test_trianglify_gon(
    cp_pool_t *pool,
    char const *psfn CP_UNUSED,
    cq_v_line2_t *gon)
{
    fprintf(stderr, "WRITING %s\n", psfn);
    cq_ps_doc_begin(psfn);

    size_t size_cnt = gon->size;

    cq_sweep_t *s = cq_sweep_new(pool, NULL, size_cnt);
    cq_sweep_add_v_line2(s, gon, 1);

    cq_vec2_minmax_t minmax = CQ_VEC2_MINMAX_INIT;
    cq_sweep_minmax(&minmax, s);
    cq_ps_init(&minmax);

    ps_page(gon);

    cq_sweep_intersect(s);
    cq_sweep_reduce(s, &comb, comb_size);

    cp_err_t err[1] = {};
    cq_csg2_poly_t *poly = CP_CLONE1(&CQ_CSG2_POLY_INIT);
    if (!cq_sweep_poly(err, s, poly)) {
        fprintf(stderr, "INFO: %s\n", err->msg.data);
    }
    cq_csg2_poly_delete(poly);

    *err = (cp_err_t){};
    cq_csg2_poly_t *tri = CP_CLONE1(&CQ_CSG2_POLY_INIT);
    if (!cq_sweep_trianglify(err, s, tri)) {
        fprintf(stderr, "INFO: %s\n", err->msg.data);
    }
    cq_csg2_poly_delete(tri);

    cq_v_line2_t *r = CP_NEW(*r);
    cp_v_init0(r, 0);
    cq_sweep_get_v_line2(r, s);

    cq_sweep_delete(s);

    assert(!cq_has_intersect(NULL, NULL, NULL, r));
    cp_v_delete(&r);

    cq_ps_doc_end();
}

static inline cq_dim_t rand_coord(unsigned x)
{
    return (rand() % (int)x) - ((int)x / 2);
}

CP_UNUSED
static void test_random_trianglify(
    cp_pool_t *pool,
    char const *psfn,
    unsigned mode,
    unsigned cnt,
    unsigned sz)
{
    cq_v_line2_t *g = CP_NEW(*g);
    cq_line2_t l;
    l.b.x = rand_coord(sz);
    l.b.y = rand_coord(sz);
    cq_vec2_t v0 = l.b;
    for (unsigned i = 0; i < cnt; i++) {
        switch (mode) {
        case 1:
            l.a = l.b;
            l.b.x = rand_coord(sz);
            l.b.y = rand_coord(sz);
            break;
        case 2:
            l.a = l.b;
            l.b.x += rand_coord(sz);
            l.b.y += rand_coord(sz);
            break;
        }
        cp_v_push(g, l);
    }
    l.a = l.b;
    l.b = v0;
    cp_v_push(g, l);
    test_trianglify_gon(pool, psfn, g);
    cp_v_delete(&g);
}

static inline bool vec2_nil(cq_vec2_t p)
{
    return (p.x == CQ_DIM_MIN) || (p.y == CQ_DIM_MIN);
}

static cq_vec2_t const EE = CQ_VEC2(CQ_DIM_MIN, CQ_DIM_MIN);

typedef struct {
    size_t max;
    cq_sweep_t *s;
    size_t mask;
    cq_v_line2_t *g;
    cq_vec2_t p;
    cq_vec2_t q;
    cq_vec2_t r;
    cq_vec2_minmax_t minmax;
} constr_gon_t;

static void flush_gon(
    constr_gon_t *c)
{
    if (!vec2_nil(c->r)) {
        // fprintf(stderr, "DEBUG: E: %d,%d..%d,%d\n", q.x,q.y, r.x,r.y);
        cp_v_push(c->g, CQ_LINE2(c->q, c->r));

        /* next poly */
        cq_sweep_add_v_line2(c->s, c->g, c->mask);

        cq_sweep_minmax(&c->minmax, c->s);
        cq_ps_init(&c->minmax);
        ps_page(c->g);

        cp_v_clear(c->g, 0);
        c->mask = (c->mask << 1) & 0x7;
        if (c->mask == 0) {
            c->mask = 1;
        }
    }
    c->p = c->q = c->r = EE;
}

static void fuzz(
    cp_pool_t *pool,
    int argc,
    char **argv)
{
    unsigned max = 250; /* max. number of lines */
    for (int i = 1; i < argc; i++) {
        char const *fn = argv[i];
        if (strncmp(fn, "--max=", 6) == 0) {
            max = (unsigned)atoi(fn + 6);
            continue;
        }

        FILE *f = fopen(fn, "rt");
        if (f == NULL) {
            fprintf(stderr, "%s: ERROR: %m\n", fn);
            exit(1);
        }

        /* load params */
        unsigned bit_cnt = 1;
        if (fread(&bit_cnt, sizeof(bit_cnt), 1, f) != 1) {
            bit_cnt = 8;
        }
        if ((bit_cnt < 2) || (bit_cnt > 31)) {
            fprintf(stderr, "%u: unsupported coordinate bit count\n", bit_cnt);
            exit(1);
        }
        /* bit_cnt <- 2..31 */

        unsigned chunk = 1;
        unsigned shift_r = 0;
        unsigned max_shift_l = 0;
        if (bit_cnt <= 8) {
            chunk = 1;
            shift_r = 8 - bit_cnt;
            max_shift_l = 23;
        }
        else if (bit_cnt <= 16) {
            chunk = 2;
            shift_r = 16 - bit_cnt;
            max_shift_l = 15;
        }
        else {
            chunk = 4;
            shift_r = 32 - bit_cnt;
            max_shift_l = 0;
        }

        unsigned shift_l = 0;
        if (fread(&shift_l, sizeof(shift_l), 1, f) != 1) {
            shift_l = 0;
        }
        if (shift_l > max_shift_l) {
            fprintf(stderr, "ERROR: bad shift: %u, max is %u\n", shift_l, max_shift_l);
            exit(1);
        }

        /* init */
        char const *psfn = "out/fuzz/out.ps";
        fprintf(stderr, "WRITING %s\n", psfn);
        cq_ps_doc_begin(psfn);

        /* load poly */
        constr_gon_t c[1] = {{
            .s = cq_sweep_new(pool, NULL, 0),
            .max = max,
            .mask = 1,
            .g = CP_NEW(*c->g),
            .p = EE,
            .q = EE,
            .r = EE,
            .minmax = CQ_VEC2_MINMAX_INIT,
        }};
        size_t total_cnt = 0;
        for (;;) {
            size_t cnt;
            bool is_sep = false;
            switch (chunk) {
            case 1:{
                struct { signed char x, y; } p1;
                cnt = fread(&p1, sizeof(p1), 1, f);
                is_sep = (p1.x == -0x80) || (p1.y == -0x80);
                c->p.x = p1.x;
                c->p.y = p1.y;
                break;}
            case 2:{
                struct { signed short x, y; } p1;
                cnt = fread(&p1, sizeof(p1), 1, f);
                is_sep = (p1.x == -0x8000) || (p1.y == -0x8000);
                c->p.x = p1.x;
                c->p.y = p1.y;
                break;}
            default:
                cnt = fread(&c->p, sizeof(c->p), 1, f);
                is_sep = (c->p.x == CQ_DIM_MIN) || (c->p.y == CQ_DIM_MIN);
                c->p.x >>= 1;
                c->p.y >>= 1;
                break;
            }
            if (cnt == 0) {
                break;
            }
            c->p.x >>= shift_r;
            c->p.x <<= shift_l;

            c->p.y >>= shift_r;
            c->p.y <<= shift_l;

            if (is_sep) {
                flush_gon(c);
            }
            else {
                c->p.x /= 2;
                c->p.y /= 2;
                if (vec2_nil(c->r)) {
                    // fprintf(stderr, "DEBUG: A: %d,%d\n", p.x,p.y);
                    c->q = c->r = c->p;
                }
                else {
                    // fprintf(stderr, "DEBUG: L: %d,%d..%d,%d\n", q.x,q.y, p.x,p.y);
                    cp_v_push(c->g, CQ_LINE2(c->q, c->p));
                    c->q = c->p;
                }
                total_cnt++;
            }
            if (total_cnt > c->max) {
                fprintf(stderr, "too large, giving up.\n");
                exit(0); /* larger things are not better, but take longer in fuzzing */
            }
        }
        fclose(f);
        flush_gon(c);

        cp_v_delete(&c->g);

        cq_sweep_intersect(c->s);
        cq_sweep_reduce(c->s, &comb, comb_size);

        cq_v_line2_t *res = CP_NEW(*res);
        cp_v_init0(res, 0);
        cq_sweep_get_v_line2(res, c->s);

        cq_csg2_poly_t *poly = CP_CLONE1(&CQ_CSG2_POLY_INIT);
        bool poly_ok = cq_sweep_poly(NULL, c->s, poly);
        assert(poly_ok);
        cq_csg2_poly_delete(poly);

        cq_csg2_poly_t *tri = CP_CLONE1(&CQ_CSG2_POLY_INIT);
        bool tri_ok = cq_sweep_trianglify(NULL, c->s, tri);
        assert(tri_ok);
        cq_csg2_poly_delete(tri);

        cq_sweep_delete(c->s);

        assert(!cq_has_intersect(NULL, NULL, NULL, res));
        cp_v_delete(&res);


        cq_ps_doc_end();
    }
}

int main(int argc, char **argv)
{
    cp_pool_t pool[1];
    cp_pool_init(pool);

    comb_size = 0;
    for (unsigned b0 = 0; b0 < 2; b0++) {
        for (unsigned b1 = 0; b1 < 2; b1++) {
            for (unsigned b2 = 0; b2 < 2; b2++) {
                cp_bool_bitmap_set(&comb, comb_size, (b0 & b1) | b2);
                comb_size++;
            }
        }
    }

    bool do_random = false;
    if (argc >= 2) {
        if (strcmp(argv[1], "--random") == 0) {
            do_random = true;
        }
        else {
            fuzz(pool, argc, argv);
            return 0;
        }
    }
    cq_mat_test();

#if 0
    test_slice(pool, OUT_TEST"useless_box.ps", &useless_box_vvvec3);
    test_slice(pool, OUT_TEST"corner1b.ps",    &corner1b_vvvec3);

    test_slice(pool, OUT_TEST"corner2.ps",     &corner2_vvvec3);
#endif

#if 1
#define TEST_TRIANGLIFY_GON(NAME) \
    test_trianglify_gon(pool, OUT_TEST"" #NAME ".ps", &NAME##_vline2)
#include "hob3lop-test-2.inc"
#endif

    srand(11);
    test_random_trianglify(pool, OUT_TEST"random1.ps",   1, 30, 128);
    test_random_trianglify(pool, OUT_TEST"randfail1.ps", 1, 30, 32);

    srand(401);
    test_random_trianglify(pool, OUT_TEST"random1.ps",   1, 10, 128);
    test_random_trianglify(pool, OUT_TEST"random2.ps",   1, 10, 32);
    test_random_trianglify(pool, OUT_TEST"randfail2.ps", 1, 10, 16);

    srand(489);
    test_random_trianglify(pool, OUT_TEST"randfail3.ps",   1, 10, 128);

    srand(13717);
    test_random_trianglify(pool, OUT_TEST"random1.ps",   1, 10, 128);
    test_random_trianglify(pool, OUT_TEST"randfail4.ps", 1, 10, 32);

    srand(0);
    test_random_trianglify(pool, OUT_TEST"random1.ps", 1, 30, 150000);
    test_random_trianglify(pool, OUT_TEST"random1.ps", 1, 30, 128);
    test_random_trianglify(pool, OUT_TEST"random1.ps", 1, 30, 20000000);
    test_random_trianglify(pool, OUT_TEST"random1.ps", 1, 30, 16);
    test_random_trianglify(pool, OUT_TEST"randfail5.ps", 1, 30, 9);

    srand(34);
    test_random_trianglify(pool, OUT_TEST"random0.ps",   1, 30, 150000);
    test_random_trianglify(pool, OUT_TEST"randfail6.ps", 1, 30, 128);

    if (!do_random) {
        return 0;
    }

    for (unsigned i = 0;; i++) {
        srand(i);
        FILE *f = fopen(OUT_TEST"random.srand.new", "wt");
        if (f == NULL) {
            fprintf(stderr, "ERROR: %m\n");
            exit(1);
        }
        fprintf(f, "%u\n", i);
        fclose(f);
        rename(OUT_TEST"random.srand.new", OUT_TEST"random.srand");

        test_random_trianglify(pool, OUT_TEST"random0.ps", 1, 30, 150000);
        test_random_trianglify(pool, OUT_TEST"random1.ps", 1, 30, 128);
        test_random_trianglify(pool, OUT_TEST"random2.ps", 1, 30, 20000000);
        test_random_trianglify(pool, OUT_TEST"random3.ps", 1, 30, 16);
        test_random_trianglify(pool, OUT_TEST"random4.ps", 1, 30, 9);

        test_random_trianglify(pool, OUT_TEST"random5.ps", 2, 30, 5);
    }

#if 0
    for (unsigned i = 48456926;; i++) {
        srand(i);
        FILE *f = fopen(OUT_TEST"random.srand.new", "wt");
        if (f == NULL) {
            fprintf(stderr, "ERROR: %m\n");
            exit(1);
        }
        fprintf(f, "%u\n", i);
        fclose(f);
        rename(OUT_TEST"random.srand.new", OUT_TEST"random.srand");

        test_random_trianglify(pool, OUT_TEST"random1.ps", 1, 10, 128);
        test_random_trianglify(pool, OUT_TEST"random2.ps", 1, 10, 32);
        test_random_trianglify(pool, OUT_TEST"random3.ps", 1, 10, 16);
        test_random_trianglify(pool, OUT_TEST"random4.ps", 1, 10, 256);

        test_random_trianglify(pool, OUT_TEST"random5.ps", 2, 10, 5);
    }
#endif

    return 0;
}
