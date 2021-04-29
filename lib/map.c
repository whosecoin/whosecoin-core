#include "map.h"
#include "list.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct map {
    size_t n_buckets;
    list_t **buckets;
    size_t size;
    destructor_t destroy_key;
    destructor_t destroy_val;
    comparator_t compare_key;
    hash_t hash;
};

typedef struct entry {
    void *key;
    void *val;
    map_t *map;
} entry_t;

static void entry_destroy(entry_t *e) {
    assert(e != NULL);
    assert(e->map != NULL);
    if (e->map->destroy_key) e->map->destroy_key(e->key);
    if (e->map->destroy_val) e->map->destroy_val(e->val);
    free(e);
}

static int entry_compare(entry_t *e1, entry_t *e2) {
    assert(e1 != NULL);
    assert(e2 != NULL);
    assert(e1->map != NULL);
    assert(e1->map->compare_key != NULL);
    return e1->map->compare_key(e1->key, e2->key);
}

static size_t map_get_bucket(map_t *map, void *key) {
    size_t hash = map->hash(key);
    return hash % map->n_buckets;
}

size_t map_size(map_t *map) {
    return map->size;
}

map_t* map_create(size_t n_buckets, hash_t hash, destructor_t destroy_key, destructor_t destroy_val, comparator_t compare) {
    map_t *res = malloc(sizeof(map_t));
    res->buckets = calloc(n_buckets, sizeof(list_t*));
    assert(res->buckets != NULL);
    res->n_buckets = n_buckets;
    res->size = 0;
    res->destroy_key = destroy_key;
    res->destroy_val = destroy_val;
    res->compare_key = compare;
    res->hash = hash;
    return res;
}


static entry_t* map_get_entry(map_t *map, void *key) {
    size_t bi = map_get_bucket(map, key);
    list_t *bucket = map->buckets[bi];
    if (bucket == NULL) return NULL;
    entry_t e = {key, NULL, map};
    size_t ei = list_find(bucket, &e, (comparator_t) entry_compare);
    if (ei == list_size(bucket)) return NULL;
    return list_get(bucket, ei);
}

void* map_get(map_t *map, void *key) {
    entry_t *e = map_get_entry(map, key);
    if (e == NULL) return NULL;
    return e->val;
}

int map_set(map_t *map, void *key, void *val) {
    entry_t *old_entry = map_get_entry(map, key);
    if (old_entry != NULL) {
        if (map->destroy_key) map->destroy_key(key);
        if (map->destroy_val) map->destroy_val(val);
        return 0;
    }

    size_t i = map_get_bucket(map, key);
    list_t *bucket = map->buckets[i];
    entry_t *entry = malloc(sizeof(entry_t));
    assert(entry != NULL);
    entry->key = key;
    entry->val = val;
    entry->map = map;

    if (bucket == NULL) {
        list_t *list = list_create(1);
        list_add(list, entry);
        map->buckets[i] = list;
        map->size += 1;
    } else {
        list_add(bucket, entry);
        map->size += 1;
    }

    return 1;
}

void map_destroy(map_t *map) {
    for (size_t i = 0; i < map->n_buckets; i++) {
        list_t *bucket = map->buckets[i];
        if (bucket == NULL) continue;
        for (size_t j = 0; j < list_size(bucket); j++) {
            entry_t *e = list_get(bucket, j);
            entry_destroy(e);
        }
        list_destroy(bucket, NULL);
    }
    free(map->buckets);
    free(map);
}
