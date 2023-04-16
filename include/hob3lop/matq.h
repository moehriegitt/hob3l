/* -*- Mode: C -*- */

#ifndef HOB3LOP_MAT_H_
#define HOB3LOP_MAT_H_

#include <hob3lop/op-def.h>
#include <hob3lop/matq_tam.h>

static inline bool cq_vec2_eq(cq_vec2_t const *a, cq_vec2_t const *b)
{
    return (a->x == b->x) && (a->y == b->y);
}

/*
 * Required bit size for arithmetics without overflow.
 * This excludes min(int) for any type int.  The largest
 * negative value we allow is -max(int).
 *
 * s{i} is a signed i-bit integer (1 bit sign, i-1 bits value)
 * u{i} is an unsigned i-bit integer
 *
 * add/sub:
 *
 *   s{i} + s{i} => s{i+1}
 *   s{i} - s{i} => s{i+1}
 *   u{i} + u{i} => u{i+1}
 *   u{i} - u{i} => u{i+1}
 *
 * mul:
 *   s{i} * s{j} => s{i+j-1}
 *   u{i} * u{j} => u{i+j}
 *
 * conversion:
 *   abs(s{i})   => u{i-1}
 *   +u{i}       => s{i+1}
 *   -u{i}       => s{i+1}
 */

/**
 * Euclidean variant:
 * for normal width type
 */
extern cq_divmod_t cq_divmod(
    cq_dim_t x,
    cq_dim_t d);

/**
 * Euclidean variant:
 * for wide type
 */
extern cq_divmodw_t cq_divmodw(
    cq_dimw_t x,
    cq_dimw_t d);

/**
 * Euclidean variant: for wide/normal width => normal width
 * This may fail with an overflow.
 */
extern cq_divmod_t cq_divmodx(
    cq_dimw_t x,
    cq_dim_t d);

/**
 * The 'V# vs K--L' decision function that takes into
 * account a 0.5 pixel tolerance square around V (the square is
 * called V#).  This is a modified cross3_z function, returning:
 *
 *    +1  V# is above K--L (V#..L..K runs clockwise)
 *    -1  V# is below K--L (V#..L..K runs counter-clockwise)
 *    0   K--L crosses V#
 *
 * Because this is a generalised cross product, the parameters
 * are in the same order as in cq_vec2_right_cross3_z().
 *
 * Optionally,this may even avoid overflow of the subtractions by
 * computing the subtractions within the larger type.
 * It's OK on x86-64, but on 32-bit CPUs, that'll make this bad,
 * because the multiplications are forced into 64-bit mode.
 * To have the same code on 32 and 64-bit, this option is off
 * by default.
 */
extern int cq_cmp_edge_rnd(
    cq_dim_t vx, cq_dim_t vy,
    cq_dim_t lx, cq_dim_t ly,
    cq_dim_t kx, cq_dim_t ky);

CP_NORETURN
extern void cq_overflow_fail_(
    char const *file,
    int line,
    char const *cond);

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
    cq_vec2_t p4);

/**
 * Compares the fractional parts */
extern int cq_dimif_cmp_frac_aux(cq_dimif_t const *a, cq_dimif_t const *b);

static inline cq_dim_t cq_div(cq_dim_t x, cq_dim_t d)
{
   return cq_divmod(x,d).div;
}

static inline cq_dim_t cq_mod(cq_dim_t x, cq_dim_t d)
{
   return cq_divmod(x,d).mod;
}

static inline cq_dim_t cq_divx(cq_dimw_t x, cq_dim_t d)
{
   return cq_divmodx(x,d).div;
}

static inline cq_dim_t cq_modx(cq_dimw_t x, cq_dim_t d)
{
   return cq_divmodx(x,d).mod;
}

static inline cq_dimw_t cq_divw(cq_dimw_t x, cq_dimw_t d)
{
   return cq_divmodw(x,d).div;
}

static inline cq_dimw_t cq_modw(cq_dimw_t x, cq_dimw_t d)
{
   return cq_divmodw(x,d).mod;
}

