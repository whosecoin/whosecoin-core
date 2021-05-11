#ifndef HTTP_H
#define HTTP_H

#include "buffer.h"

typedef struct request request_t;

request_t* request_create(char *pattern, char *url);
char* request_get_url(request_t *req);
char* request_get_param(request_t *req, int i);
void request_destroy(request_t *request);

typedef struct response response_t;
response_t* response_create();
void response_set_code(response_t *res, int code);
int response_get_code(response_t *res);
dynamic_buffer_t* response_get_body(response_t *res);
void response_destroy(response_t *res);

typedef struct http http_t;
typedef void (*http_handler)(request_t *req, response_t *res);

http_t *http_create();
void http_register(http_t *http, char *pattern, http_handler handler);
void http_listen(http_t *http, int port);
void http_destroy(http_t *http);

#endif /* HTTP_H */