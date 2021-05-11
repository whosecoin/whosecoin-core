#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>

#include <uv.h>
#include <sodium.h>

#include <message.h>
#include <settings.h>
#include <network.h>
#include <tuple.h>
#include <blockchain.h>
#include <pool.h>

#include "util/http.h"
#include "util/json.h"

#define VERSION_STRING "1.0.0-alpha"
#define BLOCK_TIME 3
#define EPOCH_LENGTH 16


uv_loop_t *loop;
uv_timer_t timer_req;
uv_tty_t tty_in;

blockchain_t *blockchain;
network_t *network;
pool_t* pool;
http_t *http;

uint8_t pk[crypto_vrf_PUBLICKEYBYTES];
uint8_t sk[crypto_vrf_SECRETKEYBYTES];

void on_sigint(uv_signal_t *handle, int signum) {
    uv_stop(loop);
    uv_signal_stop(handle);
    printf("\rinfo: shutting down\n");
}

/**
 * Return a buffer containing the global public key.
 * @return a buffer containing the user's public key.
 */
uint8_t* get_public_key() {
    return pk;
}

/**
 * Return a buffer containing the global secret key.
 * @return a buffer containing the user's secret key.
 */
uint8_t* get_secret_key() {
    return sk;
}

/**
 * Send the block to all neighbors in the network.
 * @param block the block
 */
void broadcast_block(block_t *block) {
    assert(block != NULL);
    dynamic_buffer_t buf = dynamic_buffer_create(64);
    block_write(block, &buf);
    network_broadcast(network, EVENT_BLOCK, (buffer_t *) &buf);
    dynamic_buffer_destroy(buf);
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
    uint8_t *hash = block_get_hash(block);
    dynamic_buffer_t buf = dynamic_buffer_create(64);
    tuple_write_start(&buf);
    tuple_write_binary(&buf, crypto_generichash_BYTES, hash);
    tuple_write_end(&buf);
    network_send(network, EVENT_BLOCKS_REQUEST, (buffer_t *) &buf, peer);
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
    network_send(network, EVENT_PEERS_REQUEST, (buffer_t*) &buf, peer);
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
    network_send(network, EVENT_POOL_REQUEST, (buffer_t*) &buf, peer);
    dynamic_buffer_destroy(buf);
}

// msg: NULL
void on_connect(peer_t *peer, tuple_t *msg) {
    dynamic_buffer_t buf = dynamic_buffer_create(32);
    tuple_write_start(&buf);
    tuple_write_i32(&buf, settings.port);
    tuple_write_string(&buf, VERSION_STRING);
    tuple_write_end(&buf);
    network_send(network, EVENT_HANDSHAKE, (buffer_t*) &buf, peer);
    dynamic_buffer_destroy(buf);

    synchronize_peers(peer);
    synchronize_blockchain(peer, blockchain_get_principal(blockchain));
}

