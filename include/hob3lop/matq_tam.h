/* -*- Mode: C -*- */
/* Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef HOB3LOP_MAT_TAM_H_
#define HOB3LOP_MAT_TAM_H_

#include <hob3lop/op-def.h>
#include <hob3lop/gon_tam.h>

/**
 * Overflow failure */
#define cq_overflow_fail(msg) cq_overflow_fail_(__FILE__, __LINE__, msg)

/**
 * Overflow failure if condition is true */
#define cq_ovf_if(cond) \
    do{ if(cond) { cq_overflow_fail(#cond); } }while(0)

/**
 * Overflow failure unless condition is true */
#define cq_ovf_unless(cond) cq_ovf_if(!(cond))

#endif /* HOB3LOP_MAT_TAM_H_ */
