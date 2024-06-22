/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_ARCH_H_
#define CP_ARCH_H_

#if defined(__WIN64)
#  define CP_LL "I64"
#  define CP_Z  CP_LL
#elif defined(__WIN32)
#  define CP_LL "I64"
#  define CP_Z  ""
#else
#  define CP_LL "ll"
#  define CP_Z  "z"
#  define cp_qsort_r qsort_r
#endif

#endif /* CP_ARCH_H_ */
