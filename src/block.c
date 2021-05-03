#include "block.h"
#include "transaction.h"

#include "util/map.h"
#include "util/json.h"

#include <sodium.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#define N_ACCOUNT_BUCKETS 16
#define COINBASE_TRANSACTION 64

static uint8_t NULL_ACCOUNT[crypto_sign_PUBLICKEYBYTES] = {0};

typedef struct account {
    uint64_t value;         // the value of the account
    struct account *prev;   // a reference to the previous account value
} account_t;

struct block {
    buffer_t hash;          // the hash of the block
    uint32_t height;        // the height of the block
    uint64_t timestamp;     // the timestamp when the block was created
    block_t *prev_block;    // the previous block
    buffer_t merkle_root;   // the merkle root of the transaction list
    uint32_t difficulty;    // the difficulty of the block
    uint32_t nonce;         // the nonce of the block
    list_t *transactions;   // a list of the blocks transactions
    list_t *next_blocks;    // a list of blocks the build off of block
    map_t *accounts;        // a map from public keys to current account state
    bool header_dirty;      // whether or not the hash must be recalculated
};

static size_t hash(void *h) {
    return *(size_t*)((char *) h + crypto_generichash_BYTES - sizeof(size_t));
}

static int compare(void *h1, void *h2) {
    return memcmp(h1, h2, crypto_generichash_BYTES);
}

static buffer_t compute_hash(uint8_t *data, size_t length) {
    buffer_t hash = buffer_create(crypto_generichash_BYTES);
    crypto_generichash(hash.data, hash.length, data, length, NULL, 0);
    return hash;
}

account_t* block_get_account(block_t *block, buffer_t pub_key) {
    while (block != NULL) {
        account_t *account = map_get(block->accounts, pub_key.data);
        if (account != NULL) {
            return account;
        }
        block = block->prev_block;
    }
    return NULL;
}

uint64_t account_get_value(account_t *account) {
    assert(account != NULL);
    return account->value;
}

static bool are_transaction_valid(block_t *block) {
    for (size_t i = 0; i < list_size(block->transactions); i++) {
        transaction_t *txn = list_get(block->transactions, i);
        uint64_t value = transaction_get_value(txn);
        buffer_t sender = transaction_get_sender(txn);
        buffer_t recipient = transaction_get_recipient(txn);
        bool is_coinbase = transaction_is_coinbase(txn);
    
        // every transaction must either be sent from the null account, or be
        // properly signed by the sender;
        if (!is_coinbase && !transaction_is_signed(txn)) {
            return false;
        }

        account_t *sender_account = map_get(block->accounts, sender.data);
        if (sender_account != NULL) {
            sender_account->value -= value;
        } else {
            account_t *prev_sender_account = block_get_account(block->prev_block, sender);
            uint64_t prev_value = prev_sender_account != NULL ? prev_sender_account->value: 0;
            sender_account = malloc(sizeof(account_t));
            assert(sender_account != NULL);
            sender_account->value = prev_value - value;
            sender_account->prev = prev_sender_account;
            map_set(block->accounts, sender.data, sender_account);
        }

        if (sender_account->value < 0 && !is_coinbase) return false;

        account_t *recipient_account = map_get(block->accounts, recipient.data);
        if (recipient_account != NULL) {
            recipient_account->value += value;
        } else {
            account_t *prev_recipient_account = block_get_account(block->prev_block, recipient);
            uint64_t prev_value = prev_recipient_account != NULL ? prev_recipient_account->value: 0;
            recipient_account = malloc(sizeof(account_t));
            assert(recipient_account != NULL);
            recipient_account->value = prev_value + value;
            recipient_account->prev = prev_recipient_account;
            map_set(block->accounts, recipient.data, recipient_account);
        }
    }

    buffer_t null_public_key = (buffer_t){crypto_vrf_PUBLICKEYBYTES, NULL_ACCOUNT};
    account_t *null_account = map_get(block->accounts, null_public_key.data);
    if (null_account != NULL) {
        if (null_account->prev != NULL) {
            return null_account->prev->value - null_account->value <= COINBASE_TRANSACTION;
        } else {
            return -null_account->value <= COINBASE_TRANSACTION;
        }
    }
    return true;
}

