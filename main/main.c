#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>

#include <uv.h>
#include <sodium.h>

#include <guid.h>
#include <message.h>
#include <settings.h>
#include <network.h>
#include <tuple.h>
#include <blockchain.h>
#include <miner.h>
#include <pool.h>
#include <http.h>

#include <rest.h>
#include <json.h>

#define VERSION_STRING "1.0.0-alpha"
#define BLOCK_TIME 5
#define EPOCH_LENGTH 16

blockchain_t *blockchain;
miner_t* miner;
pool_t* pool;
rest_t *rest;

unsigned char pk[crypto_sign_PUBLICKEYBYTES];
unsigned char sk[crypto_sign_SECRETKEYBYTES];

void on_sigint(uv_signal_t *handle, int signum) {
    uv_stop(loop);
    uv_signal_stop(handle);
    printf("\rinfo: shutting down\n");
}

buffer_t get_public_key() {
    return (buffer_t){crypto_sign_PUBLICKEYBYTES, (char *) &pk};
}

buffer_t get_secret_key() {
    return (buffer_t){crypto_sign_SECRETKEYBYTES, (char *) &sk};
}

uint64_t elapsed_time(size_t a, size_t b) {
    if (a == b) return BLOCK_TIME;

    block_t *block_b = blockchain_get_longest(blockchain);
    while (block_get_height(block_b) != b) {
        block_b = block_prev(block_b);
    }

    block_t *block_a = block_b;
    while (block_get_height(block_a) != a) {
        block_a = block_prev(block_a);
    }

    uint64_t time_b = block_get_timestamp(block_b);
    uint64_t time_a = block_get_timestamp(block_a);
    uint64_t elapsed = time_b - time_a;
    return elapsed;
}

uint32_t compute_difficulty() {
    size_t b = blockchain_height(blockchain);
    block_t *block = blockchain_get_longest(blockchain);
    if (b < 2) return 16;
    if ((b + 1) % EPOCH_LENGTH != 0) return block_get_difficulty(block);
    size_t a = b > EPOCH_LENGTH ? b - EPOCH_LENGTH: 1;
    uint64_t elapsed = elapsed_time(a, b);
    if (elapsed > BLOCK_TIME * (b - a)) {
        return block_get_difficulty(block) - 1;
    } else if (elapsed < BLOCK_TIME * (b - a)) {
        return block_get_difficulty(block) + 1;
    } else {
        return block_get_difficulty(block);
    }
}

/**
 * Retrieves a block by hash from the global blockchain object.
 * @param hash the hash of the block
 * @return a pointer to the block
 */
block_t* lookup_block(buffer_t hash) {
    return blockchain_get_block(blockchain, hash);
}

/**
 * Retrieves a transaction by hash from the global blockchain object.
 * @param hash the hash of the block
 * @return a pointer to the transaction
 */
transaction_t* lookup_transaction(buffer_t hash) {
    return blockchain_get_transaction(blockchain, hash);
}

/**
 * Synchronize the blockchain database with the given peer starting from
 * the specified block. This will send a request to download all blocks starting from
 * the specified block.
 * 
 * @param peer the peer to request blocks from
 * @param block the base of the branch to download 
 */
void synchronize_blockchain(peer_t *peer, block_t *block) {
    buffer_t hash = block_get_hash(block);
    dynamic_buffer_t buf = dynamic_buffer_create(64);
    tuple_write_start(&buf);
    tuple_write_binary(&buf, hash.length, hash.data);
    tuple_write_end(&buf);
    network_send(EVENT_BLOCKS_REQUEST, (buffer_t *) &buf, peer);
    dynamic_buffer_destroy(buf);
}

/**
 * Send an peer-list synchronization request to the specified peer.
 * @param peer - the peer to synchronize with
 */
void synchronize_peers(peer_t *peer) {
    dynamic_buffer_t buf = dynamic_buffer_create(32);
    tuple_write_start(&buf);
    tuple_write_end(&buf);
    network_send(EVENT_PEERS_REQUEST, (buffer_t*) &buf, peer);
    dynamic_buffer_destroy(buf);
}

/**
 * Send an memory pool synchronization request to the specified peer.
 * @param peer - the peer to synchronize with
 */
