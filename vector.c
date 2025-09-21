#include "include/vector.h"
#include <stdio.h>

#define VECTOR_INIT_CAPACITY 4

void vector_init(vector *v, size_t element_size)
{
    v->capacity = VECTOR_INIT_CAPACITY;
    v->total = 0;
    v->element_size = element_size;
    v->data = malloc(v->capacity * v->element_size);
}

int vector_total(vector *v)
{
    return v->total;
}

static void vector_resize(vector *v, int capacity)
{
    char *data = realloc(v->data, capacity * v->element_size);
    if (data) {
        v->data = data;
        v->capacity = capacity;
    }
}

void vector_add(vector *v, void *item)
{
    if (v->capacity == v->total)
        vector_resize(v, v->capacity * 2);
    memcpy(v->data + v->total * v->element_size, item, v->element_size);
    v->total++;
}

void *vector_get(vector *v, int index)
{
    if (index >= 0 && index < v->total)
        return v->data + index * v->element_size;
    return NULL;
}

void vector_delete(vector *v, int index)
{
    if (index < 0 || index >= v->total)
        return;

    if (index < v->total - 1) {
        memmove(v->data + index * v->element_size,
                v->data + (index + 1) * v->element_size,
                (v->total - index - 1) * v->element_size);
    }

    v->total--;

    if (v->total > 0 && v->total == v->capacity / 4)
        vector_resize(v, v->capacity / 2);
}

void vector_delete_fast(vector *v, int index)
{
    if (index < 0 || index >= v->total)
        return;

    if (v->total > 1 && index < v->total - 1) {
        memcpy(v->data + index * v->element_size,
               v->data + (v->total - 1) * v->element_size,
               v->element_size);
    }

    v->total--;

    if (v->total > 0 && v->total == v->capacity / 4)
        vector_resize(v, v->capacity / 2);
}

void vector_free(vector *v)
{
    free(v->data);
}

void vector_remove_range(vector *v, int index, int count)
{
    if (index < 0 || count <= 0 || index + count > v->total)
        return;

    if (index + count < v->total) {
        memmove(v->data + index * v->element_size,
                v->data + (index + count) * v->element_size,
                (v->total - (index + count)) * v->element_size);
    }
    v->total -= count;
}