buffer_t compute_merkle_root_helper(buffer_t *hashes_prev, size_t n_hashes_prev) {
    
    // the hash of an empty list is the zero hash.
    if (n_hashes_prev == 0) return buffer_create(crypto_generichash_BYTES);
    if (n_hashes_prev == 1) return buffer_copy(hashes_prev[0]);

    assert(hashes_prev != NULL);
    const int n_hashes = n_hashes_prev % 2 == 0 ? n_hashes_prev / 2: n_hashes_prev / 2 + 1;
    buffer_t hashes[n_hashes];
    
    for (size_t i = 0; i < n_hashes; i++) {
        if (2 * i + 1 >= n_hashes_prev) {
            hashes[i] = buffer_copy(hashes_prev[2 * i]);
        } else {
            hashes[i] = buffer_create(crypto_generichash_BYTES);
            buffer_t hash1 = hashes_prev[2 * i];
            buffer_t hash2 = hashes_prev[2 * i + 1];
            dynamic_buffer_t dynamic_buffer = dynamic_buffer_create(2 * crypto_generichash_BYTES);
            dynamic_buffer_write(hash1.data, hash1.length, &dynamic_buffer);
            dynamic_buffer_write(hash2.data, hash2.length, &dynamic_buffer);
            hashes[i] = compute_hash(dynamic_buffer.data, dynamic_buffer.length);
            dynamic_buffer_destroy(dynamic_buffer);
        }
    }

    buffer_t result = compute_merkle_root_helper(hashes, n_hashes);
    for (size_t i = 0; i < n_hashes; i++) {
        buffer_destroy(hashes[i]);
    }
    return result;
}

buffer_t compute_merkle_root(tuple_t *txns) {
    const size_t n_txns = tuple_size(txns);
    buffer_t hashes[n_txns];
    for (size_t i = 0; i < n_txns; i++) {
        tuple_t *txn = tuple_get_tuple(txns, i);
        hashes[i] = compute_hash(txn->start, txn->length);
    }
    buffer_t merkle_root = compute_merkle_root_helper(hashes, n_txns);
    for (size_t i = 0; i < n_txns; i++) {
        buffer_destroy(hashes[i]);
    }
    return merkle_root;
}



block_t* block_create(block_t *prev, list_t *txns) {
    block_t *result = calloc(1, sizeof(block_t));
    assert(result != NULL);
    result->height = 1 + block_get_height(prev);
    result->timestamp = time(NULL);
    result->prev_block = prev;
    result->difficulty = 0;
    result->nonce = 0;
    result->transactions = txns;
    result->header_dirty = true;
    result->merkle_root = block_get_merkle_root(result);
    result->hash = block_get_hash(result);
    result->next_blocks = list_create(1);
    result->accounts = map_create(N_ACCOUNT_BUCKETS, hash, NULL, free, compare);
    if (prev != NULL) list_add(prev->next_blocks, result);
    
    if (are_transaction_valid(result) == false) {
        block_destroy(result);
        return NULL;
    }
   
    return result;
}

bool is_header_valid(tuple_t *tuple) {
    assert(tuple != NULL);
    if (tuple_size(tuple) != 6) return false;
    if (tuple_get_type(tuple, 0) != TUPLE_U64) return false;
    if (tuple_get_type(tuple, 1) != TUPLE_BINARY) return false;
    if (tuple_get_binary(tuple, 1).length != crypto_sign_PUBLICKEYBYTES) return false;
    if (tuple_get_type(tuple, 2) != TUPLE_BINARY) return false;
    if (tuple_get_binary(tuple, 2).length != crypto_sign_PUBLICKEYBYTES) return false;
    if (tuple_get_type(tuple, 3) != TUPLE_U32) return false;
    if (tuple_get_type(tuple, 4) != TUPLE_U32) return false;
    if (tuple_get_type(tuple, 5) != TUPLE_U32) return false;
    return true;
}

bool block_is_valid(tuple_t *tuple) {
    assert(tuple != NULL);
    if (tuple_size(tuple) != 2) return false;
    if (tuple_get_type(tuple, 0) != TUPLE_START) return false;
    if (tuple_get_type(tuple, 1) != TUPLE_START) return false;
    
    tuple_t *header = tuple_get_tuple(tuple, 0);
    tuple_t *txns = tuple_get_tuple(tuple, 1);

    if (!is_header_valid(header)) return false;
    if (tuple_size(txns) != tuple_get_u32(header, 3)) return false;
    for (size_t i = 0; i < tuple_size(txns); i++) {
        if (tuple_get_type(txns, i) != TUPLE_START) return false;
        tuple_t *txn = tuple_get_tuple(txns, i);
        if (!transaction_is_valid(txn)) {
            printf("invalid\n");
            return false;
        }
    }

    // check that merkle root matches merkle root from header
    buffer_t merkle_root = compute_merkle_root(txns);
    buffer_t merkle_root_header = tuple_get_binary(header, 2);
    int merkle_roots_match = buffer_compare(&merkle_root, &merkle_root_header) == 0;
    buffer_destroy(merkle_root);
    if (!merkle_roots_match) return false;
    
    return true;
}

