#ifndef HASH_H
#define HASH_H

#include "bsd/queue.h"
#include <stdlib.h>

typedef unsigned int (*hash_func)(const void *);
typedef int (*hash_equal_func)(const void *, const void *);
typedef void (*hash_key_destroy_func)(void *);
typedef void (*hash_value_destroy_func)(void *);

struct hash_entry {
    void *key;
    void *value;
    SLIST_ENTRY(hash_entry) entries;
};

SLIST_HEAD(hash_bucket, hash_entry);

typedef struct {
    struct hash_bucket *buckets;
    int size;
    hash_func hash_fn;
    hash_equal_func equal_fn;
    hash_key_destroy_func key_destroy_fn;
    hash_value_destroy_func value_destroy_fn;
} hash_table;

hash_table *hash_table_new(hash_func, hash_equal_func, hash_key_destroy_func, hash_value_destroy_func);
void hash_table_insert(hash_table *, void *, void *);
void *hash_table_lookup(hash_table *, const void *);
void hash_table_foreach(hash_table *, void (*func)(void *, void *, void *), void *);
int hash_table_foreach_remove(hash_table *ht, int (*func)(void *, void *, void *), void *user_data);
void hash_table_destroy(hash_table *);

unsigned int str_hash(const void *str);
int str_equal(const void *a, const void *b);
unsigned int ptr_hash(const void *p);
int ptr_equal(const void *a, const void *b);

#endif
