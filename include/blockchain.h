#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <block.h>
#include <buffer.h>
#include <transaction.h>

typedef struct blockchain blockchain_t;

blockchain_t *blockchain_create(void (*on_extended)(block_t*, block_t*));
void blockchain_add_block(blockchain_t *bc, block_t *block);
size_t blockchain_height(blockchain_t *bc);
block_t *blockchain_get_block(blockchain_t *bc, buffer_t hash);
transaction_t *blockchain_get_transaction(blockchain_t *bc, buffer_t hash);
block_t *blockchain_get_longest(blockchain_t *bc);
void blockchain_destroy(blockchain_t *bc);

#endif /* BLOCKCHAIN_H */