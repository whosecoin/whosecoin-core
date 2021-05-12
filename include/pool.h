#ifndef POOL_H
#define POOL_H

#include <transaction.h>

/**
 * The pool structure represents a pool of unconfirmed transactions.
 * When constructing a block, nodes should pull pending transactions from
 * the memory pool.
 */
typedef struct pool pool_t;

/**
 * Construct a new empty transaction pool
 * @return an empty transaction pool.
 */
pool_t* pool_create();

/**
 * Destroy the given memory pool and free all associated memory.
 * @param pool the transaction pool
 */
void pool_destroy(pool_t *pool);

/**
 * Return the number of transactions remaining in the pool
 * @param pool the transaction pool
 * @return the number of transactions in the pool
 */
size_t pool_size(pool_t *pool);

/**
 * Add a transaction to the pool if it does not already exist. 
 * Otherwise, destroy the transaction. Transactions are compared for equality
 * by comparing their hashes.
 * 
 * @param pool the transaction pool.
 * @param txn the transacton.
 */
void pool_add(pool_t *pool, transaction_t *txn);

/**
 * Get the transaction at the specified index.
 * @param pool the transaction pool.
 * @param index the transaction index
 * @return a pointer to the transaction
 */
transaction_t* pool_get(pool_t *pool, size_t index);

/**
 * Remove and return the transaction with the specified index.
 * @param pool the transaction pool.
 * @param index the transaction index.
 * @return a pointer to the transaction.
 */
transaction_t* pool_remove(pool_t *pool, size_t index);

#endif /* POOL_H */