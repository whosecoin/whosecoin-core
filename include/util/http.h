#ifndef HTTP_H
#define HTTP_H

#include "buffer.h"

/**
 * The http_t struct is a single-threaded http 1.0 server built on 
 * top of libuv that allows construction of basic REST APIs. Dynamic
 * endpoints can be registered with a syntax similar to the express.js
 * library.
 */
typedef struct http http_t;

typedef struct request request_t;
typedef struct response response_t;
typedef void (*http_handler)(request_t *req, response_t *res);

/**
 * Create a new HTTP server. Before the server can be used, endpoint handlers
 * must be registered with the http_register method and the server must be
 * started with the http_listen method. When the server is no longer in use,
 * it should be destroyed with the http_destroy method.
 * 
 * @return the server
 */
http_t* http_create();

/**
 * Register an endpoint handler that will be triggered by any request with a url
 * that matches the provided pattern. For instance the pattern "/foo" with match
 * the url "/foo" and "/foo/". The wildcard character ':' will match any path up
 * to the next '/'. For example, the pattern "/foo/:" will match "/foo/bar",
 * "/foo/baz", etc. However, the pattern will not match "/foo/bar/baz".
 * 
 * @param http the server
 * @param pattern the pattern to be matched
 * @param handler the handler to be called for any requests that match pattern
 */
void http_register(http_t *http, char *pattern, http_handler handler);

/**
 * Start listening for incoming connections on the specified port.
 * 
 * @param http the server
 * @param port the port
 */
void http_listen(http_t *http, int port);

/**
 * Destroy the server and free all associated memory.
 * 
 * @param http the server
 */
void http_destroy(http_t *http);

/**
 * If the url matches the pattern, as described in http_register, create a
 * new request object with the given url.
 * 
 * @param pattern the pattern to be matched
 * @param url the url to match to pattern
 * @return the request or null
 */
request_t* request_create(char *pattern, char *url);

/**
 * Return the url associated with the request.
 * @param req the request
 * @return the url string
 */
char* request_get_url(request_t *req);

/**
 * Return the url parameter that matched the ith wildcard in the pattern
 * that was used to create the request.
 * 
 * @param req the request
 * @param i the parameter index.
 * @return the patameter string
 */
char* request_get_param(request_t *req, int i);

/**
 * Destroy the request and free all memory owned by the request.
 * @param request the request.
 */
void request_destroy(request_t *request);

/**
 * Create a new empty response.
 * @return the response.
 */
response_t* response_create();

/**
 * Set the status code of the response. The status code should be one of
 * the standard HTTP status codes (2XX for success, 4XX for failure, etc).
 * 
 * @param res the response
 * @param code the code
 */
void response_set_code(response_t *res, int code);

/**
 * Return the status code of the response.
 * @param res the response
 * @return the status code.
 */
int response_get_code(response_t *res);

/**
 * Return a pointer to the dynamic buffer associated with
 * the response body. Endpoint handlers should write body data directly
 * to this dynamic buffer.
 * 
 * @param res the response
 * @return a pointer to the body dynamic buffer.
 */
dynamic_buffer_t* response_get_body(response_t *res);

/**
 * Destroy the response object and free all associated memory.
 * @param res the response
 */
void response_destroy(response_t *res);

#endif /* HTTP_H */