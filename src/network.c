#include <uv.h>
#include <stdlib.h>

#include <network.h>
#include <message.h>
#include <settings.h>
#include <message_history.h>
#include "util/buffer.h"
#include "util/list.h"

uv_loop_t *loop;
uv_tcp_t server;

event_handler_t handlers[EVENT_COUNT];

typedef struct peer {
    uv_tcp_t *socket;
    char addr[16];
    int port;
    uint8_t *text;
    size_t text_len;
} peer_t;

list_t *peers;

void network_init() {
    peers = list_create(1);
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

size_t network_peer_count() {
    return list_size(peers);
}

peer_t *network_get_peer(size_t i) {
    return list_get(peers, i);
}

int network_has_peer(char *addr, int port) {
    for (size_t i = 0; i < list_size(peers); i++) {
        peer_t *peer = list_get(peers, i);
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
static void broadcast_message(uint8_t *message, int len) {
    struct shared_buffer_t *buf = shared_buffer_create(message, len);
    shared_buffer_retain(buf);
    for (size_t i = 0; i < list_size(peers); i++) {
        peer_t *peer = list_get(peers, i);
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

void network_broadcast(uint32_t type, buffer_t *buffer) {
    uint8_t message[MESSAGE_HEADER_SIZE + buffer->length];
    guid_t guid = guid_new();
    message_set_magic(message, MESSAGE_MAGIC_NUMBER);
    message_set_length(message, buffer->length);
    message_set_guid(message, guid);
    message_set_type(message, type);
    memcpy(message + MESSAGE_HEADER_SIZE, buffer->data, buffer->length);

    message_history_add(guid);
    broadcast_message(message, MESSAGE_HEADER_SIZE + buffer->length);
}

void network_send(uint32_t event, buffer_t *buffer, peer_t *peer) {
    uint8_t message[MESSAGE_HEADER_SIZE + buffer->length];
    guid_t guid = guid_null();
    message_set_magic(message, MESSAGE_MAGIC_NUMBER);
    message_set_length(message, buffer->length);
    message_set_guid(message, guid);
    message_set_type(message, event);
    memcpy(message + MESSAGE_HEADER_SIZE, buffer->data, buffer->length);

    message_history_add(guid);
    send_message(message, MESSAGE_HEADER_SIZE + buffer->length, peer);
}

static void handle_message(uv_tcp_t *peer, uint8_t *message, size_t len) {
    guid_t guid = message_get_guid(message);
    uint32_t length = message_get_length(message);
    uint16_t type = message_get_type(message);
    uint8_t *payload = message + MESSAGE_HEADER_SIZE;
    buffer_t buffer = {length, payload};
    if (guid_is_null(guid)) {
        if (type < EVENT_COUNT && handlers[type]) {
            tuple_t *tuple = tuple_parse(&buffer);
            handlers[type]((peer_t *) peer->data, tuple);
            tuple_destroy(tuple);
        }
    } else if (!message_history_has(guid)) {
        if (type < EVENT_COUNT && handlers[type]) {
            tuple_t *tuple = tuple_parse(&buffer);
            handlers[type]((peer_t *) peer->data, tuple);
            tuple_destroy(tuple);
        }
        message_history_add(guid);
        broadcast_message(message, len);
    }
}

/* Allocate buffers as requested by UV */
static void alloc_read_buffer(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
    char *base = (char *) calloc(1, size);
    if (!base) {
        *buf = uv_buf_init(NULL, 0);
    } else {
        *buf = uv_buf_init(base, size);
    }
}

static void on_socket_closed(uv_handle_t *socket) {
    peer_t *peer = socket->data;
    if (handlers[EVENT_DISCONNECT]) handlers[EVENT_DISCONNECT](peer, NULL);
    size_t index = list_find(peers, socket->data, NULL);
    if (index != list_size(peers)) list_remove(peers, index);
    free(peer->text);
    free(peer);
    free(socket);
}

void network_disconnect(peer_t *peer) {
    if (!peer) return;
    uv_close((uv_handle_t*) peer->socket, on_socket_closed);
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
    
    uv_tcp_t *client = (uv_tcp_t *)stream;
    peer_t *data = stream->data;

    /*
     * If an error occured while reading, we should cleanup any allocated
     * memory and close the connection.
     */
    if (nread < 0) {
        free(buf->base);
        network_disconnect(data);
        return;
    }

    /*
     * Stores data stream as dynamically sized string.
     * Every time the server recieves more data from the stream,
     * it appends the data to the string and updates the size.
     * 
     * We concatenate a '\0' character to the end of the string
     * so that we can use string processing functions such as strstr provided
     * by the standard library.
     */
    if (!data->text) {
        data->text = malloc(nread + 1);
        memcpy(data->text, buf->base, nread);
        data->text_len = nread;
        data->text[data->text_len] = '\0';
    } else {
        data->text = realloc(data->text, data->text_len + nread + 1);
        memcpy(data->text + data->text_len, buf->base, nread);
        data->text_len += nread;
        data->text[data->text_len] = '\0';
    }

    free(buf->base);

    /*
     * We will iterate over all messages currently stored in the buffer and
     * handle them one by one.
     */
    uint8_t *text_end = data->text + data->text_len;
    uint8_t *message_start = data->text;
    while (message_start + 2 * sizeof(uint32_t) <= text_end) {
        
        /* parse fields from message header */
        uint32_t magic = message_get_magic(message_start);
        uint32_t payload_len = message_get_length(message_start);
        uint32_t message_len = MESSAGE_HEADER_SIZE + payload_len;

        /* 
         * If the first field is not the magic number, skip ahead until the
         * start of the next message is found.
         */
        if (magic != MESSAGE_MAGIC_NUMBER) {
            message_start += sizeof(uint32_t);
            continue;
        }

        /*
         * If the entirety of the current message is not contained in the buffer,
         * then we should wait for more data to process the message.
         */
        if (message_start + message_len > text_end) {
            break;
        }

        handle_message(client, message_start, message_len);
        message_start += message_len;
    }

    /*
     * Copy all unprocessed bytes into a newly allocated buffer
     * and free all processed data.
     */
    if (message_start != data->text) {
        int remainder_len = text_end - message_start;
        uint8_t *remainder = (uint8_t *) malloc(remainder_len);
        memcpy(remainder, message_start, remainder_len);
        free(data->text);
        data->text = remainder;
        data->text_len = remainder_len;
    }
}

void create_peer_from_tcp_socket(uv_tcp_t *socket) {
    struct sockaddr_in addr;
    uv_tcp_getpeername(socket, (struct sockaddr *) &addr, &(int){sizeof(addr)});
    
    peer_t *peer = calloc(1, sizeof(peer_t));
    peer->socket = socket;
    strcpy(peer->addr, inet_ntoa(addr.sin_addr));
    peer->port = 0;
    socket->data = peer;
    list_add(peers, peer);

    uv_read_start((uv_stream_t *)socket, alloc_read_buffer, on_read);

}

/* Callback for handling the new connection */
static void on_incoming_connection(uv_stream_t *server, int status) {
    
    if (status != 0) {
        printf("error: incoming connection: %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *socket = (uv_tcp_t *) calloc(1, sizeof(uv_tcp_t));
    uv_tcp_init(loop, socket);
    
    if (uv_accept(server, (uv_stream_t *)socket) == 0) {
        create_peer_from_tcp_socket(socket);
        if (handlers[EVENT_CONNECT]) handlers[EVENT_CONNECT]((peer_t *) socket->data, NULL);
    } else {
        printf("connection error\n");
        free(socket);
    }
}

void network_register(uint32_t event, event_handler_t handler) {
    handlers[event] = handler;
}


void on_outgoing_connection(uv_connect_t* connection, int status) {
    
    if (status != 0) {
        struct sockaddr_in *addr = connection->data;
        printf("error: unable to connect to %s:%d\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
        free(connection->data);
        free(connection);
        return;
    }

    uv_tcp_t* socket = (uv_tcp_t *) connection->handle;
    free(connection->data);
    free(connection);

    create_peer_from_tcp_socket(socket);
    if (handlers[EVENT_CONNECT]) handlers[EVENT_CONNECT]((peer_t *) socket->data, NULL);
}

/**
 * Connect asynchronously to the peer with the specified address and port.
 * Once a connection has been established, or failed to be established, the
 * on_outgoing_connect function will be called. 
 * 
 * @param address the ipv4 address of the peer
 * @param port the port of the peer's server
 */
int network_connect(char *address, int port) {
    /* allocate socket for connecting to peer */
    uv_tcp_t *socket = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, socket);

    /* connect asynchronously to node with specified address and port */
    struct sockaddr_in *peer_addr = calloc(1, sizeof(struct sockaddr_in));
    uv_ip4_addr(address, port, peer_addr);
    uv_connect_t *connect = malloc(sizeof(uv_connect_t));
    connect->data = peer_addr;
    int err = uv_tcp_connect(connect, socket, (struct sockaddr*) peer_addr, on_outgoing_connection);
    if (err != 0) free(peer_addr);
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
int network_listen(int port, int backlog) {
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", settings.port, &addr);
    uv_tcp_init(loop, &server);
    uv_tcp_bind(&server, (struct sockaddr *) &addr, 0);
    return uv_listen((uv_stream_t*) &server, backlog, on_incoming_connection);
}