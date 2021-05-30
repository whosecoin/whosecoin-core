#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

/**
 * The heap_t type is a priority queue implemented as a complete binary tree.
 * The top element in the heap is the smallest element as determined by
 * the comparator_t passed in the constructor.
 */
typedef struct heap heap_t;

/**
 * A function type that compares two elements a and b and returns a
 * negative number if a < b, returns a positive number if a > b, and
 * returns 0 if a == b.
 */
typedef int (*comparator_t)(void*, void*);

/**
 * A function type that destroys an object and frees all associated memory.
 */
typedef void (*destructor_t)(void*);

/**
 * Construct a new binary heap with the given callback to compare and destroy
 * elements.
 * 
 * @param cmp a comparator_t to compare elements
 * @param free a destructor_t to free elements
 * @return the heap
 */
heap_t* heap_create(comparator_t e_compare, destructor_t e_destroy);

/**
 * Destroy a binary heap by destroying all elements contained in the heap
 * and freeing all associated memory.
 * 
 * @param self the heap
 */
void heap_destroy(heap_t *self);

/**
 * Return the number of elements in the binary heap.
 * 
 * @param self the heap
 * @return the number of elements in the heap
 */
size_t heap_size(heap_t *self);

/**
 * Return the ith elements in the heap.
 * 
 * @param self the heap
 * @param i the element index
 * @return a the ith element.
 */
void* heap_get(heap_t *self, size_t i);

/**
 * Add the given element to the heap.
 * 
 * @param self the heap
 * @param element the element to add
 */
void heap_add(heap_t *self, void *element);

/**
 * Remove and return the ith element in the heap.
 * 
 * @param self the heap
 * @param i the element index
 * @return a the ith element.
 */
void* heap_remove(heap_t *self, size_t i);

/**
 * Return the top element in the heap.
 * 
 * @param self the heap
 * @return the top element
 */
void* heap_top(heap_t *self);

/**
 * Remove and return the top element in the heap.
 * 
 * @param self the heap
 * @return the top element.
 */
void* heap_pop(heap_t *self);

#endif /* HEAP_H */