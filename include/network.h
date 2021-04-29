#ifndef NETWORK_H
#define NETWORK_H

#include <uv.h>
#include <buffer.h>
#include <tuple.h>

extern uv_loop_t *loop;

typedef struct peer peer_t;

char* peer_get_addr(peer_t *peer);
int peer_get_port(peer_t *peer);
void peer_set_port(peer_t *peer, int port);

typedef void (*event_handler_t)(peer_t*, tuple_t*);

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

void network_init();
void network_register(uint32_t event, event_handler_t handler);
int network_connect(char *addr, int port);
void network_disconnect(peer_t *peer);
int network_listen(int port, int backlog);
void network_send(uint32_t event, buffer_t *buffer, peer_t *peer);
void network_broadcast(uint32_t event, buffer_t *buffer);
size_t network_peer_count();
peer_t *network_get_peer(size_t i);
int network_has_peer(char *addr, int port);

#endif /* NETWORK_H */