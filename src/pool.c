#include <pool.h>
#include "util/list.h"
#include <transaction.h>
#include <assert.h>
#include <string.h>
#include <sodium.h>

struct pool {
    list_t *list;
};

int transaction_compare(transaction_t *txn1, transaction_t *txn2) {
    const uint8_t *b1 = transaction_get_hash(txn1);
    const uint8_t *b2 = transaction_get_hash(txn2);
    return memcmp(b1, b2, crypto_generichash_BYTES);
}

pool_t* pool_create() {
    pool_t *result = malloc(sizeof(pool_t));
    assert(result != NULL);
    result->list = list_create(64);
    return result;
}

void pool_destroy(pool_t *pool) {
    if (pool == NULL) return;
    list_destroy(pool->list, (void (*)(void *)) transaction_destroy);
    free(pool);
}

size_t pool_size(pool_t *pool) {
    assert(pool != NULL);
    return list_size(pool->list);
}

void pool_add(pool_t *pool, transaction_t *txn) {
    assert(pool != NULL);
    assert(txn != NULL);
    size_t i = list_find(pool->list, txn, (compare_t) transaction_compare);
    if (i != list_size(pool->list)) {
        transaction_destroy(txn);
    } else {
        list_add(pool->list, txn);
    }
}

transaction_t* pool_get(pool_t *pool, size_t index) {
    assert(pool != NULL);
    return list_get(pool->list, index);
}

transaction_t* pool_remove(pool_t *pool, size_t index) {
    return list_remove(pool->list, index);
}