// msg: (port: i32, version: string)
void on_handshake(peer_t *peer, tuple_t *msg) {

    // If we are already connected to this peer node, we should disconnect
    // from the node immediately without printing out any messages.
    char *addr = peer_get_addr(peer);
    int32_t port = tuple_get_i32(msg, 0); 
    if (network_has_peer(network, addr, port)) {
        network_disconnect(network, peer);
        return;
    }

    // If the peer node has a version string that doesn't match ours, we
    // should disconnect, but still print out a connection message.
    char *version = tuple_get_string(msg, 1);
    if (strcmp(VERSION_STRING, version) != 0) {
        network_disconnect(network, peer);
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
    for (size_t i = 0; i < network_peer_count(network); i++) {
        peer_t *p = network_get_peer(network, i);
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
    network_send(network, EVENT_PEERS_RESPONSE, (buffer_t*) &buf, peer);
    dynamic_buffer_destroy(buf);
}

// msg: (('127.0.0.1', 1960), ('127.0.0.1', 1961), ('127.0.0.1', 1962))
void on_peers_response(peer_t *peer, tuple_t *msg) {
    for (size_t i = 0; i < tuple_size(msg); i++) {
        tuple_t *peer_tuple = tuple_get_tuple(msg, i);
        char* addr = tuple_get_string(peer_tuple, 0);
        int port = tuple_get_i32(peer_tuple, 1);
        if (!network_has_peer(network, addr, port)) {
            network_connect(network, addr, port, NULL, NULL);
        }
    }
}

/**
 * Event handler for network messages of 'block' type. When we recieve a
 * block from a peer, we should validate it and add it to our blockchain.
 */
void on_block(peer_t *peer, tuple_t *msg) {

    /* Attempt to parse the message into a block */
    block_t *block = block_create_from_tuple(msg, lookup_block);
    /* If the message was not malformed and is not yet in the block database */ 
    if (block != NULL && blockchain_add_block(blockchain, block)) {
       
        /* 
         * Attempt to fork the blockchain. This will only succeed if priority
         * is lower than all other forks.
         */
        block_t *prev = block_get_prev(block);
        if (prev != NULL && block_get_child_with_public_key(prev, get_public_key()) == NULL) {
            list_t *txns = list_create(1);
            while (pool_size(pool) != 0) {
                transaction_t *txn = pool_remove(pool, 0);
                list_add(txns, txn);
            }
            block_t *next = block_create(get_public_key(), get_secret_key(), prev, txns); 
            if (next != NULL && blockchain_add_block(blockchain, next)) {
                broadcast_block(next);
            }  else {
                printf("invalid block\n");
            }
        }

    }
}

/**
 * Event handler for network messages of 'blocks_response' type. When we recieve a
 * block from a peer, we should validate it and add it to our blockchain.
 */
void on_blocks_response(peer_t *peer, tuple_t *msg) {

    /* Iterate through all blocks from earliest to latest */
    for (size_t i = tuple_size(msg); i > 0; i -= 1) {
        
        /* Attempt to parse the message into a block */
        tuple_t *block_tuple = tuple_get_tuple(msg, i - 1);
        block_t *block = block_create_from_tuple(block_tuple, lookup_block);
 
        /* If the message was not malformed and is not yet in the block database */ 
        if (block != NULL && blockchain_add_block(blockchain, block)) {
            
            /* 
            * Attempt to fork the blockchain. This will only succeed if priority
            * is lower than all other forks.
            */
            block_t *prev = block_get_prev(block);
            if (prev != NULL && block_get_child_with_public_key(prev, get_public_key()) == NULL) {
                list_t *txns = list_create(1);
                while (pool_size(pool) != 0) {
                    transaction_t *txn = pool_remove(pool, 0);
                    list_add(txns, txn);
                }
                block_t *next = block_create(get_public_key(), get_secret_key(), prev, txns); 
                if (next != NULL && blockchain_add_block(blockchain, next)) {
                    broadcast_block(next);
                } else {
                printf("invalid block\n");
            }
            }
        }
    }
}

/**
 * Event handler for network messages of 'blocks_request' type. When we recieve a
 * block from a peer, we should validate it and add it to our blockchain.
 */
void on_blocks_request(peer_t *peer, tuple_t *msg) {
    buffer_t hash = tuple_get_binary(msg, 0);
    block_t *block = lookup_block(hash);
    block_t *iter = blockchain_get_principal(blockchain);
    dynamic_buffer_t buf = dynamic_buffer_create(64);
    tuple_write_start(&buf);
    while (iter != block && iter != NULL) {
        block_write(iter, &buf);
        iter = block_get_prev(iter);
    }
    tuple_write_end(&buf);
    network_send(network, EVENT_BLOCKS_RESPONSE, (buffer_t *) &buf, peer);
    dynamic_buffer_destroy(buf);
}

/**
 * Synchronize pool of pending transactions with peer by sending entire pool
 * to peer. 
 */
void on_pool_request(peer_t *peer, tuple_t *msg) {
    dynamic_buffer_t buf = dynamic_buffer_create(64);
    tuple_write_start(&buf);
    for (size_t i = 0; i < pool_size(pool); i += 1) {
        transaction_t *txn = pool_get(pool, i);
        transaction_write(txn, &buf);
    }
    tuple_write_end(&buf);
    network_send(network, EVENT_POOL_RESPONSE, (buffer_t *) &buf, peer);
    dynamic_buffer_destroy(buf);
}

/**
 * Synchronize pool of pending transactions with peer. Parse all transactions
 * sent by peer and add them to the pool if they are valid.
 */
void on_pool_response(peer_t *peer, tuple_t *msg) {
    for (size_t i = 0; i < tuple_size(msg); i += 1) {
        tuple_t *txn_tuple = tuple_get_tuple(msg, i);
        transaction_t *txn = transaction_create_from_tuple(txn_tuple);
        if (txn != NULL) pool_add(pool, txn);
    }
}

/**
 * When a transaction is recieved, add it to the pool of pending tramsactions
 * if valid.
 */
void on_transaction(peer_t *peer, tuple_t *msg) {
    transaction_t *txn = transaction_create_from_tuple(msg);
    if (txn != NULL) {
        pool_add(pool, txn);
    }
}

/**
 * Once per second, we attempt to append a new block onto the principal leaf node of
 * the blockchain. Before doings so, we should make sure that we have not already
 * created a block from this node.
 */
void on_timer(uv_timer_t* handle) {
    block_t *block = blockchain_get_principal(blockchain); 
    if (block != NULL && block_get_child_with_public_key(block, get_public_key()) == NULL) {
        list_t *txns = list_create(1);
        while (pool_size(pool) != 0) {
            transaction_t *txn = pool_remove(pool, 0);
            list_add(txns, txn);
        }
        block_t *next = block_create(get_public_key(), get_secret_key(), block, txns); 
        if (next != NULL) {
            blockchain_add_block(blockchain, next);
            broadcast_block(next);    
        } else {
            printf("invalid block\n");
        }
    }   
}

/**
 * As soon as the blockchain is extended, we should start mining a new block
 * extending the new longest chain.
 */
void on_extended(block_t *prev, block_t *block) {
    
    // If prev is not an ancestor of block, then a fork has overtaken the
    // longest chain. This invalidates all transactions after the common
    // ancestor of the fork. We should add all of these transactions back to
    // the mempool so they can be confirmed again.
    while (!block_has_ancestor(block, prev)) {
        for (size_t i = 0; i < block_get_transaction_count(prev); i++) {
            transaction_t *txn = block_get_transaction(prev, i);
            pool_add(pool, txn);
        }
        prev = block_get_prev(prev);
    }

    uv_timer_stop(&timer_req);
    uv_timer_start(&timer_req, on_timer, 1000 * BLOCK_TIME, 0);

}


/**
 * Return true if the string is a valid hex representation of a hash
 * and false otherwise.
 * @param hex a hex string
 * @return whether or not hex is a hash
 */
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
void on_http_blocks_request(request_t *req, response_t *res) {
    dynamic_buffer_t *buf = response_get_body(res);
    block_t *block = blockchain_get_principal(blockchain);
    json_write_array_start(buf);
    while (block != NULL) {
        block_write_json(block, buf);
        block = block_get_prev(block);
    }
    json_write_array_end(buf);
    json_write_end(buf);
}

/*
 * GET /block/:hash/
 * Respond with a JSON representation of the block with the given hash. 
 * If the hash is invalid, respond with a 400 error code. 
 * If no block with the hash exists, response with a 404 error code.
 */
void on_http_block_request(request_t *req, response_t *res) {
    dynamic_buffer_t *buf = response_get_body(res);
    char *arg = request_get_param(req, 0);
    
    // validate that the argument is a valid hex string of proper length.
    if (!is_valid_hash(arg)) {
        response_set_code(res, 400);
        return;
    }

    // convert the hex string into a binary buffer.
    buffer_t hash = buffer_from_hex(arg);

    // query the blockchain for a block with the given hash.
    block_t *block = blockchain_get_block(blockchain, hash);

    // response with the block if it exists and 404 otherwise.
    if (block != NULL) {
        block_write_json(block, buf);
        json_write_end(buf);
    } else {
        response_set_code(res, 404);
    }

    // clean up binary buffer.
    buffer_destroy(hash);
}

static void alloc_read_buffer(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
    char *base = (char *) calloc(1, size);
    if (!base) {
        *buf = uv_buf_init(NULL, 0);
    } else {
        *buf = uv_buf_init(base, size);
    }
}

static char* binary_to_hex(const uint8_t *data, size_t size) {
    char* res = calloc(1, size * 2 + 1);
    for (uint8_t i = 0; i < size; i++) {
        sprintf(res + 2 * i, "%02x", data[i]);
    }
    return res;
} 

static uint8_t* hex_to_binary(const char *data, size_t size) {
    if (strlen(data) != 2 * size) return NULL;
    uint8_t* res = calloc(1, size);
    for (uint8_t i = 0; i < size; i++) {
        int x;
        sscanf(data + 2 * i, "%02x", &x);
        res[i] = (uint8_t) x;
    }
    return res;
} 

void read_stdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread == -1) {
        fprintf(stderr, "Read Error!\n");
    }
    else {
        if (nread > 0) {
            char command[nread];
            memcpy(command, buf->base, nread);
            command[nread - 1] = '\0';
            
            if (strstr(command, "value") == command) {
                block_t *principal = blockchain_get_principal(blockchain);
                account_t *account = block_get_account(principal, get_public_key());
                if (account != NULL) {
                    uint64_t value = account_get_value(account);
                    printf("%llu\n", value);
                } else {
                    printf("0\n");
                }
            } else if (strstr(command, "public_key") == command) {
                uint8_t *pk = get_public_key();
                char *pk_hex = binary_to_hex(pk, crypto_vrf_PUBLICKEYBYTES);
                printf("%s\n", pk_hex);
                free(pk_hex);
            } else if (strstr(command, "send") == command) {
                char recipient_hex[2 * crypto_vrf_PUBLICKEYBYTES + 1] = {0};
                uint64_t value = 0;
                static_assert(crypto_vrf_PUBLICKEYBYTES == 32, "invalid assumption in scanf");
                int res = sscanf(command, "send %llu %64s", &value, recipient_hex);
                if (res == 2) {
                    uint8_t *recipient = hex_to_binary(recipient_hex, crypto_vrf_PUBLICKEYBYTES);
                    transaction_t *txn = transaction_create(get_public_key(), get_secret_key(), recipient, value, 0);
                    if (txn != NULL) {
                        pool_add(pool, txn);
                    } else {
                        printf("invalid transaction\n");
                    }
                    free(recipient);
                }
            } else if (strstr(command, "pool") == command) {
                size_t size = pool_size(pool);
                printf("%d pending transations\n", size);
                for (size_t i = 0; i < size; i++) {
                    transaction_t *txn = pool_get(pool, i);
                    dynamic_buffer_t buf = dynamic_buffer_create(32);
                    transaction_write_json(txn, &buf);
                    json_write_end(&buf);
                    printf("%.*s\n", buf.length, buf.data);
                    dynamic_buffer_destroy(buf);
                }
            } else {
                printf("unknown command '%s'\n", command);
            }
        }
    } 
    if (buf->base) free(buf->base);
}

