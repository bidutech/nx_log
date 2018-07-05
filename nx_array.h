/*
 * twemproxy - A fast and lightweight proxy for memcached protocol.
 * Copyright (C) 2011 Twitter, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _NX_ARRAY_H_
#define _NX_ARRAY_H_



#include <sys/types.h>


#define nx_free(_p) do {free(_p); (_p) = NULL;} while (0)

#define nx_alloc(_s)  malloc((size_t)(_s))
#define nx_realloc(_p, _s)   realloc(_p,(size_t)(_s))

#define NX_OK        0
#define NX_ERROR    -1
#define NX_EAGAIN   -2
#define NX_ENOMEM   -3

typedef int rstatus_t_; /* return type */

typedef int (*nx_array_compare_t_)(const void *, const void *);
typedef  rstatus_t_ (*nx_array_each_t_)(void *, void *);

struct nx_array {
    u_int32_t nelem;  /* # element */
    void     *elem;  /* element */
    size_t   size;   /* element size */
    u_int32_t nalloc; /* # allocated element */
};

#define nx_null_array { 0, NULL, 0, 0 }

static inline void
nx_array_null(struct nx_array *a)
{
    a->nelem = 0;
    a->elem = NULL;
    a->size = 0;
    a->nalloc = 0;
}

static inline void
nx_array_set(struct nx_array *a, void *elem, size_t size, u_int32_t nalloc)
{
    a->nelem = 0;
    a->elem = elem;
    a->size = size;
    a->nalloc = nalloc;
}

static inline u_int32_t
nx_array_n(const struct nx_array *a)
{
    return a->nelem;
}

struct nx_array *nx_array_create(u_int32_t n, size_t size);
void nx_array_destroy(struct nx_array *a);
rstatus_t_ nx_array_init(struct nx_array *a, u_int32_t n, size_t size);
void nx_array_deinit(struct nx_array *a);

u_int32_t nx_array_idx(struct nx_array *a, void *elem);
void *nx_array_push(struct nx_array *a);
void *nx_array_pop(struct nx_array *a);
void *nx_array_get(struct nx_array *a, u_int32_t idx);
void *nx_array_top(struct nx_array *a);
void nx_array_swap(struct nx_array *a, struct nx_array *b);
void nx_array_sort(struct nx_array *a, nx_array_compare_t_ compare);
rstatus_t_ nx_array_each(struct nx_array *a, nx_array_each_t_ func, void *data);

#endif
