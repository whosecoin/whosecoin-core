#include "rest.h"

#include "util/buffer.h"
#include "util/list.h"

#include <uv.h>
#include <assert.h>
#include <stdlib.h>

#define BACKLOG 128

typedef struct rest {
    uv_tcp_t server;
    list_t *patterns;
    list_t *handlers;
} rest_t;

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

void on_write(uv_write_t *req, int status) {
    free(req);
}

void on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    if (nread < 0) {
        uv_close((uv_handle_t*) client, (uv_close_cb) free);
    } else {
        char method[8] = {0};
        char url[2048] = {0};
        sscanf(buf->base, "%7s %2047s ", method, url);

        bool handled = false;
        rest_t *rest = (rest_t*) client->data;
        for (size_t i = 0; i < list_size(rest->patterns); i++) {
            char *pattern = list_get(rest->patterns, i);
            rest_handler handler = list_get(rest->handlers, i);
            request_t *req = request_create(pattern, url);
            if (req != NULL) {
                response_t *res = response_create();
                handler(req, res);
                int status_code = response_get_code(res);
                char *reason = http_reason_phrase(status_code);
                dynamic_buffer_t *body = response_get_body(res);
                char header[2048] = {0};
                sprintf(header, "HTTP/1.0 %d %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\nAccess-Control-Allow-Origin: *\r\n\r\n", status_code, reason, body->length);
                dynamic_buffer_t message = dynamic_buffer_create(32);
                dynamic_buffer_write(header, strlen(header), &message);
                dynamic_buffer_write(body->data, body->length, &message);
                request_destroy(req);
                response_destroy(res);


                uv_write_t *req = (uv_write_t *) malloc(sizeof(uv_write_t));
                uv_buf_t wrbuf = uv_buf_init((char *) message.data, message.length);
                uv_write(req, client, &wrbuf, 1, on_write);
                dynamic_buffer_destroy(message);
                handled = true;
                break;
            }
        }
        
        if (!handled) {
            char header[2048] = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
            uv_write_t *req = (uv_write_t *) malloc(sizeof(uv_write_t));
            uv_buf_t wrbuf = uv_buf_init(header, strlen(header));
            uv_write(req, client, &wrbuf, 1, on_write);
        }
      
    }

    // free data buffer
    free(buf->base);
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        puts("error: unable to handle new connection");
        return;
    }

    uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), client);
    client->data = server->data;
    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        uv_read_start((uv_stream_t*)client, alloc_buffer, on_read);
    } else {
        uv_close((uv_handle_t*) client, (uv_close_cb) free);
    }
}

rest_t *rest_create() {
    rest_t *res = malloc(sizeof(rest_t));
    assert(res != NULL);
    uv_tcp_init(uv_default_loop(), &res->server);
    res->server.data = res;
    res->patterns = list_create(4);
    res->handlers = list_create(4);
    return res;
}

void rest_destroy(rest_t *rest) {
    list_destroy(rest->patterns, NULL);
    list_destroy(rest->handlers, NULL);
    free(rest);
}

void rest_listen(rest_t *rest, int port) {
    assert(rest != NULL);
    struct sockaddr_in addr;
    uv_tcp_init(uv_default_loop(), &rest->server);
    uv_ip4_addr("0.0.0.0", port, &addr);
    uv_tcp_bind(&rest->server, (const struct sockaddr*) &addr, 0);
    int r = uv_listen((uv_stream_t*)&rest->server, BACKLOG, on_new_connection);
    if (r != 0) {
        puts("error: unable to start rest server");
    } else {
        printf("info: accepting http connections on port %d\n", port);
    }
}

void rest_register(rest_t *rest, char *pattern, rest_handler handler) {
    assert(rest != NULL && pattern != NULL && handler != NULL);
    list_add(rest->patterns, pattern);
    list_add(rest->handlers, handler);
}