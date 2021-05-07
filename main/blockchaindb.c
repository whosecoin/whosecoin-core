#include "tuple.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sodium.h>
#include <util/map.h>
#include <time.h>
#include <blockchain.h>


void on_extended(block_t *prev, block_t *block) {
    printf("%d\n", block_get_height(block));
}

int main(int argc, char **argv) {

    uint32_t N = 128;
    
    unsigned char pk[crypto_vrf_PUBLICKEYBYTES];
    unsigned char sk[crypto_vrf_SECRETKEYBYTES];
    buffer_t public_key = {crypto_sign_PUBLICKEYBYTES, pk};
    crypto_vrf_keypair(pk, sk);

    // create a blockchain that calls on_extended when the longest chain is extended
    blockchain_t *blockchain = blockchain_create(on_extended);

    // create a chain of N blocks with a single coinbase transaction
    for (int i = 0; i < N; i++) {
        block_t *prev_block = blockchain_get_principal(blockchain);
        list_t *txns = list_create(1);
        block_t *block = block_create(pk, sk, prev_block, txns);
        if (block) blockchain_add_block(blockchain, block);
        int height = block_get_height(block);
        account_t *account = block_get_account(block, pk);
        uint64_t value = account_get_value(account);
        printf("%d: %llu\n", height, value);
    }

    // destroy the blockchain and free all associated memory
    blockchain_destroy(blockchain);

    return 0;
}