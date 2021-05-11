#include "util/http.h"
#include "util/buffer.h"
#include "util/list.h"

#include <uv.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define BACKLOG 128
#define MAX_METHOD_LENGTH 8
#define MAX_URL_LENGTH 4096
#define MAX_HEADER_SIZE 16384

/**
 * The http_t struct is a minimal HTTP 1.0 server that enables hosting a simple
 * http API. The http_t struct consists of an TCP server built on top of libuv,
 * and a parallel list of url patterns and handlers. When a request is made,
 * the patterns are checked one by one. As soon as a pattern matches, the 
 * associated handler function is called.
 */
typedef struct http {
    uv_tcp_t server;
    list_t *patterns;
    list_t *handlers;
} http_t;

typedef struct request {
    char *url;
    list_t *params;
} request_t;

typedef struct response {
    dynamic_buffer_t buf;
    int code;
} response_t;

/* An internal struct that holds all data related to an async write */
typedef struct write_context {
  uv_write_t req;
  uv_buf_t buf;
} write_context_t;

/* An internal struct that holds all data related to a client connection */
typedef struct client_context {
    http_t *server;
    dynamic_buffer_t buf;
} client_context_t;

char* http_reason_phrase(int status_code) {
    switch (status_code) {
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Moved Temporarily";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    default: return "Unknown";
    }
}

/* 
 * The on_alloc callback function is called when more data is recieved on
 * a client's tcp stream and is responsible for allocating memory buffer 
 * capable of holding data.
 */
static void on_alloc(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
    buf->base = (char*) malloc(len);
    buf->len = len;
}

/* 
 * The on_write callback function is called when a http response is finished
 * being written to the client's tcp socket.
 */
static void on_write(uv_write_t *write, int status) {
    write_context_t *ctx = (write_context_t *) write;
    free(ctx->buf.base);
    free(ctx);
}

/* 
 * The on_close callback function is called when a client tcp socket is closed
 * and is responsible for cleaning up all client specific memory.
 */
static void on_close(uv_handle_t *handle) {
    client_context_t *ctx = handle->data;
    dynamic_buffer_destroy(ctx->buf);
    free(ctx);
    free(handle);
}

/*
 * Return the end of the http header "\r\n\r\n" or 0 if not found.
 * Note that the end of the header will never be 0 since the double
 * newline is 4 bytes long, so using 0 as a special value will not
 * cause any issues.
 */
static size_t find_header_end(buffer_t *buf) {
    if (buf->length < 4) return 0;
    for (size_t i = 0; i < buf->length - 3; i++) {
        if (memcmp(buf->data + i, "\r\n\r\n", 4) == 0) return i + 4;
    }
    return 0;
}

/*
 * Write an http response status line to the dynamic buffer.
 */
static void write_status_line(int status_code, dynamic_buffer_t *buf) {
    char *reason = http_reason_phrase(status_code);
    assert(strlen(reason) < 64);
    char line[1024] = {0};
    sprintf(line, "HTTP/1.0 %d %s\r\n", status_code, reason);
    dynamic_buffer_write(line, strlen(line), buf);
}

/*
 * Write an http header key-value pair to the dynamic buffer.
 */
static void write_header(char *key, char *val, dynamic_buffer_t *buf) {
    size_t key_len = strlen(key);
    size_t val_len = strlen(val);
    size_t length = key_len + val_len + 8;
    char line[length];
    memset(line, 0, length);
    sprintf(line, "%s: %s\r\n", key, val);
    dynamic_buffer_write(line, strlen(line), buf);
}

static void write_end(dynamic_buffer_t *buf) {
    char *end = "\r\n";
    dynamic_buffer_write(end, strlen(end), buf);
}

static void on_request(uv_stream_t *client, uint8_t *data, size_t length) {
    // parse the method and url from the request.
    char method[MAX_METHOD_LENGTH] = {0};
    char url[MAX_URL_LENGTH] = {0};
    int res = sscanf(data, "%7s %2047s ", method, url);
    if (res != 2) return;
   
    client_context_t *ctx = client->data;
    http_t *http = ctx->server;
    for (size_t i = 0; i < list_size(http->patterns); i++) {
        char *pattern = list_get(http->patterns, i);
        http_handler handler = list_get(http->handlers, i);
        request_t *req = request_create(pattern, url);
        if (req != NULL) {

            response_t *res = response_create();
            handler(req, res);
            
            // calculate the content-length field
            char content_length[64] = {0};
            dynamic_buffer_t *body = response_get_body(res);
            sprintf(content_length, "%d", body->length);

            // write http response header
            dynamic_buffer_t message = dynamic_buffer_create(32);
            write_status_line(response_get_code(res), &message);
            write_header("Content-Type", "application/json", &message);
            write_header("Content-Length", content_length, &message);
            write_header("Access-Control-Allow-Origin", "*", &message);
            write_end(&message);

            // write http response body
            dynamic_buffer_write(body->data, body->length, &message);

            // write http response to client tcp socket
            write_context_t *write = malloc(sizeof(write_context_t));
            write->buf = uv_buf_init((char *) message.data, message.length);
            uv_write(write, client, &write->buf, 1, on_write);

            request_destroy(req);
            response_destroy(res);
            return;
        }
    }
    
    // write http response header
    dynamic_buffer_t message = dynamic_buffer_create(32);
    write_status_line(404, &message);
    write_header("Content-Length", "0", &message);
    write_header("Access-Control-Allow-Origin", "*", &message);
    write_end(&message);

    // write http response to client tcp socket
    write_context_t *write = malloc(sizeof(write_context_t));
    write->buf = uv_buf_init((char *) message.data, message.length);
    uv_write(write, client, &write->buf, 1, on_write);
}

