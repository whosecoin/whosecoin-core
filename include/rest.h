#ifndef REST_H
#define REST_H

#include <util/buffer.h>
#include <util/http.h>

typedef struct rest rest_t;
typedef void (*rest_handler)(request_t *req, response_t *res);

rest_t *rest_create();
void rest_register(rest_t *rest, char *pattern, rest_handler handler);
void rest_listen(rest_t *rest, int port);
void rest_destroy(rest_t *rest);


#endif /* REST_H */