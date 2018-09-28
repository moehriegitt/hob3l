/* -*- Mode: C -*- */

#include <stdio.h>
#include <cpmat/vchar.h>

extern void cp_vchar_fini(
    cp_vchar_t *v)
{
    if (v == NULL) {
        return;
    }

    free(v->data);
    memset(v, 0, sizeof(*v));
}

static void __grow(
    cp_vchar_t *vec,
    size_t new_size)
{
    if (vec->alloc >= new_size)
        return;

    size_t new_alloc = vec->alloc;
    if (new_alloc < 4)
        new_alloc = 4;

    while (new_alloc < new_size) {
        new_alloc *= 2;
        assert(new_alloc > vec->alloc);
    }

    vec->data = realloc(vec->data, sizeof(vec->data[0]) * new_alloc);
    vec->alloc = new_alloc;
}

extern void cp_vchar_clear(
    cp_vchar_t *v)
{
    if (v->size == 0) {
        return;
    }
    v->size = 0;
    v->data[0] = 0;
}

extern void cp_vchar_append_arr(
    cp_vchar_t *v,
    char const *data,
    size_t size)
{
    __grow(v, v->size + size + 1);
    memcpy(v->data + v->size, data, size);
    v->size += size;
    v->data[v->size] = 0;
}

extern void cp_vchar_swap(
    cp_vchar_t *a,
    cp_vchar_t *b)
{
    cp_vchar_t h = *a;
    *a = *b;
    *b = h;
}

extern int cp_vchar_vprintf(
    cp_vchar_t *v,
    char const *format,
    va_list va)
{
    /* Ensure there is any space at all, otherwise we must not invoke
     * vsnprintf */
    __grow(v, 1);
    for(;;) {
        size_t have = v->alloc - v->size;
        int done_i = vsnprintf(v->data + v->size, have, format, va);
        size_t done = (size_t)done_i;
        if (done < have) {
            v->size += done;
            assert(v->size < v->alloc);
            return done_i;
        }

        assert((int)done > 0);
        if ((int)done <= 0) {
            __grow(v, v->alloc * 2);
        }
        else {
            __grow(v, v->size + done + 1);
        }
    }
}

extern int cp_vchar_printf(
    cp_vchar_t *v,
    char const *format,
    ...)
{
    va_list va;
    va_start(va, format);
    int i = cp_vchar_vprintf(v, format, va);
    va_end(va);
    return i;
}
