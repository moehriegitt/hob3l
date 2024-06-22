/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_BOOL_BITMAP_TAM_H_
#define CP_BOOL_BITMAP_TAM_H_

/**
 * Maximum number of polygons to delay.
 */
#define CP_BOOL_BITMAP_MAX_LAZY 10

/**
 * Bitmap to store boolean function
 */
typedef union {
    unsigned char      b[((1U << CP_BOOL_BITMAP_MAX_LAZY) +  7) /  8];
    unsigned short     s[((1U << CP_BOOL_BITMAP_MAX_LAZY) + 15) / 16];
    unsigned int       i[((1U << CP_BOOL_BITMAP_MAX_LAZY) + 31) / 32];
    unsigned long long w[((1U << CP_BOOL_BITMAP_MAX_LAZY) + 63) / 64];
} cp_bool_bitmap_t;

#endif /* CP_BOOL_BITMAP_TAM_H_ */
