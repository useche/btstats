#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    int capacity;
    int total;
    size_t element_size;
} vector;

void vector_init(vector *v, size_t element_size);
int vector_total(vector *v);
void vector_add(vector *v, void *item);
void *vector_get(vector *v, int index);
void vector_delete(vector *v, int index);
void vector_delete_fast(vector *v, int index);
void vector_remove_range(vector *v, int index, int count);
void vector_free(vector *v);

#endif
