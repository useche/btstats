#include "include/hash.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define HASH_TABLE_SIZE 1024

unsigned int str_hash(const void *str)
{
    const char *s = str;
    unsigned int hash = 0;
    int c;

    while ((c = *s++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

int str_equal(const void *a, const void *b)
{
    return strcmp(a, b) == 0;
}

unsigned int ptr_hash(const void *p)
{
    // A simple hash for pointers
    return (unsigned int)((uintptr_t)p >> 3);
}

int ptr_equal(const void *a, const void *b)
{
    return a == b;
}

hash_table *hash_table_new(hash_func hash_fn, hash_equal_func equal_fn,
                           hash_key_destroy_func key_destroy_fn,
                           hash_value_destroy_func value_destroy_fn)
{
    hash_table *ht = malloc(sizeof(hash_table));
    ht->buckets = calloc(HASH_TABLE_SIZE, sizeof(struct hash_bucket));
    ht->size = HASH_TABLE_SIZE;
    ht->hash_fn = hash_fn;
    ht->equal_fn = equal_fn;
    ht->key_destroy_fn = key_destroy_fn;
    ht->value_destroy_fn = value_destroy_fn;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        SLIST_INIT(&ht->buckets[i]);
    }
    return ht;
}

void hash_table_insert(hash_table *ht, void *key, void *value)
{
    unsigned int index = ht->hash_fn(key) % ht->size;
    struct hash_entry *entry;

    SLIST_FOREACH(entry, &ht->buckets[index], entries) {
        if (ht->equal_fn(entry->key, key)) {
            if (ht->value_destroy_fn)
                ht->value_destroy_fn(entry->value);
            entry->value = value;
            if (ht->key_destroy_fn)
                ht->key_destroy_fn(key); // key is not stored, so free it
            return;
        }
    }

    entry = malloc(sizeof(struct hash_entry));
    entry->key = key;
    entry->value = value;
    SLIST_INSERT_HEAD(&ht->buckets[index], entry, entries);
}

void *hash_table_lookup(hash_table *ht, const void *key)
{
    unsigned int index = ht->hash_fn(key) % ht->size;
    struct hash_entry *entry;

    SLIST_FOREACH(entry, &ht->buckets[index], entries) {
        if (ht->equal_fn(entry->key, key)) {
            return entry->value;
        }
    }
    return NULL;
}

void hash_table_foreach(hash_table *ht, void (*func)(void *, void *, void *), void *user_data)
{
    for (int i = 0; i < ht->size; i++) {
        struct hash_entry *entry;
        SLIST_FOREACH(entry, &ht->buckets[i], entries) {
            func(entry->key, entry->value, user_data);
        }
    }
}

void hash_table_destroy(hash_table *ht)
{
    for (int i = 0; i < ht->size; i++) {
        struct hash_entry *entry;
        while (!SLIST_EMPTY(&ht->buckets[i])) {
            entry = SLIST_FIRST(&ht->buckets[i]);
            SLIST_REMOVE_HEAD(&ht->buckets[i], entries);
            if (ht->key_destroy_fn)
                ht->key_destroy_fn(entry->key);
            if (ht->value_destroy_fn)
                ht->value_destroy_fn(entry->value);
            free(entry);
        }
    }
    free(ht->buckets);
    free(ht);
}

int hash_table_foreach_remove(hash_table *ht, int (*func)(void *, void *, void *), void *user_data)
{
    int removed_count = 0;
    for (int i = 0; i < ht->size; i++) {
        struct hash_entry *entry, *prev;
        entry = SLIST_FIRST(&ht->buckets[i]);
        prev = NULL;
        while(entry != NULL) {
            struct hash_entry *next = SLIST_NEXT(entry, entries);
            if (func(entry->key, entry->value, user_data)) {
                removed_count++;
                if (prev == NULL) {
                    SLIST_REMOVE_HEAD(&ht->buckets[i], entries);
                } else {
                    SLIST_NEXT(prev, entries) = next;
                }
                if (ht->key_destroy_fn)
                    ht->key_destroy_fn(entry->key);
                if (ht->value_destroy_fn)
                    ht->value_destroy_fn(entry->value);
                free(entry);
            } else {
                prev = entry;
            }
            entry = next;
        }
    }
    return removed_count;
}
