#include <uv.h>
#include <stdlib.h>
#include <assert.h>

#include <network.h>
#include <message.h>
#include <settings.h>

#include "util/buffer.h"
#include "util/list.h"

typedef struct network {
    uv_tcp_t server;
    event_handler_t handlers[EVENT_COUNT];
    list_t *peers;
    list_t *message_history
} network_t;

typedef struct peer {
    network_t *server;
    uv_tcp_t *socket;
    char addr[16];
    int port;
    dynamic_buffer_t buf;
    void *data;
    void (*free_data)();
} peer_t;


#define MESSAGE_HISTORY_SIZE 1024

static int message_history_compare(guid_t *a, guid_t *b) {
    return guid_compare(*a, *b);
}

static void message_history_add(network_t *self, guid_t guid) {
    guid_t *data = malloc(sizeof(guid_t));
    memcpy(data, &guid, sizeof(guid_t));
    if (list_size(self->message_history) >= MESSAGE_HISTORY_SIZE) {
        guid_t *old = list_remove(self->message_history, 0);
        free(old);
    }
    list_add(self->message_history, data);
}

static int message_history_has(network_t *self, guid_t guid) {
    return list_find(self->message_history, &guid, message_history_compare) != list_size(self->message_history);
}

network_t* network_create() {
    network_t *res = calloc(1, sizeof(network_t));
    assert (res != NULL);
    res->peers = list_create(1);
    uv_tcp_init(uv_default_loop(), &res->server);
    res->server.data = res;
    res->message_history = list_create(MESSAGE_HISTORY_SIZE);
    return res;
}

void network_destroy(network_t *self) {
    list_destroy(self->message_history, free);
    free(self);
}


/*
 * The shared_buffer_t struct is a reference
 * 
 */
struct shared_buffer_t {
    uv_buf_t data;
    int ref_count;
};

static struct shared_buffer_t* shared_buffer_create(uint8_t *data, int len) {
    struct shared_buffer_t *buf = (struct shared_buffer_t*) calloc(1, sizeof(struct shared_buffer_t));
    uint8_t *base = malloc(len);
    memcpy(base, data, len);
    buf->data = uv_buf_init((char *) base, len);
    return buf;
}

static void shared_buffer_retain(struct shared_buffer_t *buf) {
   buf->ref_count += 1;
}

static void shared_buffer_release(struct shared_buffer_t *buf) {
    buf->ref_count -= 1;
    if (buf->ref_count == 0) {
        free(buf->data.base);
        free(buf);
    }
}

struct write_context_t {
  uv_write_t req;
  struct shared_buffer_t *buf;
};


char* peer_get_addr(peer_t *peer) {
    return peer->addr;
}

int peer_get_port(peer_t *peer) {
    return peer->port;
}

void peer_set_port(peer_t *peer, int port) {
    peer->port = port;
}

size_t network_peer_count(network_t *self) {
    return list_size(self->peers);
}

peer_t *network_get_peer(network_t *self, size_t i) {
    return list_get(self->peers, i);
}

int network_has_peer(network_t *self, char *addr, int port) {
    for (size_t i = 0; i < list_size(self->peers); i++) {
        peer_t *peer = list_get(self->peers, i);
        char *peer_addr = peer_get_addr(peer);
        int peer_port = peer_get_port(peer);
        if (strcmp(addr, peer_addr) == 0 && port == peer_port) return 1;
    }
    return 0;
}

/*
 * Once write is complete, free all memory associated with the write context.
 * 
 * Since each write context contains both a uv_write_t struct and a reference
 * counted shared buffer, we should decrement the reference count of the
 * buffer and only free it if there is only one 
 */
static void on_write(uv_write_t* wreq, int status) {
    if (status) fprintf(stderr, "error: writing data: %s\n", uv_err_name(status));
    struct write_context_t *req = (struct write_context_t *) wreq;
    shared_buffer_release(req->buf);
    free(req);
}

/**
 * Broadcast the provided message to all peers by writing it to each peer one
 * at a time.
 * 
 * For efficiency, we create a reference counted buffer containing the message
 * to share among all write requests. This allows us to keep just a single copy
 * of the message, rather than a different copy for each peer.
 * 
 * @param message the message to send
 * @param len the length of the message in bytes
 */
static void broadcast_message(network_t *self, uint8_t *message, int len) {
    struct shared_buffer_t *buf = shared_buffer_create(message, len);
    shared_buffer_retain(buf);
    for (size_t i = 0; i < list_size(self->peers); i++) {
        peer_t *peer = list_get(self->peers, i);
        struct write_context_t *req = (struct write_context_t*) malloc(sizeof(struct write_context_t));
        req->buf = buf;
        shared_buffer_retain(buf);
        uv_write((uv_write_t*) req, (uv_stream_t*) peer->socket, &(req->buf->data), 1, on_write);
    }
    shared_buffer_release(buf);
}

