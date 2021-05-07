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
#define COINBASE_TRANSACTION 1

static uint8_t NULL_ACCOUNT[crypto_sign_PUBLICKEYBYTES] = {0};

typedef struct account {
    uint64_t value;         // the value of the account
    struct account *prev;   // a reference to the previous account value
} account_t;

struct block {
    
    /* header */
    uint64_t timestamp;
    block_t *prev_block;
    uint8_t merkle_root[crypto_generichash_BYTES];
    uint8_t public_key[crypto_vrf_PUBLICKEYBYTES];
    uint8_t sortition_proof[crypto_vrf_PROOFBYTES];
    list_t *transactions;

    /* computed meta-data */
    uint8_t hash[crypto_generichash_BYTES];   
    uint8_t sortition_seed[crypto_generichash_BYTES];
    uint8_t sortition_priority[crypto_vrf_OUTPUTBYTES];
    list_t *children;
    uint32_t height;
    map_t *accounts;
};

static char* binary_to_hex(const uint8_t *data, size_t size) {
    char* res = calloc(1, size * 2 + 1);
    for (uint8_t i = 0; i < size; i++) {
        sprintf(res + 2 * i, "%02x", data[i]);
    }
    return res;
} 


/*
 * The hash function for BLAKE2 hashes. Return the least significant bytes in
 * the hash. If we are doing proof of stake, the most significant bytes will
 * all be zero.
 */
static size_t hash(uint8_t *hash) {
    return *((size_t *)(hash + crypto_generichash_BYTES) - 1);
}

/*
 * The comparison function for sorting and equating BLAKE2b hashes. We can just
 * use the the builtin function memcmp for this functionality.
 */
static int compare(void *h1, void *h2) {
    return memcmp(h1, h2, crypto_generichash_BYTES);
}

static int compare_block(void *b1, void *b2) {
    return (uintptr_t) b1 - (uintptr_t)b2;
}

/*
 * Search the branch from leaf to root for accounts matching the given public key.
 * Return it as soon as it is found.
 */
const account_t* block_get_account(const block_t *block, const uint8_t *public_key) {
    while (block != NULL) {
        account_t *account = map_get(block->accounts, public_key);
        if (account != NULL) {
            return account;
        }
        block = block->prev_block;
    }
    return NULL;
}

uint64_t account_get_value(const account_t *account) {
    assert(account != NULL);
    return account->value;
}

/*
 * Compute the block hash and store it in the block. Since blocks are immutable,
 * this is only called during block construction.
 */
static void block_compute_hash(block_t *block) {
    assert(block != NULL);
    dynamic_buffer_t buf = dynamic_buffer_create(256);
    block_write_header(block, &buf);
    crypto_generichash(block->hash, crypto_generichash_BYTES, buf.data, buf.length, NULL, 0);
    dynamic_buffer_destroy(buf);
    return;
}


/*
 * Compute the sortition seed as the hash of the concatenation of the previous
 * blocks sortition seed and the public key of the creator of the previous block.
 */
static void block_compute_seed(block_t *block) {
    assert(block != NULL);
    const size_t N = crypto_generichash_BYTES + crypto_vrf_PUBLICKEYBYTES;
    uint8_t buffer[crypto_generichash_BYTES + crypto_vrf_PUBLICKEYBYTES] = {0};
    
    if (block->prev_block != NULL) {
        memcpy(buffer, block->prev_block->sortition_seed, crypto_generichash_BYTES);
        memcpy(buffer + crypto_generichash_BYTES, block->prev_block->public_key, crypto_vrf_PUBLICKEYBYTES);
    }
    crypto_generichash(block->sortition_seed, crypto_generichash_BYTES, buffer, N, NULL, 0);
    return;
}

uint8_t* block_get_public_key(block_t *block) {
    return block->public_key;
}

/**
 * Iterate through all transactions in the block and verify that they are 
 * all valid. A transaction is valid if and only if it has not been confirmed
 * yet and the he sender has enough value in their account.
 * 
 * This function builds the account metadata, so it should be called exactly
 * once during construction.
 */
