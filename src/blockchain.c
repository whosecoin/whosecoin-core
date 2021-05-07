#include "blockchain.h"
#include "util/map.h"
#include <assert.h>
#include <sodium.h>
#include <string.h>

#define N_BLOCK_BUCKETS (1 << 12)
#define N_TXN_BUCKETS (1 << 12)
#define PROOF_OF_STAKE 1

struct blockchain {
    map_t *blocks;
    map_t *blocks_by_priority;
    map_t *txns;
    block_t *principal;
    void (*on_extended)(block_t*, block_t*);
};

static size_t hash(void *h) {
    return *(size_t*)((char *) h + crypto_generichash_BYTES - sizeof(size_t));
}

static int compare(void *h1, void *h2) {
    return memcmp(h1, h2, crypto_generichash_BYTES);
}

static size_t hash_priority(void *h) {
    return *(size_t*)((char *) h + crypto_vrf_OUTPUTBYTES - sizeof(size_t));
}

static int compare_priority(void *h1, void *h2) {
    return memcmp(h1, h2, crypto_vrf_OUTPUTBYTES);
}

blockchain_t *blockchain_create(void (*on_extended)(block_t*, block_t*)) {
    blockchain_t *bc = malloc(sizeof(blockchain_t));
    assert(bc != NULL);
    bc->blocks = map_create(N_BLOCK_BUCKETS, hash, NULL, (destructor_t) block_destroy, compare);
    bc->blocks_by_priority = map_create(N_BLOCK_BUCKETS, hash_priority, NULL, NULL, compare_priority);
    bc->txns = map_create(N_TXN_BUCKETS, hash, NULL, NULL, compare);
    bc->principal = NULL;
    bc->on_extended = on_extended;
    return bc;
}

bool blockchain_has_block_with_priority(blockchain_t *bc, uint8_t *priority) {
    return map_get(bc->blocks_by_priority, priority) != NULL;
}

bool blockchain_add_block(blockchain_t *bc, block_t *block) {
    block_t *old = map_get(bc->blocks, block_get_hash(block));
    if (old != NULL) {
        block_destroy(block);
        return false;
    }
    map_set(bc->blocks, block_get_hash(block), block);
    map_set(bc->blocks_by_priority, block_get_priority(block), block);

    block_t *prev = block_get_prev(block);
    if (prev != NULL) block_add_child(prev, block);
    for (size_t i = 0; i < block_get_transaction_count(block); i++) {
        transaction_t *txn = block_get_transaction(block, i);
        map_set(bc->txns, transaction_get_hash(txn), txn);
    }

#ifdef PROOF_OF_STAKE
    size_t principal_height = block_get_height(bc->principal);
    size_t block_height = block_get_height(block);
    if (block_get_prev(block) == bc->principal) {
        block_t *prev = bc->principal;
        bc->principal = block;
        bc->on_extended(prev, bc->principal);
    } else if (block_get_prev(block) == block_get_prev(bc->principal)) {
        if (memcmp(block_get_priority(block), block_get_priority(bc->principal), crypto_vrf_OUTPUTBYTES) < 0) {
            block_t *prev = bc->principal;
            bc->principal = block;
            bc->on_extended(prev, bc->principal);
        }
    } else {
        block_t *iter = bc->principal;
        block_t *prev = NULL;
        while (block_get_prev(block) != iter && iter != NULL) {
            prev = iter;
            iter = block_get_prev(iter);
        }
        if (block_get_prev(block) == iter) {
            if (memcmp(block_get_priority(block), block_get_priority(prev), crypto_vrf_OUTPUTBYTES) < 0) {
                block_t *prev = bc->principal;
                bc->principal = block;
                bc->on_extended(prev, bc->principal);
            }
        }
    }
#elif
    if (bc->principal == NULL || block_get_height(block) > block_get_height(bc->principal)) {
        block_t *prev = bc->principal;
        bc->principal = block;
        bc->on_extended(prev, bc->principal);
    }
#endif
    return true;
}

block_t *blockchain_get_block(blockchain_t *bc, buffer_t hash) {
    return map_get(bc->blocks, hash.data);
}

transaction_t *blockchain_get_transaction(blockchain_t *bc, buffer_t hash) {
    return map_get(bc->txns, hash.data);
}

block_t *blockchain_get_principal(blockchain_t *bc) {
    return bc->principal;
}

void blockchain_destroy(blockchain_t *bc) {
    map_destroy(bc->blocks);
    map_destroy(bc->blocks_by_priority);
    map_destroy(bc->txns);
    free(bc);
}

size_t blockchain_height(blockchain_t *bc) {
    if (bc->principal == NULL) return 0;
    return block_get_height(bc->principal);
}