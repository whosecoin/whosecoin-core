#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct buffer {
    uint32_t length;
    char *data;
} buffer_t;

buffer_t buffer_read(FILE *file);
buffer_t buffer_create(uint32_t length);
buffer_t buffer_copy(buffer_t buffer);
char* buffer_to_hex(buffer_t buffer);
buffer_t buffer_from_hex(char *hex);

bool buffer_is_null(buffer_t buffer);

void buffer_print(buffer_t buffer);
int buffer_compare(buffer_t *b1, buffer_t *b2);
void buffer_destroy(buffer_t buffer);

typedef struct dynamic_buffer {
    uint32_t length;
    char *data;
    uint32_t capacity;
} dynamic_buffer_t;

dynamic_buffer_t dynamic_buffer_create(int initial_capacity);
void dynamic_buffer_write(void *src, size_t n, dynamic_buffer_t *buffer);
void dynamic_buffer_putc(uint8_t c, dynamic_buffer_t *buffer);
void dynamic_buffer_destroy(dynamic_buffer_t buffer);

#endif /* BUFFER_H */