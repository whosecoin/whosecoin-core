#ifndef BLOCK_H
#define BLOCK_H

#include "tuple.h"
#include "list.h"
#include "transaction.h"

typedef struct block block_t;
typedef struct account account_t;

/**
 * Create a block with the given list of transactions linked to the
 * specified previous previous block. By default, the block will
 * be set with a timestamp of the current system time,
 * a difficulty of zero, and a nonce of zero.
 * 
 * @param prev the previous block in the hashchain
 * @param txns the transactions contained by the current block.
 */
block_t* block_create(block_t *prev, list_t *txns);

/**
 * Create a block from its tuple representation using the given 'find' function
 * to create references between blocks. If the tuple is invalid, return NULL.
 * 
 * @param tuple the tuple
 * @param find the block lookup function
 * @return the new block
 */
block_t* block_create_from_tuple(tuple_t *tuple, block_t* (*find)(buffer_t));

/**
 * Return true if the given ancestor is an ancestor of the given block. 
 * 
 * @param block
 * @param ancestor
 * @return a boolean indicating if block has the specified ancestor.
 */
bool block_has_ancestor(block_t *block, block_t *ancestor);

/**
 * Set the nonce field of the specified block.
 * 
 * @param block the block
 * @param nonce the new nonce
 */
void block_set_nonce(block_t *block, uint32_t nonce);

/**
 * Set the timestamp field of the specified block.
 * 
 * @param block the block
 * @param timestamp the new timestamp
 */
void block_set_timestamp(block_t *block, uint64_t timestamp);

/**
 * Set the difficulty field of the specified block.
 * 
 * @param block the block
 * @param difficulty the new difficulty
 */
void block_set_difficulty(block_t *block, uint32_t difficulty);

/**
 * Return the nonce field of the specified block.
 * 
 * @param block the block
 * @return the nonce
 */
uint32_t block_get_nonce(block_t *block);

/**
 * Return the timestamp field of the specified block.
 * 
 * @param block the block
 * @return the timestamp
 */
uint64_t block_get_timestamp(block_t *block);

/**
 * Return the difficulty field of the specified block.
 * 
 * @param block the block
 * @return the difficulty
 */
uint32_t block_get_difficulty(block_t *block);

/**
 * Return the hash of the block header.
 * 
 * @param block the block
 * @return a buffer containing the hash
 */
buffer_t block_get_hash(block_t *block);

/**
 * Return the previous block.
 *
 * @param block the block
 * @return the previous block
 */
block_t* block_prev(block_t *block);

/**
 * 
 * 
 */
int block_mine(block_t *block, volatile int *stop);

/**
 * Return the merkle root of the block.
 * 
 * @param block the block
 * @return a buffer containing the merkle root
 */
buffer_t block_get_merkle_root(block_t *block);

/**
 * Return the height of the block. Note that this value is one indexed.
 * A block with no previous block has height 1.
 * @param block
 * @return
 */
uint32_t block_get_height(block_t *block);

/**
 * Return the ith transaction in the block
 * @param block the block
 * @param i the transaction index.
 * @return a pointer to the transaction
 */
transaction_t* block_get_transaction(block_t *block, size_t i);

/**
 * Return the number of transactions in a block
 * @param block the block
 * @return the number of transactions
 */
size_t block_get_transaction_count(block_t *block);

/**
 * Destroy the block and free all associated memory.
 * 
 * @param block the block
 */
void block_destroy(block_t *block);

account_t* block_get_account(block_t *block, buffer_t pub_key);

uint64_t account_get_value(account_t *account);

/**
 * Write a tuple representation of the block to a dynamic buffer. 
 *  
 * @param block the block
 * @param buf the buffer to write to
 */
void block_write(block_t *block, dynamic_buffer_t *buf);

/**
 * Write a json representation of the block to a dynamic buffer. 
 *  
 * @param block the block
 * @param buf the buffer to write to
 */
void block_write_json(block_t *block, dynamic_buffer_t *buf);

#endif /* BLOCK_H */