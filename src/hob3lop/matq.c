/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <hob3lop/matq.h>
#include <hob3lop/gon.h>

extern void cq_overflow_fail_(
    char const *file,
    int line,
    char const *cond)
{
    fprintf(stderr,
        "%s:%d: ERROR: numeric overflow: %s\n"
        "\tThe input coordinates may be too large.\n"
        "\tTry --grid with a value smaller than the current %s.\n",
        file, line, cond, cq_dim_scale_str());
#ifndef NDEBUG
    abort();
#endif
    exit(1);
}

#if 0
/** floor variant */
extern cq_divmod_t cq_divmod(
    cq_dim_t x,
    cq_dim_t d)
{
    cq_divmod_t v = {
        .div = x / d,
        .mod = x % d
    };
    if ((v.mod ^ d) >= 0) {
        return v;
    }
    v.div -= 1;
    v.mod += d;
    return v;
}
#endif

/**
 * Euclidean variant:
 * for normal width type
 */
extern cq_divmod_t cq_divmod(
    cq_dim_t x,
    cq_dim_t d)
{
    cq_divmod_t v = {
        .div = x / d,
        .mod = x % d
    };
    if (v.mod >= 0) {
        return v;
    }
    if (d >= 0) {
        v.div -= 1;
        v.mod += d;
    }
    else {
        v.div += 1;
        v.mod -= d;
    }
    return v;
}

/**
 * Euclidean variant:
 * for wide type
 */
extern cq_divmodw_t cq_divmodw(
    cq_dimw_t x,
    cq_dimw_t d)
{
    cq_divmodw_t v = {
        .div = x / d,
        .mod = x % d
    };
    if (v.mod >= 0) {
        return v;
    }
    if (d >= 0) {
        v.div -= 1;
        v.mod += d;
    }
    else {
        v.div += 1;
        v.mod -= d;
    }
    return v;
}

/**
 * Euclidean variant: for wide/normal width => normal width
 * This may fail with an overflow.
 */
extern cq_divmod_t cq_divmodx(
    cq_dimw_t x,
    cq_dim_t d)
{
    cq_divmod_t v = {
        .div = (cq_dim_t)(x / d),
        .mod = (cq_dim_t)(x % d)
    };
    if ((v.mod + (v.div * d)) != x) {
        cq_fail("overflow in divide");
    }
    if (v.mod >= 0) {
        return v;
    }
    if (d >= 0) {
        v.div -= 1;
        v.mod += d;
    }
    else {
        v.div += 1;
        v.mod -= d;
    }
    return v;
}

extern int cq_cmp_edge_rnd(
    cq_dim_t vx, cq_dim_t vy,
    cq_dim_t lx, cq_dim_t ly,
    cq_dim_t kx, cq_dim_t ky)
{
    cq_dimw_t bb = cq_right_cross3_z(vx, vy, lx, ly, kx, ky); /* Note: v,l,k (not v,k,l!) */

    /* obivous case */
    if (bb >= +CQ_DIM_W) {
        return +1;
    }
    if (bb <= -CQ_DIM_W) {
        return -1;
    }

    /* check plausibility */
    if ((vx < lx) && (vx < kx)) { return bb == 0 ? -1 : CP_SIGN(bb); }
    if ((vx > lx) && (vx > kx)) { return bb == 0 ? +1 : CP_SIGN(bb); }
    if ((vy < ly) && (vy < ky)) { return bb == 0 ? -1 : CP_SIGN(bb); }
    if ((vy > ly) && (vy > ky)) { return bb == 0 ? +1 : CP_SIGN(bb); }

    /* check crossing of rounded rect */
    cq_dim_t ky_ly = cq_dim_sub(ky, ly);
    cq_dim_t kx_lx = cq_dim_sub(kx, lx);

    bb *= 2;

    cq_dimw_t bbp = cq_dimw_add(bb, ky_ly);
    cq_dimw_t bbm = cq_dimw_sub(bb, ky_ly);

    if (cq_dimw_add(bbm, kx_lx) > 0) {
        if (cq_dimw_add(bbp, kx_lx) >= 0) {
            return +1;
        }
    }
    if (cq_dimw_sub(bbm, kx_lx) <= 0) {
        if (cq_dimw_sub(bbp, kx_lx) <= 0) {
            return -1;
        }
    }

    return 0;
}

/*
 * 96 bit arthmetics -- hopefully we don't need this too often.
 */

typedef struct {
    cq_udimw_t hi;
    cq_udim_t  lo;
} cq_udime_t;

#define CQ_UDIME_0 ((cq_udime_t){0,0})

CP_UNUSED
static cq_udime_t cq_udime_add(
    cq_udime_t a,
    cq_udime_t b)
{
    cq_udimw_t hi = cq_udimw_add(a.hi, b.hi);
    cq_udim_t lo;
    if (cq_udim_add_ovf(&lo, a.lo, b.lo)) {
        hi = cq_udimw_add(hi, 1);
    }
    return (cq_udime_t){ .hi = hi, .lo = lo };
}

static cq_udime_t cq_udime_sub(
    cq_udime_t a,
    cq_udime_t b)
{
    cq_udimw_t hi = cq_udimw_sub(a.hi, b.hi);
    cq_udim_t lo;
    if (cq_udim_sub_ovf(&lo, a.lo, b.lo)) {
        hi = cq_udimw_sub(hi, 1);
    }
    return (cq_udime_t){ .hi = hi, .lo = lo };
}