static inline cq_dimw_t cq_divw_rnd(cq_dimw_t x, cq_dimw_t d)
{
   cq_divmodw_t r = cq_divmodw(x,d);
   if (r.mod >= ((d / 2) + (d & 1))) {
       r.div++;
   }
   return r.div;
}

/* overflow aware arithmetics */
static inline bool cq_udim_add_ovf(
    cq_udim_t *r,
    cq_udim_t a,
    cq_udim_t b)
{
    return __builtin_add_overflow(a, b, r);
}

static inline bool cq_udim_sub_ovf(
    cq_udim_t *r,
    cq_udim_t a,
    cq_udim_t b)
{
    return __builtin_sub_overflow(a, b, r);
}

#define CQ_MK_OP2OVF(To, Ti, Name, Op) \
    static inline To Name(Ti a, Ti b) \
    { \
        To r; \
        cq_ovf_if(Op(a, b, &r)); \
        return r; \
    }

CQ_MK_OP2OVF(cq_dim_t,   cq_dim_t,   cq_dim_add,   __builtin_add_overflow)
CQ_MK_OP2OVF(cq_udim_t,  cq_udim_t,  cq_udim_add,  __builtin_add_overflow)
CQ_MK_OP2OVF(cq_dimw_t,  cq_dimw_t,  cq_dimw_add,  __builtin_add_overflow)
CQ_MK_OP2OVF(cq_udimw_t, cq_udimw_t, cq_udimw_add, __builtin_add_overflow)

CQ_MK_OP2OVF(cq_dim_t,   cq_dim_t,   cq_dim_sub,   __builtin_sub_overflow)
CQ_MK_OP2OVF(cq_udim_t,  cq_udim_t,  cq_udim_sub,  __builtin_sub_overflow)
CQ_MK_OP2OVF(cq_dimw_t,  cq_dimw_t,  cq_dimw_sub,  __builtin_sub_overflow)
CQ_MK_OP2OVF(cq_udimw_t, cq_udimw_t, cq_udimw_sub, __builtin_sub_overflow)

CQ_MK_OP2OVF(cq_dimw_t,  cq_dim_t,   cq_dim_mul,   __builtin_mul_overflow)
CQ_MK_OP2OVF(cq_udimw_t, cq_udim_t,  cq_udim_mul,  __builtin_mul_overflow)

static inline cq_dim_t cq_dim_neg(cq_dim_t x)
{
    return cq_dim_sub(0,x);
}

static inline cq_dimw_t cq_dimw_neg(cq_dimw_t x)
{
    return cq_dimw_sub(0,x);
}

static inline cq_udim_t cq_udim_abs(cq_dim_t x)
{
    x = (x < 0) ? cq_dim_neg(x) : x;
    return (cq_udim_t)x;
}

static inline cq_udimw_t cq_udimw_abs(cq_dimw_t x)
{
    x = (x < 0) ? cq_dimw_neg(x) : x;
    return (cq_udimw_t)x;
}

static inline cq_udim_t cq_udimw_hi(cq_udimw_t x)
{
    return (cq_udim_t)(x >> CQ_DIM_BITS);
}

static inline cq_udim_t cq_udimw_lo(cq_udimw_t x)
{
    return (cq_udim_t)x;
}

static inline cq_udimw_t cq_udimw(cq_udim_t max, cq_udim_t min)
{
    return ((cq_udimw_t)max << CQ_DIM_BITS) | min;
}

/* quad width arith (just a little bit of it) */

extern cq_udimq_t cq_udimw_mul_aux(cq_udimw_t a, cq_udimw_t b);

static inline cq_udimq_t cq_udimw_mul(cq_udimw_t a, cq_udimw_t b)
{
#if CQ_HAVE_INTQ
    return (cq_udimq_t){ .x = a * (cq_udimq_raw_t)b };
#else
    return cq_udimw_mul_aux(a, b);
#endif
}

