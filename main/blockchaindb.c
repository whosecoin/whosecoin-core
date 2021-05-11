/*
 * This file provides a simple example of generating a vrf keypair, 
 * creating a blockchain data structure, and adding blocks to the
 * blockchain.
 */

#include "tuple.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sodium.h>
#include <util/map.h>
#include <time.h>
#include <blockchain.h>

#define N 128

/**
 * This callback function is triggered when the leaf node of the principal
 * blockchain is changed.
 * 
 * @param prev the previous leaf node.
 * @param block the new leaf node.
 */
void on_extended(block_t *prev, block_t *block) {
    printf("height: %d\n", block_get_height(block));
}

int main(int argc, char **argv) {
    
    // generate a VRF public and private key
    unsigned char pk[crypto_vrf_PUBLICKEYBYTES];
    unsigned char sk[crypto_vrf_SECRETKEYBYTES];
    crypto_vrf_keypair(pk, sk);

    // create a blockchain that calls on_extended when the longest chain is extended
    blockchain_t *blockchain = blockchain_create(on_extended);

    // create a chain of N blocks
    for (int i = 0; i < N; i++) {
        block_t *prev_block = blockchain_get_principal(blockchain);
        list_t *txns = list_create(1);
        block_t *block = block_create(pk, sk, prev_block, txns);
        assert(block != NULL);
        blockchain_add_block(blockchain, block);
    }

    // destroy the blockchain and free all associated memory
    blockchain_destroy(blockchain);

    return 0;
}