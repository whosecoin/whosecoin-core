#include "http.h"

#include <list.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

struct request {
    char *url;
    list_t *params;
};

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

typedef struct response {
    dynamic_buffer_t buf;
    int code;
} response_t;

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