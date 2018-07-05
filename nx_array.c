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

#include <stdlib.h>
#include <assert.h>
#include "nx_array.h"

struct nx_array *
nx_array_create(u_int32_t n, size_t size)
{
    struct nx_array *a;

    assert(n != 0 && size != 0);
    a = nx_alloc(sizeof(*a));
    if (a == NULL) {
        return NULL;
    }

    a->elem = nx_alloc(n * size);
    if (a->elem == NULL) {
        nx_free(a);
        return NULL;
    }

    a->nelem = 0;
    a->size = size;
    a->nalloc = n;

    return a;
}

void
nx_array_destroy(struct nx_array *a)
{
    nx_array_deinit(a);
    nx_free(a);
}

rstatus_t_
nx_array_init(struct nx_array *a, u_int32_t n, size_t size)
{
    assert(n != 0 && size != 0);

    a->elem = nx_alloc(n * size);
    if (a->elem == NULL) {
        return NX_ENOMEM;
    }

    a->nelem = 0;
    a->size = size;
    a->nalloc = n;

    return NX_OK;
}

void
nx_array_deinit(struct nx_array *a)
{
    assert(a->nelem == 0);

    if (a->elem != NULL) {
        nx_free(a->elem);
    }
}

u_int32_t
nx_array_idx(struct nx_array *a, void *elem)
{
    u_int8_t *p, *q;
    u_int32_t off, idx;

    assert(elem >= a->elem);

    p = a->elem;
    q = elem;
    off = (u_int32_t)(q - p);

    assert(off % (u_int32_t)a->size == 0);

    idx = off / (u_int32_t)a->size;

    return idx;
}

void *
nx_array_push(struct nx_array *a)
{
    void *elem, *new;
    size_t size;

    if (a->nelem == a->nalloc) {

        /* the array is full; allocate new array */
        size = a->size * a->nalloc;
        new = nx_realloc(a->elem, 2 * size);
        if (new == NULL) {
            return NULL;
        }

        a->elem = new;
        a->nalloc *= 2;
    }

    elem = (u_int8_t *)a->elem + a->size * a->nelem;
    a->nelem++;

    return elem;
}

void *
nx_array_pop(struct nx_array *a)
{
    void *elem;

    assert(a->nelem != 0);

    a->nelem--;
    elem = (u_int8_t *)a->elem + a->size * a->nelem;

    return elem;
}

void *
nx_array_get(struct nx_array *a, u_int32_t idx)
{
    void *elem;

    assert(a->nelem != 0);
    assert(idx < a->nelem);

    elem = (u_int8_t *)a->elem + (a->size * idx);

    return elem;
}

void *
nx_array_top(struct nx_array *a)
{
    assert(a->nelem != 0);

    return nx_array_get(a, a->nelem - 1);
}

void
nx_array_swap(struct nx_array *a, struct nx_array *b)
{
    struct nx_array tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Sort nelem elements of the array in ascending order based on the
 * compare comparator.
 */
void
nx_array_sort(struct nx_array *a, nx_array_compare_t_ compare)
{
    assert(a->nelem != 0);

    qsort(a->elem, a->nelem, a->size, compare);
}

/*
 * Calls the func once for each element in the array as long as func returns
 * success. On failure short-circuits and returns the error status.
 */
rstatus_t_
nx_array_each(struct nx_array *a, nx_array_each_t_ func, void *data)
{
    u_int32_t i, nelem;

    assert(nx_array_n(a) != 0);
    assert(func != NULL);

    for (i = 0, nelem = nx_array_n(a); i < nelem; i++) {
        void *elem = nx_array_get(a, i);
        rstatus_t_ status;

        status = func(elem, data);
        if (status != NX_OK) {
            return status;
        }
    }

    return NX_OK;
}