int main(int argc, char **argv) {
   
    parse_arguments(argc, argv);

    // generate a new public-private keypair
    crypto_vrf_keypair(pk, sk);

    loop = uv_default_loop();
    uv_timer_init(loop, &timer_req);
    uv_tty_init(loop, &tty_in, STDIN_FILENO, true);
    uv_read_start((uv_stream_t*)&tty_in, alloc_read_buffer, read_stdin);

    blockchain = blockchain_create(on_extended);
    network = network_create();
    pool = pool_create();
    

    loop = uv_default_loop();

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
        int err = network_connect(network, addr, port, NULL, NULL);
        if (err) {
            printf("error: unable to connect to peer %s:%d\n", addr, port);
        }
    }
 
    network_register(network, EVENT_CONNECT, on_connect);
    network_register(network, EVENT_DISCONNECT, on_disconnect);
    network_register(network, EVENT_HANDSHAKE, on_handshake);
    network_register(network, EVENT_PEERS_REQUEST, on_peers_request);
    network_register(network, EVENT_PEERS_RESPONSE, on_peers_response);
    network_register(network, EVENT_BLOCK, on_block);
    network_register(network, EVENT_BLOCKS_REQUEST, on_blocks_request);
    network_register(network, EVENT_BLOCKS_RESPONSE, on_blocks_response);
    network_register(network, EVENT_POOL_REQUEST, on_pool_request);
    network_register(network, EVENT_POOL_RESPONSE, on_pool_response);
    network_register(network, EVENT_TRANSACTION, on_transaction);

    /*
     * Listen on the specified port for incoming peer connections. 
     * 
     * Note that not every node will necessarily accept incoming connections.
     * If specified, a node can act as a "client-only" node that connects and
     * sends messages to other nodes, but does not allow nodes to initiate a
     * connection.
     */
    if (settings.should_listen) {
        int err = network_listen(network, settings.port, settings.backlog);
        if (err != 0) {
            printf("error: unable to listen on port %d: %s\n", settings.port, uv_err_name(err));
        } else {
            printf("info: accepting connections on port %d\n", settings.port);
        }
    }

    /*
     * Create a REST server that exposes an API for querying JSON representations
     * of the current blockchain state.
     */
    http = http_create();
    http_register(http, "/block/", on_http_blocks_request);
    http_register(http, "/block/:/", on_http_block_request);
    http_listen(http, 8080);

    list_t *txns = list_create(1);
    block_t *block = block_create(get_public_key(), get_secret_key(), NULL, txns);
    if (block != NULL) {
        blockchain_add_block(blockchain, block);
        broadcast_block(block);
    }
    
    /*
     * Start the libuv event loop. This will take complete control over
     * program flow until uv_stop is called.
     */
    uv_run(loop, UV_RUN_DEFAULT);

    blockchain_destroy(blockchain);
    pool_destroy(pool);
    http_destroy(http);
    network_destroy(network);
}