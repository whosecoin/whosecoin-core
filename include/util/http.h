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

char* http_reason_phrase(int status_code);

#endif /* HTTP_H */