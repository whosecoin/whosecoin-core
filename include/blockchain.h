#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <util/buffer.h>

#include <block.h>
#include <transaction.h>

/**
 * The blockchain_t struct is a blockchain tree data structure that owns and 
 * manages all block and transaction data within the system. The blockchain_t
 * struct allows blocks to be added to the blockchain. Using the protocol
 * rules, the blockchain will maintain a 'principal' block chain that should
 * be treated as the 'true' public ledger state. When the leaf node of the
 * principal block chain changes, the blockchain will notify the callback
 * passed in the constructor to indicate that the public ledger state has 
 * changed. 
 */
typedef struct blockchain blockchain_t;

/**
 * Create a new blockchain data structure that will call the specified callback
 * when the ledger state changes.
 * 
 * @param on_extended a callback to be triggered when ledger state changes
 * @return the data structure
 */
blockchain_t *blockchain_create(void (*on_extended)(block_t*, block_t*));

/**
 * Add a block to the blockchain. If the block is built on top of the
 * principal block chain leaf node, then it will become the newest leaf
 * node of the previous block chain and the on_extended callback will be
 * triggered. If the blockchain is a direct fork of the principal block
 * chain (it's previous block is an ancestor of the current principal
 * leaf node) then it will become the newest leaf node of the principal
 * blockchain. This results in a rollback of the ledger state. When this
 * occurs, the on_extended function will be called.
 * 
 * If a block with the same hash is already found in the blockchain, this
 * function will free the block and return false. Otherwise, this function
 * will return true.
 * 
 * @param bc the blockchain data structure
 * @param block the block to add to bc
 * @return false if the block already exist and true otherwise.
 */
bool blockchain_add_block(blockchain_t *bc, block_t *block);

/**
 * Return the height of the leaf node of the principal blockchain.
 * 
 * @param bc the blockchain.
 * @return the height of the leaf node of the principal blockchain.
 */
size_t blockchain_height(blockchain_t *bc);

/**
 * Return a pointer to the block with the given hash.
 * 
 * @param bc the blockchain
 * @param hash the block hash to search for.
 */
block_t *blockchain_get_block(blockchain_t *bc, buffer_t hash);

/**
 * Return a pointer to the transaction with the given hash
 * 
 * @param bc the blockchain
 * @param hash the transaction hash to search for.
 */
transaction_t *blockchain_get_transaction(blockchain_t *bc, buffer_t hash);

/**
 * Return a pointer to the leaf node of the current principal block chain.
 * 
 * @param bc the blockchain.
 */
block_t *blockchain_get_principal(blockchain_t *bc);

/**
 * Destroy the blockchain data structure and free all blocks and transactions
 * stored in the blockchain.
 * 
 * @param bc the blockchain.
 */
void blockchain_destroy(blockchain_t *bc);

#endif /* BLOCKCHAIN_H */