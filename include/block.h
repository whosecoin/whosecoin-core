#ifndef BLOCK_H
#define BLOCK_H

#include "tuple.h"
#include "util/list.h"
#include "transaction.h"

/**
 * The block_t struct is a single node in the blockchain tree data structure.
 * Each block references a previous block, which is its parent in the
 * blocktrain tree. Each block is associated with a ledger state: a set of
 * accounts and account balances. The list of transactions contained in a
 * block modifies the state of the previous block by adding or subtracting
 * from the account balances of the parent block. Each block has a creator,
 * whose identify is tied to a public-private keypair.
 */
typedef struct block block_t;

/**
 * The account_t struct represents the current state of an account defined
 * by a public key. Since our blockchain does not support smart contracts,
 * only currency transactions, the account state is simply the number of
 * tokens owned by the public key.
 */
typedef struct account account_t;

/**
 * Create a new block linked to the specified parent block containing the
 * given list of transactions. By default, the block will be set with a
 * timestamp of the current system time. 
 * 
 * @param public_key the public key of the block creator.
 * @param private_key the private key of the block creator.
 * @param prev the previous block in the hashchain.
 * @param txns the transactions contained by the current block.
 */
block_t* block_create(
    const uint8_t *public_key,
    const uint8_t *private_key,
    block_t *prev, 
    list_t *txns
);

/**
 * Create a block from its tuple representation using the given 'find' function
 * to create references between blocks. If the tuple is invalid, return NULL.
 * 
 * @param tuple the tuple representation of the block
 * @param find the block lookup function
 * @return the block
 */
block_t* block_create_from_tuple(tuple_t *tuple, block_t* (*find)(buffer_t));

/**
 * Add a child block to a block's list of children. This allows all forks of
 * the blockchain tree to be traversed from the root node down. Currently,
 * this must be done manually after block construction.
 * 
 * TODO: Connect parent and children during block creation.
 * 
 * @param block the parent node
 * @param child the child node.
 */
void block_add_child(block_t *block, block_t *child);

/**
 * Return the value used to seed the vrf functions of all participants.
 * This value is calculated as the BLAKE2 hash of the concatenation of the seed
 * of the previous block and the public key of the block creator of the
 * previous block.
 * 
 * @param block the block
 * @return a pointer to a 
 */
uint8_t* block_get_seed(block_t *block);

/**
 * Return the public key of the creator of the previous block.
 * 
 * @param block
 * @return the public key.
 */
uint8_t* block_get_public_key(block_t *block);

/**
 * Return true if the given ancestor is an ancestor of the given block. 
 * 
 * @param block
 * @param ancestor
 * @return a boolean indicating if block has the specified ancestor.
 */
bool block_has_ancestor(block_t *block, block_t *ancestor);

/**
 * Return the block that is a direct child of the given block that was created
 * by the given public key or NULL if no such block exists.
 * 
 * @param block the block
 * @param pk the public key to search for
 * @return the child block or null.
 */
block_t* block_get_child_with_public_key(block_t *block, uint8_t *pk);

/**
 * Return the timestamp field of the specified block.
 * 
 * @param block the block
 * @return the timestamp
 */
uint64_t block_get_timestamp(block_t *block);

/**
 * Return the priority of the given block. When two blocks have the same
 * previous blocks, the block with the lowest priority will be chosen as
 * the 'true' block.
 * 
 * @param block the block
 * @return the block priority.
 */
uint8_t* block_get_priority(block_t *block);

/**
 * Return true if the given public key is permitted to stake a new block on
 * top of the given block and false otherwise. All public keys are permitted
 * to create a genesis block, so this function always returns true when block
 * is NULL. Currently, the staking rules mandates that a public key must have
 * been created more than WAITING_PERIOD=16 blocks prior to allow the public
 * key to stake. If the current block height is less than 16, all public keys
 * are allowed to stake.
 * 
 * @param block the block to stake on top of
 * @param public_key the public key of the potential block creator.
 */
bool is_staking_allowed(const block_t *block, const uint8_t *public_key);

/**
 * Return the hash of the block header.
 * 
 * @param block the block
 * @return a buffer containing the hash
 */
uint8_t* block_get_hash(block_t *block);

/**
 * Return the previous block.
 *
 * @param block the block
 * @return the previous block
 */
block_t* block_get_prev(block_t *block);

/**
 * Return the merkle root of the block.
 * 
 * @param block the block
 * @return a buffer containing the merkle root
 */
const uint8_t* block_get_merkle_root(block_t *block);

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

/**
 * Return the account associated with the given block and public key or
 * NULL if no account matching the given public key exists at the given
 * block.
 * 
 * @param block the block
 * @param public_key the public key identifing an account.
 * @return the account
 */
const account_t* block_get_account(const block_t *block, const uint8_t *public_key);

/**
 * Return the value associated with the account.
 * 
 * @param account the account
 * @return the value in the account.
 */
uint64_t account_get_value(const account_t *account);

/**
 * Write a tuple representation of the block to a dynamic buffer. 
 *  
 * @param block the block
 * @param buf the buffer to write to
 */
void block_write(block_t *block, dynamic_buffer_t *buf);

/**
 * Write a tuple representation of the block header to a dynamic buffer.
 * 
 * @param block the block
 * @param buf the buffer
 */
void block_write_header(block_t *block, dynamic_buffer_t *buf);

/**
 * Write a json representation of the block to a dynamic buffer. 
 *  
 * @param block the block
 * @param buf the buffer to write to
 */
void block_write_json(block_t *block, dynamic_buffer_t *buf);

/**
 * Write a json representation of the block header to a dynamic buffer.
 * 
 * @param block the block
 * @param buf the buffer
 */
void block_write_json_header(block_t *block, dynamic_buffer_t *buf);

#endif /* BLOCK_H */