static cq_udime_t cq_udime_mul_wn(
    cq_udimw_t a,
    cq_udim_t  b)
{
    /* this cannot overflow, as 32x32 => 64 always works */
    cq_udim_t al = (cq_udim_t)a;
    cq_udim_t ah = (cq_udim_t)(a >> CQ_DIM_BITS);

    cq_udimw_t low = al * (cq_udimw_t)b;
    cq_udimw_t hiw = ah * (cq_udimw_t)b;

    hiw += (cq_udim_t)(low >> CQ_DIM_BITS);

    return (cq_udime_t){ .hi = hiw, .lo = (cq_udim_t)low };
}

static cq_udim_t cq_dim_div_ew(
    cq_udime_t u,
    cq_udimw_t v)
{
    cq_udimw_t u1 = u.hi;
    cq_udim_t  u0 = u.lo;

    if (v == 0) {
        cq_overflow_fail("division by zero");
    }
    if (u1 >= v) {
        cq_overflow_fail("overflow in division");
    }

    /* shift maximally */
    unsigned su = 0;
#ifdef __GNUC__
    if (v <= 0x00000000ffffffff) { v = v << 32; u1 = (u1 << 32) | u0; u0 = 0; }
    su = (unsigned)__builtin_clzll(v);
    if (su != 0) {
        v <<= su;
        u1 = (u1 << su) | (u0 >> (32 - su));
        u0 = u0 << su;
    }
#else
    if (v <= 0x00000000ffffffff) { v = v << 32; u1 = (u1 << 32) | u0; u0 = 0; }
    if (v <= 0x0000ffffffffffff) { v = v << 16; su += 16; }
    if (v <= 0x00ffffffffffffff) { v = v <<  8; su +=  8; }
    if (v <= 0x0fffffffffffffff) { v = v <<  4; su +=  4; }
    if (v <= 0x3fffffffffffffff) { v = v <<  2; su +=  2; }
    if (v <= 0x7fffffffffffffff) { v = v <<  1; su +=  1; }
    if (su != 0) {
        u1 = (u1 << su) | (u0 >> (32 - su));
        u0 = u0 << su;
    }
#endif

    /* compute */
    cq_udim_t v1 = (cq_udim_t)(v >> CQ_DIM_BITS);
    cq_udim_t v0 = (cq_udim_t)v;
    cq_udimw_t q = u1 / v1;
    cq_udimw_t r = u1 - (q * v1);

    for (;;) {
        if ((q >= CQ_DIM_W) || ((q * v0) > ((r * CQ_DIM_W) + u0))) {
            q = q - 1;
            r = r + v1;
            if (r < CQ_DIM_W) {
                continue;
            }
        }
        break;
    }

    return (cq_udim_t)q;
}

/**
 * This computes sx*(sm/sd) under the assumption that 0 <= sm/sd <= 1.
 */
static void cq_dimif_mul_div_nww(
    cq_dimif_t *res, cq_dim_t sx, cq_dimw_t sm, cq_dimw_t sd)
{
    assert(sm != sd);

    /* handle sign */
    bool neg = (sx < 0) ^ (sm < 0) ^ (sd < 0);
    cq_udim_t  x = cq_udim_abs (sx);
    cq_udimw_t m = cq_udimw_abs(sm);
    cq_udimw_t d = cq_udimw_abs(sd);

    /* We need 0 <= m/d < 1, otherwise this does not work.
     * The caller must check this.  m == 0 works here, but intersect also handles it */
    assert(m > 0);
    assert(m < d);

    /* mul */
    cq_udime_t w  = cq_udime_mul_wn(m, x);
    assert(w.hi < d); /* otherwise m>=d, which is excluded */

    /* quotient */
    cq_udim_t q = cq_dim_div_ew(w, d);

    /* modulo */
    cq_udime_t re = cq_udime_sub(w, cq_udime_mul_wn(d, q));
    assert(re.hi < CQ_DIM_W);
    cq_udimw_t r = re.lo + (re.hi * CQ_DIM_W);
    assert(r < d);

    /* negate */
    cq_dim_t  sq = (cq_dim_t)q;
    assert(sq >= 0);
    if (neg) {
        sq = -sq;
        if (r != 0) {
            r = d - r;
            sq--;
        }
    }
    assert(r < d);

    res->i = sq;
    res->n = r;
    res->d = d;
}

CP_UNUSED
static cq_dim_t cq_dim_mul_div_rnd_nww(
    cq_dim_t sx, cq_dimw_t sm, cq_dimw_t sd)
{
    cq_dimif_t res;
    cq_dimif_mul_div_nww(&res, sx, sm, sd);
    return cq_round(&res);
}

extern cq_udimq_t cq_udimw_mul_aux(cq_udimw_t a, cq_udimw_t b)
{
    cq_udim_t ah = cq_udimw_hi(a);
    cq_udim_t al = cq_udimw_lo(a);
    cq_udim_t bh = cq_udimw_hi(b);
    cq_udim_t bl = cq_udimw_lo(b);

    cq_udimw_t rlh = cq_udim_mul(al, bh);
    cq_udimw_t rhl = cq_udim_mul(ah, bl);
    cq_udimw_t rll = cq_udim_mul(al, bl);
    cq_udimw_t rhh = cq_udim_mul(ah, bh);

    cq_udimw_t rm = CQ_REDUCE(cq_udimw_add,
        (cq_udimw_t)0,
        cq_udimw_hi(rll),
        cq_udimw_lo(rhl),
        cq_udimw_lo(rlh));

    cq_udimw_t rh = CQ_REDUCE(cq_udimw_add,
        rhh,
        cq_udimw_hi(rhl),
        cq_udimw_hi(rlh),
        cq_udimw_hi(rm));

    cq_udimw_t rl = cq_udimw(cq_udimw_lo(rm), cq_udimw_lo(rll));

    return cq_udimq(rh, rl);
}