static inline int cq_udimq_cmp(cq_udimq_t a, cq_udimq_t b)
{
#if CQ_HAVE_INTQ
    return CP_CMP(a.x, b.x);
#else
    int i = CP_CMP(a.h, b.h);
    if (i != 0) {
        return i;
    }
    return CP_CMP(a.l, b.l);
#endif
}

static inline int cq_udimq_eq(cq_udimq_t a, cq_udimq_t b)
{
#if CQ_HAVE_INTQ
    return a.x == b.x;
#else
    return (a.h == b.h) && (a.l == b.l);
#endif
}

static inline cq_udimw_t cq_udimq_max(cq_udimq_t x)
{
#if CQ_HAVE_INTQ
    return (cq_udimw_t)(x.x >> CQ_DIMW_BITS);
#else
    return x.h;
#endif
}

static inline cq_udimw_t cq_udimq_min(cq_udimq_t x)
{
#if CQ_HAVE_INTQ
    return (cq_udimw_t)x.x;
#else
    return x.l;
#endif
}

static inline cq_udimq_t cq_udimq(cq_udimw_t max, cq_udimw_t min)
{
#if CQ_HAVE_INTQ
    return (cq_udimq_t){ .x = ((cq_udimq_raw_t)max << CQ_DIMW_BITS) | min };
#else
    return (cq_udimq_t){ .h = max, .l = min };
#endif
}


/* geometrix auxiliary functions */

static inline cq_dimw_t cq_cross_z(
    cq_dim_t ax, cq_dim_t ay,
    cq_dim_t bx, cq_dim_t by)
{
    return cq_dimw_sub(cq_dim_mul(ax,by), cq_dim_mul(ay,bx));
}

/**
 * Cross product on coordinates */
static inline cq_dimw_t cq_right_cross3_z(
    cq_dim_t ax, cq_dim_t ay,
    cq_dim_t ox, cq_dim_t oy,
    cq_dim_t bx, cq_dim_t by)
{
    return cq_cross_z(
        cq_dim_sub(ax, ox), cq_dim_sub(ay, oy),
        cq_dim_sub(bx, ox), cq_dim_sub(by, oy));
}

/**
 * = cross_z (a - o, b - o)
 * Note: o is the center vertex of a three point path: a-o-b
 *
 * This gives positive z values when vertices of a convex polygon in
 * the x-y plane are passed clockwise to this functions as a,o,b.
 * E.g. (1,0), (0,0), (0,1) gives +1.  E.g., the cross3
 * product is right-handed.
 *
 * In other word: if o is the origin, then this is positive iff
 * o--b is clockwise of o--a.
 */
static inline cq_dimw_t cq_vec2_right_cross3_z(
    cq_vec2_t const *a,
    cq_vec2_t const *o,
    cq_vec2_t const *b)
{
    return cq_right_cross3_z(a->x, a->y, o->x, o->y, b->x, b->y);
}

static inline int cq_vec2_cmp_edge_rnd(
    cq_vec2_t const *v,
    cq_vec2_t const *l,
    cq_vec2_t const *k)
{
    return cq_cmp_edge_rnd(v->x, v->y, l->x, l->y, k->x, k->y);
}

static inline int cq_vec2_cmp_line2_rnd(
    cq_vec2_t const *v,
    cq_line2_t const *e)
{
    /* kl->a is k and kl->b is l */
    return cq_vec2_cmp_edge_rnd(v, &e->b, &e->a);
}

static inline cq_dimw_t cq_sqr_len(
    cq_dim_t x,
    cq_dim_t y)
{
    return cq_dimw_add(cq_dim_mul(x,x), cq_dim_mul(y,y));
}

static inline cq_dimw_t cq_vec2_sqr_len(
    cq_vec2_t const *a)
{
    return cq_sqr_len(a->x, a->y);
}

static inline cq_dimw_t cq_vec2_sqr_dist(
    cq_vec2_t const *a,
    cq_vec2_t const *b)
{
    return cq_sqr_len(cq_dim_sub(a->x, b->x), cq_dim_sub(a->y, b->y));
}