block_t* block_create_from_tuple(tuple_t *tuple, block_t* (*find)(buffer_t)) {
    assert(tuple != NULL);
    if (!block_is_valid(tuple)) return NULL;
    block_t *result = calloc(1, sizeof(block_t));
    tuple_t *header = tuple_get_tuple(tuple, 0);
    tuple_t *txns = tuple_get_tuple(tuple, 1);

    result->timestamp = tuple_get_u64(header, 0);
    result->prev_block = find(tuple_get_binary(header, 1));
    result->merkle_root = buffer_copy(tuple_get_binary(header, 2));
    result->difficulty = tuple_get_u32(header, 4);
    result->nonce = tuple_get_u32(header, 5);
    result->height = 1 + block_get_height(result->prev_block);
    result->hash = compute_hash(header->start, header->length);
    result->next_blocks = list_create(1);
    result->accounts = map_create(N_ACCOUNT_BUCKETS, hash, NULL, free, compare);
    if (result->prev_block != NULL) list_add(result->prev_block->next_blocks, result);
    result->transactions = list_create(tuple_size(txns));
    for (size_t i = 0; i < tuple_size(txns); i++) {
        tuple_t *txn_tuple = tuple_get_tuple(txns, i);
        transaction_t *txn = transaction_create_from_tuple(txn_tuple);
        list_add(result->transactions, txn);
    }
    
    if (are_transaction_valid(result) == false) {
        block_destroy(result);
        return NULL;
    }
   
    return result;
}


void block_set_nonce(block_t *block, uint32_t nonce) {
    block->nonce = nonce;
    block->header_dirty = true;
}

void block_set_timestamp(block_t *block, uint64_t timestamp) {
    block->timestamp = timestamp;
    block->header_dirty = true;
}

void block_set_difficulty(block_t *block, uint32_t difficulty) {
    block->difficulty = difficulty;  
    block->header_dirty = true; 
}

uint32_t block_get_nonce(block_t *block) {
    return block->nonce;
}

uint64_t block_get_timestamp(block_t *block) {
    return block->timestamp;  
}

uint32_t block_get_difficulty(block_t *block) {
    return block->difficulty;  
}

block_t* block_prev(block_t *block) {
    return block->prev_block;
}

uint32_t block_get_height(block_t *block) {
    if (block == NULL) return 0;
    else return block->height;
}
buffer_t block_get_merkle_root(block_t *block) {
    assert(block != NULL);
    if (block->merkle_root.data != NULL) return block->merkle_root;
    if (list_size(block->transactions) == 0) {
        return buffer_create(crypto_generichash_BYTES);
    } else {
        return compute_merkle_root_helper(list_get(block->transactions, 0), list_size(block->transactions));
    }
}

static int hash_satisfies_difficulty(uint8_t hash[], uint32_t difficulty) {
    if (difficulty > 8 * crypto_generichash_BYTES) return false;
    for (size_t i = 0; i < difficulty / 8; i++) {
        if (hash[i] != 0x00) return false;
    }
    if (difficulty % 8 == 0) return true;
    uint8_t last = hash[difficulty / 8];
    uint8_t mask = ((uint8_t) 0xff) >> ((uint8_t) (difficulty % 8));
    return last < mask;
}

int block_mine(block_t *block, volatile int *stop) {

    int success = 0;

    buffer_t prev_block = block_get_hash(block->prev_block);
    buffer_t merkle_root = block_get_merkle_root(block);

    dynamic_buffer_t buf = dynamic_buffer_create(32);
    tuple_write_start(&buf);
        tuple_write_u64(&buf, block->timestamp);
        tuple_write_binary(&buf, prev_block.length, prev_block.data);
        tuple_write_binary(&buf, merkle_root.length, merkle_root.data);
        tuple_write_u32(&buf, list_size(block->transactions));
        tuple_write_u32(&buf, block->difficulty);
        tuple_write_u32(&buf, block->nonce);
    tuple_write_end(&buf);

    tuple_t *tuple = tuple_parse((buffer_t *) &buf);
    uint8_t hash[crypto_generichash_BYTES] = {0};
    for (uint32_t nonce = 0; nonce < UINT32_MAX; nonce++) {
        tuple_set_u32(tuple, 5, nonce);
        crypto_generichash(hash, crypto_generichash_BYTES, (uint8_t *) tuple->start, tuple->length, NULL, 0);
        if (hash_satisfies_difficulty(hash, block->difficulty)) {
            block_set_nonce(block, nonce);
            success = 1;
            break;
        }
        if (nonce % 100 == 0 && *stop) break;
    }

    tuple_destroy(tuple);
    dynamic_buffer_destroy(buf);
    return success;
}