extern int cq_dimif_cmp_frac_aux(cq_dimif_t const *a, cq_dimif_t const *b)
{
    /* numerators are both 0 => equal */
    if ((a->n | b->n) == 0) {
        return 0;
    }

    /* numerators are equal and denominators are equal => equal */
    if ((a->n == b->n) && (a->d == b->d)) {
        return 0;
    }

    /* numerator larger, denominator smaller => larger */
    if ((a->n >= b->n) && (a->d <= b->d)) {
        return +1;
    }
    /* numerator smaller, denominator larger => smaller */
    if ((a->n <= b->n) && (a->d >= b->d)) {
        return -1;
    }

    /* calculate in quad precision */
    return cq_udimq_cmp(cq_udimw_mul(a->n, b->d), cq_udimw_mul(a->d, b->n));
}

/**
 * Returns:
 *   -1 = collinear: parallel or overlapping: more checks necessary for
 *        overlap detection.
 *
 *    0 = no crossing within the segments: no further check necessary
 *
 *   >0 = one crossing, as returned in the output params
 *        bit 0: always 1
 *        bit 1: at end point p1
 *        bit 2: at end point p2
 *        bit 3: at end point p3
 *        bit 4: at end point p4
 *
 *        => 1 means it is a true crossing.
 *
 * This returns 0 also if one pair of points is equal, i.e., the lines
 * connect, but do not overlap.  If they are collinear, this will still
 * return -1 (it will do so even if they don't touch or are parallel).
 */
extern int cq_vec2if_intersect(
    cq_vec2if_t *i,
    cq_vec2_t p1,
    cq_vec2_t p2,
    cq_vec2_t p3,
    cq_vec2_t p4)
{
    /* This can handle 31bit signed ints as input (excluding min(s31))
     * without overflow; we assume for the calculations that cq_dim_t is s32. */

    /* differences (bits: s31 - s31 => s32) */
    cq_dim_t x2_1 = cq_dim_sub(p2.x, p1.x);
    cq_dim_t y2_1 = cq_dim_sub(p2.y, p1.y);
    cq_dim_t x4_3 = cq_dim_sub(p4.x, p3.x);
    cq_dim_t y4_3 = cq_dim_sub(p4.y, p3.y);

    /* determinant (bits: (s32*s32)-(s32*s32) => s63-s63 => s64)
     * This is the cross product z component of the differences.
     * (right_cross3_z, again).
     */
    cq_dimw_t d = cq_dimw_sub(cq_dim_mul(x2_1, y4_3), cq_dim_mul(y2_1, x4_3));
    if (d == 0) {
        return -1;
    }

    /* compute 0 <= t/d, u/d <= 1: the position on each segment */
    cq_dim_t x3_1 = cq_dim_sub(p3.x, p1.x);
    cq_dim_t y3_1 = cq_dim_sub(p3.y, p1.y);
    cq_dimw_t t = cq_dimw_sub(cq_dim_mul(x3_1, y4_3), cq_dim_mul(y3_1, x4_3));
    cq_dimw_t u = cq_dimw_sub(cq_dim_mul(x3_1, y2_1), cq_dim_mul(y3_1, x2_1));

    /* corner cases */

    /* d sign */
    bool d_neg = d < 0;
    if (d_neg) {
        d = cq_dimw_neg(d);
    }

    /* t sign */
    bool t_neg = false;
    if (t != 0) {
        t_neg = d_neg;
        if (t < 0) {
            t_neg = !d_neg;
            t = cq_dimw_neg(t);
        }
        if (t_neg || (t > d)) {
            return 0; /* t < 0, t > 1 => not between end points */
        }
    }

    /* u sign */
    bool u_neg = false;
    if (u != 0) {
        u_neg = d_neg;
        if (u < 0) {
            u_neg = !d_neg;
            u = cq_dimw_neg(u);
        }
        if (u_neg || (u > d)) {
            return 0; /* u < 0, u > 1 => not between end points */
        }
    }

    /* p1..p2 position: t/d */
    if (t == 0) {
        *i = CQ_VEC2IFI(p1.x, p1.y);
        return 1 | 2 | ((u == 0) * 8) | ((u == d) * 16);
    }
    if (t == d) {
        *i = CQ_VEC2IFI(p2.x, p2.y);
        return 1 | 4 | ((u == 0) * 8) | ((u == d) * 16);
    }

    /* p3..p4 position: u/d */
    if (u == 0) {
        *i = CQ_VEC2IFI(p3.x, p3.y);
        return 1 | 8;
    }
    if (u == d) {
        *i = CQ_VEC2IFI(p4.x, p4.y);
        return 1 | 16;
    }

    /* calculation of non-trivial intersection */
    cq_dimif_mul_div_nww(&i->x, x2_1, t, d);
    cq_dimif_mul_div_nww(&i->y, y2_1, t, d);

    i->x.i = cq_dim_add(p1.x, i->x.i);
    i->y.i = cq_dim_add(p1.y, i->y.i);

#ifndef NDEBUG
    /* This uses exact arithmetics.  So deriving the intersection from t
     * must yield exactly the same as deriving it from u: */
    cq_dimif_t jx, jy;
    cq_dimif_mul_div_nww(&jx, x4_3, u, d);
    cq_dimif_mul_div_nww(&jy, y4_3, u, d);

    jx.i = cq_dim_add(p3.x, jx.i);
    jy.i = cq_dim_add(p3.y, jy.i);
    cq_assert(i->x.i == jx.i);
    cq_assert(i->x.n == jx.n);
    cq_assert(i->x.d == jx.d);
    cq_assert(i->y.i == jy.i);
    cq_assert(i->y.n == jy.n);
    cq_assert(i->y.d == jy.d);
#endif

    return 1;
}