static bool are_transactions_valid(block_t *block) {

    /* credit the block creator with the coinbase transaction */
    account_t *prev_creator_account = block_get_account(block->prev_block, block->public_key);
    uint64_t prev_value = prev_creator_account != NULL ? prev_creator_account->value: 0;
    account_t *creator_account = malloc(sizeof(account_t));
    assert(creator_account != NULL);
    creator_account->value = prev_value + COINBASE_TRANSACTION;
    creator_account->prev = prev_creator_account;
    map_set(block->accounts, block->public_key, creator_account);

    for (size_t i = 0; i < list_size(block->transactions); i++) {
        transaction_t *txn = list_get(block->transactions, i);
        const uint8_t *sender = transaction_get_sender(txn);
        const uint8_t *recipient = transaction_get_recipient(txn);
        uint64_t value = transaction_get_value(txn);

        // TODO: check for double spending in current blockchain

        account_t *sender_account = map_get(block->accounts, sender);
        if (sender_account != NULL) {
            sender_account->value -= value;
        } else {
            account_t *prev_sender_account = block_get_account(block->prev_block, sender);
            uint64_t prev_value = prev_sender_account != NULL ? prev_sender_account->value: 0;
            sender_account = malloc(sizeof(account_t));
            assert(sender_account != NULL);
            sender_account->value = prev_value - value;
            sender_account->prev = prev_sender_account;
            map_set(block->accounts, sender, sender_account);
        }

        if (sender_account->value < 0) return false;

        account_t *recipient_account = map_get(block->accounts, recipient);
        if (recipient_account != NULL) {
            recipient_account->value += value;
        } else {
            account_t *prev_recipient_account = block_get_account(block->prev_block, recipient);
            uint64_t prev_value = prev_recipient_account != NULL ? prev_recipient_account->value: 0;
            recipient_account = malloc(sizeof(account_t));
            assert(recipient_account != NULL);
            recipient_account->value = prev_value + value;
            recipient_account->prev = prev_recipient_account;
            map_set(block->accounts, recipient, recipient_account);
        }
    }

    return true;
}

/**
 * Compute the merkle root of an array of hashes in place.
 * 
 * @param hashes an array of hashes.
 * @param n the number of hashes
 */
void compute_merkle_root(uint8_t (*hashes)[crypto_generichash_BYTES], size_t n) {
    if (n <= 1) return;
    size_t i = 0;
    for (size_t j = 0; j < n; j += 2) {
        uint8_t *dst = hashes + i;
        uint8_t *src = hashes + j;
        if (j + 1 == n) {
            memcpy(dst, src, crypto_generichash_BYTES);
        } else {
            crypto_generichash(dst, crypto_generichash_BYTES, src, 2 * crypto_generichash_BYTES, NULL, 0);
        }
        i += 1;
    }
    compute_merkle_root(hashes, i);
}

/**
 * Compute the merkle root of all transactions in a tuple.
 * 
 * @param txns a tuple containing a list of transactions
 */
static void merkle_root_from_tuple(tuple_t *txns, uint8_t *result) {
    const size_t n = tuple_size(txns);
    uint8_t hashes[n][crypto_generichash_BYTES];
    
    // compute hash of all transactions
    for (size_t i = 0; i < n; i++) {
        tuple_t *txn = tuple_get_tuple(txns, i);
        crypto_generichash(hashes[i], crypto_generichash_BYTES, txn->start, txn->length, NULL, 0);
    }

    // recursively hash together adjacent elements
    compute_merkle_root(hashes, n);    
    if (n > 0) memcpy(result, hashes[0], crypto_generichash_BYTES);
    else memset(result, 0, crypto_generichash_BYTES);
}

/**
 * Compute the merkle root of all transactions in a list.
 * 
 * @param txns a tuple containing a list of transactions
 */
void merkle_root_from_list(list_t *txns, uint8_t *result) {
    size_t n = list_size(txns);
    uint8_t hashes[n][crypto_generichash_BYTES];

    // compute hash of all transactions
    for (size_t i = 0; i < n; i++) {
        transaction_t *txn = list_get(txns, i);
        uint8_t *buffer = transaction_get_hash(txn);
        memcpy(hashes[i], buffer, crypto_generichash_BYTES);
    }

    // recursively hash together adjacent elements
    compute_merkle_root(hashes, n);

    if (n > 0) memcpy(result, hashes[0], crypto_generichash_BYTES);
    else memset(result, 0, crypto_generichash_BYTES);
}

uint8_t* block_get_seed(block_t *block) {
    return block->sortition_seed;
}