static inline cq_dimw_t cq_line2_sqr_len(
    cq_line2_t const *l)
{
    return cq_vec2_sqr_dist(&l->a, &l->b);
}

static inline cq_dim_t cq_round(cq_dimif_t const *x)
{
    cq_dim_t sq = x->i;

    /*  d    d2
     *  7 => 4
     *  8 => 4
     *  9 => 5
     * 10 => 5 */
    cq_udimw_t d2 = (x->d / 2) + (x->d & 1);
    if (x->n >= d2) {
        /*    floor    ceil
         * d  n        n
         * 7: 0..3 vs. 4..6
         * 8: 0..3 vs. 4..7 */
        sq = cq_dim_add(sq, 1);
    }
    return sq;
}

static inline int cq_dimif_cmp_frac(cq_dimif_t const *a, cq_dimif_t const *b)
{
#ifdef CQ_HAVE_INTQ
    return cq_udimq_cmp(cq_udimw_mul(a->n, b->d), cq_udimw_mul(a->d, b->n));
#else
    return cq_dimif_cmp_frac_aux(a, b);
#endif
}

static inline int cq_dimif_cmp(cq_dimif_t const *a, cq_dimif_t const *b)
{
    int i = CP_CMP(a->i, b->i);
    if (i != 0) {
        return i;
    }
    return cq_dimif_cmp_frac(a,b);
}

static inline bool cq_dimif_eq(cq_dimif_t const *a, cq_dimif_t const *b)
{
    return cq_dimif_cmp(a,b) == 0;
}

static inline cq_vec2if_t cq_vec2if_from_vec2(
    cq_vec2_t const *v)
{
    return CQ_VEC2IFI(v->x, v->y);
}

static inline bool cq_vec2if_eq(
    cq_vec2if_t const *a,
    cq_vec2if_t const *b)
{
    return cq_dimif_eq(&a->x, &b->x) && cq_dimif_eq(&a->y, &b->y);
}

static inline int cq_vec2if_cmp(
    cq_vec2if_t const *a,
    cq_vec2if_t const *b)
{
    int i = cq_dimif_cmp(&a->x, &b->x);
    if (i != 0) {
        return i;
    }
    return cq_dimif_cmp(&a->y, &b->y);
}

static inline int cq_dim_dimif_cmp(
    cq_dim_t a,
    cq_dimif_t const *b)
{
    if (a < b->i) { return -1; }
    if (a > b->i) { return +1; }
    if (b->n > 0) { return -1; }

    return 0;
}

static inline int cq_vec2_vec2if_cmp(
    cq_vec2_t   const *a,
    cq_vec2if_t const *b)
{
    if (a->x < b->x.i) { return -1; }
    if (a->x > b->x.i) { return +1; }
    if (b->x.n > 0)    { return -1; }

    if (a->y < b->y.i) { return -1; }
    if (a->y > b->y.i) { return +1; }
    if (b->y.n > 0)    { return -1; }

    return 0;
}

static inline int cq_vec2_intersect(
    cq_vec2_t *i,
    cq_vec2_t p1,
    cq_vec2_t p2,
    cq_vec2_t p3,
    cq_vec2_t p4)
{
    cq_vec2if_t j;
    int o = cq_vec2if_intersect(&j, p1, p2, p3, p4);
    i->x = cq_round(&j.x);
    i->y = cq_round(&j.y);
    return o;
}

static inline int cq_line2if_intersect(
    cq_vec2if_t *i,
    cq_line2_t const *l1,
    cq_line2_t const *l2)
{
    return cq_vec2if_intersect(i, l1->a, l1->b, l2->a, l2->b);
}

static inline double cq_f_from_dimif(cq_dimif_t const *x)
{
    return (double)x->i + (((double)x->n) / (double)x->d);
}

/** unit test */
extern void cq_mat_test(void);

#endif /* HOB3LOP_MAT_H_ */
