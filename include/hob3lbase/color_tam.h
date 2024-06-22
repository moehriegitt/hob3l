/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

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

#define CP_COLOR_RGBA_GREY(x)  ((cp_color_rgba_t){ .r = x, .g = x, .b = x, .a = 255 })
#define CP_COLOR_RGBA_WHITE    CP_COLOR_RGBA_GREY(255)
#define CP_COLOR_RGBA_BLACK    CP_COLOR_RGBA_GREY(0)

#endif /* CP_COLOR_TAM_H_ */