void synchronize_pool(peer_t *peer) {
    dynamic_buffer_t buf = dynamic_buffer_create(32);
    tuple_write_start(&buf);
    tuple_write_end(&buf);
    network_send(EVENT_POOL_REQUEST, (buffer_t*) &buf, peer);
    dynamic_buffer_destroy(buf);
}

// msg: NULL
void on_connect(peer_t *peer, tuple_t *msg) {
    dynamic_buffer_t buf = dynamic_buffer_create(32);
    tuple_write_start(&buf);
    tuple_write_i32(&buf, settings.port);
    tuple_write_string(&buf, VERSION_STRING);
    tuple_write_end(&buf);
    network_send(EVENT_HANDSHAKE, (buffer_t*) &buf, peer);
    dynamic_buffer_destroy(buf);

    synchronize_peers(peer);
    synchronize_blockchain(peer, blockchain_get_longest(blockchain));
}

// msg: (port: i32, version: string)
void on_handshake(peer_t *peer, tuple_t *msg) {

    // If we are already connected to this peer node, we should disconnect
    // from the node immediately without printing out any messages.
    char *addr = peer_get_addr(peer);
    int32_t port = tuple_get_i32(msg, 0); 
    if (network_has_peer(addr, port)) {
        network_disconnect(peer);
        return;
    }

    // If the peer node has a version string that doesn't match ours, we
    // should disconnect, but still print out a connection message.
    char *version = tuple_get_string(msg, 1);
    if (strcmp(VERSION_STRING, version) != 0) {
        network_disconnect(peer);
    }
    
    // Set the port of the peer to the one specified in their handshake.
    // This is the port on which the peer accepts incoming connections.
    peer_set_port(peer, port);
    printf("[+] %s:%d\n", peer_get_addr(peer), peer_get_port(peer));
}

/**
 * When we disconnect from a socket, we print out a message indicating that
 * the node has been disconnected. If the peer has a port number less than
 * or equal to zero, we do nothing since these nodes failed their handshake
 */
void on_disconnect(peer_t *peer, tuple_t *msg) {
    if (peer_get_port(peer) > 0) {
        printf("[-] %s:%d\n", peer_get_addr(peer), peer_get_port(peer));
    }
}

// msg: ()
void on_peers_request(peer_t *peer, tuple_t *msg) {

    char *peer_addr = peer_get_addr(peer);;
    int peer_port = peer_get_port(peer);

    dynamic_buffer_t buf = dynamic_buffer_create(32);
    tuple_write_start(&buf);
    for (size_t i = 0; i < network_peer_count(); i++) {
        peer_t *p = network_get_peer(i);
        char *addr = peer_get_addr(p);
        int port = peer_get_port(p);
        if (port <= 0) continue;
        if (strcmp(peer_addr, addr) != 0 || port != peer_port) {
            tuple_write_start(&buf);
            tuple_write_string(&buf, addr);
            tuple_write_i32(&buf, port);
            tuple_write_end(&buf);
        }
    }
    tuple_write_end(&buf);
    network_send(EVENT_PEERS_RESPONSE, (buffer_t*) &buf, peer);
    dynamic_buffer_destroy(buf);
}

// msg: (('127.0.0.1', 1960), ('127.0.0.1', 1961), ('127.0.0.1', 1962))
void on_peers_response(peer_t *peer, tuple_t *msg) {
    for (size_t i = 0; i < tuple_size(msg); i++) {
        tuple_t *peer_tuple = tuple_get_tuple(msg, i);
        char* addr = tuple_get_string(peer_tuple, 0);
        int port = tuple_get_i32(peer_tuple, 1);
        if (!network_has_peer(addr, port)) {
            network_connect(addr, port);
        }
    }
}

/**
 * Event handler for network messages of 'block' type. When we recieve a
 * block from a peer, we should validate it and add it to our blockchain.
 */
void on_block(peer_t *peer, tuple_t *msg) {
    block_t *block = block_create_from_tuple(msg, lookup_block);
    if (block != NULL) {
        blockchain_add_block(blockchain, block);
    }
}

/**
 * Event handler for network messages of 'blocks_response' type. When we recieve a
 * block from a peer, we should validate it and add it to our blockchain.
 */
void on_blocks_response(peer_t *peer, tuple_t *msg) {
    for (size_t i = tuple_size(msg); i > 0; i -= 1) {
        tuple_t *block_tuple = tuple_get_tuple(msg, i - 1);
        block_t *block = block_create_from_tuple(block_tuple, lookup_block);
        if (block != NULL) blockchain_add_block(blockchain, block);
    }
}

