#ifndef JSON_H
#define JSON_H

#include <buffer.h>

void json_write_object_start(dynamic_buffer_t *res);
void json_write_object_end(dynamic_buffer_t *res);
void json_write_array_start(dynamic_buffer_t *res);
void json_write_array_end(dynamic_buffer_t *res);
void json_write_number(dynamic_buffer_t *res, double n);
void json_write_string(dynamic_buffer_t *res, char *str);
void json_write_key(dynamic_buffer_t *res, char *str);
void json_write_end(dynamic_buffer_t *res);

#endif /* JSON_H */