block_t* block_create(const uint8_t *public_key, const uint8_t *private_key, block_t *prev, list_t *txns) {

    block_t *result = calloc(1, sizeof(block_t));
    assert(result != NULL);
    
    // header
    result->prev_block = prev;
    result->timestamp = time(NULL);
    merkle_root_from_list(txns, result->merkle_root);
    result->transactions = txns;
    memcpy(result->public_key, public_key, crypto_vrf_PUBLICKEYBYTES);
    block_compute_seed(result);
    crypto_vrf_prove(result->sortition_proof, private_key, result->sortition_seed, crypto_generichash_BYTES);
    crypto_vrf_proof_to_hash(result->sortition_priority, result->sortition_proof);

    block_compute_hash(result);

    result->height = 1 + block_get_height(prev);
    result->accounts = map_create(N_ACCOUNT_BUCKETS, hash, NULL, free, compare);
    result->children = list_create(1);

    if (!are_transactions_valid(result)) {
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
    if (tuple_get_binary(tuple, 1).length != crypto_generichash_BYTES) return false;
    if (tuple_get_type(tuple, 2) != TUPLE_BINARY) return false;
    if (tuple_get_binary(tuple, 2).length != crypto_generichash_BYTES) return false;
    if (tuple_get_type(tuple, 3) != TUPLE_BINARY) return false;
    if (tuple_get_binary(tuple, 3).length != crypto_vrf_PUBLICKEYBYTES) return false;
    if (tuple_get_type(tuple, 4) != TUPLE_BINARY) return false;
    if (tuple_get_binary(tuple, 4).length != crypto_vrf_PROOFBYTES) return false;
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
    if (tuple_size(txns) != tuple_get_u32(header, 5)) return false;
    for (size_t i = 0; i < tuple_size(txns); i++) {
        if (tuple_get_type(txns, i) != TUPLE_START) return false;
        tuple_t *txn = tuple_get_tuple(txns, i);
        if (!transaction_is_valid(txn)) {
            return false;
        }
    }

    // check that merkle root matches merkle root from header
    uint8_t merkle_root[crypto_generichash_BYTES] = {0};
    merkle_root_from_tuple(txns, &merkle_root);
    buffer_t merkle_root_header = tuple_get_binary(header, 2);
    return memcmp(merkle_root, merkle_root_header.data, crypto_generichash_BYTES) == 0;
}

block_t* block_create_from_tuple(tuple_t *tuple, block_t* (*find)(buffer_t)) {
    assert(tuple != NULL);
    if (!block_is_valid(tuple)) {
        return NULL;
    }

    block_t *result = calloc(1, sizeof(block_t));
    tuple_t *header = tuple_get_tuple(tuple, 0);
    tuple_t *txns = tuple_get_tuple(tuple, 1);

    uint64_t timestamp = tuple_get_u64(header, 0);
    buffer_t prev_block = tuple_get_binary(header, 1);
    buffer_t merkle_root = tuple_get_binary(header, 2);
    buffer_t public_key = tuple_get_binary(header, 3);
    buffer_t sortition_proof = tuple_get_binary(header, 4);

    result->timestamp = timestamp;
    result->prev_block = find(tuple_get_binary(header, 1));
    memcpy(result->merkle_root, merkle_root.data, crypto_generichash_BYTES);
    memcpy(result->public_key, public_key.data, crypto_vrf_PUBLICKEYBYTES);
    memcpy(result->sortition_proof, sortition_proof.data, crypto_vrf_PROOFBYTES);

    // verify that the sortition priority was generated fairly.
    block_compute_seed(result);
    
    if (crypto_vrf_verify(
        result->sortition_priority,
        result->public_key,
        result->sortition_proof,
        result->sortition_seed,
        crypto_generichash_BYTES
    ) != 0) {
        free(result);
        return NULL;
    }

    result->height = 1 + block_get_height(result->prev_block);
    result->accounts = map_create(N_ACCOUNT_BUCKETS, hash, NULL, free, compare);    
    result->children = list_create(1);
    result->transactions = list_create(tuple_size(txns));
    for (size_t i = 0; i < tuple_size(txns); i++) {
        tuple_t *txn_tuple = tuple_get_tuple(txns, i);
        transaction_t *txn = transaction_create_from_tuple(txn_tuple);
        list_add(result->transactions, txn);
    }
    
    block_compute_hash(result);

    if (are_transactions_valid(result) == false) {
        block_destroy(result);
        printf("invalid transactions\n");
        return NULL;
    }
   
    return result;
}

uint64_t block_get_timestamp(block_t *block) {
    return block->timestamp;  
}

block_t* block_get_prev(block_t *block) {
    return block->prev_block;
}

uint8_t* block_get_priority(block_t *block) {
    return block->sortition_priority;
}

uint32_t block_get_height(block_t *block) {
    if (block == NULL) return 0;
    else return block->height;
}

const uint8_t* block_get_merkle_root(block_t *block) {
    return block->merkle_root;
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

/*
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
*/

uint8_t* block_get_hash(block_t *block) {
    static uint8_t null_hash[crypto_generichash_BYTES] = {0};
    if (block == NULL) {
        return null_hash;
    } else {
        return block->hash;
    }
}

void block_destroy(block_t *block) {
    if (block == NULL) return;
    list_destroy(block->transactions, (void (*)(void *)) transaction_destroy);
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
    uint8_t *prev_hash = block_get_hash(block->prev_block);
    tuple_write_start(buf);
        tuple_write_u64(buf, block->timestamp);
        tuple_write_binary(buf, crypto_generichash_BYTES, prev_hash);
        tuple_write_binary(buf, crypto_generichash_BYTES, block->merkle_root);
        tuple_write_binary(buf, crypto_vrf_PUBLICKEYBYTES, block->public_key);
        tuple_write_binary(buf, crypto_vrf_PROOFBYTES, block->sortition_proof);
        tuple_write_u32(buf, list_size(block->transactions));
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
    char *prev_block = binary_to_hex(block_get_hash(block_get_prev(block)), crypto_generichash_BYTES);
    char *merkle_root = binary_to_hex(block->merkle_root, crypto_generichash_BYTES);
    char *public_key = binary_to_hex(block->public_key, crypto_vrf_PUBLICKEYBYTES);
    char *sortition_proof = binary_to_hex(block->sortition_proof, crypto_vrf_PROOFBYTES);
    char *sortition_priority = binary_to_hex(block->sortition_priority, crypto_vrf_OUTPUTBYTES);
    char *sortition_seed = binary_to_hex(block->sortition_seed, crypto_generichash_BYTES);

    json_write_object_start(buf);
        json_write_key(buf, "timestamp");
        json_write_number(buf, block_get_timestamp(block));
        json_write_key(buf, "prev_block");
        json_write_string(buf, prev_block);
        json_write_key(buf, "merkle_root");
        json_write_string(buf, merkle_root);
        json_write_key(buf, "public_key");
        json_write_string(buf, public_key);
        json_write_key(buf, "sortition_proof");
        json_write_string(buf, sortition_proof);
        json_write_key(buf, "sortition_priority");
        json_write_string(buf, sortition_priority);
        json_write_key(buf, "sortition_seed");
        json_write_string(buf, sortition_seed);
        json_write_key(buf, "n_transactions");
        json_write_number(buf, list_size(block->transactions));
    json_write_object_end(buf);

    free(prev_block);
    free(merkle_root);
    free(public_key);
    free(sortition_proof);
    free(sortition_priority);
    free(sortition_seed);

}

bool block_has_ancestor(block_t *block, block_t *ancestor) {
    if (block == ancestor) return true;
    else if (block == NULL) return false;
    else if (ancestor == NULL) return true;
    else return block_has_ancestor(block_get_prev(block), ancestor);
}

void block_add_child(block_t *block, block_t *child) {
    list_add(block->children, child);
}

void block_write_json(block_t *block, dynamic_buffer_t *buf) {
    
    char *block_hash = binary_to_hex(block_get_hash(block), crypto_generichash_BYTES);

    json_write_object_start(buf);
        json_write_key(buf, "hash");
        json_write_string(buf, block_hash);
        json_write_key(buf, "height");
        json_write_number(buf, block_get_height(block));
        
        json_write_key(buf, "siblings");
        json_write_array_start(buf);
        if (block->prev_block) {
            for (size_t i = 0; i < list_size(block->prev_block->children); i++) {
                block_t *sibling = list_get(block->prev_block->children, i);
                if (sibling != block) {
                    block_write_json_header(sibling, buf);
                }
            }
        }
        json_write_array_end(buf);
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