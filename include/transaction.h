#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <util/buffer.h>
#include <tuple.h>

typedef struct transaction transaction_t;

/**
 * Create a signed transaction with the specified public and private keys
 * of the sender, public key of the recipient, and value.
 * 
 * @param private_key the private key of the sender.
 * @param public_key the public key of the sender.
 * @param recipient the public key of the recipient.
 * @param v the value being transferred.
 * @return the transaction.
 */
transaction_t* transaction_create(buffer_t private_key, buffer_t public_key, buffer_t recipient, uint64_t v);

/**
 * Create a transaction from its tuple representation. Return NULL if the
 * transaction is invalid. A tuple can be checked for validity without creating
 * a new tuple by calling the transaction_is_valid method.
 * 
 * @param tuple the tuple representation of the transaction
 * @return the transaction
 */
transaction_t* transaction_create_from_tuple(tuple_t *tuple);

/**
 * Create a coinbase transaction with the specified value. A coinbase
 * transaction is a transaction whose sender is the zero address and signature.
 * 
 * @param recipient the public key of the recipient
 * @param v the value of the transaction
 * @return the transaction.
 */
transaction_t* transaction_create_coinbase(buffer_t recipient, uint64_t v);

/**
 * Return a buffer containing the public key of the transaction sender.
 * 
 * @param txn the transaction
 * @return the public key of the transaction sender.
 */
buffer_t transaction_get_sender(transaction_t *txn);

/**
 * Return a buffer containing the public key of the transaction recipient.
 * 
 * @param txn the transaction
 * @return the public key of the transaction recipient.
 */
buffer_t transaction_get_recipient(transaction_t *txn);

/**
 * Return a buffer containing the transaction signature.
 * 
 * @param txn the transaction
 * @return the transaction signature.
 */
buffer_t transaction_get_signature(transaction_t *txn);

/**
 * Return the value exchanged in the transaction.
 * 
 * @param txn the transaction
 * @return the transaction value.
 */
uint64_t transaction_get_value(transaction_t *txn);

/**
 * Return the nonce of the transaction.
 * 
 * @param txn the transaction
 * @return the transaction nonce.
 */
uint32_t transaction_get_nonce(transaction_t *txn);

/**
 * Return a buffer containing the transaction hash.
 * 
 * @param txn the transaction
 * @return the transaction hash.
 */
buffer_t transaction_get_hash(transaction_t *txn);

/**
 * Return true if the transaction signature is a valid signature of the
 * transaction hash by the sender of the transaction.
 * 
 * @param txn the transaction
 * @return a boolean indicating whether or not txn has a valid signature.
 */
bool transaction_is_signed(transaction_t *txn);

/**
 * Return true of the transaction is a coinbase transaction and false
 * otherwise. A transaction is a coinbase transaction if and only if the
 * sender of the transaction is the zero address.
 *
 * @param txn the transaction
 * @return a boolean indicating whether or not txn is coinbase.
 * 
 */
bool transaction_is_coinbase(transaction_t *txn);

/**
 * Destroy the transaction and free all associated memory.
 * 
 * @param txn the transaction
 */
void transaction_destroy(transaction_t *txn);

/**
 * Validate a tuple representation of a signed transaction. Check that it
 * contains exactly the right number of elements and types.
 * 
 * @param tuple the tuple
 * @return a boolean indicating whether the tuple is valid.
 */
bool transaction_is_valid(tuple_t *tuple);

/**
 * Write a tuple representation of the transaction to a dynamic buffer.
 * 
 * @param txn the transaction
 * @param buf the buf.
 */
void transaction_write(transaction_t *txn, dynamic_buffer_t *buf);

/**
 * Write a json representation of the transaction to a dynamic buffer.
 * 
 * @param txn the transaction
 * @param buf the buf.
 */
void transaction_write_json(transaction_t *txn, dynamic_buffer_t *buf);

#endif /* TRANSACTION_H */