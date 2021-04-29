#include "transaction.h"
#include <sodium.h>
#include <assert.h>
#include <string.h>
#include <json.h>

static const uint8_t NULL_ACCOUNT[crypto_sign_PUBLICKEYBYTES] = {0};

struct transaction {
    buffer_t hash;
    buffer_t sender;
    buffer_t recipient;
    uint64_t value;
    uint32_t nonce; 
    buffer_t signature;
};

static buffer_t compute_hash(char *data, size_t length) {
    buffer_t hash = buffer_create(crypto_generichash_BYTES);
    crypto_generichash((uint8_t *) hash.data, hash.length, (uint8_t *) data, length, NULL, 0);
    return hash;
}

bool transaction_is_valid(tuple_t *tuple) {
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


transaction_t* transaction_create(buffer_t private_key, buffer_t public_key, buffer_t recipient, uint64_t v) {
    transaction_t *result = calloc(1, sizeof(transaction_t));
    result->sender = buffer_copy(public_key);
    result->recipient = buffer_copy(recipient);
    result->value = v;
    result->nonce = randombytes_random();
    result->hash = transaction_get_hash(result);
    result->signature = buffer_create(crypto_sign_BYTES);
    crypto_sign_detached((uint8_t *) result->signature.data, NULL, (uint8_t *) result->hash.data, result->hash.length, (uint8_t *) private_key.data);
    return result;
}

transaction_t* transaction_create_coinbase(buffer_t recipient, uint64_t v) {
    transaction_t *result = calloc(1, sizeof(transaction_t));
    result->sender = buffer_create(crypto_sign_PUBLICKEYBYTES);
    result->recipient = buffer_copy(recipient);;
    result->value = v;
    result->nonce = randombytes_random();
    result->hash = transaction_get_hash(result);
    result->signature = buffer_create(crypto_sign_BYTES);
    return result;
}

transaction_t* transaction_create_from_tuple(tuple_t *tuple) {
    assert(tuple != NULL);
    if (!transaction_is_valid(tuple)) return NULL;
    transaction_t *result = malloc(sizeof(transaction_t));
    tuple_t *txn = tuple_get_tuple(tuple, 0);
    buffer_t sender = tuple_get_binary(txn, 0);
    buffer_t recipient = tuple_get_binary(txn, 1);
    buffer_t sig = tuple_get_binary(tuple, 1);
    result->hash = compute_hash(txn->start, txn->length);
    result->sender = buffer_copy(sender);
    result->recipient = buffer_copy(recipient);
    result->value = tuple_get_u64(txn, 2);
    result->nonce = tuple_get_u32(txn, 3);
    result->signature = buffer_copy(sig);
    return result;
}

buffer_t transaction_get_signature(transaction_t *txn) {
    return txn->signature;
}

uint32_t transaction_get_nonce(transaction_t *txn) {
    return txn->nonce;
}
buffer_t transaction_get_sender(transaction_t *txn) {
    return txn->sender;
}

buffer_t transaction_get_recipient(transaction_t *txn) {
    return txn->recipient;
}

uint64_t transaction_get_value(transaction_t *txn) {
    return txn->value;
}

buffer_t transaction_get_hash(transaction_t *txn) {
    if (txn->hash.data != NULL) return txn->hash;
    dynamic_buffer_t buf = dynamic_buffer_create(32);
    tuple_write_start(&buf);
    tuple_write_binary(&buf, txn->sender.length, txn->sender.data);
    tuple_write_binary(&buf, txn->recipient.length, txn->recipient.data);
    tuple_write_u64(&buf, txn->value);
    tuple_write_u32(&buf, txn->nonce);
    tuple_write_end(&buf);

    txn->hash = compute_hash(buf.data, buf.length); 
    dynamic_buffer_destroy(buf);
    return txn->hash;
}

void transaction_destroy(transaction_t *txn) {
    buffer_destroy(txn->sender);
    buffer_destroy(txn->recipient);
    buffer_destroy(txn->hash);
    buffer_destroy(txn->signature);
    free(txn);
}

bool transaction_is_coinbase(transaction_t *txn) {
    return memcmp(txn->sender.data, NULL_ACCOUNT, txn->sender.length) == 0;
}

bool transaction_is_signed(transaction_t *txn) {
    return crypto_sign_verify_detached((uint8_t *) txn->signature.data, (uint8_t *) txn->hash.data, txn->hash.length, (uint8_t *) txn->sender.data) == 0;
}

void transaction_write(transaction_t *txn, dynamic_buffer_t *buf) {
    tuple_write_start(buf);
    tuple_write_start(buf);
    tuple_write_binary(buf, txn->sender.length, txn->sender.data);
    tuple_write_binary(buf, txn->recipient.length, txn->recipient.data);
    tuple_write_u64(buf, txn->value);
    tuple_write_u32(buf, txn->nonce);
    tuple_write_end(buf);
    tuple_write_binary(buf, txn->signature.length, txn->signature.data);
    tuple_write_end(buf);
}

void transaction_write_json(transaction_t *txn, dynamic_buffer_t *buf) {
    
    char *sender = buffer_to_hex(transaction_get_sender(txn));
    char *recipient = buffer_to_hex(transaction_get_recipient(txn));
    char *hash = buffer_to_hex(transaction_get_hash(txn));
    char *signature = buffer_to_hex(transaction_get_signature(txn));

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