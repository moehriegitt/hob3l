/* -*- Mode: C -* */

#ifndef __CP_ARITH_TAM_H
#define __CP_ARITH_TAM_H

#include <cpmat/def.h>

typedef struct {
    double min;
    double step;
    size_t cnt;
} cp_range_t;

#define cp_deg(rad) (((rad) / (cp_f_t)180) * CP_PI)

#endif /* __CP_ARITH_H */
