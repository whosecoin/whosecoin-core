#include "buffer.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void buffer_print(buffer_t buffer) {
    for (uint8_t i = 0; i < buffer.length; i++) {
        printf("%02x", (uint8_t) buffer.data[i]);
    }
}

char* buffer_to_hex(buffer_t buffer) {
    char* res = calloc(1, buffer.length * 2 + 1);
    for (uint8_t i = 0; i < buffer.length; i++) {
        sprintf(res + 2 * i, "%02x", (uint8_t) buffer.data[i]);
    }
    return res;
}

buffer_t buffer_from_hex(char *hex) {
    uint32_t length = strlen(hex) / 2;
    uint8_t* data = malloc(strlen(hex));
    for (uint8_t i = 0; i < length; i++) {
        int x;
        sscanf(hex + 2 * i, "%02x", &x);
        data[i] = x;
    }
    return (buffer_t){length, data};
}

buffer_t buffer_create(uint32_t length) {
    buffer_t result;
    result.length = length;
    result.data = calloc(length, 1);
    assert(result.data != NULL);
    return result;
}

buffer_t buffer_copy(buffer_t buffer) {
    buffer_t result;
    result.length = buffer.length;
    result.data = malloc(buffer.length);
    assert(result.data != NULL);
    memcpy(result.data, buffer.data, buffer.length);
    return result;
}

bool buffer_is_null(buffer_t buffer) {
    for (size_t i = 0; i < buffer.length; i++) {
        if (buffer.data[i] != '\0') return false;
    }
    return true;;
}

buffer_t buffer_read(FILE *file) {

    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // allocate memory to store bson document
    buffer_t result;
    result.data = calloc(1, length + 1);
    result.length = length;
    assert(result.data != NULL);

    // read bson document from file
    size_t n_read = fread(result.data, length, 1, file);
    if (n_read != 1) {
        free(result.data);
        result.data = NULL;
        result.length = 0;
        return result;
    }

    return result;
}

void buffer_destroy(buffer_t buffer) {
    free(buffer.data);
}

int buffer_compare(buffer_t *b1, buffer_t *b2) {
    if (b1->length != b2->length) return b1->length - b2->length;
    for (uint32_t i = 0; i < b1->length; i++) {
        if (b1->data[i] != b2->data[i]) return b1->data[i] - b2->data[i];
    }
    return 0;
}

dynamic_buffer_t dynamic_buffer_create(int initial_capacity) {
    dynamic_buffer_t result;
    result.length = 0;
    result.capacity = initial_capacity < 1 ? 1: initial_capacity;
    result.data = calloc(1, initial_capacity);
    return result;
}

void dynamic_buffer_write(void *src, size_t n, dynamic_buffer_t *buffer) {
    if (buffer->length + n >= buffer->capacity) {
        while (buffer->length + n >= buffer->capacity) buffer->capacity *= 2;
        buffer->data = realloc(buffer->data, buffer->capacity);
        assert(buffer->data != NULL);
    }
    memcpy(buffer->data + buffer->length, src, n);
    buffer->length += n;
}

void dynamic_buffer_putc(uint8_t c, dynamic_buffer_t *buffer) {
    if (buffer->length + 1 >= buffer->capacity) {
        buffer->capacity *= 2;
        buffer->data = realloc(buffer->data, buffer->capacity);
        assert(buffer->data != NULL);
    }
    buffer->data[buffer->length] = c;
    buffer->length += 1;
}

void dynamic_buffer_destroy(dynamic_buffer_t buffer) {
    free(buffer.data);
}