/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#include <stdio.h>
#include <hob3lbase/panic.h>
#include <hob3lbase/bool-bitmap.h>

static unsigned char spread1[16] = {
    0x00,0x03,0x0c,0x0f,0x30,0x33,0x3c,0x3f,0xc0,0xc3,0xcc,0xcf,0xf0,0xf3,0xfc,0xff
};

static union {
    unsigned char  b[16*2];
    unsigned short w[16];
} spread2 = { .b = {
    0x00,0x00,0x0f,0x00,0xf0,0x00,0xff,0x00,0x00,0x0f,0x0f,0x0f,0xf0,0x0f,0xff,0x0f,
    0x00,0xf0,0x0f,0xf0,0xf0,0xf0,0xff,0xf0,0x00,0xff,0x0f,0xff,0xf0,0xff,0xff,0xff
}};

/**
 * Expand a bitmap bitwise, i.e., duplicate each bit a number of times.
 *
 * The expansion is done in-place.
 */
extern void cp_bool_bitmap_spread(
    cp_bool_bitmap_t *c,
    /** The size of the current bitmap as log2() of the number of bits in use. */
    size_t have,
    /** By what factor to multiply each bit, as log2() of the factor. */
    size_t add)
{
    while (have < 3) {
        if (add == 0) {
            return;
        }
        c->b[0] = spread1[c->b[0] & 0x0f];
        have++;
        add--;
    }

    size_t cnt = (1U << (have - 3));
    void *comb = c->w;
    unsigned char *a = comb;
    a += cnt;
    assert(have < 32);
    assert(add < 32);
    assert((have + add) < 32);
    assert((1U << (have + add)) <= (cp_countof(c->b) * 8));
    switch (add) {
    case 0:
        break;

    case 1: {
        unsigned char *b = comb;
        b += 2*cnt;
        while (cnt-- > 0) {
            unsigned char x = *--a;
            *--b = spread1[x >> 4];
            *--b = spread1[x & 0x0f];
        }
        break;}

    case 2: {
        unsigned short *b = comb;
        b += 2*cnt;
        while (cnt-- > 0) {
            unsigned char x = *--a;
            *--b = spread2.w[x >> 4];
            *--b = spread2.w[x & 0x0f];
        }
        break;}

    case 3: {
        unsigned char *b = comb;
        b += 8*cnt;
        while (cnt-- > 0) {
            unsigned char x = *--a;
            for (unsigned char w = 0x80; w != 0; w >>= 1) {
                *--b = (x & w) ? 0xff : 0;
            }
        }
        break;}

    case 4: {
        unsigned short *b = comb;
        b += 8*cnt;
        while (cnt-- > 0) {
            unsigned char x = *--a;
            for (unsigned char w = 0x80; w != 0; w >>= 1) {
                *--b = (x & w) ? 0xffff : 0;
            }
        }
        break;}

    case 5: {
        unsigned int *b = comb;
        b += 8*cnt;
        while (cnt-- > 0) {
            unsigned char x = *--a;
            for (unsigned char w = 0x80; w != 0; w >>= 1) {
                *--b = (x & w) ? ~0U : 0;
            }
        }
        break;}

    default: {
        size_t kcnt = (1U << (add - 6));
        unsigned long long *b = comb;
        b += 8*cnt*kcnt;
        while (cnt-- > 0) {
            unsigned char x = *--a;
            for (unsigned char w = 0x80; w != 0; w >>= 1) {
                unsigned long long q = (x & w) ? ~0ULL : 0;
                for (size_t k = 0; k < kcnt; k++) {
                    assert(b > c->w);
                    assert(b <= c->w + cp_countof(c->w));
                    *--b = q;
                }
            }
        }
        break;}
    }
}

/**
 * Duplicate r's bitmap so that r->size can be incremented.
 *
 * I.e., the first (1U << size) bits are copied into
 * the second block of that size in the r->comb array.
 *
 * The expansion is done in-place.
 */
extern void cp_bool_bitmap_repeat(
    cp_bool_bitmap_t *r,
    /** The size of the current bitmap as log2() of the number of bits in use. */
    size_t have,
    /** How many times to repeat the bitmap, as log2() of the factor. */
    size_t add)
{
    size_t want = have + add;
    assert(have < 32);
    assert(add < 32);
    assert(want < 32);
    assert((1U << want) <= (cp_countof(r->b) * 8));
    while (have < want) {
        switch (have) {
        case 0:
            r->b[0] &= 1;
            r->b[0] |= (unsigned char)(r->b[0] << 1);
            break;

        case 1:
            r->b[0] &= 3;
            r->b[0] |= (unsigned char)(r->b[0] << 2);
            break;

        case 2:
            r->b[0] &= 15;
            r->b[0] |= (unsigned char)(r->b[0] << 4);
            break;

#if CP_BOOL_BITMAP_MAX_LAZY <= 3
        default:
            CP_DIE();
#else
        case 3:
            r->b[1] = r->b[0];
            break;

        case 4:
            r->s[1] = r->s[0];
            break;

        case 5:
            r->i[1] = r->i[0];
            break;

        case 6:{
            unsigned long long w0 = r->w[0];
            size_t cnt = 1U << (want - 6);
            for (size_t i = 1; i < cnt; i++) {
                r->w[i] = w0;
            }
            return;/*!*/}

        default:{
            assert(have > 6);
            size_t skip = 1U << (have - 6);
            size_t cnt  = 1U << (want - 6);
            for (size_t i = skip; i < cnt; i++) {
                r->w[i] = r->w[i - skip];
            }
            return;/*!*/}
#endif
        }
        have++;
    }
}

/**
 * Combine two bitmaps bitwise according to the given operation.
 *
 * r := r op b
 */
extern void cp_bool_bitmap_combine(
    cp_bool_bitmap_t *r,
    cp_bool_bitmap_t const *b,
    /** number of bits given as log2() of the bitcount */
    size_t size,
    cp_bool_op_t op)
{
    assert(size < 32);
    size_t cnt = ((1U << size) + 63) / 64;
    switch (op) {
    case CP_OP_ADD:
        for (cp_size_each(i,cnt)) {
            r->w[i] |= b->w[i];
        }
        break;

    case CP_OP_CUT:
        for (cp_size_each(i,cnt)) {
            r->w[i] &= b->w[i];
        }
        break;

    case CP_OP_SUB:
        for (cp_size_each(i,cnt)) {
            r->w[i] &= ~b->w[i];
        }
        break;

    case CP_OP_XOR:
        for (cp_size_each(i,cnt)) {
            r->w[i] = r->w[i] ^ b->w[i];
        }
        break;

    default:
        CP_DIE("boolean operation");
    }
}
