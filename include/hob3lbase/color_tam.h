/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_COLOR_TAM_H_
#define CP_COLOR_TAM_H_

typedef union {
    unsigned char c[3];
    struct {
        unsigned char r,g,b;
    } rgb;
    struct {
        unsigned char r,g,b;
    };
} cp_color_rgb_t;

typedef union {
    unsigned char c[4];
    struct {
        unsigned char r,g,b,a;
    };
    cp_color_rgb_t rgb;
} cp_color_rgba_t;

#endif /* CP_COLOR_TAM_H_ */
