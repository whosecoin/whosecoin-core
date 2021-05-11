#ifndef HASH_MAP_H
#define HASH_MAP_H

#include "buffer.h"

/**
 * The map_t data structure is a simple generic hash map implementation
 * that allows (key, value) pairs to be stored, retrieved, and removed.
 */
typedef struct map map_t;

typedef void (*destructor_t)(void*);
typedef int (*comparator_t)(void*, void*);
typedef size_t (*hash_t)(void*);

/**
 * Create a generic hash map with the specified number of buckets and 
 * the provided hash function, destructors, and comparators.
 * 
 * @param n_buckets the number of buckets in hash table
 * @param hash the hash function for key type.
 * @param destroy_key the destructor for key type.
 * @param destroy_val the destructor for value type.
 * @param compare the comparator function for key type.
 * @return an empty map.
 */
map_t* map_create(
    size_t n_buckets,
    hash_t hash,
    destructor_t destroy_key,
    destructor_t destroy_val,
    comparator_t compare
);

/**
 * Return the number of entries stored in the map.
 * @param map the map
 * @return the number of entries.
 */
size_t map_size(const map_t *map);

/**
 * Return the value with the given key, or NULL if no such entry exists.
 * The comparator_t function provided in the constructor is used to determine
 * key equality.
 * 
 * @param map the map
 * @param key the key to query.
 * @return the value or NULL.
 */
void* map_get(const map_t *map, const void *key);

/**
 * Create a new entry with the given key and value. If the key already exists
 * in the table, as determined by the comparator_t function, destroy the
 * previous key, return the previous value, and create a new entry with the
 * given key value pair.
 * 
 * @param map the map
 * @param key the key
 * @param value the value
 * @return the old value or NULL. 
 */
void* map_set(map_t *map, void *key, void *value);

/**
 * Remove and return the element with the given key. If no element with the
 * given key exists, return NULL.
 * 
 * @param map the map
 * @param key the key
 */
void* map_remove(map_t *map, const void *key);

/**
 * Destroy the map, and all keys and values contained in the map using the
 * provided key and value destructors.
 * 
 * @param map the map.
 */
void map_destroy(map_t *map);

#endif /* HASH_MAP_H */