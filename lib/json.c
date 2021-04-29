#include "json.h"
#include <string.h>
#include <math.h>

void json_write_end(dynamic_buffer_t *res) {
    if (res->data[res->length - 1] == ',') {
        res->length -= 1;
    }
}

void json_write_object_start(dynamic_buffer_t *res) {
    dynamic_buffer_putc('{', res);
}

void json_write_object_end(dynamic_buffer_t *res) {
    if (res->data[res->length - 1] != '{') {
        res->data[res->length - 1] = '}';
    } else {
        dynamic_buffer_putc('}', res);
    }
    dynamic_buffer_putc(',', res);
}

void json_write_array_start(dynamic_buffer_t *res) {
    dynamic_buffer_putc('[', res);
}

void json_write_array_end(dynamic_buffer_t *res) {
    if (res->data[res->length - 1] != '[') {
        res->data[res->length - 1] = ']';
    } else {
        dynamic_buffer_putc(']', res);
    }
    dynamic_buffer_putc(',', res);
}

void json_write_number(dynamic_buffer_t *res, double n) {
    char buffer[64] = {0};
    if (floor(n) == n) {
        sprintf(buffer, "%llu,", (uint64_t) n);
    } else {
        sprintf(buffer, "%lf,", n);
    }
    dynamic_buffer_write(&buffer, strlen(buffer), res);
}

void json_write_string(dynamic_buffer_t *res, char *str) {
    dynamic_buffer_putc('"', res);
    dynamic_buffer_write(str, strlen(str), res);
    dynamic_buffer_putc('"', res);
    dynamic_buffer_putc(',', res);
}

void json_write_key(dynamic_buffer_t *res, char *str) {
    dynamic_buffer_putc('"', res);
    dynamic_buffer_write(str, strlen(str), res);
    dynamic_buffer_putc('"', res);
    dynamic_buffer_putc(':', res);
}
