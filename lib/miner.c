#include <uv.h>
#include "miner.h"

struct miner {
    uv_async_t async;
    uv_mutex_t mutex;
    uv_thread_t thread;
    uv_barrier_t barrier;
    int skip;
    int stop;
    block_t *block;
    block_t *next_block;
    on_block_mined_t cb;
};

struct mining_result_t {
    block_t *block;
    miner_t *miner;
};

void thread_cb(void *arg) {
    miner_t *miner = arg;
    while (!miner->stop) {  
        
        uv_mutex_lock(&miner->mutex);
        miner->block = miner->next_block;
        miner->next_block = NULL;
        miner->skip = 0;
        uv_mutex_unlock(&miner->mutex);

        if (miner->block != NULL) {
            block_mine(miner->block, &miner->skip);
            if (!miner->stop) {
                uv_async_send(&miner->async);  
                uv_barrier_wait(&miner->barrier);   
            }
        }

    }
}

static void async_cb(uv_async_t* handle) {
    miner_t *miner = handle->data;
    if (miner->skip) block_destroy(miner->block);
    else miner->cb(miner, miner->block);
    uv_barrier_wait(&miner->barrier);
}

miner_t* miner_create(on_block_mined_t cb) {
    miner_t* miner = malloc(sizeof(miner_t));
    miner->stop = 0;
    miner->skip = 0;
    miner->block = NULL;
    miner->next_block = NULL;
    miner->cb = cb;
    uv_barrier_init(&miner->barrier, 2);
    uv_mutex_init(&miner->mutex);
    uv_async_init(uv_default_loop(), &miner->async, async_cb);
    miner->async.data = miner;
    uv_thread_create(&miner->thread, thread_cb, miner);
    return miner;
}

void miner_mine(miner_t *miner, block_t *block) {
    uv_mutex_lock(&miner->mutex);
    miner->skip = 1;
    if (miner->next_block) block_destroy(miner->next_block);
    miner->next_block = block;
    uv_mutex_unlock(&miner->mutex);
}

void miner_destroy(miner_t *miner) {
    uv_mutex_lock(&miner->mutex);
    miner->stop = 1;
    miner->skip = 1;
    uv_mutex_unlock(&miner->mutex);
    uv_thread_join(&miner->thread);
    block_destroy(miner->block);
    block_destroy(miner->next_block);
    uv_mutex_destroy(&miner->mutex);
    uv_barrier_destroy(&miner->barrier);
    free(miner);
}

void miner_destroy(miner_t *miner);