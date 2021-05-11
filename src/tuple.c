#include "tuple.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

size_t tuple_size(tuple_t *tuple) {
    assert(tuple != NULL);
    return list_size(tuple->elements);
}

void tuple_destroy(tuple_t *tuple) {
    assert(tuple != NULL);
    for (size_t i = 0; i < list_size(tuple->elements); i++) {
        if (tuple_get_type(tuple, i) == TUPLE_START) {
            tuple_t *sub_tuple = tuple_get_tuple(tuple, i);
            tuple_destroy(sub_tuple);
        }
    }
    list_destroy(tuple->elements, NULL);
    free(tuple);
}

void tuple_element_print(char *element) {
    uint8_t kind = element[0];
    char *data = element + 1;
    switch (kind) {
        case TUPLE_NULL:
            printf("null");
            break;
        case TUPLE_I32:
            printf("%d", ntohl(((int32_t*)data)[0]));
            break;
        case TUPLE_I64:
            printf("%lld", ntohll(((int64_t*)data)[0]));
            break;
        case TUPLE_U32:
            printf("%u", ntohl(((uint32_t*)data)[0]));
            break;
        case TUPLE_U64:
            printf("%llu", ntohll(((uint64_t*)data)[0]));
            break;
        case TUPLE_F32:
            printf("%f", ((float*)data)[0]);
            break;
        case TUPLE_F64:
            printf("%f", ((double*)data)[0]);
            break;    
        case TUPLE_BOOL:
            printf(((uint8_t*)data)[0] ? "true": "false");
            break;
        case TUPLE_STRING:
            printf("'%s'", data);
            break;
        case TUPLE_BINARY: {
            uint32_t binary_size = ((uint32_t*)data)[0];
            char *binary_data = data + sizeof(uint32_t);
            for (size_t i = 0; i < binary_size; i++) {
                printf("%02x", (uint8_t) binary_data[i]);
            }
            break;
        }
        case TUPLE_START:
            tuple_print((tuple_t*) element);
            break;
    }
}

void tuple_print(tuple_t *tuple) {
    printf("(");
    for (size_t i = 0; i < list_size(tuple->elements); i++) {
        if (i != 0) printf(" ");
        char *element = list_get(tuple->elements, i);
        tuple_element_print(element);
    }
    printf(")");
}

tuple_t* tuple_parse(buffer_t *buffer) {

    // empty tuple is two bytes
    if (buffer->data == NULL || buffer->length < 2) return NULL;
    
    // allocate and initialize tuple
    tuple_t *tuple = malloc(sizeof(tuple_t));
    assert(tuple != NULL);
    tuple->kind = TUPLE_START;
    tuple->elements = list_create(1);
    tuple->start = buffer->data;
    
    uint8_t* iter = buffer->data;
    uint8_t* iter_end = iter + buffer->length;
    
    // the first byte must be TUPLE_START
    if (*iter != TUPLE_START) return NULL;
    iter += 1;

    while (iter < iter_end) {
        switch (*iter) {
            case TUPLE_NULL:
                list_add(tuple->elements, iter++);
                break;
            case TUPLE_I32:
                list_add(tuple->elements, iter++);
                iter += sizeof(int32_t);
                break;
            case TUPLE_I64:
                list_add(tuple->elements, iter++);
                iter += sizeof(int64_t);
                break;
            case TUPLE_U32:
                list_add(tuple->elements, iter++);
                iter += sizeof(uint32_t);
                break;
            case TUPLE_U64:
                list_add(tuple->elements, iter++);
                iter += sizeof(uint64_t);
                break;
            case TUPLE_F32:
                list_add(tuple->elements, iter++);
                iter += sizeof(float);
                break;
            case TUPLE_F64:
                list_add(tuple->elements, iter++);
                iter += sizeof(double);
                break;    
            case TUPLE_BOOL:
                list_add(tuple->elements, iter++);
                iter += 1;
                break;
            case TUPLE_STRING:
                list_add(tuple->elements, iter++);
                iter += strlen((char *) iter) + 1;
                break;
            case TUPLE_BINARY: {
                list_add(tuple->elements, iter++);
                uint32_t binary_size = ((uint32_t*)(iter))[0];
                iter += sizeof(uint32_t);
                assert(iter + binary_size < iter_end);
                iter += binary_size;
                break;
            }
            case TUPLE_START: {
                buffer_t buffer = {iter_end - iter, iter};
                tuple_t *sub_tuple = tuple_parse(&buffer);
                if (sub_tuple == NULL) {
                    tuple_destroy(tuple);
                    return NULL;
                }
                list_add(tuple->elements, sub_tuple);
                iter += sub_tuple->length;
                break;
            }
            case TUPLE_END:
                iter++;
                tuple->length = iter - buffer->data;
                return tuple;
        }
    }

    tuple_destroy(tuple);
    return NULL;
}

void tuple_write_i32(dynamic_buffer_t *file, int32_t v) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_I32, file);
    v = htonl(v);
    dynamic_buffer_write(&v, sizeof(v), file);
}

