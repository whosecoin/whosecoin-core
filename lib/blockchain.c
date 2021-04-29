#include "blockchain.h"
#include "map.h"
#include <assert.h>
#include <sodium.h>
#include <string.h>

#define N_BLOCK_BUCKETS (1 << 12)
#define N_TXN_BUCKETS (1 << 12)

struct blockchain {
    map_t *blocks;
    map_t *txns;
    block_t *longest;
    void (*on_extended)(block_t*, block_t*);
};

static size_t hash(void *h) {
    return *(size_t*)((char *) h + crypto_generichash_BYTES - sizeof(size_t));
}

static int compare(void *h1, void *h2) {
    return memcmp(h1, h2, crypto_generichash_BYTES);
}

blockchain_t *blockchain_create(void (*on_extended)(block_t*, block_t*)) {
    blockchain_t *bc = malloc(sizeof(blockchain_t));
    assert(bc != NULL);
    bc->blocks = map_create(N_BLOCK_BUCKETS, hash, NULL, (destructor_t) block_destroy, compare);
    bc->txns = map_create(N_TXN_BUCKETS, hash, NULL, NULL, compare);
    bc->longest = NULL;
    bc->on_extended = on_extended;
    return bc;
}

void blockchain_add_block(blockchain_t *bc, block_t *block) {
    int res = map_set(bc->blocks, block_get_hash(block).data, block);
    if (res == 0) return;
    for (size_t i = 0; i < block_get_transaction_count(block); i++) {
        transaction_t *txn = block_get_transaction(block, i);
        map_set(bc->txns, transaction_get_hash(txn).data, txn);
    }

    if (bc->longest == NULL || block_get_height(block) > block_get_height(bc->longest)) {
        block_t *prev = bc->longest;
        bc->longest = block;
        bc->on_extended(prev, bc->longest);
    }
}

block_t *blockchain_get_block(blockchain_t *bc, buffer_t hash) {
    return map_get(bc->blocks, hash.data);
}

transaction_t *blockchain_get_transaction(blockchain_t *bc, buffer_t hash) {
    return map_get(bc->txns, hash.data);
}

block_t *blockchain_get_longest(blockchain_t *bc) {
    return bc->longest;
}

void blockchain_destroy(blockchain_t *bc) {
    map_destroy(bc->blocks);
    map_destroy(bc->txns);
    free(bc);
}

size_t blockchain_height(blockchain_t *bc) {
    if (bc->longest == NULL) return 0;
    return block_get_height(bc->longest);
}