/**
 * Event handler for network messages of 'blocks_request' type. When we recieve a
 * block from a peer, we should validate it and add it to our blockchain.
 */
void on_blocks_request(peer_t *peer, tuple_t *msg) {
    buffer_t hash = tuple_get_binary(msg, 0);
    block_t *block = lookup_block(hash);
    block_t *iter = blockchain_get_longest(blockchain);
    dynamic_buffer_t buf = dynamic_buffer_create(64);
    tuple_write_start(&buf);
    while (iter != block) {
        block_write(iter, &buf);
        iter = block_prev(iter);
    }
    tuple_write_end(&buf);
    network_send(EVENT_BLOCKS_RESPONSE, (buffer_t *) &buf, peer);
    dynamic_buffer_destroy(buf);
}

void on_pool_request(peer_t *peer, tuple_t *msg) {
    dynamic_buffer_t buf = dynamic_buffer_create(64);
    tuple_write_start(&buf);
    for (size_t i = 0; i < pool_size(pool); i += 1) {
        transaction_t *txn = pool_get(pool, i);
        transaction_write(txn, &buf);
    }
    tuple_write_end(&buf);
    network_send(EVENT_POOL_RESPONSE, (buffer_t *) &buf, peer);
    dynamic_buffer_destroy(buf);
}

void on_pool_response(peer_t *peer, tuple_t *msg) {
    for (size_t i = 0; i < tuple_size(msg); i += 1) {
        tuple_t *txn_tuple = tuple_get_tuple(msg, i);
        transaction_t *txn = transaction_create_from_tuple(txn_tuple);
        if (txn != NULL) pool_add(pool, txn);
    }
}

void on_transaction(peer_t *peer, tuple_t *msg) {
    transaction_t *txn = transaction_create_from_tuple(msg);
    if (txn != NULL) {
        pool_add(pool, txn);
    }
}

/**
 * As soon as the blockchain is extended, we should start mining a new block
 * extending the new longest chain.
 */
void on_extended(block_t *prev, block_t *block) {
    
    // print the account value at each block
    account_t *account = block_get_account(block, get_public_key());
    if (account != NULL) {
      //  printf("value: %llu\n", account_get_value(account));
    }

    // If prev is not an ancestor of block, then a fork has overtaken the
    // longest chain. This invalidates all transactions after the common
    // ancestor of the fork. We should add all of these transactions back to
    // the mempool so they can be confirmed again.
    while (!block_has_ancestor(block, prev)) {
        for (size_t i = 0; i < block_get_transaction_count(prev); i++) {
            transaction_t *txn = block_get_transaction(prev, i);
            if (!transaction_is_coinbase(txn)) {
                pool_add(pool, txn);
            }
        }
        prev = block_prev(prev);
    }

    uint32_t d = compute_difficulty();
    list_t *txns = list_create(1);
    transaction_t *txn = transaction_create_coinbase(get_public_key(), 64);
    list_add(txns, txn);
    block_t *next_block = block_create(block, txns);
    block_set_difficulty(next_block, d);
    miner_mine(miner, next_block);
}

/**
 * As soon as a block is mined, we should add it to our database and broadcast
 * it to our network.
 */
void on_block_mined(miner_t *miner, block_t *block) {
    blockchain_add_block(blockchain, block);
    dynamic_buffer_t buf = dynamic_buffer_create(64);
    block_write(block, &buf);
    network_broadcast(EVENT_BLOCK, (buffer_t *) &buf);
    dynamic_buffer_destroy(buf);
}

bool is_valid_hash(char *hex) {
    if (strlen(hex) != 2 * crypto_generichash_BYTES) return false;
    for (char *c = hex; *c != '\0'; c++) {
        if (!isxdigit(*c)) return false;
    }
    return true;
}

/*
 * GET /block/
 * Respond with a JSON array containing the principal blockchain.
 * Order the response by descending block height.
 */
void on_rest_blocks_request(request_t *req, response_t *res) {
    dynamic_buffer_t *buf = response_get_body(res);
    block_t *block = blockchain_get_longest(blockchain);
    while (block != NULL) {
        block_write_json(block, buf);
        block = block_prev(block);
    }
    json_write_array_end(buf);
    json_write_end(buf);
}

