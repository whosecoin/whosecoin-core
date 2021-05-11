#ifndef NETWORK_H
#define NETWORK_H

#include <uv.h>
#include <util/buffer.h>
#include <tuple.h>

enum {
    EVENT_CONNECT,
    EVENT_DISCONNECT,
    EVENT_HANDSHAKE,
    EVENT_PEERS_REQUEST,
    EVENT_PEERS_RESPONSE,
    EVENT_BLOCKS_REQUEST,
    EVENT_BLOCKS_RESPONSE,
    EVENT_POOL_REQUEST,
    EVENT_POOL_RESPONSE,
    EVENT_BLOCK,
    EVENT_TRANSACTION,
    EVENT_COUNT,
};

typedef struct peer peer_t;
char* peer_get_addr(peer_t *peer);
int peer_get_port(peer_t *peer);
void peer_set_port(peer_t *peer, int port);

/**
 * The network_t struct is a peer-to-peer gossip network. It allows messages in 
 * the binary tuple data format to be propogated through the network to all
 * connected peers. 
 */
typedef struct network network_t;

typedef void (*event_handler_t)(peer_t*, tuple_t*);
int network_has_peer(network_t *self, char *addr, int port);

/**
 * The peer_t struct represents a single peer connection.
 */
typedef struct peer peer_t;

/**
 * Return the remote address of the peer.
 * @param peer the peer
 * @return the remote address string of the peer
 */
char* peer_get_addr(peer_t *peer);

/**
 * Return the port number on which the peer is connected
 * @param peer the peer
 * @return the port number
 */
int peer_get_port(peer_t *peer);

/**
 * Return the extra data field associated with the field.
 * This is an application specific value.
 * @param peer the peer.
 * @return the extra data field.
 */
void* peer_get_data(peer_t *peer);

/**
 * Create a new peer-to-peer network connection.
 * @return the network
 */
network_t *network_create();

/**
 * Destroy the network and free all associated memory;
 */
void network_destroy(network_t *self);

/**
 * Return the number of peers directly connected to the node.
 * @param self the network
 * @return the number of peers
 */
size_t network_peer_count(network_t *self);

/**
 * Return the ith peer connected to the network.
 * @param self the network
 * @param i the peer index
 * @return the ith peer
 */
peer_t* network_get_peer(network_t *self, size_t i);

/**
 * Connect to the peer with the given address, port and assign the resulting
 * peer_t with the given data field. The data field can store any application
 * specific extra data associated with a peer.
 * 
 * @param self the network.
 * @param addr the remote address
 * @param port the remote port
 * @param data the extra data field
 * @param free_data the destructor for the extra data field
 */
int network_connect(network_t *self, char *addr, int port, void *data, void (*free_data)(void*));

/**
 * Disconnect from the given peer.
 * @param self the network.
 * @param peer the peer.
 */
void network_disconnect(network_t *self, peer_t *peer);

/**
 * Listen for incoming connections on the specified port.
 * @param self the network
 * @param port the port to listen on.
 */
int network_listen(network_t *self, int port, int backlog);

/**
 * Send a message to a specific peer node.
 * @param self the network
 * @param event the message type (application defined) 
 * @param buffer a buffer containing a valid binary tuple
 * @param peer the peer to send the message to.
 */
void network_send(network_t *self, uint32_t event, buffer_t *buffer, peer_t *peer);

/**
 * Send a message to all nodes in the network.
 * @param self the network
 * @param event the message type (application defined) 
 * @param buffer a buffer containing a valid binary tuple
 */
void network_broadcast(network_t *self, uint32_t event, buffer_t *buffer);

/**
 * Register an event handler that will be called every time a new message of
 * the specified message type is recieved.
 * 
 * @param self the network.
 * @param event the message type (application defined)
 * @param handler the message handler.
 */
void network_register(network_t *self, uint32_t event, event_handler_t handler);

#endif /* NETWORK_H */