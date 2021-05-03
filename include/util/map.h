#ifndef HASH_MAP_H
#define HASH_MAP_H

#include "buffer.h"

typedef struct map map_t;

typedef void (*destructor_t)(void*);
typedef int (*comparator_t)(void*, void*);
typedef size_t (*hash_t)(void*);

map_t* map_create(size_t n_buckets, hash_t hash, destructor_t destroy_key, destructor_t destroy_val, comparator_t compare);
size_t map_size(map_t *map);
void* map_get(map_t *map, void *key);
int map_set(map_t *map, void *key, void *value);
void map_destroy(map_t *map);

#endif /* HASH_MAP_H */