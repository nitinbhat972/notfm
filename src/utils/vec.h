#pragma once
#include <stdlib.h>

#define Vec(T) struct { T *data; size_t len; size_t cap; }

typedef Vec(char*) StrVec;

#define vec_init(v) do { \
    (v)->data = NULL; \
    (v)->len = 0; \
    (v)->cap = 0; \
} while (0)

#define vec_free(v) do { \
    free((v)->data); \
    (v)->data = NULL; \
    (v)->len = 0; \
    (v)->cap = 0; \
} while (0)

#define vec_push(v, val) do { \
    if ((v)->len >= (v)->cap) { \
        size_t new_cap = (v)->cap == 0 ? 8 : (v)->cap * 2; \
        void *new_data = realloc((v)->data, new_cap * sizeof(*(v)->data)); \
        if (new_data) { \
            (v)->data = new_data; \
            (v)->cap = new_cap; \
        } else { \
            break; \
        } \
    } \
    (v)->data[(v)->len++] = (val); \
} while (0)

#define vec_clear(v) do { \
    (v)->len = 0; \
} while (0)

static inline void vec_clear_str(StrVec *v) {
    for (size_t i = 0; i < v->len; i++) free(v->data[i]);
    v->len = 0;
}

#define vec_foreach(v, it) \
    for (size_t _i = 0, _keep = 1; _keep && _i < (v)->len; _i++, _keep = !_keep) \
        for (it = (v)->data[_i]; _keep; _keep = !_keep)