/*
 * GET /block/:/
 * Resond with a JSON object representation of the block with the given hash. 
 * If the hash is invalid, respond with a 400 error code. 
 * If no block with the hash exists, response with a 404 error code.
 */
void on_rest_block_request(request_t *req, response_t *res) {
    dynamic_buffer_t *buf = response_get_body(res);
    char *arg = request_get_param(req, 0);
    
    if (!is_valid_hash(arg)) {
        response_set_code(res, 400);
        return;
    }

    buffer_t hash = buffer_from_hex(arg);
    block_t *block = blockchain_get_block(blockchain, hash);
    if (block != NULL) {
        block_write_json(block, buf);
        json_write_end(buf);
    } else {
        response_set_code(res, 404);
    }
    buffer_destroy(hash);
}

int main(int argc, char **argv) {
   
    parse_arguments(argc, argv);

    // generate a new public-private keypair
    crypto_sign_keypair(pk, sk);

    loop = uv_default_loop();

    blockchain = blockchain_create(on_extended);
    miner = miner_create(on_block_mined);
    pool = pool_create();
    
    rest = rest_create();
    rest_register(rest, "/block/", on_rest_blocks_request);
    rest_register(rest, "/block/:/", on_rest_block_request);

    list_t *txns = list_create(1);
    block_t *block = block_create(NULL, txns);
    block_set_difficulty(block, compute_difficulty());
    miner_mine(miner, block);

    /* create a libuv event loop to manage asynchronous events */
    network_init();


    /* 
     * Register signal handler for SIGINT. When the user manually kills the
     * node, we gracefully stop the libuv event loop so that the program can
     * exit normally, allowing the builtin clang memory sanitizer to check
     * for memory leaks.
     * 
     * Note that we do not use the standard POSIX signal handling mechanism
     * for compatability with windows. The libuv signal handling mechanism
     * emulates SIGINT whenever the users presses CTRL+C.
     */
    uv_signal_t sig;
    uv_signal_init(loop, &sig);
    uv_signal_start(&sig, on_sigint, SIGINT);

    /*
     * Attempt to establish a peer-to-peer connection for each of the
     * connections specified in the command line arguments. 
     * 
     * Note that nearly all nodes should attempt to connect to another node
     * when joining the network. The only exception is the 'bootstrap' node
     * that other nodes first connect to when joining the network.
     */
    for (int i = 0; i < settings.n_peer_connections; i++) {
        char *addr = settings.peer_addresses[i];
        int port = settings.peer_ports[i];
        int err = network_connect(addr, port);
        if (err) {
            printf("error: unable to connect to peer %s:%d\n", addr, port);
        }
    }
 
    network_register(EVENT_CONNECT, on_connect);
    network_register(EVENT_DISCONNECT, on_disconnect);
    network_register(EVENT_HANDSHAKE, on_handshake);
    network_register(EVENT_PEERS_REQUEST, on_peers_request);
    network_register(EVENT_PEERS_RESPONSE, on_peers_response);
    network_register(EVENT_BLOCK, on_block);
    network_register(EVENT_BLOCKS_REQUEST, on_blocks_request);
    network_register(EVENT_BLOCKS_RESPONSE, on_blocks_response);
    network_register(EVENT_POOL_REQUEST, on_pool_request);
    network_register(EVENT_POOL_RESPONSE, on_pool_response);
    network_register(EVENT_TRANSACTION, on_transaction);

    /*
     * Listen on the specified port for incoming peer connections. 
     * 
     * Note that not every node will necessarily accept incoming connections.
     * If specified, a node can act as a "client-only" node that connects and
     * sends messages to other nodes, but does not allow nodes to initiate a
     * connection.
     */
    if (settings.should_listen) {
        int err = network_listen(settings.port, settings.backlog);
        if (err != 0) {
            printf("error: unable to listen on port %d: %s\n", settings.port, uv_err_name(err));
        } else {
            printf("info: accepting connections on port %d\n", settings.port);
        }
    }

    rest_listen(rest, 8080);
    
    /*
     * Start the libuv event loop. This will take complete control over
     * program flow until uv_stop is called.
     */
    uv_run(loop, UV_RUN_DEFAULT);

    miner_destroy(miner);
    blockchain_destroy(blockchain);
    pool_destroy(pool);
}