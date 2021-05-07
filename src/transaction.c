#include "transaction.h"
#include <sodium.h>
#include <assert.h>
#include <string.h>
#include "util/json.h"

static const uint8_t NULL_ACCOUNT[crypto_sign_PUBLICKEYBYTES] = {0};

struct transaction {

    uint8_t sender[crypto_sign_PUBLICKEYBYTES]; 
    uint8_t recipient[crypto_sign_PUBLICKEYBYTES];
    uint64_t value; 
    uint32_t nonce;

    uint8_t signature[crypto_sign_BYTES];
    uint8_t hash[crypto_generichash_BYTES];
};

static char* hash_to_hex(const uint8_t *data) {
    char* res = calloc(1, crypto_generichash_BYTES * 2 + 1);
    for (uint8_t i = 0; i < crypto_generichash_BYTES; i++) {
        sprintf(res + 2 * i, "%02x", data[i]);
    }
    return res;
} 

static char* public_key_to_hex(const uint8_t *data) {
    char* res = calloc(1, crypto_sign_PUBLICKEYBYTES * 2 + 1);
    for (uint8_t i = 0; i < crypto_sign_PUBLICKEYBYTES; i++) {
        sprintf(res + 2 * i, "%02x", data[i]);
    }
    return res;
} 

static char* signature_to_hex(const uint8_t *data) {
    char* res = calloc(1, crypto_sign_BYTES * 2 + 1);
    for (uint8_t i = 0; i < crypto_sign_BYTES; i++) {
        sprintf(res + 2 * i, "%02x", data[i]);
    }
    return res;
} 

bool transaction_is_valid(const tuple_t *tuple) {
    assert(tuple != NULL);
    if (tuple_size(tuple) != 2) return false;
    if (tuple_get_type(tuple, 0) != TUPLE_START) return false;
    if (tuple_get_type(tuple, 1) != TUPLE_BINARY) return false;
    if (tuple_get_binary(tuple, 1).length != crypto_sign_BYTES) return false;

    tuple_t *txn = tuple_get_tuple(tuple, 0);
    if (tuple_get_type(txn, 0) != TUPLE_BINARY) return false;
    if (tuple_get_binary(txn, 0).length != crypto_sign_PUBLICKEYBYTES) return false;
    if (tuple_get_type(txn, 1) != TUPLE_BINARY) return false;
    if (tuple_get_binary(txn, 1).length != crypto_sign_PUBLICKEYBYTES) return false;
    if (tuple_get_type(txn, 2) != TUPLE_U64) return false;
    if (tuple_get_type(txn, 3) != TUPLE_U32) return false;

    return true;
}

static void transaction_compute_hash(transaction_t *txn) {
    assert(txn != NULL);
    dynamic_buffer_t buf = dynamic_buffer_create(256);
    tuple_write_start(&buf);
    tuple_write_binary(&buf, crypto_sign_PUBLICKEYBYTES, txn->sender);
    tuple_write_binary(&buf, crypto_sign_PUBLICKEYBYTES, txn->recipient);
    tuple_write_u64(&buf, txn->value);
    tuple_write_u32(&buf, txn->nonce);
    tuple_write_end(&buf);
    crypto_generichash(txn->hash, crypto_generichash_BYTES, buf.data, buf.length, NULL, 0);
    dynamic_buffer_destroy(buf);
}

transaction_t* transaction_create(const uint8_t *public_key, const uint8_t *private_key, const uint8_t *recipient, uint64_t value, uint32_t nonce) {
    assert(public_key != NULL);
    assert(private_key != NULL);
    assert(recipient != NULL);

    transaction_t *result = calloc(1, sizeof(transaction_t));
    assert(result != NULL);
    
    memcpy(result->sender, public_key, crypto_sign_PUBLICKEYBYTES);
    memcpy(result->recipient, recipient, crypto_sign_PUBLICKEYBYTES);
    result->value = value;
    result->nonce = nonce;

    transaction_compute_hash(result);
    crypto_sign_detached(result->signature, NULL, result->hash, crypto_generichash_BYTES, private_key);

    return result;
}

transaction_t* transaction_create_from_tuple(const tuple_t *tuple) {
    assert(tuple != NULL);
    if (!transaction_is_valid(tuple)) return NULL;

    transaction_t *result = calloc(1, sizeof(transaction_t));
    assert(result != NULL);
    
    tuple_t *txn = tuple_get_tuple(tuple, 0);
    buffer_t signature = tuple_get_binary(tuple, 1);

    buffer_t sender = tuple_get_binary(txn, 0);
    buffer_t recipient = tuple_get_binary(txn, 1);
    uint64_t value = tuple_get_u64(txn, 2);
    uint32_t nonce = tuple_get_u32(txn, 3);

    memcpy(result->sender, sender.data, sender.length);
    memcpy(result->recipient, recipient.data, recipient.length);
    result->value = value;
    result->nonce = nonce;
    
    transaction_compute_hash(result);
    memcpy(result->signature, signature.data, signature.length);

    if (crypto_sign_verify_detached(result->signature, result->hash, crypto_sign_PUBLICKEYBYTES, result->sender) != 0) {
        free(result);
        return NULL;
    }

    return result;
}

const uint8_t* transaction_get_sender(const transaction_t *txn) {
    assert(txn != NULL);
    return txn->sender;
}

const uint8_t* transaction_get_recipient(const transaction_t *txn) {
    assert(txn != NULL);
    return txn->recipient;
}

uint64_t transaction_get_value(const transaction_t *txn) {
    assert(txn != NULL);
    return txn->value;
}

uint32_t transaction_get_nonce(const transaction_t *txn) {
    assert(txn != NULL);
    return txn->nonce;
}

const uint8_t* transaction_get_signature(const transaction_t *txn) {
    assert(txn != NULL);
    return txn->signature;
}

const uint8_t* transaction_get_hash(const transaction_t *txn) {
   assert(txn != NULL);
   return txn->hash;
}

void transaction_destroy(transaction_t *txn) {
    free(txn);
}

void transaction_write(const transaction_t *txn, dynamic_buffer_t *buf) {
    tuple_write_start(buf);
    tuple_write_start(buf);
    tuple_write_binary(buf, crypto_sign_PUBLICKEYBYTES, txn->sender);
    tuple_write_binary(buf, crypto_sign_PUBLICKEYBYTES, txn->recipient);
    tuple_write_u64(buf, txn->value);
    tuple_write_u32(buf, txn->nonce);
    tuple_write_end(buf);
    tuple_write_binary(buf, crypto_sign_BYTES, txn->signature);
    tuple_write_end(buf);
}

void transaction_write_json(const transaction_t *txn, dynamic_buffer_t *buf) {
    
    char *sender = public_key_to_hex(txn->sender);
    char *recipient = public_key_to_hex(txn->recipient);
    char *hash = hash_to_hex(txn->hash);
    char *signature = signature_to_hex(txn->signature);

    json_write_object_start(buf);
    json_write_key(buf, "hash");
    json_write_string(buf, hash);
    json_write_key(buf, "sender");
    json_write_string(buf, sender);
    json_write_key(buf, "recipient");
    json_write_string(buf, recipient);
    json_write_key(buf, "value");
    json_write_number(buf, transaction_get_value(txn));
    json_write_key(buf, "nonce");
    json_write_number(buf, transaction_get_nonce(txn));
    json_write_key(buf, "signature");
    json_write_string(buf, signature);
    json_write_object_end(buf);

    free(sender);
    free(recipient);
    free(hash);
    free(signature);
    
}