/* ********************************************************************** */
/* ********************************************************************** */
/* ********************************************************************** */
/* tests */

extern void cq_mat_test(void)
{
#ifndef NDEBUG
    /* div 96:64 => 32 */
    assert(cq_dim_div_ew((cq_udime_t){ 0xe213c556, 0xb278366e }, 0xb8b7e2b05dd) == 0x13951A);

    /* misc */
    assert(cq_right_cross3_z (1,0, 0,0, 0,1) > 0);
    assert(cq_cmp_edge_rnd   (1,0, 0,0, 0,1) > 0);

    /* tolerant edge-vector comparison */
#define cmp_edge(vx,vy, kx,ky, lx,ly) cq_cmp_edge_rnd(vx,vy, lx,ly, kx,ky)

    assert(cmp_edge(5,3, 0,0, 9,5) == 0);  /* from Hobby paper: */
    assert(cmp_edge(5,4, 3,5, 10,2) == 0);
    assert(cmp_edge(8,3, 3,5, 10,2) == 0);

    /* exact crossings: */
    assert(cmp_edge(7,4, 4,2, 10,6) == 0);
    assert(cmp_edge(70,40, 40,20, 100,60) == 0);
    assert(cmp_edge(700,400, 400,200, 1000,600) == 0);
    assert(cmp_edge(7000,4000, 4000,2000, 10000,6000) == 0);
    assert(cmp_edge(0x7000,0x4000, 0x4000,0x2000, 0xA000,0x6000) == 0);
    assert(cmp_edge(0x70000,0x40000, 0x40000,0x20000, 0xA0000,0x60000) == 0);
    assert(cmp_edge(0x700000,0x400000, 0x400000,0x200000, 0xA00000,0x600000) == 0);
    assert(cmp_edge(0x7000000,0x4000000, 0x4000000,0x2000000, 0xA000000,0x6000000) == 0);
    assert(cmp_edge(7,4, 4,4, 11,4) == 0);
    assert(cmp_edge(7,4, 4,5,  7,4) == 0);
    assert(cmp_edge(7,4, 6,2,  7,4) == 0);
    assert(cmp_edge(6,2, 6,2,  7,4) == 0);

    assert(cmp_edge(5,2, 6,2,  7,2) == -1);
    assert(cmp_edge(8,2, 6,2,  7,2) == +1);
    assert(cmp_edge(7,2, 6,2,  7,2) ==  0);
    assert(cmp_edge(6,2, 6,2,  7,2) ==  0);

    /* inexact crossings */
    assert(cmp_edge(7,4, 4,2,  9,6) == 0);
    assert(cmp_edge(7,4, 4,2,  8,4) == 0);
    assert(cmp_edge(7,4, 4,2,  8,4) == 0);
    assert(cmp_edge(7,4, 4,2, 10,5) == 0);
    assert(cmp_edge(7,4, 4,5, 10,2) == 0);
    assert(cmp_edge(7,4, 4,5, 11,1) == 0);
    assert(cmp_edge(7,4, 4,5, 13,1) == 0);
    assert(cmp_edge(7,4, 4,5, 13,2) == 0);
    assert(cmp_edge(7,4, 4,5, 13,3) == 0);
    assert(cmp_edge(7,4, 4,5,  8,3) == 0);
    assert(cmp_edge(7,4, 4,5,  9,3) == 0);
    assert(cmp_edge(7,4, 4,5, 10,3) == 0);
    assert(cmp_edge(7,4, 4,5, 11,3) == 0);
    assert(cmp_edge(7,4, 4,5, 12,3) == 0);
    assert(cmp_edge(7,4, 4,5, 13,3) == 0);
    assert(cmp_edge(7,4, 4,5, 14,3) == 0);
    assert(cmp_edge(7,4, 4,5, 15,3) == 0);
    assert(cmp_edge(7,4, 4,5,  8,4) == 0);
    assert(cmp_edge(7,4, 4,5,  9,4) == 0);
    assert(cmp_edge(7,4, 4,5, 10,4) == 0);
    assert(cmp_edge(7,4, 6,8,  8,3) == 0);
    assert(cmp_edge(7,4, 7,8,  8,0) == 0);
    assert(cmp_edge(7,4, 5,1,  7,5) == 0);

    /* corner crossings */
    assert(cmp_edge(7,4, 4,2, 8, 6) == -1); /* top-left     => v is below */
    assert(cmp_edge(7,4, 4,5, 11,4) == -1); /* top-right    => v is below */
    assert(cmp_edge(7,4, 4,5, 9, 2) ==  0); /* bottom-left  => v is on */
    assert(cmp_edge(7,4, 4,2, 11,5) == +1); /* bottom-right => v is above */
    assert(cmp_edge(7,4, 6,2,  8,4) == +1);
    assert(cmp_edge(7,4, 7,8,  8,1) == -1);

    /* fully outside */
    assert(cmp_edge(7,4, 4,5, 13,4) == -1);
    assert(cmp_edge(7,4, 4,5, 13,5) == -1);
    assert(cmp_edge(7,4, 4,5,  7,3) == +1);
    assert(cmp_edge(7,4, 4,5, 12,4) == -1);
    assert(cmp_edge(7,4, 6,2,  9,4) == +1);
    assert(cmp_edge(7,4, 6,2,  6,4) == -1); /* right-of == below */
    assert(cmp_edge(7,4, 6,2,  6,7) == -1); /* right-of == below */

    assert(cmp_edge(7,4, 6,8,  7,5) == -1); /* the extension would cut (7,4)# */
    assert(cmp_edge(7,4, 7,1,  7,3) == +1); /* the extension would cut */

    /* large mul */
    /* some test cases generated with commonlisp:

    (defun mulrand()
        (let* (
            (a (random (expt 2 64)))
            (b (random (expt 2 64)))
            (x (* a b)))
            (format t "    { 0x~16,'0x, 0x~16,'0x, 0x~16,'0x, 0x~16,'0x },~&"
                a
                b
                (ash x -64)
                (logand x (1- (expt 2 64))))))

    */
    static struct { cq_udimw_t a,b,h,l; } tm[] = {
    { 0x2BAC9857313D563A, 0x5B4B0C9099E996B, 0xF9327DC628802D, 0x15F3E84C7EF7B43E },
    { 0x6C39A3701CF8F63A, 0xBD6276D15400194F, 0x50102DF56635F54D, 0x4D34D49248DFA5E6 },
    { 0x90D1FA19A3ADE9C4, 0xBE4E485D84BB5EAE, 0x6BA82089310047BC, 0x8352B04DEE36DB38 },
    { 0xCAFF0797817C956C, 0x21869F603FE2B4B2, 0x1A959FD939015001, 0x8555E3A51F07D518 },
    { 0x1A06636E8576E855, 0x3D256DCD3ABE81D4, 0x63753C74A331E57, 0xF21644BC22A13B64 },
    { 0x3435A4DD4B6109AB, 0x9852290FD55DCD57, 0x1F109B7205B3A066, 0x4D5E4FE21CD7381D },
    { 0x5E8E265E245F3628, 0xC7C2C58F3B86F4CA, 0x49C870848C880F7D, 0x9891049D03AEDB90 },
    { 0xCAFEB929C0A6CD91, 0x71C9A233147E9188, 0x5A3A525881FD762B, 0xDAF69458546A5608 },
    { 0x111948B1AD089038, 0xF0BE63DCD5DA8E39, 0x10146B960D3644AE, 0x1DEACEF3AE972C78 },
    { 0x88F26E230FEA0C7A, 0xFA4FC0E532576C51, 0x85E7698EA8C1688D, 0xA5906BEAD7C76A9A },
    { 0xBB98F3EC42043C41, 0x5A0C3CA9DA65B98E, 0x41FCBD60CF77C6DB, 0x4381B2A5CD89650E },
    { 0xC0534932E7FD5282, 0x2D2FD2FBD8E5B57C, 0x21F291A9E9C77ED5, 0xBC4C3F5CF853E0F8 },
    { 0x3990E55712B3B8BD, 0x258493AE434726F2, 0x86FC11139E76B41, 0xCDBF09BE0EBBB0AA },
    { 0x0C12D4E45DFD68C7, 0xA69F1CA0F24745FD, 0x07DBB717F543E51D, 0x819DD1BA5DDF2FAB },
    { 0x4C21AF2E518E9545, 0x9DABE96547926C8F, 0x2EE3C85832F9145E, 0x76F52537F9F87D8B },
    { 0xAE62F73BD4F3C8FB, 0x94FF5266033A3442, 0x657F23A40C8889FE, 0x04507946E58ACCB6 },
    { 0x0EB3F2B4AE00E8E5, 0xE764CC2B9366E117, 0x0D4A2A05BDAFC681, 0x7E950FB9B9043193 },
    { 0x7755F6E0D3AAFC6B, 0x335A9C972D18283E, 0x17F05D6670607936, 0x19F7E90973E1D9EA },
    { 0xDBC4881FD4CC36E3, 0xB5AAF26B5936D191, 0x9BF4B4DCD7011191, 0xA4207FCBBF5C6993 },
    { 0x3350E971372CB10C, 0x69416CC358D60916, 0x15194D18004CA32F, 0x128C73077018A308 },
    { 0x67E11BEEE555D293, 0x6A7B1296538A607F, 0x2B3526442A810616, 0xEA019D2F20C896ED },
    { 0x7EF04E4D80766B90, 0xA9EEBA376D7C80AB, 0x544303700AAACAA0, 0x925F6F186EA1D930 },
    { 0x4CAEBC60C25D7E08, 0x006BE0D8259D8B24, 0x002050625FF94ADA, 0xA6CB14718B7C1120 },
    { 0xE2DEFE940A51B76B, 0x33ADF204B7232BA1, 0x2DCC93E48289BFF7, 0x7197CFCBC7D4534B },
    { 0xC6CEC973B64B9A15, 0x7101FF23AC315984, 0x57C2D3DE8B942528, 0xCE2FC5E7E191BFD4 },
    { 0xCB891D2408D856CE, 0x30DCE6F85A57C95B, 0x26D956EE7176C4FA, 0xBC92E25AED10993A },
    { 0x371FE4E812AB564C, 0x5C1FED24A4490E61, 0x13D6563089970983, 0x95EE01C8BD4FDACC },
    { 0x9BAB069D65A472AE, 0xE989C1A216F16BFD, 0x8E026D4A4BE86ADA, 0x1E55E4D019420FF6 },
    { 0x865AE23C3DD0E101, 0xFF91F84A4AD3573F, 0x862123232E6704D3, 0x7DB07C52F0B1B63F },
    { 0xD8BC415C4BACCABD, 0xF4ACF0515D4FA868, 0xCF25D83D41FA92E4, 0x951194EE5C9164C8 },
    { 0x4A0BB6AF61AB9746, 0x4DB62BC5583F7C60, 0x167A36EFAD497A83, 0x44D5A52C07D8A240 },
    { 0x30366D83F5CCE56E, 0x17FF17147905C640, 0x0484EE66B0697943, 0xB041E52365D26F80 },
    { 0x49E66940AEC434EF, 0xB24040847AFE90C8, 0x3374C169224FD977, 0x2CD48FF95231CAB8 },
    { 0xD4B834EE12F93FF4, 0x456AFFBA53F1EFE8, 0x39AE8F086601653D, 0x13756B21354AC120 },
    { 0xC47802AEE667048F, 0xD4198D3633F0A239, 0xA2C6FE50CC894B2C, 0x417E866540E281D7 },
    { 0x72043B3ED31F7E42, 0xE51B89D214C06282, 0x660A0CD346E6AC75, 0x41CDD4591DD36184 },
    { 0x92337D789EFA0F79, 0x635CF649B7BDE936, 0x38BF00AE5103C9E7, 0x8A6A0F510C296486 },
    { 0xB6DC0DD16FB29B0B, 0x5B259AAC9BDA9DF3, 0x411B15331D764C69, 0xE9309AA93FFCEA71 },
    { 0x05DAB5DC757F4DBF, 0xC26EDDEC381BF903, 0x047246DE6BAC69D4, 0x91F35C612E41B03D },
    { 0x33B713C7C2E317C3, 0x77CE0D55A1C294DD, 0x1833BA355ECE8B42, 0x1CF3DCE72C8E3F57 },
    { 0x294359DB1F42D635, 0x1931140FB7FEEFA2, 0x040F7CE6649E21AD, 0x030FEB6499DD088A },
    { 0x0D557DFE40832C9C, 0x416BA29EF9D9DCA2, 0x0368503199286D68, 0x1608C03219944AB8 },
    { 0xB577D9320EB50E6D, 0x2152C6614DB6A5D7, 0x179F2002B16A9549, 0x66545BE116D95E8B },
    };
    for (cp_arr_each(i, tm)) {
        cq_udimq_t r = cq_udimw_mul(tm[i].a, tm[i].b);
        assert(cq_udimq_max(r) == tm[i].h);
        assert(cq_udimq_min(r) == tm[i].l);

        cq_udimq_t s = cq_udimw_mul_aux(tm[i].a, tm[i].b);
        assert(cq_udimq_max(s) == tm[i].h);
        assert(cq_udimq_min(s) == tm[i].l);

        assert(cq_udimq_cmp(r, s) == 0);
        assert(cq_udimq_eq (r, s));
    }

    /* fraction tests */
    /* some tests generated with commonlisp

       (defun cmptest()
           (let* (
               (i1 (- (random (expt 2 32)) (expt 2 31)))
               (d1 (1+ (random (1- (expt 2 64)))))
               (n1 (random d1))
               (i2 (- (random (expt 2 32)) (expt 2 31)))
               (d2 (1+ (random (1- (expt 2 64)))))
               (n2 (random d2))
               (f1 (/ n1 d1))
               (f2 (/ n2 d2))
               (x1 (+ i1 f1))
               (x2 (+ i2 f2))
               (cmpf (cond ((< f1 f2) -1) ((> f1 f2) +1) (t 0)))
               (cmpi (cond ((< x1 x2) -1) ((> x1 x2) +1) (t 0))))
               (format t "{{.i=~a,.n=~au,.d=~au},{.i=~a,.n=~au,.d=~au},~a,~a},~&"
                   i1 n1 d1 i2 n2 d2 cmpi cmpf)))

     */
    static struct { cq_dimif_t a,b; int i,f; } tf[] = {
{{.i=-1291654197,.n=7471171128976308762u,.d=9174073484829791258u},{.i=1480181069,.n=7484163858137382507u,.d=8308306126497845884u},-1,-1},
{{.i=1146797945,.n=121133094821748030u,.d=2447399115714574807u},{.i=-1546603912,.n=11487118262313143618u,.d=11543425216520907982u},1,-1},
{{.i=-493871716,.n=8701588488077055277u,.d=18120473326092076456u},{.i=-1105513073,.n=1766429435597501646u,.d=1925751683648206365u},1,-1},
{{.i=20021313,.n=1463387893604211517u,.d=16061694664293824818u},{.i=-413412484,.n=2537004009379351236u,.d=2910916985403286311u},1,-1},
{{.i=1604527519,.n=3114075657695946931u,.d=4920777115920778639u},{.i=21678929,.n=2999842204082131371u,.d=13572010831836483559u},1,1},
{{.i=-572407801,.n=9073213489009505689u,.d=14316207661024630973u},{.i=1513299414,.n=2415096718806445827u,.d=2481953283415370567u},-1,-1},
{{.i=-1479134322,.n=12428778604770495356u,.d=16307307351015589276u},{.i=2118483981,.n=9957034974897428804u,.d=10876046667103106857u},-1,-1},
{{.i=-400494655,.n=600627928742392346u,.d=8916735080939089675u},{.i=1724243993,.n=192415376756321800u,.d=342327456243325896u},-1,-1},
{{.i=-1759212531,.n=5679078035356935700u,.d=6075833854445459635u},{.i=350559601,.n=66394910396101074u,.d=188658402130892228u},-1,1},
{{.i=-1012423662,.n=4042149132076786893u,.d=11121163281383790100u},{.i=-1582853048,.n=13034716161783087530u,.d=18295352072434637407u},1,-1},
{{.i=-1551821207,.n=13372746073306995717u,.d=16412560663540944962u},{.i=-1035462608,.n=3960940883864989365u,.d=4019458488908208529u},-1,-1},
{{.i=885488092,.n=3854639208428849879u,.d=3973320145455069847u},{.i=378694045,.n=185914258211481810u,.d=201720777902160121u},1,1},
{{.i=753445868,.n=9950515757189648103u,.d=12420030016210490594u},{.i=1372847270,.n=3100596992175705344u,.d=5474717024635720727u},-1,1},
{{.i=492989630,.n=1999588533526520472u,.d=3133748711166941979u},{.i=1470077256,.n=3564082075241148719u,.d=8773849312683941787u},-1,1},
{{.i=-2008696510,.n=10573861343288689778u,.d=16841373287337374928u},{.i=1899250025,.n=747309449381736938u,.d=3775038664191710723u},-1,1},
{{.i=-143304461,.n=4754471445943654929u,.d=7186143226056663429u},{.i=-1469328296,.n=816000631905187240u,.d=1573987993301021033u},1,1},
{{.i=1340968091,.n=186617915156400802u,.d=4785892644839208866u},{.i=975234152,.n=9689594827053388081u,.d=14547174690184105094u},1,-1},
{{.i=1508812064,.n=1342132546761157364u,.d=7812927756093082556u},{.i=1798338585,.n=3409584840396398823u,.d=6857924428683228819u},-1,-1},
{{.i=-867726611,.n=352875370932902003u,.d=2905010303061457705u},{.i=1947284252,.n=476659045006155682u,.d=906467143763655571u},-1,-1},
{{.i=-360655033,.n=1224288737684735982u,.d=1853097302314101672u},{.i=-1406066295,.n=6365402268621529758u,.d=7500881683188524925u},1,-1},
{{.i=-173011339,.n=6716032890214786539u,.d=9379941578438885861u},{.i=-766756201,.n=456353962497888860u,.d=882823019191783766u},1,1},
{{.i=-486945550,.n=3845030542468700880u,.d=8303315354613110969u},{.i=1347891862,.n=396856396999660812u,.d=414893296782457382u},-1,-1},
{{.i=-1788151999,.n=3938241191097598021u,.d=17483667023364401401u},{.i=1680698959,.n=9796237783298239639u,.d=15993105814260532321u},-1,-1},
{{.i=1238353316,.n=4899888154218234543u,.d=7498518016911439021u},{.i=-215512835,.n=436898306474704760u,.d=10041949228960361029u},1,1},
{{.i=-650152775,.n=977128874578661523u,.d=13940539358584393337u},{.i=343567687,.n=2320949695526231191u,.d=6213378576725810935u},-1,-1},
{{.i=930483449,.n=120968747040399283u,.d=558739738053460808u},{.i=-528173687,.n=1448010115492486395u,.d=14480188707973816298u},1,1},
{{.i=-2137780237,.n=10205731584347752681u,.d=15562770895043367854u},{.i=412198638,.n=8052463294176556341u,.d=17207289474109423031u},-1,1},
{{.i=432845703,.n=6924909671704550723u,.d=7605849565667803527u},{.i=-921791092,.n=3525629548104056936u,.d=7877398397109616558u},1,1},
{{.i=-1507033449,.n=2044413701076606683u,.d=4432268161321974115u},{.i=-1380453441,.n=4688636970206316877u,.d=13682172998160308659u},-1,1},
{{.i=-1975958042,.n=3273608604522125481u,.d=6415792310786131426u},{.i=-416436110,.n=9898292291240419302u,.d=12025791666174885564u},-1,-1},
    };
    for (cp_arr_each(i, tf)) {
        assert(cq_dimif_cmp     (&tf[i].a, &tf[i].b) == tf[i].i);
        assert(cq_dimif_cmp_frac(&tf[i].a, &tf[i].b) == tf[i].f);
    }

    /* some of the special precision math function */
    assert(cq_dim_mul_div_rnd_nww(100, 1, 5) == 20);
    assert(cq_dim_mul_div_rnd_nww(100, 2, 5) == 40);
    assert(cq_dim_mul_div_rnd_nww(100, 3, 5) == 60);
    assert(cq_dim_mul_div_rnd_nww(100, 4, 5) == 80);
    assert(cq_dim_mul_div_rnd_nww(2, 1, 2) == 1);
    assert(cq_dim_mul_div_rnd_nww(20, 10, 20) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 100, 200) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 1000, 2000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 10000, 20000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 100000, 200000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 1000000, 2000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 10000000, 20000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 100000000, 200000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 1000000000, 2000000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 10000000000, 20000000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 100000000000, 200000000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 1000000000000, 2000000000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 10000000000000, 20000000000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 100000000000000, 200000000000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 1000000000000000, 2000000000000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 10000000000000000, 20000000000000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 100000000000000000, 200000000000000000) == 10);
    assert(cq_dim_mul_div_rnd_nww(20, 1000000000000000000, 2000000000000000000) == 10);

    assert(cq_dim_mul_div_rnd_nww(18, 10, 30) == 6);
    assert(cq_dim_mul_div_rnd_nww(19, 10, 30) == 6);
    assert(cq_dim_mul_div_rnd_nww(20, 10, 30) == 7);
    assert(cq_dim_mul_div_rnd_nww(21, 10, 30) == 7);

    assert(cq_dim_mul_div_rnd_nww(16, 10, 40) == 4);
    assert(cq_dim_mul_div_rnd_nww(17, 10, 40) == 4);
    assert(cq_dim_mul_div_rnd_nww(18, 10, 40) == 5);
    assert(cq_dim_mul_div_rnd_nww(19, 10, 40) == 5);
    assert(cq_dim_mul_div_rnd_nww(20, 10, 40) == 5);
    assert(cq_dim_mul_div_rnd_nww(21, 10, 40) == 5);
    assert(cq_dim_mul_div_rnd_nww(22, 10, 40) == 6);

    cq_vec2_t it;
    assert(cq_vec2_intersect(&it,
        CQ_VEC2(-5, 15), CQ_VEC2(7, 10),
        CQ_VEC2(-5, 19), CQ_VEC2(3, 8)) == 1);
    assert(it.x == -1);
    assert(it.y == 13);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 0,-1), CQ_VEC2( 0,+1),
        CQ_VEC2(-1, 0), CQ_VEC2(+1, 0)) == 1);
    assert(it.x == 0);
    assert(it.y == 0);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 0, 0), CQ_VEC2( 0,+1),
        CQ_VEC2(-1, 0), CQ_VEC2(+1, 0)) == (1|2));
    assert(it.x == 0);
    assert(it.y == 0);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 0,-1), CQ_VEC2( 0, 0),
        CQ_VEC2(-1, 0), CQ_VEC2(+1, 0)) == (1|4));
    assert(it.x == 0);
    assert(it.y == 0);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 0,-1), CQ_VEC2( 0,+1),
        CQ_VEC2( 0, 0), CQ_VEC2(+1, 0)) == (1|8));
    assert(it.x == 0);
    assert(it.y == 0);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 0,-1), CQ_VEC2( 0,+1),
        CQ_VEC2(-1, 0), CQ_VEC2( 0, 0)) == (1|16));
    assert(it.x == 0);
    assert(it.y == 0);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 4, 6), CQ_VEC2( 4,10),
        CQ_VEC2( 4, 8), CQ_VEC2( 0, 7)) == (1|8));
    assert(it.x == 4);
    assert(it.y == 8);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 4,10), CQ_VEC2( 4, 6),
        CQ_VEC2( 4, 8), CQ_VEC2( 0, 7)) == (1|8));
    assert(it.x == 4);
    assert(it.y == 8);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 4,10), CQ_VEC2( 4, 6),
        CQ_VEC2( 0, 7), CQ_VEC2( 4, 8)) == (1|16));
    assert(it.x == 4);
    assert(it.y == 8);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 4, 6), CQ_VEC2( 4,10),
        CQ_VEC2( 0, 7), CQ_VEC2( 4, 8)) == (1|16));
    assert(it.x == 4);
    assert(it.y == 8);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 0, 1), CQ_VEC2( 0, 2),
        CQ_VEC2( 0, 2), CQ_VEC2( 3, 4)) == (1|4|8));

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 2, 0), CQ_VEC2( 2,+1),
        CQ_VEC2(-1, 0), CQ_VEC2(+1, 0)) == 0);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 2,-1), CQ_VEC2( 2, 0),
        CQ_VEC2(-1, 0), CQ_VEC2(+1, 0)) == 0);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 0,-1), CQ_VEC2( 0,+1),
        CQ_VEC2( 0, 2), CQ_VEC2(+1, 2)) == 0);

    assert(cq_vec2_intersect(&it,
        CQ_VEC2( 0,-1), CQ_VEC2( 0,+1),
        CQ_VEC2(-1, 2), CQ_VEC2( 0, 2)) == 0);

    for (int i = 0; i < 100000; i++) {
        cq_dim_t a = (rand() - rand()) & 0x3fffffff;
        cq_dim_t a2 = a * 2;
        cq_dimw_t b = ((cq_dimw_t)rand() << 16) + rand();
        cq_dimw_t b2 = b * 2;
        assert(cq_dim_mul_div_rnd_nww(a2, b, b2) == a);
    }

    for (int i = 0; i < 100000; i++) {
        cq_udim_t  a = (cq_udim_t)(rand() - rand());
        cq_udimw_t b = ((cq_udimw_t)rand() << 32) + (cq_udim_t)rand();
        cq_udimw_t c = (((cq_udimw_t)rand() << 32) + (cq_udim_t)rand()) % b;
        cq_udime_t m = cq_udime_add(cq_udime_mul_wn(b, a), cq_udime_mul_wn(c, 1));
        cq_udim_t  q = cq_dim_div_ew(m, b);
        assert(q == a);
    }
#endif
}
