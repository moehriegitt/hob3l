/* -*- Mode: C -* */

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
