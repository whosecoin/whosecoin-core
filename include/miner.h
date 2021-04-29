#ifndef MINER_H
#define MINER_H

#include <block.h>

typedef struct miner miner_t;
typedef void (*on_block_mined_t) (miner_t *miner, block_t *block);

miner_t* miner_create(on_block_mined_t cb);
void miner_mine(miner_t *miner, block_t *block);
void miner_destroy(miner_t *miner);

#endif /* MINER_H */