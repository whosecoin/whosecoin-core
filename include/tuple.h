#ifndef TUPLE_H
#define TUPLE_H

#include "util/buffer.h"
#include "util/list.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define TUPLE_START     '('
#define TUPLE_END       ')'
#define TUPLE_I32       'i'
#define TUPLE_I64       'I'
#define TUPLE_U32       'u'
#define TUPLE_U64       'U'
#define TUPLE_F32       'f'
#define TUPLE_F64       'F'
#define TUPLE_BOOL      'b'
#define TUPLE_BINARY    'B'
#define TUPLE_STRING    's'
#define TUPLE_NULL      'n'

typedef struct tuple {
    uint8_t kind;
    list_t *elements;
    uint8_t *start;
    size_t length;
} tuple_t;

void tuple_destroy(tuple_t *tuple);

/**
 * Parse the first valid tuple from the specified buffer. If the buffer does
 * not contain a valid tuple, return NULL. 
 * 
 * @param buffer a buffer of binary data
 * @return the tuple
 */
tuple_t* tuple_parse(buffer_t *buffer);

/**
 * Print a string representation of the tuple to stdout. 
 * 
 * @param tuple the tuple
 */
void tuple_print(tuple_t *tuple);

/**
 * Return the number of elements in the tuple. Note that this value only
 * includes direct children of the tuple. A sub-tuple is counted as a single
 * element regardless of how many children it has.
 * 
 * @param tuple the tuple 
 */
size_t tuple_size(tuple_t *tuple);

void tuple_write_i32(dynamic_buffer_t*, int32_t);
void tuple_write_i64(dynamic_buffer_t*, int64_t);
void tuple_write_u32(dynamic_buffer_t*, uint32_t);
void tuple_write_u64(dynamic_buffer_t*, uint64_t);
void tuple_write_f32(dynamic_buffer_t*, float);
void tuple_write_f64(dynamic_buffer_t*, double);
void tuple_write_boolean(dynamic_buffer_t*, bool);
void tuple_write_string(dynamic_buffer_t*, char*);
void tuple_write_binary(dynamic_buffer_t*, size_t, const uint8_t*);
void tuple_write_start(dynamic_buffer_t*);
void tuple_write_end(dynamic_buffer_t*);

uint8_t tuple_get_type(const tuple_t *tuple, size_t i);
int32_t tuple_get_i32(const tuple_t *tuple, size_t i);
int64_t tuple_get_i64(const tuple_t *tuple, size_t i);
uint32_t tuple_get_u32(const tuple_t *tuple, size_t i);
uint64_t tuple_get_u64(const tuple_t *tuple, size_t i);
float tuple_get_f32(const tuple_t *tuple, size_t i);
double tuple_get_f64(const tuple_t *tuple, size_t i);
int tuple_get_bool(const tuple_t *tuple, size_t i);
char* tuple_get_string(const tuple_t *tuple, size_t i);
buffer_t tuple_get_binary(const tuple_t *tuple, size_t i);
tuple_t* tuple_get_tuple(const tuple_t *tuple, size_t i);

void tuple_set_i32(tuple_t *tuple, size_t i, int32_t v);
void tuple_set_i64(tuple_t *tuple, size_t i, int64_t v);
void tuple_set_u32(tuple_t *tuple, size_t i, uint32_t v);
void tuple_set_u64(tuple_t *tuple, size_t i, uint64_t v);
void tuple_set_f32(tuple_t *tuple, size_t i, float v);
void tuple_set_f64(tuple_t *tuple, size_t i, double v);
void tuple_set_bool(tuple_t *tuple, size_t i, bool v);

#endif /* TUPLE_H */