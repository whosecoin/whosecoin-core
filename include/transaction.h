#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <util/buffer.h>
#include <tuple.h>

/*
 * A transaction represents a transfer of value from one entity to another.
 * A transaction contains the public keys identifying the sender and the
 * recipient of the transaction, the value transferred in the transaction,
 * a signature to verify that the transaction was initiated by the sender,
 * and a nonce value to distinguish otherwise identical transactions.
 * 
 * Two transactions are identical if they have the same hash value. 
 * To prevent double spending, two identical transactions cannot be spent.
 */
typedef struct transaction transaction_t;

/**
 * Create a signed transaction with the specified public and private keys
 * of the sender, public key of the recipient, and value.
 * 
 * @param public_key the public key of the sender.
 * @param private_key the private key of the sender.
 * @param recipient the public key of the recipient.
 * @param value the value being transferred.
 * @param nonce a unique identifing integer.
 * @return the transaction
 */
transaction_t* transaction_create(
    const uint8_t *public_key,
    const uint8_t *private_key,
    const uint8_t *recipient,
    uint64_t value,
    uint32_t nonce
);

/**
 * Create a transaction from its tuple representation. Return NULL if the
 * transaction is invalid. A tuple can be checked for validity without creating
 * a new tuple by calling the transaction_is_valid method.
 * 
 * @param tuple the tuple representation of the transaction
 * @return the transaction
 */
transaction_t* transaction_create_from_tuple(const tuple_t *tuple);

/**
 * Return true if the tuple type matches the transaction type schema. 
 * @param tuple the tuple
 * @return true if the tuple is valid and false otherwise.
 */
bool transaction_is_valid(const tuple_t *tuple);

/**
 * Return a buffer containing the public key of the transaction sender.
 * 
 * @param txn the transaction
 * @return the public key of the transaction sender.
 */
const uint8_t* transaction_get_sender(const transaction_t *txn);

/**
 * Return a buffer containing the public key of the transaction recipient.
 * 
 * @param txn the transaction
 * @return the public key of the transaction recipient.
 */
const uint8_t* transaction_get_recipient(const transaction_t *txn);

/**
 * Return a buffer containing the transaction signature.
 * 
 * @param txn the transaction
 * @return the transaction signature.
 */
const uint8_t* transaction_get_signature(const transaction_t *txn);

/**
 * Return the value exchanged in the transaction.
 * 
 * @param txn the transaction
 * @return the transaction value.
 */
uint64_t transaction_get_value(const transaction_t *txn);

/**
 * Return the nonce of the transaction.
 * 
 * @param txn the transaction
 * @return the transaction nonce.
 */
uint32_t transaction_get_nonce(const transaction_t *txn);

/**
 * Return a buffer containing the transaction hash.
 * 
 * @param txn the transaction
 * @return the transaction hash.
 */
const uint8_t* transaction_get_hash(const transaction_t *txn);

/**
 * Destroy the transaction and free all associated memory.
 * 
 * @param txn the transaction
 */
void transaction_destroy(transaction_t *txn);

/**
 * Write a tuple representation of the transaction to a dynamic buffer.
 * 
 * @param txn the transaction
 * @param buf the buf.
 */
void transaction_write(const transaction_t *txn, dynamic_buffer_t *buf);

/**
 * Write a json representation of the transaction to a dynamic buffer.
 * 
 * @param txn the transaction
 * @param buf the buf.
 */
void transaction_write_json(const transaction_t *txn, dynamic_buffer_t *buf);

#endif /* TRANSACTION_H */