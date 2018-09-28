/* -*- Mode: C -*- */

#include <cpmat/stream.h>

extern int cp_printf(
    cp_stream_t *s,
    char const *form,
    ...)
{
    va_list va;
    va_start(va, form);
    int i = cp_vprintf(s, form, va);
    va_end(va);
    return i;
}