/*
 * The on_read callback function is called when data has been read from a
 * client's tcp socket.
 */
static void on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    // destroy the connection if an error occurs
    if (nread < 0) {
        uv_close((uv_handle_t*) client, on_close);
        free(buf->base);
        return;
    } 

    /* 
     * Write data packet onto the dynamic buffer in the client context.
     * If the packet contains the request header, then handle the request
     * and splice the request header from the dyamic buffer.
     */
    client_context_t *ctx = client->data;
    dynamic_buffer_write(buf->base, buf->len, &ctx->buf);
    free(buf->base);
    size_t end = find_header_end((buffer_t *) &ctx->buf);
    if (end != 0) {
        on_request(client, ctx->buf.data, ctx->buf.length);
        size_t length = ctx->buf.length - end;
        memmove(ctx->buf.data, ctx->buf.data + end, length);
        ctx->buf.length = length;
    }

}

void on_connection(uv_stream_t *server, int status) {
    // error while handling new connection
    if (status < 0) return;

    // initialize client struct
    uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), client);

    // initialize client context struct
    client_context_t *ctx = malloc(sizeof(client_context_t));
    ctx->buf = dynamic_buffer_create(32);
    ctx->server = server->data;
    client->data = ctx;

    // finialize connection and start reading request
    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        uv_read_start((uv_stream_t*) client, on_alloc, on_read);
    } else {
        uv_close((uv_handle_t*) client, on_close);
    }
}

http_t *http_create() {
    http_t *res = malloc(sizeof(http_t));
    assert(res != NULL);
    uv_tcp_init(uv_default_loop(), &res->server);
    res->server.data = res;
    res->patterns = list_create(4);
    res->handlers = list_create(4);
    return res;
}

void http_destroy(http_t *http) {
    list_destroy(http->patterns, NULL);
    list_destroy(http->handlers, NULL);
    free(http);
}

void http_listen(http_t *http, int port) {
    assert(http != NULL);
    struct sockaddr_in addr;
    uv_tcp_init(uv_default_loop(), &http->server);
    uv_ip4_addr("0.0.0.0", port, &addr);
    uv_tcp_bind(&http->server, (const struct sockaddr*) &addr, 0);
    int r = uv_listen((uv_stream_t*) &http->server, BACKLOG, on_connection);
    if (r != 0) {
        puts("error: unable to start http server");
    } else {
        printf("info: accepting http connections on port %d\n", port);
    }
}

void http_register(http_t *http, char *pattern, http_handler handler) {
    assert(http != NULL && pattern != NULL && handler != NULL);
    list_add(http->patterns, pattern);
    list_add(http->handlers, handler);
}


bool resolve_pattern(list_t *params, char *pattern, char *url) {
    if (*pattern == '\0' && *url == '\0') {
        return true;
    } else if (*pattern == '\0' && url[0] == '/' && url[1] == '\0') {
        return true;
    } else if (*url == '\0' && pattern[0] == '/' && pattern[1] == '\0') {
        return true;
    } else if (*pattern == *url) {
        return resolve_pattern(params, pattern + 1, url + 1);
    } else if (*pattern == ':') {
        char *end = strchr(url, '/');
        if (end == NULL) end = strchr(url, '\0');
        char *param = calloc(1, end - url + 1);
        memcpy(param, url, end - url);
        list_add(params, param);
        return resolve_pattern(params, pattern + 1, end);
    } else {
        return false;
    }
}

request_t* request_create(char *pattern, char *url) {
    list_t *params = list_create(2);
    bool match = resolve_pattern(params, pattern, url);
    if (match) {
        request_t *request = malloc(sizeof(request_t));
        assert(request != NULL);
        request->url = url;
        request->params = params;
        return request;
    } else {
       list_destroy(params, free);
        return NULL; 
    }
}

char* request_get_url(request_t *req) {
    return req->url;
}

char* request_get_param(request_t *req, int i) {
    return list_get(req->params, i);
}

void request_destroy(request_t *req) {
    list_destroy(req->params, free);
    free(req);
}


response_t* response_create() {
    response_t *res = malloc(sizeof(response_t));
    assert(res != NULL);
    res->buf = dynamic_buffer_create(32);
    res->code = 200;
    return res;
}

void response_set_code(response_t *res, int code) {
    res->code = code;
}

int response_get_code(response_t *res) {
    return res->code;
}

dynamic_buffer_t* response_get_body(response_t *res) {
    assert(res != NULL);
    return &res->buf;
}

void response_destroy(response_t *res) {
    if (res == NULL) return;
    dynamic_buffer_destroy(res->buf);
    free(res);
}
