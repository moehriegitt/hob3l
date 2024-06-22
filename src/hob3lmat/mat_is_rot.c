/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */
/* Automatically generated by mkrot. */
#include <hob3lmat/mat.h>

/** Whether a matrix is a rectangular rotation */
extern bool cp_mat3_is_rect_rot(
    cp_mat3_t const *m)
{
    /*   ? 0 0   ? 0 0   0 ? 0   0 ? 0   0 0 ?   0 0 ?
     *   0 ? 0   0 0 ?   ? 0 0   0 0 ?   ? 0 0   0 ? 0
     *   0 0 ?   0 ? 0   0 0 ?   ? 0 0   0 ? 0   ? 0 0  */
    if (!cp_eq(m->m[0][0], 0)) {
        /*   O ? 0   O ? 0   O 0 ?   O 0 ?
         *   ? 0 0   0 0 ?   ? 0 0   0 ? 0
         *   0 0 ?   ? 0 0   0 ? 0   ? 0 0  */
        if (!cp_eq(m->m[1][0], 0)) {
            /*   O ? 0   O 0 ?
             *   O 0 ?   O ? 0
             *   ? 0 0   ? 0 0  */
            if (!cp_eq(m->m[0][1], 0)) { return false; }
            if (!cp_eq(m->m[1][2], 0)) { return false; }
            if (!cp_eq(m->m[2][2], 0)) { return false; }
            if (!cp_eq(m->m[1][1], 0)) {
                /*   O X 0
                 *   O O X
                 *   ? 0 O  */
                if (!cp_eq(m->m[2][0], 0)) { return false; }
                return true;
            }
            /*   O O ?
             *   O X O
             *   ? 0 O  */
            if (!cp_eq(m->m[2][1], 0)) { return false; }
            return true;
        }
        /*   O ? 0   O 0 ?
         *   X 0 0   X 0 0
         *   0 0 ?   0 ? 0  */
        if (!cp_eq(m->m[1][1], 0)) { return false; }
        if (!cp_eq(m->m[2][1], 0)) { return false; }
        if (!cp_eq(m->m[0][2], 0)) { return false; }
        if (!cp_eq(m->m[0][1], 0)) {
            /*   O O X
             *   X O 0
             *   0 X 0  */
            if (!cp_eq(m->m[2][2], 0)) { return false; }
            return true;
        }
        /*   O X O
         *   X O 0
         *   0 O ?  */
        if (!cp_eq(m->m[2][0], 0)) { return false; }
        if (!cp_eq(m->m[1][2], 0)) { return false; }
        return true;
    }
    /*   X 0 0   X 0 0
     *   0 ? 0   0 0 ?
     *   0 0 ?   0 ? 0  */
    if (!cp_eq(m->m[1][0], 0)) { return false; }
    if (!cp_eq(m->m[2][0], 0)) { return false; }
    if (!cp_eq(m->m[0][1], 0)) { return false; }
    if (!cp_eq(m->m[0][2], 0)) { return false; }
    if (!cp_eq(m->m[1][1], 0)) {
        /*   X O O
         *   O O ?
         *   O ? 0  */
        if (!cp_eq(m->m[2][2], 0)) { return false; }
        return true;
    }
    /*   X O O
     *   O X 0
     *   O 0 ?  */
    if (!cp_eq(m->m[2][1], 0)) { return false; }
    if (!cp_eq(m->m[1][2], 0)) { return false; }
    return true;
}
