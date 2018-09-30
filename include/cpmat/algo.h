/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_ALGO_H
#define __CP_ALGO_H

#include <cpmat/mat_gen_tam.h>

/**
 * Rotate around u by an angle given as sin+cos components.
 *
 * This is THE generic rotation matrix generator for this libray which
 * is used to fill in both mat3 and mat4 matrix structures.
 *
 * Asserts that u and sc are both unit or [0,0].
 *
 * Returns a rotation matrix as three row vectors r0,r1,r2.
 */
extern void cp_dim3_rot_unit(
    cp_vec3_t *r0,
    cp_vec3_t *r1,
    cp_vec3_t *r2,
    cp_vec3_t const *u,
    cp_vec2_t const *sc);

/**
 * Rotate the unit vector u into the [0,0,1] axis.
 *
 * The rotation is around the ([0,0,1] x u) axis.
 *
 * Asserts that u is unit or [0,0].
 *
 * Returns the rotation matrix as three row vectors r0,r1,r2.
 */
extern void cp_dim3_rot_unit_into_z(
    cp_vec3_t *r0,
    cp_vec3_t *r1,
    cp_vec3_t *r2,
    cp_vec3_t const *u);

/**
 * Mirror matrix.
 *
 * Asserts that u is unit or [0,0].
 */
extern void cp_dim2_mirror_unit(
    cp_vec2_t *r0,
    cp_vec2_t *r1,
    cp_vec2_t const *u);

/**
 * Mirror matrix.
 *
 * Asserts that u is unit or [0,0].
 */
extern void cp_dim3_mirror_unit(
    cp_vec3_t *r0,
    cp_vec3_t *r1,
    cp_vec3_t *r2,
    cp_vec3_t const *u);

#endif /* __CP_ALGO_H */