void tuple_write_i64(dynamic_buffer_t *file, int64_t v) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_I64, file);
    v = htonll(v);
    dynamic_buffer_write(&v, sizeof(v), file);
}

void tuple_write_u32(dynamic_buffer_t *file, uint32_t v) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_U32, file);
    v = htonl(v);
    dynamic_buffer_write(&v, sizeof(v), file);
}

void tuple_write_u64(dynamic_buffer_t *file, uint64_t v) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_U64, file);
    v = htonll(v);
    dynamic_buffer_write(&v, sizeof(v), file);
}

void tuple_write_f32(dynamic_buffer_t *file, float v) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_F32, file);
    dynamic_buffer_write(&v, sizeof(v), file);
}

void tuple_write_f64(dynamic_buffer_t *file, double v) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_F64, file);
    dynamic_buffer_write(&v, sizeof(v), file);
}

void tuple_write_bool(dynamic_buffer_t *file, bool v) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_BOOL, file);
    dynamic_buffer_write(&v, sizeof(char), file);
}

void tuple_write_string(dynamic_buffer_t *file, char* v) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_STRING, file);
    dynamic_buffer_write(v, strlen(v) + 1, file);
}

void tuple_write_binary(dynamic_buffer_t *file, size_t s, const uint8_t* v) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_BINARY, file);
    dynamic_buffer_write(&s, sizeof(uint32_t), file);
    dynamic_buffer_write(v, s, file);
}

void tuple_write_start(dynamic_buffer_t* file) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_START, file); 
}

void tuple_write_end(dynamic_buffer_t *file) {
    assert(file != NULL);
    dynamic_buffer_putc(TUPLE_END, file);
}

uint8_t tuple_get_type(const tuple_t *tuple, size_t i) {
    uint8_t *element = list_get(tuple->elements, i);
    return *element;
}

int32_t tuple_get_i32(const tuple_t *tuple, size_t i) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_I32);
    uint8_t *value = element + 1;
    return ntohl(((int32_t*)value)[0]);
}

int64_t tuple_get_i64(const tuple_t *tuple, size_t i) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_I64);
    uint8_t *value = element + 1;
    return ntohll(((int64_t*)value)[0]);  
}

uint32_t tuple_get_u32(const tuple_t *tuple, size_t i) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_U32);
    uint8_t *value = element + 1;
    return ntohl(((uint32_t*)value)[0]);  
}
uint64_t tuple_get_u64(const tuple_t *tuple, size_t i) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_U64);
    uint8_t *value = element + 1;
    return ntohll(((uint64_t*)value)[0]);  
}
float tuple_get_f32(const tuple_t *tuple, size_t i) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_F32);
    uint8_t *value = element + 1;
    return ((float*)value)[0];  
}
double tuple_get_f64(const tuple_t *tuple, size_t i) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_F64);
    uint8_t *value = element + 1;
    return ((double*)value)[0];  
}
int tuple_get_boolean(const tuple_t *tuple, size_t i) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_BOOL);
    uint8_t *value = element + 1;
    return ((uint8_t*)value)[0];  
}
char* tuple_get_string(const tuple_t *tuple, size_t i) {
    char *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_STRING);
    return element + 1;
}

buffer_t tuple_get_binary(const tuple_t *tuple, size_t i) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_BINARY);
    uint32_t *size = (uint32_t*)(element + 1);
    uint8_t *data = (uint8_t *) size + sizeof(uint32_t);
    return (buffer_t) {*size, data};
}

tuple_t* tuple_get_tuple(const tuple_t *tuple, size_t i) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_START);
    return (tuple_t*)element;
}

void tuple_set_i32(tuple_t *tuple, size_t i, int32_t v) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_I32);
    int32_t *value = (int32_t *)(element + 1);
    *value = htonl(v);
}

void tuple_set_i64(tuple_t *tuple, size_t i, int64_t v) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_I64);
    int64_t *value = (int64_t *)(element + 1);
    *value = htonll(v);
}

void tuple_set_u32(tuple_t *tuple, size_t i, uint32_t v) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_U32);
    uint32_t *value = (uint32_t *)(element + 1);;
    *value = htonl(v);
}

void tuple_set_u64(tuple_t *tuple, size_t i, uint64_t v) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_U64);
    uint64_t *value = (uint64_t *)(element + 1);
    *value = htonll(v);
}

void tuple_set_f32(tuple_t *tuple, size_t i, float v) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_F32);
    float *value = (float *)(element + 1);
    *value = v;
}

void tuple_set_f64(tuple_t *tuple, size_t i, double v) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_F64);
    double *value = (double *)(element + 1);
    *value = v; 
}

void tuple_set_bool(tuple_t *tuple, size_t i, bool v) {
    uint8_t *element = list_get(tuple->elements, i);
    assert(*element == TUPLE_F32);
    uint8_t *value = element + 1;
    *value = v;
}
