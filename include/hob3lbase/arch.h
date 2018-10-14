/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef __CP_ARCH_H
#define __CP_ARCH_H

#if defined(__WIN64)
#  define _Pll "I64"
#  define _Pz  _Pll
#elif defined(__WIN32)
#  define _Pll "I64"
#  define _Pz  ""
#else
#  define _Pll "ll"
#  define _Pz  "z"
#  define cp_qsort_r qsort_r
#endif

#endif /* __CP_ARCH_H */