static void send_message(uint8_t *message, int len, peer_t *peer) {
    struct shared_buffer_t *buf = shared_buffer_create(message, len);
    shared_buffer_retain(buf);
    struct write_context_t *req = (struct write_context_t*) malloc(sizeof(struct write_context_t));
    req->buf = buf;
    uv_write((uv_write_t*) req, (uv_stream_t*) peer->socket, &(req->buf->data), 1, on_write);
}

void network_broadcast(network_t *self, uint32_t type, buffer_t *buffer) {
    uint8_t message[MESSAGE_HEADER_SIZE + buffer->length];
    guid_t guid = guid_new();
    message_set_magic(message, MESSAGE_MAGIC_NUMBER);
    message_set_length(message, buffer->length);
    message_set_guid(message, guid);
    message_set_type(message, type);
    memcpy(message + MESSAGE_HEADER_SIZE, buffer->data, buffer->length);

    message_history_add(self, guid);
    broadcast_message(self, message, MESSAGE_HEADER_SIZE + buffer->length);
}

void network_send(network_t *self, uint32_t event, buffer_t *buffer, peer_t *peer) {
    uint8_t message[MESSAGE_HEADER_SIZE + buffer->length];
    guid_t guid = guid_null();
    message_set_magic(message, MESSAGE_MAGIC_NUMBER);
    message_set_length(message, buffer->length);
    message_set_guid(message, guid);
    message_set_type(message, event);
    memcpy(message + MESSAGE_HEADER_SIZE, buffer->data, buffer->length);

    message_history_add(self, guid);
    send_message(message, MESSAGE_HEADER_SIZE + buffer->length, peer);
}

static void handle_message(peer_t *peer, uint8_t *message, size_t len) {
    network_t *self = peer->server;
    guid_t guid = message_get_guid(message);
    uint32_t length = message_get_length(message);
    uint16_t type = message_get_type(message);
    uint8_t *payload = message + MESSAGE_HEADER_SIZE;
    buffer_t buffer = {length, payload};
    if (guid_is_null(guid)) {
        if (type < EVENT_COUNT && self->handlers[type]) {
            tuple_t *tuple = tuple_parse(&buffer);
            self->handlers[type](peer, tuple);
            tuple_destroy(tuple);
        }
    } else if (!message_history_has(self, guid)) {
        if (type < EVENT_COUNT && self->handlers[type]) {
            tuple_t *tuple = tuple_parse(&buffer);
            self->handlers[type](peer, tuple);
            tuple_destroy(tuple);
        }
        message_history_add(self, guid);
        broadcast_message(self, message, len);
    }
}

/* Allocate buffers as requested by UV */
static void on_alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
    char *base = (char *) calloc(1, size);
    if (!base) {
        *buf = uv_buf_init(NULL, 0);
    } else {
        *buf = uv_buf_init(base, size);
    }
}

static void on_socket_closed(uv_handle_t *socket) {
    peer_t *peer = socket->data;
    network_t *server = peer->server;
    if (server->handlers[EVENT_DISCONNECT]) server->handlers[EVENT_DISCONNECT](peer, NULL);
    size_t index = list_find(server->peers, socket->data, NULL);
    if (index != list_size(server->peers)) list_remove(server->peers, index);
    dynamic_buffer_destroy(peer->buf);
    if (peer->free_data) peer->free_data(peer->data);
    free(peer);
    free(socket);
}

void network_disconnect(network_t *self, peer_t *peer) {
    if (!peer) return;
    uv_close((uv_handle_t*) peer->socket, on_socket_closed);
}

uint8_t* find_message_start(dynamic_buffer_t *buf) {
    for (size_t i = 0; i < buf->length - MESSAGE_HEADER_SIZE + 1; i++) {
        if (message_get_magic(buf->data + i) == MESSAGE_MAGIC_NUMBER) return buf->data + i;
    }
    return NULL;
}

/* 
 * Callback for read function. For large messages, the data may be split into
 * multiple calls to this function. Also, a single call to this function may
 * contain multiple messages. Thus, at every call to this function, we simply
 * concatenate newly read data to the end of a large string, then process this
 * string to parse out messages.
 * 
 * Note that this function owns the dynamically allocated buffer passed in as
 * as the "buf" argument and is responsible for freeing its memory under all
 * circumstances.
 */