buffer_t block_get_hash(block_t *block) {
    static uint8_t null_hash[crypto_generichash_BYTES] = {0};
    if (block == NULL) return (buffer_t) {crypto_generichash_BYTES, null_hash};
    if (!block->header_dirty) return block->hash;
    if (block->header_dirty && block->hash.data != NULL) buffer_destroy(block->hash);

    buffer_t prev_block = block_get_hash(block->prev_block);
    buffer_t merkle_root = block_get_merkle_root(block);

    dynamic_buffer_t buf = dynamic_buffer_create(32);
    tuple_write_start(&buf);
        tuple_write_u64(&buf, block->timestamp);
        tuple_write_binary(&buf, prev_block.length, prev_block.data);
        tuple_write_binary(&buf, merkle_root.length, merkle_root.data);
        tuple_write_u32(&buf, list_size(block->transactions));
        tuple_write_u32(&buf, block->difficulty);
        tuple_write_u32(&buf, block->nonce);
    tuple_write_end(&buf);

    block->hash = compute_hash(buf.data, buf.length); 
    block->header_dirty = 0;
    dynamic_buffer_destroy(buf);
    return block->hash;
}

void block_destroy(block_t *block) {
    if (block == NULL) return;
    list_destroy(block->transactions, (void (*)(void *)) transaction_destroy);
    list_destroy(block->next_blocks, NULL);
    buffer_destroy(block->merkle_root);
    buffer_destroy(block->hash);
    map_destroy(block->accounts);
    free(block);
}

transaction_t* block_get_transaction(block_t *block, size_t i) {
    assert(block != NULL);
    return (transaction_t*) list_get(block->transactions, i);
}

size_t block_get_transaction_count(block_t *block) {
    assert(block != NULL);
    return list_size(block->transactions);
}

void block_write_header(block_t *block, dynamic_buffer_t *buf) {
    buffer_t prev_block = block_get_hash(block->prev_block);
    buffer_t merkle_root = block_get_merkle_root(block);
    tuple_write_start(buf);
        tuple_write_u64(buf, block->timestamp);
        tuple_write_binary(buf, prev_block.length, prev_block.data);
        tuple_write_binary(buf, merkle_root.length, merkle_root.data);
        tuple_write_u32(buf, list_size(block->transactions));
        tuple_write_u32(buf, block->difficulty);
        tuple_write_u32(buf, block->nonce);
    tuple_write_end(buf);
}

void block_write(block_t *block, dynamic_buffer_t *buf) {
    tuple_write_start(buf);
    block_write_header(block, buf);
    tuple_write_start(buf);
    for (size_t i = 0; i < list_size(block->transactions); i++) {
        transaction_t *txn = list_get(block->transactions, i);
        transaction_write(txn, buf);
    }
    tuple_write_end(buf);
    tuple_write_end(buf);
}

void block_write_json_header(block_t *block, dynamic_buffer_t *buf) {
    char *prev_hash = buffer_to_hex(block_get_hash(block_prev(block)));
    char *merkle_root = buffer_to_hex(block_get_merkle_root(block));
    json_write_object_start(buf);
        json_write_key(buf, "timestamp");
        json_write_number(buf, block_get_timestamp(block));
        json_write_key(buf, "prev_block");
        json_write_string(buf, prev_hash);
        json_write_key(buf, "merkle_root");
        json_write_string(buf, merkle_root);
        json_write_key(buf, "difficulty");
        json_write_number(buf, block_get_difficulty(block));
        json_write_key(buf, "nonce");
        json_write_number(buf, block_get_nonce(block));
    json_write_object_end(buf);
    free(prev_hash);
    free(merkle_root);
}

bool block_has_ancestor(block_t *block, block_t *ancestor) {
    if (block == ancestor) return true;
    else if (block == NULL) return false;
    else if (ancestor == NULL) return true;
    else return block_has_ancestor(block_prev(block), ancestor);
}

void block_write_json(block_t *block, dynamic_buffer_t *buf) {
    
    char *block_hash = buffer_to_hex(block_get_hash(block));

    json_write_object_start(buf);
        json_write_key(buf, "hash");
        json_write_string(buf, block_hash);
        json_write_key(buf, "height");
        json_write_number(buf, block_get_height(block));
        json_write_key(buf, "header");
        block_write_json_header(block, buf);
    json_write_key(buf, "transactions");
    json_write_array_start(buf);
    for (size_t i = 0; i < block_get_transaction_count(block); i++) {
        transaction_t *txn = block_get_transaction(block, i);
        transaction_write_json(txn, buf);
    }
    json_write_array_end(buf);
    json_write_object_end(buf);

    free(block_hash);
}