static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    
    peer_t *peer = stream->data;

    /*
     * If an error occured while reading, we should cleanup any allocated
     * memory and close the connection.
     */
    if (nread < 0) {
        free(buf->base);
        network_disconnect(peer->server, peer);
        return;
    }

    /* concatenate data packet onto the peer read buffer */
    dynamic_buffer_write(buf->base, buf->len, &peer->buf);
    free(buf->base);

    uint8_t *start = find_message_start(&peer->buf);
    while (start != NULL) {

        // splice out any bytes that occur before the magic number
        size_t padding = (size_t) start - (size_t) peer->buf.data;
        memmove(peer->buf.data, start, peer->buf.length - padding);
        peer->buf.length -= padding;

        // parse message header fields
        guid_t guid = message_get_guid(peer->buf.data);
        uint32_t length = message_get_length(peer->buf.data);
        uint16_t type = message_get_type(peer->buf.data);

        // wait for more data until we recieve the entire message body
        size_t message_length = MESSAGE_HEADER_SIZE + length;
        if (peer->buf.length < message_length) return;

        // handle the message 
        handle_message(peer, peer->buf.data, message_length);

        // splice out the message from the peer buffer
        memmove(peer->buf.data, peer->buf.data + message_length, peer->buf.length - message_length);
        peer->buf.length -= message_length;

        start = find_message_start(&peer->buf);
    }

}

void create_peer_from_tcp_socket(network_t *self, uv_tcp_t *socket, void *data, void (*free_data)(void*)) {
    struct sockaddr_in addr;
    uv_tcp_getpeername(socket, (struct sockaddr *) &addr, &(int){sizeof(addr)});
    
    peer_t *peer = calloc(1, sizeof(peer_t));
    peer->server = self;
    peer->socket = socket;
    strcpy(peer->addr, inet_ntoa(addr.sin_addr));
    peer->port = addr.sin_port;
    peer->data = data;
    peer->free_data = free_data;
    peer->buf = dynamic_buffer_create(32);
    socket->data = peer;
    list_add(self->peers, peer);

    uv_read_start((uv_stream_t *)socket, on_alloc, on_read);

}

/* Callback for handling the new connection */
static void on_incoming_connection(uv_stream_t *server, int status) {
    
    network_t *self = server->data;

    if (status != 0) {
        printf("error: incoming connection: %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *socket = (uv_tcp_t *) calloc(1, sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), socket);
    
    if (uv_accept(server, (uv_stream_t *)socket) == 0) {
        create_peer_from_tcp_socket(self, socket, NULL, NULL);
        if (self->handlers[EVENT_CONNECT]) self->handlers[EVENT_CONNECT]((peer_t *) socket->data, NULL);
    } else {
        printf("connection error\n");
        free(socket);
    }
}

void network_register(network_t *self, uint32_t event, event_handler_t handler) {
    self->handlers[event] = handler;
}


typedef struct connect_ctx {
    void *data;
    void (*free_data)(void*);
    network_t *server;
    struct sockaddr_in *peer_addr;
} connect_ctx_t;

void on_outgoing_connection(uv_connect_t* connection, int status) {
    
    connect_ctx_t *ctx = connection->data;

    if (status != 0) {
        struct sockaddr_in *addr = connection->data;
        printf("error: unable to connect to %s:%d\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
        if (ctx->free_data) ctx->free_data(ctx->data);
    } else {
        uv_tcp_t* socket = (uv_tcp_t *) connection->handle;
        create_peer_from_tcp_socket(ctx->server, socket, ctx->data, ctx->free_data);
        if (ctx->server->handlers[EVENT_CONNECT]) ctx->server->handlers[EVENT_CONNECT]((peer_t *) socket->data, NULL);
    }

    free(connection->data);
    free(connection);

}

/**
 * Connect asynchronously to the peer with the specified address and port.
 * Once a connection has been established, or failed to be established, the
 * on_outgoing_connect function will be called. 
 * 
 * @param address the ipv4 address of the peer
 * @param port the port of the peer's server
 */
int network_connect(network_t *self, char *address, int port, void *data, void (*free_data)(void*)) {
    /* allocate socket for connecting to peer */
    uv_tcp_t *socket = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), socket);

    /* connect asynchronously to node with specified address and port */
    connect_ctx_t *connect_ctx = calloc(1, sizeof(connect_ctx_t));
    connect_ctx->server = self;
    connect_ctx->data = data;
    connect_ctx->free_data = free_data;
    uv_ip4_addr(address, port, &connect_ctx->peer_addr);
    uv_connect_t *connect = malloc(sizeof(uv_connect_t));
    connect->data = connect_ctx;
    int err = uv_tcp_connect(connect, socket, (struct sockaddr*) &connect_ctx->peer_addr, on_outgoing_connection);
    if (err != 0) {
        if (free_data) free_data(data);
        free(connect_ctx);
    }
    return err;
}

/**
 * Initialize socket to listen for incoming connections on the port
 * specified in the command line arguments. If no port is specified,
 * listen on the default port defined above.
 * 
 * @param port the port to listen on
 * @param backlog the size of the backlog queue for incoming connections
 */ 
int network_listen(network_t *self, int port, int backlog) {
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", settings.port, &addr);
    uv_tcp_bind(self, (struct sockaddr *) &addr, 0);
    return uv_listen((uv_stream_t*) &self->server, backlog, on_incoming_connection);
}